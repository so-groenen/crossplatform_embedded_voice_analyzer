#ifndef IRQ_BAREMETAL_HANDLER_HPP
#define IRQ_BAREMETAL_HANDLER_HPP


 
 
#include "embedded_hal/heapless.hpp"
#include <hardware/irq.h>
#include <hardware/structs/dma.h>
#include "interrupts/interface_dma_handlers.hpp"

using embedded_hal::heapless::Vector;

namespace interrupts 
{

template<irq_num_rp2350 IRQ_n, size_t N_Channels>
class DmaHandler: public IDmaHandler
{
protected:
    irq_handler_t m_handler{};
    Vector<uint, N_Channels> m_dma_channels{};
    Vector<bool, N_Channels> m_ready_flags{};
    bool is_event(size_t n) const
    {
        if constexpr (IRQ_n == DMA_IRQ_0)
        {
            return(dma_hw->ints0 & (1u << m_dma_channels[n]));
        }
        else if constexpr (IRQ_n == DMA_IRQ_1)
        {
            return(dma_hw->ints1 & (1u << m_dma_channels[n]));
        }
        else if constexpr (IRQ_n == DMA_IRQ_2)
        {
            return(dma_hw->ints2 & (1u << m_dma_channels[n]));

        }
        else if constexpr (IRQ_n == DMA_IRQ_3)
        {
            return(dma_hw->ints3 & (1u << m_dma_channels[n]));

        }
    }
    void acknowledge_isr(size_t n)
    {
        if constexpr (IRQ_n == DMA_IRQ_0)
        {
            dma_hw->ints0 = (1u << m_dma_channels[n]);
        }
        else if constexpr (IRQ_n == DMA_IRQ_1)
        {
            dma_hw->ints1 = (1u << m_dma_channels[n]);
        }
        else if constexpr (IRQ_n == DMA_IRQ_2)
        {
            dma_hw->ints2 = (1u << m_dma_channels[n]);
        }
        else if constexpr (IRQ_n == DMA_IRQ_3)
        {
            dma_hw->ints3 = (1u << m_dma_channels[n]);
        }
    }
public:
    explicit DmaHandler(irq_handler_t handler)
        : m_handler(handler)//, m_irqn(irqn)
    {
    }
    // returns a "ready flag, which will be set to True when it fires"
    bool& bind_interrupt(Dma* dma) override
    {
        size_t n = m_dma_channels.size();
        if(!m_dma_channels.push_back(dma->channel()))
            panic("Bind interrupt: m_dma_channels overflow\n");

        dma->set_irq(IRQ_n);

        static bool bound = false;
        if(m_handler && !bound)
        {
            irq_set_exclusive_handler(IRQ_n, m_handler);
            irq_set_enabled(IRQ_n, true);
            bound = true;
        }
        return m_ready_flags[n];
    }
    inline void handle_inst(size_t n) override 
    {
    }
    inline void handle_isr() override
    {   
        for(size_t n = 0; n < this->m_dma_channels.size(); n++)
        {
            if(this->is_event(n))
            {
                m_ready_flags[n] = true;
                this->acknowledge_isr(n);
                handle_inst(n);
            }
        }
    }
};


 
 
template<typename T>
concept HasDmaIsrCallback = requires(T t)
{
    {t.callback_from_isr()} -> std::same_as<void>;
    {t.get_dma_ptr()}       -> std::same_as<Dma*>;
};
 
#define _DMA_ISR_CALLBACK
#define _DMA_ISR_CALLBACK_ASSERT(X) static_assert(interrupts::HasDmaIsrCallback<X>, "must implement callback_from_isr() & get_dma_ptr()");



template<irq_num_rp2350 IRQ_n, size_t N_Channels, typename Driver>
requires HasDmaIsrCallback<Driver>
class DmaHandlerWithCallback: public DmaHandler<IRQ_n, N_Channels>
{
    Vector<Driver*, N_Channels> m_inst{};
public:    
    explicit DmaHandlerWithCallback(irq_handler_t handler)
        : DmaHandler<IRQ_n, N_Channels>(handler)//, m_irqn(irqn)
    {
    }
    inline bool& bind_driver(Driver* inst)
    {
        m_inst.push_back(inst);
        return this->bind_interrupt(inst->get_dma_ptr());
    }   
    inline void handle_inst(size_t n) override 
    {
        m_inst[n]->callback_from_isr();
    }
    void handle_isr() override
    {   
        for(size_t n = 0; n < this->m_dma_channels.size(); n++)
        {
            if(this->is_event(n))
            {
                this->m_ready_flags[n] = true;
                this->acknowledge_isr(n);
                handle_inst(n);
            }
        }
    }
};

}
 
#endif 