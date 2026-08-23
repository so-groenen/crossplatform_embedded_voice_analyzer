
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
#include <stddef.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "FreeRTOS/Mutex.hpp"
#include "FreeRTOSConfig.h"
#include "drivers/gpio.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/async_input.hpp"
#include "embedded_hal/tests.hpp"
#include "projdefs.h"
#include "drivers/async_input_handler.hpp"
#include "stats.hpp"
#include "thread_safe_ctx.hpp"
 
constexpr uint16_t PIN_20{20};
constexpr size_t num_buttons{1};
constexpr size_t stack_button{512};
 
bind_interrupt_EXTI(exti_irq, exti_handler, num_buttons);

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

class ButtonWaitForEdgeTask : public PicoTask
{
    AsyncInput m_btn;
public:
    ButtonWaitForEdgeTask()
      : PicoTask(1, 256, "edge-task"), m_btn(PIN_20, Pull::Up, this, exti_irq)
    {
    }

    void taskFunction() override
    {
        // pico2_w::WifiOutput m_led{}; // MUST BE CALLED INSIDE TASK, because it uses FreeRTOS tasks under the hood, and need schedueler to run
        while(true)
        {
            m_btn.wait_for_any_edge();
            // m_led.toggle();

            g_thread_safe_ctx([]
            {
                puts("Click!!");
            });
            delay(pdMS_TO_TICKS(10));
        }
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


int main() 
{
    stdio_init_all();
    // sleep_ms(1000);
     

    BlinkyTask blinky{};

    ButtonWaitForEdgeTask btn_task{};
    CounterTask counter_task1{400, "cnt-task2"};
    CounterTask counter_task2{1200, "cnt-task1"};
    StatsTask stats{};
    FreeRTOS::Kernel::startScheduler();
    while(1)
    {

    }
}