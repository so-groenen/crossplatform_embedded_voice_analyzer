#pragma once 

#include "FreeRTOS/Task.hpp"
#include "drivers/pico_task.hpp"
class AsyncInput;

class IAsyncInputHandler
{
    public:
        virtual ~IAsyncInputHandler() = default;
        virtual void bind_interrupt_from_rtos(AsyncInput* btn, PicoTask* task) = 0;
};