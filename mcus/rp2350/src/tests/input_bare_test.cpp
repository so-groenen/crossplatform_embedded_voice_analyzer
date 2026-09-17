
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include <stddef.h>
  
#include "drivers/countdown_timer.hpp"
#include "drivers/gpio.hpp"
#include "interrupts/input_handlers.hpp"
#include "drivers/input.hpp"
 
constexpr uint16_t BTN_PIN1{10};
constexpr uint16_t BTN_PIN2{11};
constexpr uint16_t LED_PIN1{15};
constexpr uint16_t LED_PIN2{14};
constexpr size_t num_buttons{2};  
 


using pico2_w::Output;
using pico2_w::WifiOutput;
// using interrupts::InputHandler;
// using interrupts::IsInputCallback;


class MyDriver
{
    Input& m_btn1;
    Input& m_btn2;
    Output& m_led1;
    Output& m_led2;
    i32 m_debounce_us{10'000};
    absolute_time_t m_last1{get_absolute_time()};
    absolute_time_t m_last2{get_absolute_time()};
public:
    // template<IsInputCallback Driver>
    // explicit MyDriver(Input& bt1, Input& bt2, Output& led1, Output& led2, InputHandler<Driver, num_buttons>& irq)
    INPUT_HANDLER_CLASS(MyDriver, irq, Input& bt1, Input& bt2, Output& led1, Output& led2)
        : m_btn1(bt1), m_btn2(bt2), m_led1(led1), m_led2(led2)
    {
        irq.bind_driver(this);
        irq.bind_interrupt(&m_btn1);
        irq.bind_interrupt(&m_btn2);
    }
    _ISR_INPUT_CALLBACK void callback_from_isr(u16 pin)
    {
        auto now  = get_absolute_time();
        printf("PIN %u \n", pin);

        if(pin == m_btn1.pin())
        {
            if(absolute_time_diff_us(m_last1, now) > m_debounce_us)
            {
                m_led1.toggle();
                m_last1 = now;
            }
        }
        else if(pin == m_btn2.pin())
        {
            if(absolute_time_diff_us(m_last2, now) > m_debounce_us)
            {
                m_led2.toggle();
                m_last2 = now;
            }
        }
    }
};

// using MyIRQHandler = interrupts::InputHandler<MyDriver, num_buttons>;
using MyIRQHandler = INPUT_IRQ_HANDLER(MyDriver, num_buttons);
 
BIND_INTERRUPT(irq, MyIRQHandler, handler) // bind irq handler class to C function "void handler()"
 
int main() 
{
    stdio_init_all();

    Input bt1(BTN_PIN1, Pull::Up);
    Input bt2(BTN_PIN2, Pull::Up);

    bt1.enable_interrupts(GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);
    bt2.enable_interrupts(GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);
    
    Output led1(LED_PIN1, Level::Low);
    Output led2(LED_PIN2, Level::Low);

    WifiOutput default_led{Level::Low};

    MyDriver driver(irq, bt1, bt2, led1, led2);    

    CountdownTimer<1> tasks{};
    tasks.add_task_ms(250);
    while(1)
    {
        if(tasks.is_ready())
            default_led.toggle();

    }
}