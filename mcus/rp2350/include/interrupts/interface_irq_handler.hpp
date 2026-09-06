#ifndef IRQ_HANDLER_HPP
#define IRQ_HANDLER_HPP

#include <cstddef>

namespace interrupts 
{
    class IIrqHandler
    {
        public:
        virtual ~IIrqHandler() = default;
        virtual void handle_isr() = 0;
        virtual void handle_inst(size_t n) = 0;
    };
}

#define BIND_INTERRUPT(irq_name, class, handler_name)       \
    void handler_name();                                    \
    [[maybe_unused]] static class irq_name{handler_name};   \
    void handler_name() {                                   \
        irq_name.handle_isr();                              \
    }


#endif