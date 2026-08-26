#pragma once 

#include "FreeRTOS/Task.hpp"
#include "drivers/input.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/interface_irq_handler.hpp"


 
 
class IAsyncInputHandler  : public irq::IIrqHandler 
{
    public:
        virtual ~IAsyncInputHandler() = default;
        virtual void bind_interrupt_from_rtos(Input* btn, PicoTask* task) = 0;
};