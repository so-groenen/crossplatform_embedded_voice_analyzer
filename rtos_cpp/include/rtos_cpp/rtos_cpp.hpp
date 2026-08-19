#ifndef RTOS_CPP_HPP
#define RTOS_CPP_HPP

#include "rtos/rtos.h"

namespace RtosCpp 
{
    class Task  
    {
        handle_t xtask{};
    public:
        Task()
        {
            rtos_create_task(Task::runner, this, &xtask);
        }
        virtual ~Task() = default;
        virtual void run() = 0;
    private:
        static void runner(void* tsk)
        {
            auto self = reinterpret_cast<Task*>(tsk);
            if(self) 
                self->run();
        }
    };

    inline void startSchedueler()
    {
        rtos_start();
    }
}

#endif //RTOS_CPP_HPP