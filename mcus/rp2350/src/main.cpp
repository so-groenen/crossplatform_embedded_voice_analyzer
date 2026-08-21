
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
 
 
constexpr uint16_t PIN_16{16};
  
 

constexpr size_t num_buttons{2};
bind_interrupt_EXTI(exti_irq, exti_handler, num_buttons);

// static DoProtected prote{};

class MyButtonTask : public PicoTask
{
    ExtiInput m_btn;
    pico2_w::WifiOutput m_led{};
public:
    MyButtonTask()
      :  m_btn(PIN_16, GPIO_IRQ_EDGE_FALL, this, exti_irq)
    {
    }

    void taskFunction() override
    {
        FreeRTOS::Mutex mutex{};
        auto println = [&](const char* str){mutex.lock(); puts(str); mutex.unlock();};

        while(true)
        {
            m_btn.wait_for_falling_edge();
            m_led.toggle();
            delay(pdMS_TO_TICKS(100));
        }
    }
};

int main() 
{
    using namespace pico2_w;
    // using embedded_hal::output::Test::BlinkyTask;

    // constexpr uint16_t ms_blinky{125};

    stdio_init_all();
    sleep_ms(1000);
     
    static MyButtonTask btnTask{};
    FreeRTOS::Kernel::startScheduler();
    while(1)
    {
         
    }
}