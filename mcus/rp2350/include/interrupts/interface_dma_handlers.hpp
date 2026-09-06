#ifndef IRQ_INTERFACE_HANDLER_HPP
#define IRQ_INTERFACE_HANDLER_HPP

 
// unblock threads from interrupts
// should have pointer to thread
// and to Dma to acknowledge interrupt;
// also enable interrupt



#include "drivers/dma.hpp"
#include "drivers/pico_task.hpp"
#include "interrupts/interface_irq_handler.hpp"

namespace interrupts
{
    class IDmaHandler : public IIrqHandler
    {
    public:
        virtual ~IDmaHandler() = default;
        virtual bool& bind_interrupt(Dma* dma) = 0;
    };

    class IAsyncDmaHandler
    {
    public:
        virtual ~IAsyncDmaHandler() = default;
        virtual void bind_interrupt_from_rtos(Dma* dma, PicoTask* task) = 0;
    };
}

#endif 