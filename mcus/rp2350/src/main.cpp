
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/intctrl.h>
#include <limits>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "FreeRTOS/Mutex.hpp"
#include "FreeRTOSConfig.h"
#include "drivers/gpio.hpp"

#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"
#include "drivers/exti.hpp"
#include "projdefs.h"
#include "drivers/exti_notifier.hpp"
#include "stats.hpp"
#include "thread_safe_ctx.hpp"
 
// constexpr uint16_t PIN_16{16};
// constexpr size_t num_buttons{2};
// constexpr size_t stack_button{512};
 
// bind_interrupt_EXTI(exti_irq, exti_handler, num_buttons);

// ThreadSafeCtx g_thread_safe_ctx{};

 
struct SpiConfig 
{
    uint32_t frequency;
    uint32_t phase;
    uint32_t polarity;

};


#define TEST_SIZE 1024

enum class DmaDir 
{
    Tx, 
    Rx
};

class Dma;

// unblock threads from interrupts
// should have pointer to thread
// and to Dma to acknowledge interrupt;
// also enable interrupt
// class DmaNotifier; 
// {

// };
 
class IDmaIrqHandler
{
public:
    virtual ~IDmaIrqHandler() = default;
    // virtual void enable_interrupt() = 0;
    virtual void enable_interrupt(Dma* dma) = 0;
};

class Dma 
{
    uint m_dma{};// requires(hasDMA);
    dma_channel_config m_config{};
    // irq_num_rp2350 m_dma_irq{};
    static constexpr uint moved_dma_channel{std::numeric_limits<uint>::max()};
public:
    explicit Dma()
        : m_dma(dma_claim_unused_channel(true)), m_config{dma_channel_get_default_config(m_dma)}//, m_dma_irq{dma_irq}
    {
        channel_config_set_transfer_data_size(&m_config, DMA_SIZE_8);
    }

    explicit Dma(IDmaIrqHandler& irq_handler)
        : m_dma(dma_claim_unused_channel(true)), m_config{dma_channel_get_default_config(m_dma)}//, m_dma_irq{dma_irq}
    {
        channel_config_set_transfer_data_size(&m_config, DMA_SIZE_8);
        irq_handler.enable_interrupt(this);
        // enable_interrupt(handler);
    }
    Dma(const Dma& other) = delete;
    Dma& operator=(Dma& other) = delete;

    Dma(Dma&& other)
        : m_dma{other.m_dma}, m_config{other.m_config}//, m_dma_irq{other.m_dma_irq}
    {
        // make it faulty to avoid freeing!
        other.m_dma = moved_dma_channel;
    }
    Dma& operator=(Dma&& other)
    {
        if(&other != this)
        {
            m_dma       = other.m_dma;
            other.m_dma = moved_dma_channel;

            m_config  = other.m_config;
            // m_dma_irq = other.m_dma_irq;
        }
        return *this;
    }

    

    // void enable_interrupt(irq_handler_t handler)
    // {
    //     dma_channel_set_irq0_enabled(m_dma_irq, true);

    //     // Register our callback function to the CPU vector table for DMA_IRQ_0
    //     irq_set_exclusive_handler(m_dma_irq, handler);
    //     irq_set_enabled(m_dma_irq, true);
    // }
    // void disable_interrupt()
    // {
    //     irq_set_enabled(m_dma_irq, false);
    //     dma_channel_set_irq0_enabled(m_dma_irq, false);
    //     irq_set_enabled(m_dma_irq, true);
    // }
    // read from SRAM memory and write it to write_addr (for ex SPI's FIFO / Data register or another memory location)

    void write(std::span<uint8_t> buff_to_send, volatile void * write_addr, uint dreq, bool start)
    {
        channel_config_set_dreq(&m_config, dreq);
        // Default:
        channel_config_set_read_increment(&m_config, true);
        channel_config_set_write_increment(&m_config, false);
        dma_channel_configure(m_dma, &m_config,
                                write_addr,           // write address
                                buff_to_send.data(),  // read address from memory
                                buff_to_send.size(),  // element count (each element is of size transfer_data_size)
                                start);     
    }
    // read from read_addr (for ex Spi-data register) and write into rx_buffer
    void read(std::span<uint8_t> rx_buff, const volatile void * read_addr, uint dreq, bool start)
    {
        channel_config_set_dreq(&m_config, dreq);
        channel_config_set_read_increment(&m_config, false);
        channel_config_set_write_increment(&m_config, true);
        dma_channel_configure(m_dma, &m_config,
                            rx_buff.data(),            // write address in memory
                            read_addr,                 // read from periph address
                            rx_buff.size(),            // m_spi count (each element is of size transfer_data_size)
                            false);                    // don't start yet
    }
    void start()
    {
        dma_channel_start(m_dma);
    }
    uint channel() const 
    {
        return m_dma;
    }
    void await_blocking()
    {
        dma_channel_wait_for_finish_blocking(m_dma);
    }
    bool is_busy() const 
    {
        return dma_channel_is_busy(m_dma);
    }
    ~Dma()
    {
        if(m_dma != moved_dma_channel)
            dma_channel_unclaim(m_dma);
    }
    static void start_multiple(std::span<Dma*> instances)
    {
        uint32_t mask = 0;
        for(const auto& s : instances)
        {
            mask |= (1u << s->m_dma);
        }
        dma_start_channel_mask(mask);
    }
    void handle_isr()
    {
        if(is_irq_event_core0())
        {
            acknowledge_irq_core0();
            printf(">>> Hello from interrupt!\n");
        }
    }
    void acknowledge_irq_core0()
    {
        dma_hw->ints0 = (1u << m_dma);
    }
    bool is_irq_event_core0() const
    {
        return (dma_hw->ints0 & (1u << m_dma));
    } 
    // friend class IDmaIrqHandler;
};


class DmaIrqHandler : public IDmaIrqHandler
{
    irq_handler_t m_handler{};
    irq_num_rp2350 m_irqn{};
    uint m_dma_chan{};
    // std::array<Dma*, 5> m_dma_insts

    void acknowledge_irq_core0()
    {
        dma_hw->ints0 = (1u << m_dma_chan);
    }
    bool is_irq_event_core0() const
    {
        return (dma_hw->ints0 & (1u << m_dma_chan));
    } 
public:
    explicit DmaIrqHandler(irq_handler_t handler, irq_num_rp2350 irqn)
        : m_handler(handler), m_irqn(irqn)
    {
    }
    void enable_interrupt(Dma* dma) override
    {
        m_dma_chan = dma->channel();
        dma_channel_set_irq0_enabled(m_dma_chan, true);
        
        // Register our callback function to the CPU vector table for DMA_IRQ_0 etc...
        irq_set_exclusive_handler(m_irqn, m_handler);
        irq_set_enabled(m_irqn, true);
    }
    void handle_isr()
    {
        if(is_irq_event_core0())
        {
            acknowledge_irq_core0();
            printf(">>> Hello from interrupt!\n");
        }
    }

    // void disable_interrupt()
    // {
    //     irq_set_enabled(m_dma_irq, false);
    //     dma_channel_set_irq0_enabled(m_dma_chan, false);
    //     irq_set_enabled(m_dma_irq, true);
    // }
};



class SpiAsync
{
    Dma m_dma;
    spi_inst_t* m_spi{};
public:
    SpiAsync(spi_inst_t* spi, Dma dma)
        : m_dma{std::move(dma)}, m_spi{spi}//, m_buffer(buffer)
    {
    }
    void write_async(std::span<uint8_t> buff_to_send, bool start)
    {   
        m_dma.write(buff_to_send, &spi_get_hw(m_spi)->dr, spi_get_dreq(m_spi, true), start);
    }
    void expose_dma(Dma*& other)
    {
        other = &m_dma;
    }
    void read_async(std::span<uint8_t> rx_buff, bool start)
    {
        m_dma.read(rx_buff, &spi_get_hw(m_spi)->dr, spi_get_dreq(m_spi, /*is_tx=*/false), start);
    }
    void start()
    {
        m_dma.start();
    }
    uint channel() const 
    {
        return m_dma.channel();
    }
    void await_blocking()
    {
        return m_dma.await_blocking();
    }
    bool is_busy() const 
    {
        return m_dma.is_busy();
    }
    static void start_multiple(std::span<SpiAsync*> instances)
    {
        uint32_t mask = 0;
        for(const auto& s : instances)
        {
            mask |= (1u << s->channel());
        }
        dma_start_channel_mask(mask);
    }
};

// class Handler;

Dma* dma_rx_ptr{};
// static bool has_fired{};

void rx_spi_isr()
{
    if (dma_rx_ptr->is_irq_event_core0())
    {
        dma_rx_ptr->acknowledge_irq_core0();
        // has_fired = true;
        printf(" >>*<< \n");
    }
}


// void my_dma_isr();

// static DmaIrqHandler dma_irq_handler{my_dma_isr, DMA_IRQ_0};

// void my_dma_isr()
// {
//     dma_irq_handler.handle_isr();
// }

// bind_interrupt_DMA(dma_rx, rx_handler, DMA_IRQ_0);


#define bind_interrupt_DMA(irqs, handler_name, IRQ_n)              \
    void handler_name();                                           \
    [[maybe_unused]] static DmaIrqHandler irqs{handler_name, IRQ_n}; \
    void handler_name() {                                          \
        irqs.handle_isr();                                         \
    }                                           

bind_interrupt_DMA(dma_irqs, my_handler, DMA_IRQ_0);


int main() 
{
    // Enable UART so we can print status output
    stdio_init_all();
// #if !defined(spi_default) || !defined(PICO_DEFAULT_SPI_SCK_PIN) || !defined(PICO_DEFAULT_SPI_TX_PIN) || !defined(PICO_DEFAULT_SPI_RX_PIN) || !defined(PICO_DEFAULT_SPI_CSN_PIN)
// #warning spi/spi_dma example requires a board with SPI pins
// //     puts("Default SPI pins were not defined");
// #else

    printf("SPI DMA example\n");

//     // Enable SPI at 1 MHz and connect to GPIOs
    spi_init(spi_default, 1000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
//     // Make the SPI pins available to picotool
//     bi_decl(bi_3pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));
//     // Make the CS pin available to picotool
//     bi_decl(bi_1pin_with_name(PICO_DEFAULT_SPI_CSN_PIN, "SPI CS"));

//     // Grab some unused dma channels
    // const uint dma_tx = dma_claim_unused_channel(true);
    // const uint dma_rx = dma_claim_unused_channel(true);

//     // Force loopback for testing (I don't have an SPI device handy)
    hw_set_bits(&spi_get_hw(spi_default)->cr1, SPI_SSPCR1_LBM_BITS);

    static uint8_t txbuf[TEST_SIZE];
    static uint8_t rxbuf[TEST_SIZE];
    for (uint i = 0; i < TEST_SIZE; ++i) {
        txbuf[i] = rand();
    }

     
    SpiAsync spi_tx{spi_default, Dma{}};
    SpiAsync spi_rx{spi_default, Dma{dma_irqs}};


    sleep_ms(3000);
    printf("STARTING NOW\n");
    sleep_ms(1000);
    printf("3\n");
    sleep_ms(1000);
    printf("2\n");
    sleep_ms(1000);
    printf("1\n");
    sleep_ms(1000);
    // We set the outbound DMA to transfer from a memory buffer to the SPI transmit FIFO paced by the SPI TX FIFO DREQ
    // The default is for the read address to increment every element (in this case 1 byte = DMA_SIZE_8)
    // and for the write address to remain unchanged.
    while(true)
    {
        printf("Configure TX DMA\n");
        spi_tx.write_async(txbuf, false);
 
        printf("Configure RX DMA\n");
        spi_rx.read_async(rxbuf, false);

        printf("Starting DMAs...\n");
        // start them exactly simultaneously to avoid races (in extreme cases the FIFO could overflow)

        SpiAsync* instances[] = {&spi_rx, &spi_tx};
        SpiAsync::start_multiple(instances);

        printf("Wait for RX complete...\n");
        spi_rx.await_blocking();


        if (spi_tx.is_busy())
        {
            panic("RX completed before TX");
        }
 
        printf("Done. Checking...");
        for (uint i = 0; i < TEST_SIZE; ++i) {
            if (rxbuf[i] != txbuf[i]) {
                panic("Mismatch at %d/%d: expected %02x, got %02x",
                    i, TEST_SIZE, txbuf[i], rxbuf[i]
                );
            }
        }

        printf("All good\n");
        printf("Next in 3s\n");
        sleep_ms(3000);
    }
    
}