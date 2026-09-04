
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

// #include "drivers/spi_async.hpp"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"
#include "drivers/async_input.hpp"
#include "projdefs.h"
#include "drivers/async_input_handler.hpp"
#include "stats.hpp" 
#include <array>
#include "drivers/dma.hpp"
#include "drivers/spi.hpp"
#include "drivers/async_dma_handler.hpp"
#include "types.hpp"

#define Rtos_sleep_ms(X) delay(pdMS_TO_TICKS((X)));
// #define TEST_SIZE 1024


constexpr size_t N_CHANNELS = 1;


// bind interrupt handler "irqs" to the my_handler Interrupt Service routine, and register a single DMA channel.
// base: Do not record / block / unblock tasks 


class DmaIrqTest : public DmaHandlerBase<DmaIrq::IRQ_0, N_CHANNELS>
{
public:
    DmaIrqTest(irq_handler_t handler)
       : DmaHandlerBase<DmaIrq::IRQ_0, N_CHANNELS>(handler)
    {
    }
    inline void handle_inst(size_t n) override 
    {
        printf(">>>> Hello From Interrupt \n");
    }
};

void my_handler();
DmaIrqTest irqs{my_handler};
void my_handler()
{
    irqs.handle_isr();
}

 
using namespace hardware::spi;

constexpr uint DMA_CH0{0};
constexpr uint DMA_CH1{1};
constexpr size_t TEST_SIZE{1024};

class HelloDmaRtosTask : public PicoTask
{   
    SpiDma<SpiMode::RxTx> m_spi;
public:
    HelloDmaRtosTask(const Config& config)
        : PicoTask(1, 512, "DMA-test-task"), 
        m_spi(Spi<IOMode::NonBlocking>::make(spi_default, 
                               PICO_DEFAULT_SPI_SCK_PIN,
                               PICO_DEFAULT_SPI_TX_PIN, 
                               PICO_DEFAULT_SPI_RX_PIN,  
                               Dma{DMA_CH0}, 
                               Dma{DMA_CH1}, 
                               config))
    {
        m_spi.enable_loop_back(true);
        irqs.bind_interrupt(&m_spi.dma_tx()); 
    }
    static void check_buffs(std::span<uint8_t> rbuff, std::span<uint8_t> tbuff)
    {
        printf("Done. Checking...");
        for (uint i = 0; i < rbuff.size(); ++i) 
        {
            if (rbuff[i] != rbuff[i]) 
            {
                panic("Mismatch at %d/%d: expected %02x, got %02x",
                    i, rbuff.size(), tbuff[i], rbuff[i]
                );
            }
        } 
        printf("All good!\n");
    }
    static void randomize_buff(std::span<uint8_t> rbuff)
    {
        for (uint i = 0; i < rbuff.size(); ++i) 
        {
            rbuff[i] = rand();
        }

    }
    
    void taskFunction() override
    {
        static std::array<u8, TEST_SIZE> txbuf{};
        static std::array<u8, TEST_SIZE> rxbuf{};

        if(!dma_channel_is_claimed(DMA_CH0))
            panic("channel 0 not claimed");

        if(!dma_channel_is_claimed(DMA_CH1))
            panic("channel 1 not claimed");


        Rtos_sleep_ms(3000);
        printf("-- DMA TESTS -- [baudrate:%u]\n", m_spi.get_baudrate());
        Rtos_sleep_ms(3000);

        // We set the outbound DMA to transfer from a memory buffer to the SPI transmit FIFO paced by the SPI TX FIFO DREQ
        // The default is for the read address to increment every element (in this case 1 byte = DMA_SIZE_8)
        // and for the write address to remain unchanged.
        while(true)
        {
            printf("-- DMA basic transfer \n");
            randomize_buff(txbuf);

            m_spi.write_dma(txbuf, false);
            m_spi.read_dma(rxbuf, false);


            Dma* instances[] = {&m_spi.dma_rx(), &m_spi.dma_tx()};
            Dma::start_multiple(instances);

   
            m_spi.dma_rx().await_blocking();
            if (m_spi.dma_tx().is_busy())
            {
                panic("RX completed before TX");
            }


            check_buffs(rxbuf, txbuf);
            Rtos_sleep_ms(3000);


            printf("-- blocking spi inplace transfer -- \n");
            randomize_buff(txbuf);

            auto res = m_spi.transfer_blocking(rxbuf, txbuf);
 
            if(!res)
                panic("transfer error!");
            check_buffs(rxbuf, txbuf);
            Rtos_sleep_ms(3000);

            printf("-- non-blocking spi inplace transfer -- \n");
            randomize_buff(txbuf);

            m_spi.transfer_dma(rxbuf, txbuf);
 
            m_spi.dma_rx().await_blocking();
            if (m_spi.dma_tx().is_busy())
            {
                panic("transfer_dma: RX completed before TX");
            }

            check_buffs(rxbuf, txbuf);
            Rtos_sleep_ms(3000);
        }
    }
};
 
 

int main() 
{
    stdio_init_all();

    auto config = Config::create_default();
    config.baudrate = 1000 * 1000;

    static HelloDmaRtosTask dma_task{config};

    constexpr uint32_t stats_ms{20'000};
    static StatsTask stats{stats_ms};

    // constexpr uint32_t counter_ms{500};
    // CounterTask counter_task{counter_ms, "counter-tsk"};
    FreeRTOS::Kernel::startScheduler();
    while(true)
    {
    }
}