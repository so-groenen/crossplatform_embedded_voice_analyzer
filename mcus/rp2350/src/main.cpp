
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
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

 



#define TEST_SIZE 1024

enum class DmaDir 
{
    Tx, 
    Rx
};

class SpiDMA 
{
    uint m_dma{};
    dma_channel_config m_config{};
    spi_inst_t* m_spi{};
public:
    SpiDMA(spi_inst_t* spi)
        : m_dma(dma_claim_unused_channel(true)), m_config{dma_channel_get_default_config(m_dma)}, m_spi{spi}//, m_buffer(buffer)
    {
        channel_config_set_transfer_data_size(&m_config, DMA_SIZE_8);
    }

    void enable_interrupt(irq_handler_t handler)
    {
        dma_channel_set_irq0_enabled(m_dma, true);

        // Register our callback function to the CPU vector table for DMA_IRQ_0
        irq_set_exclusive_handler(DMA_IRQ_0, handler);
        irq_set_enabled(DMA_IRQ_0, true);
    }
    void disable_interrupt()
    {
        irq_set_enabled(DMA_IRQ_0, false);
        dma_channel_set_irq0_enabled(m_dma, false);
        irq_set_enabled(DMA_IRQ_0, true);
    }

    void prepare_tx(uint8_t* buff_to_send, size_t len)
    {   
        channel_config_set_dreq(&m_config, spi_get_dreq(m_spi, true));
        // Default:
        // channel_config_set_read_increment(&m_config, true);
        // channel_config_set_write_increment(&m_config, false);
        dma_channel_configure(m_dma, &m_config,
                                &spi_get_hw(m_spi)->dr, // write address
                                buff_to_send,                 // read address from memory
                                len,                          // element count (each element is of size transfer_data_size)
                                false);                       // don't start yet

    }
    void prepare_rx(uint8_t* buff_for_rx, size_t len)
    {
        channel_config_set_dreq(&m_config, spi_get_dreq(m_spi, /*is_tx=*/false));
        channel_config_set_read_increment(&m_config, false);
        channel_config_set_write_increment(&m_config, true);
        dma_channel_configure(m_dma, &m_config,
                            buff_for_rx,                  // write address in memory
                            &spi_get_hw(m_spi)->dr, // read from periph address
                            len,                          // m_spi count (each element is of size transfer_data_size)
                            false);                       // don't start yet
 
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
    ~SpiDMA()
    {
        dma_channel_unclaim(m_dma);
    }
    static void start_multiple(std::span<SpiDMA*> instances)
    {
        uint32_t mask = 0;
        for(const auto& s : instances)
        {
            mask |= (1u << s->m_dma);
        }
        dma_start_channel_mask(mask);
    }
    void acknowledge_irq()
    {
        dma_hw->ints0 = (1u << m_dma);
    }
    bool is_irq_event() const
    {
        return (dma_hw->ints0 & (1u << m_dma));
    } 
};

SpiDMA* rx_ptr{};
static bool has_fired{};

void rx_spi_isr()
{
    if (rx_ptr->is_irq_event())
    {
        rx_ptr->acknowledge_irq();
        has_fired = true;
        printf(" >>*<< \n");
    }
}

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

     

    SpiDMA spi_tx{spi_default};
    SpiDMA spi_rx{spi_default};

    rx_ptr = &spi_rx;
    rx_ptr->enable_interrupt(rx_spi_isr);

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
        spi_tx.prepare_tx(txbuf, TEST_SIZE);
 
        printf("Configure RX DMA\n");
        spi_rx.prepare_rx(rxbuf, TEST_SIZE);

        printf("Starting DMAs...\n");
        // start them exactly simultaneously to avoid races (in extreme cases the FIFO could overflow)

        SpiDMA* instances[] = {&spi_rx, &spi_tx};
        SpiDMA::start_multiple(instances);

        printf("Wait for RX complete...\n");
        spi_rx.await_blocking();

        while(!has_fired)
        {

        }
        has_fired = false;

        printf("has_fired == true!\n");

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