#ifndef IRQ_HANDLERS_HPP
#define IRQ_HANDLERS_HPP

#include <cstddef>

namespace irq 
{
    class IIrqHandler
    {
        public:
        virtual ~IIrqHandler() = default;
        virtual void handle_isr() = 0;
        virtual void handle_inst(size_t n) = 0;
    };
}



#endif