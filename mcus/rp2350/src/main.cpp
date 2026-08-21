
#include <algorithm>
#include <array>
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
 
 
constexpr uint16_t PIN_16{16};
 
// static pico2_w::WifiOutput led{};
using TaskType =  FreeRTOS::Task;


// class ButtonExtiTask : public TaskType
// {
//     ExtiInput m_button;
//     pico2_w::WifiOutput m_led{};
// public:
//     ButtonExtiTask(uint16_t pin)
//         : m_button(pin, GPIO_IRQ_EDGE_FALL, this) 
//     {
//         m_led.set_high();
//         printf("Hello, World\n");
//         printf("Hello, World\n");
//         printf("Hello, World\n");
//     }
//     void taskFunction() override
//     {
//         while(true)
//         {
//             m_button.wait_for_falling_edge();
//             delay(pdMS_TO_TICKS(100));
//             m_led.toggle();
//         }
//     }
// };
  
void my_button_handler();


template<size_t N>
class Notifier;

class IrqButton
{
    uint16_t m_pin{};
    uint32_t m_mode{};
    volatile bool m_has_fired{false};
    TaskType* m_task{};
public:
    explicit IrqButton(uint16_t pin, uint32_t mode, TaskType* task);
    void set_mode(uint32_t new_mode);
    void await_falling_edge();
    void handle_isr();
    uint16_t pin() const 
    {
        return m_pin;
    }
    uint32_t mode() const 
    {
        return m_mode;
    }
    // void set_task(FreeRTOS::Task* task)
    // {
    //     m_task = task;
    // }
    bool has_fired() const
    {
        return m_has_fired;
    }
    void reset_fired()
    {
        m_has_fired = false;
    }
    void await()
    {
        printf("try take Notifs \n");
        // m_task->notifyTake();
    }

};


template<size_t N>
class Notifier 
{
    std::array<IrqButton*, N> m_buttons{};
    std::array<TaskType*, N> m_tasks{};
    irq_handler_t m_handler{};
    size_t n_btns{};
    void _handle_isr(const IrqButton* btn, TaskType* task)
    {
        if (gpio_get_irq_event_mask(btn->pin()) & btn->mode()) 
        {
            gpio_acknowledge_irq(btn->pin(), btn->mode());
            if(task)
            {
                bool higherPriorityTaskWoken = false;
                task->notifyGiveFromISR(higherPriorityTaskWoken);
                if(higherPriorityTaskWoken)
                    FreeRTOS::Kernel::yield();
            }
        }
    }

public:
    explicit Notifier(irq_handler_t handler)
        : m_handler(handler)
    {
        assert(handler && "handler must be valid");
    }
 
    void enable_button(IrqButton* btn, TaskType* task)
    {
        if(n_btns < N && btn && task)
        {
            m_buttons[n_btns] = btn;
            m_tasks[n_btns] = task;
            n_btns++;

            // irq_set_enabled(IO_IRQ_BANK0, false);
            // gpio_set_irq_enabled(btn->pin(), btn->mode(), false);
            
            gpio_add_raw_irq_handler(btn->pin(), m_handler);
            
            gpio_set_irq_enabled(btn->pin(), btn->mode(), true);
            irq_set_enabled(IO_IRQ_BANK0, true);
        }       
    }
    void handle_isr()
    {
        for (size_t n = 0; n < n_btns; ++n)
        {
            if(m_buttons[n] && m_tasks[n])
                _handle_isr(m_buttons[n], m_tasks[n]);
        }
    }
};


void my_notifier_handler();

static Notifier<2> irq_notifier{my_notifier_handler};


// static Notifier<2> irq{my_isr_handler};

void my_notifier_handler()
{
    irq_notifier.handle_isr();
}


// static IrqButton* btn_ptr{};
 

// void my_button_handler()
// {
//     if(btn_ptr)
//     {
//         btn_ptr->handle_isr();
//     }
// }


IrqButton::IrqButton(uint16_t pin, uint32_t mode, TaskType* task)
    : m_pin{pin}, m_mode(mode), m_task(task)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);

 
    irq_notifier.enable_button(this, task);
    // btn_ptr = this;

    // gpio_add_raw_irq_handler(m_pin, my_handler);
    // gpio_set_irq_enabled(m_pin, m_mode, true);
    // irq_set_enabled(IO_IRQ_BANK0, true);
}
void IrqButton::set_mode(uint32_t new_mode)
{
    irq_set_enabled(IO_IRQ_BANK0, false);
    gpio_set_irq_enabled(m_pin, m_mode, false);
    m_mode = new_mode;
    gpio_set_irq_enabled(m_pin, m_mode, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}
void IrqButton::await_falling_edge()
{

}

void IrqButton::handle_isr()
{
    if (gpio_get_irq_event_mask(m_pin) & m_mode) 
    {
        gpio_acknowledge_irq(m_pin, m_mode);
        m_has_fired = true;
        if(m_task)
        {
            bool higherPriorityTaskWoken = false;
            m_task->notifyGiveFromISR(higherPriorityTaskWoken);
            if(higherPriorityTaskWoken)
                FreeRTOS::Kernel::yield();
        }
            
    }
}

class MyButtonTask : public PicoTask
{
    IrqButton m_btn;
    pico2_w::WifiOutput m_led{};
public:
    MyButtonTask()
      :  PicoTask(tskIDLE_PRIORITY, 512, "blabla"), m_btn(PIN_16, GPIO_IRQ_EDGE_FALL, this)
    {
        // btn_ptr = &m_btn;
        // for(size_t n = 0; n < 10; n++)
        // {
        //     m_led.toggle();
        //     sleep_ms(250);
        // }
        // task_ptr = this;
    }
    void taskFunction() override
    {
        while(true)
        {
            // m_btn.await();
            notifyTake();
            m_led.toggle();
            // printf("click from task with notifs\n");
            // if(m_btn.has_fired())
            // {
            //     m_led.toggle();
            //     m_btn.reset_fired();
            //     printf("click from task\n");
            // }
            delay(pdMS_TO_TICKS(10)); 
            // notifyTake();
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
    
    // WifiOutput led{};
    // for(size_t n = 5; n > 0; n--)
    // {
    //     led.toggle();
    //     printf("Hello world \n");
    //     sleep_ms(250);
    // }
    // led.set_low();

    // gpio_init(PIN_16);
    // gpio_set_dir(PIN_16, GPIO_IN);
    // gpio_pull_up(PIN_16);

    // gpio_add_raw_irq_handler(PIN_16, &my_raw_gpio_isr);
    // gpio_set_irq_enabled(PIN_16, GPIO_IRQ_EDGE_FALL, true);
    // irq_set_enabled(IO_IRQ_BANK0, true);

;
    // static ButtonExtiTask button_task(PIN_16);
    // IrqButton my_btn{PIN_16, GPIO_IRQ_EDGE_FALL};
    // btn_ptr = &my_btn;

    // static pico2_w::WifiOutput led{};

    // BlinkyTask ledTask{WifiOutput{}, ms_blinky};
 

    static MyButtonTask btnTask{};
    FreeRTOS::Kernel::startScheduler();
    while(1)
    {
        // led.toggle();
        // printf("Hello world \n");
        // sleep_ms(500);
        // if(my_btn.has_fired())
        // {
        //     led.toggle();
        //     my_btn.reset_fired();
        //     printf("click \n");
        //     sleep_ms(10); 
        // }
        // sleep_ms(1000);
        // printf("Hello, World\n");
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