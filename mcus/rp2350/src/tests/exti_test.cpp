
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
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
 
class CounterTask : public PicoTask
{
public:
    CounterTask()
      : PicoTask(1, stack_button, "counter-task")
    {
    }

    void taskFunction() override
    {
        while(true)
        {
            static uint32_t counter = 0;
            g_thread_safe_ctx([]
            {
                printf("Hello! counter = %u \n", counter);
            });
            counter++;
            delay(pdMS_TO_TICKS(250));
        }
    }
};

class ButtonWaitForEdgeTask : public PicoTask
{
    ExtiInput m_btn;
    pico2_w::WifiOutput m_led{};
public:
    ButtonWaitForEdgeTask()
      : PicoTask(1, stack_button, "edge-task"), m_btn(PIN_16, Pull::Up, this, exti_irq)
    {
    }

    void taskFunction() override
    {
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
     
    static ButtonWaitForEdgeTask btn_task{};
    static CounterTask counter_task{};
    static StatsTask stats{};
    FreeRTOS::Kernel::startScheduler();
    while(1)
    {

    }
}