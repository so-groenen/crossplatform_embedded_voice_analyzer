#pragma once 

#include "FreeRTOS/Task.hpp"
#include "drivers/pico_task.hpp"
class ExtiInput;

class IExtiNotifier
{
    public:
        virtual ~IExtiNotifier() = default;
        virtual void enable_interrupt_from_rtos(ExtiInput* btn, PicoTask* task) = 0;
};