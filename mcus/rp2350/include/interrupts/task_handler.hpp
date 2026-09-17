#ifndef TASK_HANDLER_HPP
#define TASK_HANDLER_HPP

#include "drivers/pico_task.hpp"
#include "embedded_hal/heapless.hpp"

namespace interrupts{


using embedded_hal::heapless::Vector;

template<size_t N>
class TaskHandler
{
    Vector<PicoTask*, N> m_tasks{};
public:
    inline void handle_task(size_t n) 
    {
        if(this->m_tasks[n])
        {
            // #ifdef DEBUG
            // printf(" >>> Hello from RTOS DMA Handler!\n");
            // #endif 
            bool xHigherPriorityTaskWoken{};
            this->m_tasks[0]->notifyGiveFromISR(xHigherPriorityTaskWoken);
            if(xHigherPriorityTaskWoken)
                FreeRTOS::Kernel::yield();
        }
    }
    inline bool add_task(PicoTask* task)
    {
        return m_tasks.push_back(task);
    }
};


}
#endif 