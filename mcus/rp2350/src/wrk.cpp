
#include <algorithm>
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
#include "FreeRTOSConfig.h"
#include "drivers/gpio.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/exti.hpp"
#include "projdefs.h"
 
 
constexpr uint16_t PIN_16{16};
 
// static pico2_w::WifiOutput led{};




class ButtonExtiTask : public PicoTask
{
    ExtiInput m_button;
    pico2_w::WifiOutput m_led{};
public:
    ButtonExtiTask(uint16_t pin)
        : m_button(pin, GPIO_IRQ_EDGE_FALL, this) 
    {
        m_led.set_high();
        printf("Hello, World\n");
        printf("Hello, World\n");
        printf("Hello, World\n");
    }
    void taskFunction() override
    {
        while(true)
        {
            m_button.wait_for_falling_edge();
            delay(pdMS_TO_TICKS(100));
            m_led.toggle();
        }
    }
};
  

using TaskType = PicoTask;

class IrqButton
{
    uint16_t m_pin{};
    uint32_t m_mode{};
    volatile bool m_has_fired{false};
    TaskType* m_task{};
public:
    explicit IrqButton(uint16_t pin, uint32_t mode, TaskType* task);
    void set_mode(uint32_t new_mode);
    void await_isr();
    void handle_isr();
    void set_task(TaskType* task)
    {
        m_task = task;
    }
    bool has_fired() const
    {
        return m_has_fired;
    }
    void reset_fired()
    {
        m_has_fired = false;
    }
};


IrqButton* btn_ptr{};
void my_button_handler()
{
    if(btn_ptr)
        btn_ptr->handle_isr();
}


IrqButton::IrqButton(uint16_t pin, uint32_t mode, TaskType* task = nullptr)
    : m_pin{pin}, m_mode(mode)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);

    gpio_add_raw_irq_handler(m_pin, &my_button_handler);
    gpio_set_irq_enabled(m_pin, m_mode, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

    set_task(task);
}
void IrqButton::set_mode(uint32_t new_mode)
{
    irq_set_enabled(IO_IRQ_BANK0, false);
    gpio_set_irq_enabled(m_pin, m_mode, false);
    m_mode = new_mode;
    gpio_set_irq_enabled(m_pin, m_mode, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}
void IrqButton::await_isr()
{
    while(!m_has_fired)
    {
        sleep_ms(10);
    }
    m_has_fired = false;
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

class MyButtonTask : public TaskType
{
    IrqButton m_btn{PIN_16, GPIO_IRQ_EDGE_FALL, this};
    pico2_w::WifiOutput m_led{};
    // bool m_use_notifs{};
public:
    MyButtonTask(bool use_notifs = true)
      :  TaskType(tskIDLE_PRIORITY, 512, "blabla") //, m_use_notifs{use_notifs}
    {
        btn_ptr = &m_btn;

        // if(use_notifs)
        // m_btn.set_task(this);
    }
    void taskFunction() override
    {
        while(true)
        {
            if(m_btn.has_fired())
            {
                m_led.toggle();
                m_btn.reset_fired();
                printf("click from task\n");
                delay(pdMS_TO_TICKS(10)); 
            }

            // if(m_use_notifs)
            // {
            notifyTake(); // will block until interrupt occur!
            printf("notify taken \n");
            // }
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
     

    // for(size_t n = 5; n > 0; n--)
    // {
    //     led.set_low();
    //     sleep_ms(250);
    //     led.set_high();
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

    MyButtonTask btnTask{};
    FreeRTOS::Kernel::startScheduler();
    while(1)
    {
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