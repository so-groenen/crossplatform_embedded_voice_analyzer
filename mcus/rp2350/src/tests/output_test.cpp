
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/assert.h>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/time.h>
#include <stddef.h>
#include <stdint.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "FreeRTOS/Mutex.hpp"
#include "FreeRTOSConfig.h"
#include "drivers/gpio.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/async_input.hpp"
#include "embedded_hal/traits.hpp"
#include "projdefs.h"
#include "interrupts/async_input_handler.hpp"
#include "stats.hpp"
#include "thread_safe_ctx.hpp"
 
constexpr uint16_t PIN_16{16};
constexpr size_t num_buttons{2};
constexpr size_t stack_button{512};
 
 
ThreadSafeCtx g_thread_safe_ctx{};
 


class CounterTask : public PicoTask
{
    uint32_t m_delay{};
public:
    CounterTask(uint32_t delay, const char* name)
      : PicoTask(1, stack_button, name), m_delay{delay}
    {
    }

    void taskFunction() override
    {
        while(true)
        {
            static uint32_t counter = 0;
            g_thread_safe_ctx([&]
            {
                printf("Hello (delay[%u ms])! counter = %u \n", m_delay, counter);
            });
            counter++;
            delay(pdMS_TO_TICKS(m_delay));
        }
    }
};

class WifiOutputTask : public PicoTask
{
public:
    WifiOutputTask()
      : PicoTask(1, 256, "edge-task")
    {
    }

    void taskFunction() override
    {
        pico2_w::WifiOutput led{}; // MUST BE CALLED INSIDE TASK, because it uses FreeRTOS tasks under the hood, and need schedueler to run
        while(true)
        {
            led.set_high();
            hard_assert(led.is_set_high());
            hard_assert(led.get_output_level() == gpio::Level::High);
            delay(pdMS_TO_TICKS(250));
            led.set_low();
            hard_assert(led.is_set_low());
            hard_assert(led.get_output_level() == gpio::Level::Low);
            delay(pdMS_TO_TICKS(250));
            led.toggle();
            delay(pdMS_TO_TICKS(250));
            led.set_level( gpio::Level::Low);
            delay(pdMS_TO_TICKS(250));

        }
    }
};




int main() 
{
    stdio_init_all();

    constexpr uint32_t ms{1200};
    CounterTask counter_task2{ms, "cnt-task1"};
    WifiOutputTask wifi_task{};
    StatsTask stats{};
    FreeRTOS::Kernel::startScheduler();
    while(1)
    {

    }
}