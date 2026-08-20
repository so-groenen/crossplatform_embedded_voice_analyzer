#pragma once

#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
  
class PicoTask : public FreeRTOS::Task
{
public:
    inline void notifyTake() const 
    {
        notifyTake();
    }
};