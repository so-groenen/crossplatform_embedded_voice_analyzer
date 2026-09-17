#pragma once

#include "FreeRTOS/Task.hpp"
// #include "drivers/interface_async_input_handler.hpp"
#include "drivers/async_input.hpp"
#include "drivers/input.hpp"
#include "interrupts/input_handlers.hpp"
#include "interrupts/interface_input_handlers.hpp"
#include "interrupts/task_handler.hpp"
#include <cstddef>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <array>
#include <pico/platform/panic.h>


namespace interrupts
{
    template<size_t N>
    class AsyncInputHandler : public  IAsyncInputHandler, public AbstractInputHandler<N>
    {
        TaskHandler<N> m_tasks{};
    public:
        explicit AsyncInputHandler(irq_handler_t handler)
            : AbstractInputHandler<N>(handler)
        {
            assert(handler && "handler must be valid");
        }   
        void bind_interrupt_from_rtos(const Input* btn, PicoTask* task) override
        {
            this->bind_interrupt(btn);
            if(!m_tasks.add_task(task))
                panic("AsyncInputHandler: Could not add task!");
        }
        inline void handle_inst(size_t n) override
        {
            m_tasks.handle_task(n);
        }
    
    };

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

 