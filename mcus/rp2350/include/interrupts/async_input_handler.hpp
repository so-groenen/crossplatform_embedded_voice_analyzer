#pragma once

#include "FreeRTOS/Task.hpp"
// #include "drivers/interface_async_input_handler.hpp"
#include "drivers/async_input.hpp"
#include "drivers/input.hpp"
#include <cstddef>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <array>



template<size_t N>
class AsyncInputHandler : public IAsyncInputHandler
{
    std::array<uint16_t, N> m_pins{};
    std::array<PicoTask*, N> m_tasks{};
    irq_handler_t m_handler{};
    size_t n_btns{};
public:
    explicit AsyncInputHandler(irq_handler_t handler)
        : m_handler(handler)
    {
        assert(handler && "handler must be valid");
    }
    
    AsyncInputHandler& operator()(PicoTask* task)
    {
        return *this;
    }
    void bind_interrupt_from_rtos(Input* btn, PicoTask* task) override
    {
        if(n_btns < N && btn && task)
        {
            m_pins[n_btns]  = btn->pin();
            m_tasks[n_btns] = task;
            n_btns++;
            
            gpio_add_raw_irq_handler(btn->pin(), m_handler);
            // gpio_set_irq_enabled(btn->pin(), btn->mode(), true);
            irq_set_enabled(IO_IRQ_BANK0, true);
        }       
    }
    inline void handle_inst(size_t n) override
    {
        if(m_tasks[n] /*&& btn->is_awaiting()*/ ) // if we did NOT call a wait_for_blabl(), then we do not add notifs!
        {
            bool higherPriorityTaskWoken = false;
            m_tasks[n]->notifyGiveFromISR(higherPriorityTaskWoken);
            if(higherPriorityTaskWoken)
                FreeRTOS::Kernel::yield();
        }
    }
    inline void handle_isr() override
    {
        for (size_t n = 0; n < n_btns; ++n)
        {
            if (gpio_get_irq_event_mask(m_pins[n]) & AsyncInput::all_irq) 
            {
                gpio_acknowledge_irq(m_pins[n], AsyncInput::all_irq);
                handle_inst(n);
            }
        }
    }
};

#define bind_interrupt_EXTI(irq_name, handler_name, n_exti)        \
    void handler_name();                                               \
    [[maybe_unused]] static AsyncInputHandler<n_exti> irq_name{handler_name};   \
    void handler_name() {                                              \
        irq_name.handle_isr();                                          \
    }

 
// #include "FreeRTOS/Kernel.hpp"
// #include "FreeRTOS/Task.hpp"
// #include "drivers/pico_task.hpp"
// #include "drivers/async_input.hpp"
// #include "array"
// #include <cstddef>

// class ExtiAsyncInputHandler
// {
//     PicoTask* m_task{};
//     AsyncInput* m_input{};
// public:
//     void set_input(AsyncInput* input)
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

 