
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
#include <hardware/spi.h>
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
#include "drivers/async_input.hpp"
#include "drivers/gpio.hpp"
#include "drivers/spi_async.hpp"
#include "embedded_hal/traits.hpp"
#include "embedded_hal/tests.hpp"
#include "hardware/dma.h"
#include "drivers/pico_task.hpp"
#include "stats.hpp"

#include "drivers/dma.hpp"
#include "drivers/spi.hpp"
#include "drivers/dma_irq_handler.hpp"

#define Rtos_sleep_ms(X) delay(pdMS_TO_TICKS((X)));


constexpr size_t TEST_SIZE  = 1024;
constexpr size_t N_CHANNELS = 2; // only one channel fire's interrupt!

constexpr uint DMA_CH0      = 0;
constexpr uint DMA_CH1      = 1;


// bind the interrupt handler "irqs" to the my_handler interrupt callback
// use it to register N_channel = 1 interrupt from DMA channel
// and also register the corresponding task to block / unblock it!

bind_interrupt_DMA_0(rtos, irqs, my_handler, N_CHANNELS);



using spi::rtos::SpiAsync;
using spi::base::SpiDma;
using spi::Mode;
 

using embedded_hal::tests::async::AsyncTransferApp;
using spi::rtos::SpiAsyncFull;

class TransferTask : public PicoTask
{

public:
    TransferTask()
        : PicoTask(2, 512, "transfer-task")
    {
        auto config = spi::Config::create_default();
        config.baudrate = 1000 * 1000;
        spi::Spi::init_from_config(spi_default, config);

        gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
        gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
        gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
        gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

        // Loopback: DMA channel 1 will transfer to SPI-FIFO, DMA channel 0 will write it back to memory (recieve SPI data in buffer)
        hw_set_bits(&spi_get_hw(spi_default)->cr1, SPI_SSPCR1_LBM_BITS);
    }
    void taskFunction() override
    {
        SpiAsyncFull spi{spi_default, Dma{DMA_CH0}, Dma{DMA_CH1}, this, irqs};
        AsyncTransferApp<SpiAsyncFull> app{spi};
        app.run();
    }
};



using embedded_hal::tests::output::BlinkyApp;
 
class BlinkyTask : public PicoTask
{
    uint32_t blink_ms{500};
public:
    BlinkyTask()
        : PicoTask{2, 256, "blinky-task"}
    {
    }
    void taskFunction() override
    {
        pico2_w::WifiOutput led{};
        BlinkyApp app{led, blink_ms};
        app.run();
    }
};



using embedded_hal::tests::input::ButtonAnyEdge;

 
#include "drivers/async_input_handler.hpp"
constexpr uint16_t PIN_20    = 20;
constexpr size_t num_buttons = 1;

bind_interrupt_EXTI(exti_irq, exti_handler, num_buttons);

class ButtonTask : public PicoTask
{
public:
    ButtonTask()
        : PicoTask{2, 256, "button-task"}
    {
    }
    void taskFunction() override
    {
        AsyncInput button{PIN_20, Pull::Up, this, exti_irq};
        ButtonAnyEdge app{button};
        app.run();
    }
};







int main() 
{
    stdio_init_all();

    constexpr uint32_t stats_ms{10'000};
    StatsTask stats{stats_ms};

    TransferTask transfer_task{};
    BlinkyTask blinky_task{};
    ButtonTask button_task{};

    constexpr uint32_t counter_ms{500};
    CounterTask counter_task{counter_ms, "counter-tsk"};
    FreeRTOS::Kernel::startScheduler();
    while(true)
    {
    }
}