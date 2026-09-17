#ifndef IRQ_INPUT_HANDLER_HPP
#define IRQ_INPUT_HANDLER_HPP


 
 
#include "drivers/input.hpp"
#include "embedded_hal/heapless.hpp"
#include <cassert>
#include <cstddef>
#include <hardware/irq.h>
#include "interrupts/interface_input_handlers.hpp"
#include <concepts>

using embedded_hal::heapless::Vector;

namespace interrupts
{
    template<size_t N>
    class AbstractInputHandler: public IInputHandler, public InterfaceIRQHandler
    {
    protected:
        irq_handler_t m_handler{};
        Vector<u16, N> m_pins{};
        inline bool is_event(size_t n) const override
        {
            return gpio_get_irq_event_mask(m_pins[n]) & Input::all_irq;
        }
        void acknowledge_isr(size_t n) override
        {
            gpio_acknowledge_irq(m_pins[n], Input::all_irq);
        }

    public:
        explicit AbstractInputHandler(irq_handler_t handler)
            : m_handler(handler) 
        {
            // irq_set_exclusive_handler(IO_IRQ_BANK0, m_handler);
            irq_add_shared_handler(IO_IRQ_BANK0, m_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
            irq_set_enabled(IO_IRQ_BANK0, true);
        }
        void bind_interrupt(const Input* btn) override
        {
            if(!m_pins.push_back(btn->pin()))
                panic("Bind interrupt: m_pins overflow\n");
        }
        inline void handle_isr() override
        {   
            for(size_t n = 0; n < this->m_pins.size(); n++)
            {
                if(this->is_event(n))
                {
                    this->acknowledge_isr(n);
                    handle_inst(n);
                }
            }
        }
        void enable(bool v)
        {
            irq_set_enabled(IO_IRQ_BANK0, v);
        }
    
    };



    template<typename T>
    concept IsInputCallback = requires(T t, u16 pin)
    {
        {t.callback_from_isr(pin)} -> std::same_as<void>;
    };
    
    #define _ISR_INPUT_CALLBACK
    #define _ISR_INPUT_ASSERT(X) static_assert(interrupts::IsInputCallback<X>, "must implement callback_from_isr()");

    template< IsInputCallback Driver, size_t N>
    class InputHandler: public AbstractInputHandler<N>
    {
        Driver* m_driver{};
    public:    
        explicit InputHandler(irq_handler_t handler)
            : AbstractInputHandler<N>(handler)//, m_irqn(irqn)
        {
        }
        inline void bind_driver(Driver* driver)
        {
            m_driver = driver;
        }   
        inline void handle_inst(size_t n) override 
        {
            assert(m_driver && "driver must not be null");
            m_driver->callback_from_isr(this->m_pins[n]);
        }
    };



    // #define HANDLERR(X,arg,irq) explicit X(arg, TESTInputHandlerCB<N, X>& irq)
    #define INPUT_IRQ_HANDLER(X, N)               interrupts::InputHandler<X, N>
    #define INPUT_HANDLER_CLASS(X, irq, ...)     template<size_t N> \
                                                explicit X(interrupts::InputHandler<X,N>& irq, __VA_ARGS__)


    

}
 
// // 
// namespace interrupts 
// {


//     template<size_t N>
//     class InputHandlerBase: public IInputHandler
//     {
//     protected:
//         irq_handler_t m_handler{};
//         std::array<uint16_t, N> m_pins{};
//         inline bool is_event(size_t n) const
//         {
//             return gpio_get_irq_event_mask(m_pins[n]) & Input::all_irq;
//         }
//         void acknowledge_isr(size_t n)
//         {
//             gpio_acknowledge_irq(m_pins[n], Input::all_irq);
//         }
    
//     public:
//         explicit InputHandlerBase(irq_handler_t handler)
//             : m_handler(handler) 
//         {
//             irq_set_exclusive_handler(IO_IRQ_BANK0, m_handler);
//             irq_set_enabled(IO_IRQ_BANK0, true);
//         }
//         // returns a "ready flag, which will be set to True when it fires"
//         void bind_interrupt(Input* btn) override
//         {
//             if(!m_pins.push_back(btn->pin()))
//                 panic("Bind interrupt: m_pins overflow\n");
                
//             // static bool bound = false;
    
//             // if(m_handler && !bound)
//             // {

//             //     bound = true;
//             // }
//         }
//         inline void handle_inst(size_t n) override 
//         {
//         }
//         inline void handle_isr() override
//         {   
//             for(size_t n = 0; n < this->m_pins.size(); n++)
//             {
//                 if(this->is_event(n))
//                 {
//                     this->acknowledge_isr(n);
//                     handle_inst(n);
//                 }
//             }
//         }
//     };


//     template<size_t N, IsInputCallback Driver>
//     // requires HasInputIsrCallback<Driver>
//     class InputHandlerWithCallback: public InputHandlerBase<N>
//     {
//         Vector<Driver*, N> m_inst{};
//     public:    
//         explicit InputHandlerWithCallback(irq_handler_t handler)
//             : InputHandlerBase<N>(handler)//, m_irqn(irqn)
//         {
//         }
//         inline bool& bind_driver(Driver* driver, Input* input)
//         {
//             m_inst.push_back(driver);
//             return this->bind_interrupt(input);
//         }   
//         inline void handle_inst(size_t n) override 
//         {
//             m_inst[n]->callback_from_isr(this->m_pins[n]);
//         }
//         // void handle_isr() override
//         // {   
//         //     for(size_t n = 0; n < this->m_pins.size(); n++)
//         //     {
//         //         if(this->is_event(n))
//         //         {
//         //             this->acknowledge_isr(n);
//         //             handle_inst(n);
//         //         }
//         //     }
//         // }
//     };

// }
 
#endif 