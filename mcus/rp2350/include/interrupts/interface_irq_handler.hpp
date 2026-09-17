#ifndef IRQ_HANDLER_HPP
#define IRQ_HANDLER_HPP

#include <cstddef>

namespace interrupts 
{
    class InterfaceIRQHandler
    {
        public:
            virtual bool is_event(size_t n) const = 0;
            virtual void acknowledge_isr(size_t n) = 0;
            virtual void handle_inst(size_t n) = 0; 
            virtual void handle_isr() = 0;
            virtual ~InterfaceIRQHandler() = default;
    };
}




#define BIND_INTERRUPT(irq_name, class, handler_name)       \
    void handler_name();                                    \
    [[maybe_unused]] static class irq_name{handler_name};   \
    void handler_name() {                                   \
        irq_name.handle_isr();                              \
    }


#endif