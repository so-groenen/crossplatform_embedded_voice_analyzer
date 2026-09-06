
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
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
#include "FreeRTOS/Mutex.hpp"
#include "counter_task.hpp"

#include "interrupts/async_dma_handlers.hpp"
#include "drivers/gpio.hpp"
#include "drivers/spi.hpp"
#include "embedded_hal/traits.hpp"
#include "embedded_hal/tests.hpp"
#include "drivers/pico_task.hpp"
#include "projdefs.h"
#include "stats.hpp"

#include "drivers/dma.hpp"
#include "drivers/spi_async.hpp"
#include "interrupts/async_dma_handlers.hpp"
#include "interrupts/async_input_handler.hpp"
#include "drivers/async_input.hpp"

#define Rtos_sleep_ms(X) delay(pdMS_TO_TICKS((X)));



constexpr size_t TEST_SIZE  = 1024;
constexpr size_t N_CHANNELS = 2; // only one channel fire's interrupt!
constexpr uint DMA_CH0      = 0;
constexpr uint DMA_CH1      = 1;

// SPI uses: 16, 17, 18, 19
constexpr uint16_t BUTTON_PIN = 15;
constexpr size_t num_buttons  = 1;

// bind the interrupt handler "irqs" to the my_handler interrupt callback
// use it to register N_channel = 1 interrupt from DMA channel
// and also register the corresponding task to block / unblock it!

using MyAsyncHandler = interrupts::AsyncDmaHandler<DMA_IRQ_0, N_CHANNELS>;
BIND_INTERRUPT(irqs, MyAsyncHandler, my_isr_handler);


 
 
 
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    /* Handle stack overflow here */
    (void)xTask;
    (void)pcTaskName;

    // Breakpoint or infinite loop for debugging on the RP2350
    __asm volatile("bkpt #0");
    while (1) {
        // You can also blink an LED or print to UART/USB here
    }
}

 
 


using embedded_hal::tests::input::ButtonAnyEdge;
using embedded_hal::tests::input::ButtonRaiseBlinkApp;
using embedded_hal::tests::input::ButtonFallBlinkApp;
using embedded_hal::tests::output::BlinkyApp;
using embedded_hal::tests::async::AsyncTransferApp;


using spi::SpiAsyncFull;





class TransferTask : public PicoTask
{

public:
    TransferTask()
        : PicoTask(2, 1024, "transfer-task")
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





bind_interrupt_EXTI(exti_irq, exti_handler, num_buttons);

class ButtonAnyTask : public PicoTask
{
public:
    ButtonAnyTask()
        : PicoTask{2, 256, "button-any-task"}
    {
    }
    void taskFunction() override
    {
        AsyncInput button{BUTTON_PIN, Pull::Up, this, exti_irq};
        ButtonAnyEdge app{button};
        app.run();
    }
};

class ButtonRaiseBlinkTask : public PicoTask
{
public:
    ButtonRaiseBlinkTask()
        : PicoTask{2, 256, "button-raise-task"}
    {
    }
    void taskFunction() override
    {
        AsyncInput button{BUTTON_PIN, Pull::Up, this, exti_irq};
        pico2_w::WifiOutput led{};
        ButtonRaiseBlinkApp app{button, led};
        app.run();
    }
};


class ButtonFallBlinkTask : public PicoTask
{
public:
    ButtonFallBlinkTask()
        : PicoTask{2, 256, "button-fall-task"}
    {
    }
    void taskFunction() override
    {
        AsyncInput button{BUTTON_PIN, Pull::Up, this, exti_irq};
        pico2_w::WifiOutput led{};
        ButtonFallBlinkApp app{button, led};
        app.run();
    }
};







int main() 
{
    stdio_init_all();

    constexpr uint32_t stats_ms{10'000};
    StatsTask stats{stats_ms};

    TransferTask transfer_task{};

    // uncomment ONE button task:

    // ButtonAnyTask blinky_task{};
    ButtonFallBlinkTask button_task{};
    // ButtonRaiseBlinkTask button_task{};

 
    constexpr uint32_t counter_ms{500};
    CounterTask counter_task{counter_ms, "counter-tsk"};
    FreeRTOS::Kernel::startScheduler();
    while(true)
    {
    }
}