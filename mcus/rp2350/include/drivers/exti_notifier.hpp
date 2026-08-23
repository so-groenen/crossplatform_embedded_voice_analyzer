#pragma once

#include "FreeRTOS/Task.hpp"
// #include "drivers/interface_exti_notifier.hpp"
#include "drivers/exti.hpp"
#include <hardware/irq.h>
#include <array>

template<size_t N>
class Notifier : public IExtiNotifier
{
    std::array<ExtiInput*, N> m_buttons{};
    std::array<PicoTask*, N> m_tasks{};
    irq_handler_t m_handler{};
    size_t n_btns{};
    void _handle_isr(const ExtiInput* btn, PicoTask* task)
    {
        if (gpio_get_irq_event_mask(btn->pin()) & btn->mode()) 
        {
            gpio_acknowledge_irq(btn->pin(), btn->mode());
            if(task && btn->is_awaiting()) // if we did NOT call a wait_for_blabl(), then we do not add notifs!
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
    
    Notifier& operator()(PicoTask* task)
    {
        return *this;
    }
    void bind_interrupt_from_rtos(ExtiInput* btn, PicoTask* task) override
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

#define bind_interrupt_EXTI(irq_name, handler_name, n_exti)        \
    void handler_name();                                               \
    [[maybe_unused]] static Notifier<n_exti> irq_name{handler_name};   \
    void handler_name() {                                              \
        irq_name.handle_isr();                                          \
    }

 
// #include "FreeRTOS/Kernel.hpp"
// #include "FreeRTOS/Task.hpp"
// #include "drivers/pico_task.hpp"
// #include "drivers/exti.hpp"
// #include "array"
// #include <cstddef>

// class ExtiNotifier
// {
//     PicoTask* m_task{};
//     ExtiInput* m_input{};
// public:
//     void set_input(ExtiInput* input)
//     {
//         if(!input)
//             return;

//         m_input = input;
//         m_task  = &input->m_task;
//     }
//     void notifyGiveFromISR()
//     {
//         if(!m_task)
//             return;

//         bool xHigherPriorityTaskWoken;
//         m_task->notifyGiveFromISR(xHigherPriorityTaskWoken);
//         if(xHigherPriorityTaskWoken)
//         {
//             FreeRTOS::Kernel::yield();
//         }
//     }
//     void handle_exti()
//     {
//         if(!m_input)
//             return; 

//         uint32_t event = gpio_get_irq_event_mask(m_input->get_pin());
//         if (event & m_input->get_mode()) 
//         {
//             gpio_acknowledge_irq(m_input->get_pin(), event & m_input->get_mode());
//         }
//     }
// };

 