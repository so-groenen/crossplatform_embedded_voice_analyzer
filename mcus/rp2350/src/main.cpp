
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
#include "drivers/exti.hpp"
#include "projdefs.h"
#include "drivers/exti_notifier.hpp"
#include "stats.hpp"
#include "thread_safe_ctx.hpp"
 
constexpr uint16_t PIN_16{16};
constexpr size_t num_buttons{2};
constexpr size_t stack_button{512};
 
bind_interrupt_EXTI(exti_irq, exti_handler, num_buttons);

ThreadSafeCtx g_thread_safe_ctx{};

class MyButtonTask : public PicoTask
{
    ExtiInput m_btn;
public:
    MyButtonTask()
      : PicoTask(1, stack_button, "button task"), m_btn(PIN_16, Pull::Up, this, exti_irq)
    {
    }

    void taskFunction() override
    {
        while(true)
        {

            // static bool led_on = false;
            // if(m_btn.is_low() && !led_on)
            // {
            //     m_led.set_level(Level::High);
            //     led_on = true;
            // }
            // if(m_btn.is_high() && led_on)
            // {
            //     m_led.set_level(Level::Low);
            //     led_on = false;
            // }

            // delay(pdMS_TO_TICKS(10));
        }
    }
};





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
            // static pico2_w::WifiOutput led{};
            // led.toggle();

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
    ExtiInput m_btn;
public:
    ButtonWaitForEdgeTask()
      : PicoTask(1, 256, "edge-task"), m_btn(PIN_16, Pull::Up, this, exti_irq)
    {
    }

    void taskFunction() override
    {
        pico2_w::WifiOutput m_led{}; // MUST BE CALLED INSIDE TASK, because it uses FreeRTOS tasks under the hood, and need schedueler to run
        while(true)
        {
            m_btn.wait_for_any_edge();
            m_led.toggle();

            g_thread_safe_ctx([]
            {
                puts("Click!!");
            });
            delay(pdMS_TO_TICKS(10));
        }
    }
};




int main() 
{
    stdio_init_all();
    // sleep_ms(1000);
     
    ButtonWaitForEdgeTask btn_task{};
    CounterTask counter_task1{400, "cnt-task2"};
    CounterTask counter_task2{1200, "cnt-task1"};
    StatsTask stats{};
    FreeRTOS::Kernel::startScheduler();
    while(1)
    {
        // tight_loop_contents();
        // static pico2_w::WifiOutput led{};
        // printf("hello world\n");
        // sleep_ms(250);
        // led.toggle();
        // sleep_ms(250);
        // led.toggle();
    }
}