
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
#include "counter_task.hpp"
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

// ThreadSafeCtx g_thread_safe_ctx{};
 
 
struct SpiConfig 
{
    uint32_t frequency;
    uint32_t phase;
    uint32_t polarity;

};

#include "drivers/dma.hpp"
#include "drivers/spi.hpp"
#include "drivers/dma_irq_handler.hpp"

#define TEST_SIZE 1024
  

PicoTask* rx_task{};

class ReaderTask : public PicoTask
{
    SpiRTOS m_reader;
public:
    ReaderTask(spi_inst_t* spi, Dma rx_dma, rtos::IDmaIrqHandler& irq)
        : PicoTask(1, 512, "reader-task"), m_reader(spi, std::move(rx_dma), this, irq)
    {
        rx_task = this;
    }
    void taskFunction() override
    {
        static uint8_t BUFFER[512] = {};
        // for(size_t n = 0; n< 512; n++)
        // {
        //     BUFFER[n] = n % 255;
        // }
        static pico2_w::WifiOutput led{};
        while(true)
        {
            // printf("Awaiting notifs ... \n");
            notifyTake();
            m_reader.await_read(BUFFER);
            led.toggle();
            // printf("Awaiting read ... \n");
        }
    }
};


// // bind_interrupt_DMA_0(base, dma_irqs, my_handler, N_CHANNELS);
// bind_interrupt_DMA_0(rtos, rtos_irq, my_handler, N_CHANNELS);


class DmaEnabledIRQ : public Dma
{
public:
    explicit DmaEnabledIRQ(uint channel, base::IDmaIrqHandler& irq_handler)
        : Dma(channel)
    {
        irq_handler.enable_interrupt(this);
    }
};

constexpr size_t N_CHANNELS = 1;
bind_interrupt_DMA_0(base, irqs, my_handler, N_CHANNELS);


#define Rtos_sleep_ms(X) delay(pdMS_TO_TICKS((X)));

class HelloDmaRtosTask : public PicoTask
{
    // SpiRTOS m_reader;
public:
    HelloDmaRtosTask()
        : PicoTask(1, 512, "DMA-test-task")
    {
        spi_init(spi_default, 1000 * 1000);
        gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
        gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
        gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
        gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

        // Loopback: DMA channel 1 will transfer to SPI-FIFO, DMA channel 0 will write it back to memory (recieve SPI data in buffer)
        hw_set_bits(&spi_get_hw(spi_default)->cr1, SPI_SSPCR1_LBM_BITS);
    }
    static void check_buffs(std::span<uint8_t> rbuff, std::span<uint8_t> tbuff)
    {
        printf("Done. Checking...");
        for (uint i = 0; i < rbuff.size(); ++i) {
            if (rbuff[i] != rbuff[i]) {
                panic("Mismatch at %d/%d: expected %02x, got %02x",
                    i, TEST_SIZE, tbuff[i], rbuff[i]
                );
            }
        } 
        printf("All good!\n");
    }
    static void randomize_buff(std::span<uint8_t> rbuff)
    {
        for (uint i = 0; i < TEST_SIZE; ++i) 
        {
            rbuff[i] = rand();
        }

    }
    
    void taskFunction() override
    {
        static uint8_t txbuf[TEST_SIZE];
        static uint8_t rxbuf[TEST_SIZE];

        constexpr uint DMA_CH0{0};
        constexpr uint DMA_CH1{1};

        using spi::rtos::SpiAsync;
     
        
        using spi::Mode;

        // SpiAsync<Mode::Rx> spi_rx{spi_default, Dma{DMA_CH0}, this, rtos_irq};

        // SpiRTOS spi_rx{spi_default, Dma{DMA_CH0}, this, rtos_irq};  // calls the the ISR which can unblock the thread
        // SpiRTOS spi_rx{spi_default, Dma{DMA_CH0}, this, rtos_irq};
        SpiDmaHalf<Mode::Rx> spi_rx{spi_default, DmaEnabledIRQ{DMA_CH0, irqs}};
        SpiDmaHalf<Mode::Tx> spi_tx{spi_default, Dma{DMA_CH1}};
        

        // SpiDMA  spi_tx{spi_default, Dma{DMA_CH1}};                  // transfer silently
        
    
        if(!dma_channel_is_claimed(DMA_CH0))
            panic("channel 0 not claimed");

        if(!dma_channel_is_claimed(DMA_CH1))
            panic("channel 1 not claimed");


        Rtos_sleep_ms(3000);
        printf("-- DMA TESTS -- [baudrate:%u]\n", spi_rx.baudrate());
        Rtos_sleep_ms(3000);

        // We set the outbound DMA to transfer from a memory buffer to the SPI transmit FIFO paced by the SPI TX FIFO DREQ
        // The default is for the read address to increment every element (in this case 1 byte = DMA_SIZE_8)
        // and for the write address to remain unchanged.
        while(true)
        {
            printf("-- DMA basic transfer \n");
            randomize_buff(txbuf);
            // spi_tx.write_async(txbuf, true);    // Launch DMA channel 0 to transfer data to spi
            // spi_rx.await_read(rxbuf);           // start reading and block the thread ...
                                                // when all data has been recieved, the ISR will unblock the thread!
            spi_tx.write_async(txbuf, false);
            spi_rx.read_async(rxbuf, false);
            Dma* instances[] = {&spi_rx.dma(), &spi_tx.dma()};
            Dma::start_multiple(instances);


            // notifyTake();       // call to read async triggers interrupt which will increase the count, and the next "await" call will finish before hand

            spi_rx.dma().await_blocking();

            if (spi_tx.dma().is_busy())
            {
                panic("RX completed before TX");
            }
            check_buffs(rxbuf, txbuf);
            Rtos_sleep_ms(3000);


            printf("-- DMA inplace transfer -- \n");
            randomize_buff(txbuf);

            auto res = spi_tx.transfer_blocking(rxbuf, txbuf);
            if(!res)
                panic("transfer error!");
            check_buffs(rxbuf, txbuf);
            Rtos_sleep_ms(3000);



            // printf("-- DMA use RTOS  -- \n");
            // randomize_buff(txbuf);

            // spi_tx.write_async(txbuf, true);
            // spi_rx.await_read(rxbuf);         // Kernel yields control to other task & awaits

            // if (spi_tx.dma().is_busy())
            // {
            //     panic("RX completed before TX");
            // }
            // check_buffs(rxbuf, txbuf);
            // Rtos_sleep_ms(3000);
        }
    }
};
 
 

int main() 
{
    stdio_init_all();


    HelloDmaRtosTask writer{};

    constexpr uint32_t stats_ms{20'000};
    StatsTask stats{stats_ms};

    // constexpr uint32_t counter_ms{500};
    // CounterTask counter_task{counter_ms, "counter-tsk"};
    FreeRTOS::Kernel::startScheduler();
    while(true)
    {
    }
}