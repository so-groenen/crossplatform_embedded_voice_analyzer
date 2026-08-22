
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
#include <hardware/sync.h>
#include <limits>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
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
#include "drivers/dma.hpp"
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

#include "drivers/dma_raw.hpp"
#include "drivers/dma_irq_handler.hpp"

#define TEST_SIZE 1024

 
// class DmaBase;

// // unblock threads from interrupts
// // should have pointer to thread
// // and to Dma to acknowledge interrupt;
// // also enable interrupt

// namespace BareMetal
// {
//     class IDmaIrqHandler
//     {
//     public:
//         virtual ~IDmaIrqHandler() = default;
//         // virtual void enable_interrupt() = 0;
//         virtual void enable_interrupt(DmaBase* dma) = 0;
//         virtual void handle_isr() = 0;
//     };
// }

// namespace RTOS 
// {
//     class IDmaIrqHandler : public BareMetal::IDmaIrqHandler
//     {
//     public:
//         virtual ~IDmaIrqHandler() = default;
//         // virtual void enable_interrupt() = 0;
//         virtual void enable_interrupt(DmaBase* dma, PicoTask* task) = 0;
//     };
// }




// class Dma 
// {
//     uint m_dma{}; 
//     dma_channel_config m_config{};
//     static constexpr uint moved_dma_channel{std::numeric_limits<uint>::max()};
// public:
//     explicit Dma()
//         : m_dma(dma_claim_unused_channel(true)), m_config{dma_channel_get_default_config(m_dma)}//, m_dma_irq{dma_irq}
//     {
//         channel_config_set_transfer_data_size(&m_config, DMA_SIZE_8);
//     }

//     explicit Dma(IDmaIrqHandler& irq_handler)
//         : m_dma(dma_claim_unused_channel(true)), m_config{dma_channel_get_default_config(m_dma)}//, m_dma_irq{dma_irq}
//     {
//         channel_config_set_transfer_data_size(&m_config, DMA_SIZE_8);
//         irq_handler.enable_interrupt(this);
//         // enable_interrupt(handler);
//     }
//     Dma(const Dma& other) = delete;
//     Dma& operator=(Dma& other) = delete;

//     Dma(Dma&& other)
//         : m_dma{other.m_dma}, m_config{other.m_config}//, m_dma_irq{other.m_dma_irq}
//     {
//         // make it faulty to avoid freeing!
//         other.m_dma = moved_dma_channel;
//     }
//     Dma& operator=(Dma&& other)
//     {
//         if(&other != this)
//         {
//             m_dma       = other.m_dma;
//             other.m_dma = moved_dma_channel;

//             m_config  = other.m_config;
//             // m_dma_irq = other.m_dma_irq;
//         }
//         return *this;
//     }
 
//     void write(std::span<uint8_t> buff_to_send, volatile void * write_addr, uint dreq, bool start)
//     {
//         channel_config_set_dreq(&m_config, dreq);
//         // Default:
//         channel_config_set_read_increment(&m_config, true);
//         channel_config_set_write_increment(&m_config, false);
//         dma_channel_configure(m_dma, &m_config,
//                                 write_addr,           // write address
//                                 buff_to_send.data(),  // read address from memory
//                                 buff_to_send.size(),  // element count (each element is of size transfer_data_size)
//                                 start);     
//     }
//     // read from read_addr (for ex Spi-data register) and write into rx_buffer
//     void read(std::span<uint8_t> rx_buff, const volatile void * read_addr, uint dreq, bool start)
//     {
//         channel_config_set_dreq(&m_config, dreq);
//         channel_config_set_read_increment(&m_config, false);
//         channel_config_set_write_increment(&m_config, true);
//         dma_channel_configure(m_dma, &m_config,
//                             rx_buff.data(),            // write address in memory
//                             read_addr,                 // read from periph address
//                             rx_buff.size(),            // m_spi count (each element is of size transfer_data_size)
//                             false);                    // don't start yet
//     }
//     void start()
//     {
//         dma_channel_start(m_dma);
//     }
//     uint channel() const 
//     {
//         return m_dma;
//     }
//     void await_blocking()
//     {
//         dma_channel_wait_for_finish_blocking(m_dma);
//     }
//     bool is_busy() const 
//     {
//         return dma_channel_is_busy(m_dma);
//     }
//     ~Dma()
//     {
//         if(m_dma != moved_dma_channel)
//             dma_channel_unclaim(m_dma);
//     }
//     static void start_multiple(std::span<Dma*> instances)
//     {
//         uint32_t mask = 0;
//         for(const auto& s : instances)
//         {
//             mask |= (1u << s->m_dma);
//         }
//         dma_start_channel_mask(mask);
//     }
//     // void handle_isr()
//     // {
//     //     if(is_irq_event_core0())
//     //     {
//     //         acknowledge_irq_core0();
//     //         printf(">>> Hello from interrupt!\n");
//     //     }
//     // }
//     // void acknowledge_irq_core0()
//     // {
//     //     dma_hw->ints0 = (1u << m_dma);
//     // }
//     // bool is_irq_event_core0() const
//     // {
//     //     return (dma_hw->ints0 & (1u << m_dma));
//     // } 
//     // friend class IDmaIrqHandler;
// };




// class DmaBase
// {
// protected:
//     uint m_dma{}; 
//     dma_channel_config m_config{};
//     static constexpr uint moved_dma_channel{std::numeric_limits<uint>::max()};
// public:
//     explicit DmaBase(uint channel)
//         : m_dma{channel}, m_config{dma_channel_get_default_config(m_dma)}//, m_dma_irq{dma_irq}
//     {
//         dma_channel_claim(m_dma);
//         channel_config_set_transfer_data_size(&m_config, DMA_SIZE_8);
//     }
//     DmaBase(const DmaBase& other) = delete;
//     DmaBase& operator=(DmaBase& other) = delete;

//     DmaBase(DmaBase&& other)
//         : m_dma{other.m_dma}, m_config{other.m_config}//, m_dma_irq{other.m_dma_irq}
//     {
//         // make it faulty to avoid freeing!
//         other.m_dma = moved_dma_channel;
//     }
//     DmaBase& operator=(DmaBase&& other)
//     {
//         if(&other != this)
//         {
//             m_dma       = other.m_dma;
//             other.m_dma = moved_dma_channel;

//             m_config  = other.m_config;
//             // m_dma_irq = other.m_dma_irq;
//         }
//         return *this;
//     }
 
//     void write(std::span<uint8_t> buff_to_send, volatile void * write_addr, uint dreq, bool start)
//     {
//         channel_config_set_dreq(&m_config, dreq);
//         // Default:
//         channel_config_set_read_increment(&m_config, true);
//         channel_config_set_write_increment(&m_config, false);
//         dma_channel_configure(m_dma, &m_config,
//                                 write_addr,           // write address
//                                 buff_to_send.data(),  // read address from memory
//                                 buff_to_send.size(),  // element count (each element is of size transfer_data_size)
//                                 start);     
//     }
//     // read from read_addr (for ex Spi-data register) and write into rx_buffer
//     void read(std::span<uint8_t> rx_buff, const volatile void * read_addr, uint dreq, bool start)
//     {
//         channel_config_set_dreq(&m_config, dreq);
//         channel_config_set_read_increment(&m_config, false);
//         channel_config_set_write_increment(&m_config, true);
//         dma_channel_configure(m_dma, &m_config,
//                             rx_buff.data(),            // write address in memory
//                             read_addr,                 // read from periph address
//                             rx_buff.size(),            // m_spi count (each element is of size transfer_data_size)
//                             false);                    // don't start yet
//     }
//     void start()
//     {
//         dma_channel_start(m_dma);
//     }
//     uint channel() const 
//     {
//         return m_dma;
//     }
//     void await_blocking()
//     {
//         dma_channel_wait_for_finish_blocking(m_dma);
//     }
//     bool is_busy() const 
//     {
//         return dma_channel_is_busy(m_dma);
//     }
//     ~DmaBase()
//     {
//         if(m_dma != moved_dma_channel)
//             dma_channel_unclaim(m_dma);
//     }
//     static void start_multiple(std::span<DmaBase*> instances)
//     {
//         uint32_t mask = 0;
//         for(const auto& s : instances)
//         {
//             mask |= (1u << s->m_dma);
//         }
//         dma_start_channel_mask(mask);
//     }
//     // void handle_isr()
//     // {
//     //     if(is_irq_event_core0())
//     //     {
//     //         acknowledge_irq_core0();
    //         printf(">>> Hello from interrupt!\n");
    //     }
    // }
    // void acknowledge_irq_core0()
    // {
    //     dma_hw->ints0 = (1u << m_dma);
    // }
    // bool is_irq_event_core0() const
    // {
    //     return (dma_hw->ints0 & (1u << m_dma));
    // } 
    // friend class IDmaIrqHandler;
// };

// class DmaBareMetal : public DmaBase
// {
// public:
//     explicit DmaBareMetal(uint channel, BareMetal::IDmaIrqHandler& irq_handler)
//         : DmaBase(channel)
//     {
//         irq_handler.enable_interrupt(this);
//     }
// };




// enum class DmaIrq : uint32_t
// {
//     IRQ_0 = DMA_IRQ_0,
//     IRQ_1 = DMA_IRQ_1, 
//     IRQ_2 = DMA_IRQ_2,
//     IRQ_3 = DMA_IRQ_3,
// };


// template<DmaIrq IRQ_n>
// class DmaIrqHandler //: public IDmaIrqHandler
// {
// protected:
//     irq_handler_t m_handler{};
//     uint m_dma_chan{};
//     // std::array<Dma*, 5> m_dma_insts
//     bool is_event() const
//     {
//         if constexpr (IRQ_n == DmaIrq::IRQ_0)
//         {
//             return(dma_hw->ints0 & (1u << m_dma_chan));
//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_1)
//         {
//             return(dma_hw->ints1 & (1u << m_dma_chan));

//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_2)
//         {
//             return(dma_hw->ints2 & (1u << m_dma_chan));

//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_3)
//         {
//             return(dma_hw->ints3 & (1u << m_dma_chan));
//         }
//     }
//     void acknowledge_isr()
//     {
//         if constexpr (IRQ_n == DmaIrq::IRQ_0)
//         {
//             dma_hw->ints0 = (1u << m_dma_chan);
//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_1)
//         {
//             dma_hw->ints1 = (1u << m_dma_chan);
//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_2)
//         {
//             dma_hw->ints2 = (1u << m_dma_chan);
//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_3)
// //         {
// //             dma_hw->ints3 = (1u << m_dma_chan);
// //         }
// //     }
// // public:
// //     explicit DmaIrqHandler(irq_handler_t handler)
// //         : m_handler(handler)//, m_irqn(irqn)
// //     {
// //     }
// //     void enable_interrupt(DmaBase* dma) override
// //     {
// //         m_dma_chan = dma->channel();
// //         dma_channel_set_irq0_enabled(m_dma_chan, true);
        
// //         // Register our callback function to the CPU vector table for DMA_IRQ_0 etc...
// //         irq_set_exclusive_handler(static_cast<uint32_t>(IRQ_n), m_handler);
// //         irq_set_enabled(static_cast<uint32_t>(IRQ_n), true);
// //     }
// //     void handle_isr() override
// //     {   
// //         if(is_event())
// //         {
// //             acknowledge_isr();
// //         }
// //     }

// //     // void disable_interrupt()
// //     // {
// //     //     irq_set_enabled(m_dma_irq, false);
// //     //     dma_channel_set_irq0_enabled(m_dma_chan, false);
// //     //     irq_set_enabled(m_dma_irq, true);
// //     // }
// // };
// template <typename T, size_t Capacity>
// class HeaplessVector 
// {
// private:
//     std::array<T, Capacity> m_data{};
//     size_t m_size{0};

// public:
//     // Adds an element to the end of the vector
//     bool push_back(const T& value) 
//     {
//         if (m_size >= Capacity) 
//         {
//             return false;
//         }
//         m_data[m_size] = value;
//         m_size++;
//         return true;
//     }

//     // Move version of push_back
//     bool push_back(T&& value) 
//     {
//         if (m_size >= Capacity) 
//         {
//             return false;
//         }
//         m_data[m_size] = std::move(value);
//         m_size++;
//         return true;
//     }

//     // Getters for status
//     size_t size() const { return m_size; }
//     size_t capacity() const { return Capacity; }
//     bool empty() const { return m_size == 0; }

//     // Element access
//     T& operator[](size_t index) { return m_data[index]; }
//     const T& operator[](size_t index) const { return m_data[index]; }
// }; 


// namespace BareMetal   
// {

// template<DmaIrq IRQ_n, size_t N_Channels>
// class DmaIrqHandler: public BareMetal::IDmaIrqHandler
// {
// public:
//     irq_handler_t m_handler{};
//     HeaplessVector<uint, N_Channels> m_dma_channels{};
//     // std::array<Dma*, 5> m_dma_insts
//     bool is_event(size_t n) const
//     {
//         if constexpr (IRQ_n == DmaIrq::IRQ_0)
//         {
//             return(dma_hw->ints0 & (1u << m_dma_channels[n]));
//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_1)
//         {
//             return(dma_hw->ints1 & (1u << m_dma_channels[n]));

//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_2)
//         {
//             return(dma_hw->ints2 & (1u << m_dma_channels[n]));

//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_3)
//         {
//             return(dma_hw->ints3 & (1u << m_dma_channels[n]));
//         }
//     }
//     void acknowledge_isr(size_t n)
//     {
//         if constexpr (IRQ_n == DmaIrq::IRQ_0)
//         {
//             dma_hw->ints0 = (1u << m_dma_channels[n]);
//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_1)
//         {
//             dma_hw->ints1 = (1u << m_dma_channels[n]);
//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_2)
//         {
//             dma_hw->ints2 = (1u << m_dma_channels[n]);
//         }
//         else if constexpr (IRQ_n == DmaIrq::IRQ_3)
//         {
//             dma_hw->ints3 = (1u << m_dma_channels[n]);
//         }
//     }
// public:
//     explicit DmaIrqHandler(irq_handler_t handler)
//         : m_handler(handler)//, m_irqn(irqn)
//     {
//     }
//     void enable_interrupt(DmaBase* dma) override
//     {
//         m_dma_channels.push_back(dma->channel());
//         dma_channel_set_irq0_enabled(dma->channel(), true);
        
//         // Register our callback function to the CPU vector table for DMA_IRQ_0 etc...
//         irq_set_exclusive_handler(static_cast<uint32_t>(IRQ_n), m_handler);
//         irq_set_enabled(static_cast<uint32_t>(IRQ_n), true);
//     }
//     void handle_isr() override
//     {   
//         for(size_t n = 0; n < this->m_dma_channels.size(); n++)
//         {
//             if(this->is_event(n))
//             {
//                 printf(">>>> HEEEEEEEEEEEEEEEEEEEEEEEEEEINNNN \n");
//                 this->acknowledge_isr(n);
//             }
//         }
//     }
// };
// }




// namespace RTOS 
// {
// template<DmaIrq IRQ_n, size_t N>
// class DmaIrqHandler : public BareMetal::DmaIrqHandler<IRQ_n, N> , public RTOS::IDmaIrqHandler
// {
//     HeaplessVector<PicoTask*, N> m_tasks{};
// public:
//     explicit DmaIrqHandler(irq_handler_t handler)
//         : BareMetal::DmaIrqHandler<IRQ_n, N>(handler)//, m_irqn(irqn)
//     {
//     }
//     void enable_interrupt(DmaBase* dma, PicoTask* task)
//     {
//         enable_interrupt(dma);
//         if(!m_tasks.push_back(task))
//             panic("DMA handler: Task overflow!");
//         if (m_tasks.size() != this->m_dma_channels.size())
//             panic("DMA handler: Task vs DMA channel mismatch!");
//     }
//     void handle_isr() override
//     {   
//         for(size_t n = 0; n < this->m_dma_channels.size(); n++)
//         {
//             if(this->is_event(n))
//             {
//                 this->acknowledge_isr(n);
//                 if(this->m_tasks[0])
//                 {
//                     bool xHigherPriorityTaskWoken{};
//                     this->m_tasks[0]->notifyGiveFromISR(xHigherPriorityTaskWoken);
//                     if(xHigherPriorityTaskWoken)
//                         FreeRTOS::Kernel::yield();
//                 }
//             }
//         }
//     }
 
//     // void disable_interrupt()
//     // {
//     //     irq_set_enabled(m_dma_irq, false);
//     //     dma_channel_set_irq0_enabled(m_dma_chan, false);
//     //     irq_set_enabled(m_dma_irq, true);
//     // }
// };
// }



class SpiDefault
{
protected:
    spi_inst_t* m_spi{};
public:
    explicit SpiDefault(spi_inst_t* spi)
        : m_spi{spi}
    {
    }
    
};






class SpiBareMetal : public SpiDefault
{
    DmaRaw m_dma;
public:
    SpiBareMetal(spi_inst_t* spi, DmaRaw dma)
        : SpiDefault(spi), m_dma{std::move(dma)}//, m_buffer(buffer)
    {
    }
    void write_async(std::span<uint8_t> buff_to_send, bool start)
    {   
        m_dma.write(buff_to_send, &spi_get_hw(m_spi)->dr, spi_get_dreq(m_spi, true), start);
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
    static void start_multiple(std::span<SpiBareMetal*> instances)
    {
        uint32_t mask = 0;
        for(const auto& spi : instances)
        {
            mask |= (1u << spi->m_dma.channel());
        }
        dma_start_channel_mask(mask);
    }
};
 

#define bind_interrupt_DMA_0(X, irqs, handler_name, n_channels)                \
    void handler_name();                                                         \
    [[maybe_unused]] static X::DmaIrqHandler<DmaIrq::IRQ_0, n_channels> irqs{handler_name}; \
    void handler_name() {                                                        \
        irqs.handle_isr();                                                       \
    }                                           
#define bind_interrupt_DMA_1(X, irqs, handler_name, n_channels)                \
    void handler_name();                                                         \
    [[maybe_unused]] static X::DmaIrqHandler<DmaIrq::IRQ_1, n_channels> irqs{handler_name}; \
    void handler_name() {                                                        \
        irqs.handle_isr();                                                       \
    }                                           



constexpr size_t N_CHANNELS = 2;
bind_interrupt_DMA_0(base, dma_irqs, my_handler, N_CHANNELS);
 







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

    constexpr uint DMA_CH0{0};
    constexpr uint DMA_CH1{1};
    
    SpiBareMetal spi_rx{spi_default, base::Dma{DMA_CH0, dma_irqs}};
    SpiBareMetal spi_tx{spi_default, DmaRaw{DMA_CH1}};


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

        SpiBareMetal* instances[] = {&spi_rx, &spi_tx};
        SpiBareMetal::start_multiple(instances);

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