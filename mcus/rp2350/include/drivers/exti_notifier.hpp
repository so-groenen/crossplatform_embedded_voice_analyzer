#pragma once

#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/exti.hpp"
#include "array"
#include <cstddef>

class ExtiNotifier
{
    PicoTask* m_task{};
    ExtiInput* m_input{};
public:
    void set_input(ExtiInput* input)
    {
        if(!input)
            return;

        m_input = input;
        m_task  = &input->m_task;
    }
    void notifyGiveFromISR()
    {
        if(!m_task)
            return;

        bool xHigherPriorityTaskWoken;
        m_task->notifyGiveFromISR(xHigherPriorityTaskWoken);
        if(xHigherPriorityTaskWoken)
        {
            FreeRTOS::Kernel::yield();
        }
    }
    void handle_exti()
    {
        if(!m_input)
            return; 

        uint32_t event = gpio_get_irq_event_mask(m_input->get_pin());
        if (event & m_input->get_mode()) 
        {
            gpio_acknowledge_irq(m_input->get_pin(), event & m_input->get_mode());
        }
    }
};

 