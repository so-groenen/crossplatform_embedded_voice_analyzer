#ifndef IRQ_BAREMETAL_HANDLER_HPP
#define IRQ_BAREMETAL_HANDLER_HPP


 
 
#include "embedded_hal/heapless.hpp"
#include <hardware/irq.h>
#include <hardware/structs/dma.h>
#include "interrupts/interface_dma_handlers.hpp"
#include "interrupts/interface_irq_handler.hpp"

using embedded_hal::heapless::Vector;

namespace interrupts 
{

template<irq_num_rp2350 IRQ_n, size_t N_Channels>
class AbstractDmaHandler: public IDmaHandler, public InterfaceIRQHandler
{
protected:
    irq_handler_t m_handler{};
    Vector<uint, N_Channels> m_dma_channels{};
    // Vector<bool, N_Channels> m_ready_flags{};
    bool is_event(size_t n) const override
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
    void acknowledge_isr(size_t n) override
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
    explicit AbstractDmaHandler(irq_handler_t handler)
        : m_handler(handler)//, m_irqn(irqn)
    {
        irq_set_exclusive_handler(IRQ_n, m_handler);
        irq_set_enabled(IRQ_n, true);
    }
    // returns a "ready flag, which will be set to True when it fires"
    void bind_interrupt(Dma* dma) override
    {
        size_t n = m_dma_channels.size();
        if(!m_dma_channels.push_back(dma->channel()))
            panic("Bind interrupt: m_dma_channels overflow\n");

        dma->set_irq(IRQ_n);
 
    }

    inline void handle_isr() override
    {   
        for(size_t n = 0; n < this->m_dma_channels.size(); n++)
        {
            if(this->is_event(n))
            {
                // m_ready_flags[n] = true;
                this->acknowledge_isr(n);
                handle_inst(n);
            }
        }
    }
};


 
 
template<typename T>
concept IsDmaCallback = requires(T t, uint dma_chan)
{
    {t.callback_from_isr(dma_chan)} -> std::same_as<void>;
    {t.get_dma_ptr()}               -> std::same_as<Dma*>;
};
 
#define _DMA_ISR_CALLBACK
#define _DMA_ISR_CALLBACK_ASSERT(X) static_assert(interrupts::IsDmaCallback<X>, "must implement callback_from_isr() & get_dma_ptr()");



template<irq_num_rp2350 IRQ_n, size_t N_Channels, IsDmaCallback Driver>
class DmaHandlerWithCallback: public AbstractDmaHandler<IRQ_n, N_Channels>
{
    Driver* m_inst{};
public:    
    explicit DmaHandlerWithCallback(irq_handler_t handler)
        : AbstractDmaHandler<IRQ_n, N_Channels>(handler)//, m_irqn(irqn)
    {
    }
    inline void bind_driver(Driver* inst)
    {
        m_inst = inst;
        this->bind_interrupt(inst->get_dma_ptr());
    }   
    inline void handle_inst(size_t n) override 
    {
        assert(m_inst);
        m_inst->callback_from_isr(this->m_dma_channels[n]);
    }
};

#define DMA_HANDLER_CLASS(X, irq, ...)     template<irq_num_rp2350 IRQ_n, size_t N, interrupts::IsDmaCallback Driver> \
                                            explicit X(interrupts::DmaHandlerWithCallback<IRQ_n,N, Driver>& irq, __VA_ARGS__)

}
 
#endif 