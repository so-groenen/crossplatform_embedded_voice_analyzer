#pragma once

#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "FreeRTOSConfig.h"
  
 
class PicoTask : public FreeRTOS::Task
{
public:
    explicit PicoTask(const UBaseType_t priority = tskIDLE_PRIORITY, const uint16_t stackDepth = configMINIMAL_STACK_SIZE, const char* name = "my_task")
        :  FreeRTOS::Task(priority, stackDepth, name)
    {
    }
    void awaitNotifs() const 
    {
        
        notifyTake();
    }
};