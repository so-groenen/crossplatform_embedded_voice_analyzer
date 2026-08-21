#include <cassert>
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
#include "FreeRTOSConfig.h"
#include "drivers/gpio.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/exti.hpp"
#include "projdefs.h"
#include "drivers/exti_notifier.hpp"

 
constexpr uint16_t PIN_16{16};
  

constexpr size_t num_buttons{2};
bind_interrupt_EXTI(exti_irq, exti_handler, num_buttons);


class MyButtonTask : public PicoTask
{
    ExtiInput m_btn;
    pico2_w::WifiOutput m_led{};
public:
    MyButtonTask()
      :  m_btn(PIN_16, Pull::Up, this, exti_irq)
    {
    }
    void taskFunction() override
    {
        while(true)
        {
            notifyTake();
            m_led.toggle();

            delay(pdMS_TO_TICKS(10)); 
        }
    }
};

int main() 
{
    stdio_init_all(); 

    static MyButtonTask btnTask{};
    FreeRTOS::Kernel::startScheduler();
    while(1)
    {
         
    }
}