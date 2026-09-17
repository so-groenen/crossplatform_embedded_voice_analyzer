#ifndef I_INPUT_HANDLERS_HPP
#define I_INPUT_HANDLERS_HPP


#include "drivers/dma.hpp"
#include "drivers/input.hpp"
#include "drivers/pico_task.hpp"
#include "interrupts/interface_irq_handler.hpp"

namespace interrupts
{
    class IInputHandler //: public IIrqHandler
    {
    public:
        virtual ~IInputHandler() = default;
        virtual void bind_interrupt(const Input* in) = 0;
    };

    class IAsyncInputHandler
    {
        public:
            virtual ~IAsyncInputHandler() = default;
            virtual void bind_interrupt_from_rtos(const Input* btn, PicoTask* task) = 0;
    };
}
#endif 