
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <pico/platform/common.h>
#include <pico/time.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "drivers/gpio.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/exti.hpp"
#include "projdefs.h"
 
 
constexpr uint16_t PIN_16{16};
 
static pico2_w::WifiOutput led{};


class ButtonExtiTask : public PicoTask
{
    ExtiInput m_button;
    // pico2_w::WifiOutput led{};
public:
    ButtonExtiTask(uint16_t pin)
        : m_button(pin, GPIO_IRQ_EDGE_FALL, this)
    {
        led.set_high();
    }
    void taskFunction() override
    {
        while(true)
        {
            m_button.wait_for_falling_edge();
            delay(pdMS_TO_TICKS(100));
            led.toggle();
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
    printf("STARTING SCHEDULER\n");
    
    for(size_t n = 5; n > 0; n--)
    {
        led.set_low();
        sleep_ms(250);
        led.set_high();
        sleep_ms(250);
    }
    led.set_low();

    // gpio_init(PIN_16);
    // gpio_set_dir(PIN_16, GPIO_IN);
    // gpio_pull_up(PIN_16);


    // gpio_add_raw_irq_handler(PIN_16, &my_raw_gpio_isr);
    // gpio_set_irq_enabled(PIN_16, GPIO_IRQ_EDGE_FALL, true);
    // irq_set_enabled(IO_IRQ_BANK0, true);



    // init_event_handler();
    // register_event(PIN_16, GPIO_IRQ_EDGE_FALL, blinker);
    // event_handler_enable();
    // static ButtonExtiTask button_task(PIN_16);

    // BlinkyTask ledTask{WifiOutput{}, ms_blinky};
    // FreeRTOS::Kernel::startScheduler();
    while(1)
    {
        sleep_ms(1000);
        printf("Hello, World\n");
        // tight_loop_contents();
        // configASSERT(0);
        // if(!gpio_get(PIN_16))
        // {
        //     printf("CLICK \n");
        //     sleep_ms(250);
        // }
        // sleep_ms(10);
        // if(ack)
        // {
        //     led.toggle();
        //     ack = false;
        //     printf("ACK \n");
        // }

    }
}