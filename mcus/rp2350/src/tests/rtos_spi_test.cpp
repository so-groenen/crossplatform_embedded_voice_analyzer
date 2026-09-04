
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
#include <pico/stdio.h>
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
#include "drivers/spi.hpp"
#include "hardware/dma.h"
#include "drivers/pico_task.hpp"
#include "stats.hpp"

#include "drivers/dma.hpp"
#include "drivers/spi.hpp"
#include "drivers/spi_async.hpp"
#include "drivers/async_dma_handler.hpp"
#include "types.hpp"

#define Rtos_sleep_ms(X) delay(pdMS_TO_TICKS((X)));


constexpr size_t TEST_SIZE  = 1024;
constexpr size_t N_CHANNELS = 1; // only one channel fire's interrupt!

constexpr uint DMA_CH0      = 0;
constexpr uint DMA_CH1      = 1;


// bind the interrupt handler "irqs" to the my_handler interrupt callback
// use it to register N_channel = 1 interrupt from DMA channel
// and also register the corresponding task to block / unblock it!

bind_interrupt_DMA_0(rtos, irqs, my_handler, N_CHANNELS);


 

class HelloDmaRtosTask : public PicoTask
{
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
        for (uint i = 0; i < rbuff.size(); ++i) 
        {
            if (rbuff[i] != rbuff[i]) 
            {
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



        spi_init(spi_default, 1000 * 1000);
 
        auto config = hardware::spi::Config::create_default();
        auto spi_hw = hardware::spi::Spi::create(spi_default, 
                                  PICO_DEFAULT_SPI_SCK_PIN,
                                  PICO_DEFAULT_SPI_TX_PIN,
                                  PICO_DEFAULT_SPI_RX_PIN, 
                                  config);
        spi_hw.enable_loop_back(true);

        spi::async::SpiAsync<TransferMode::Rx> spi_rx{spi_default, Dma{DMA_CH0}, this, irqs};
        spi::SpiDma<TransferMode::Tx> spi_tx{spi_default, Dma{DMA_CH1}};
        
    
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

            
            spi_tx.write_async(txbuf, true); // Use DMA to transfer data under the hood
            spi_rx.read_await(rxbuf);        // this one blocks and await interrupts
                                             // interrupt is hoocked up to channel 0, so the other one will not trigger any problems.
 
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