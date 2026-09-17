#ifndef IRQ_RTOS_HANDLER_HPP
#define IRQ_RTOS_HANDLER_HPP


 
 
#include "embedded_hal/heapless.hpp"
#include <cstddef>
#include <hardware/irq.h>
#include <hardware/structs/dma.h>
#include "interrupts/dma_handlers.hpp"
#include "interrupts/task_handler.hpp"


using embedded_hal::heapless::Vector;

namespace interrupts 
{

    template<irq_num_rp2350 IRQ_n, size_t N>
    class AsyncDmaHandler final: public IAsyncDmaHandler, public AbstractDmaHandler<IRQ_n,N> 
    {
        TaskHandler<N> m_task_handler{};
    public:
        explicit AsyncDmaHandler(irq_handler_t handler)
            : AbstractDmaHandler<IRQ_n, N>(handler)//, m_irqn(irqn)
        {
        }
        void bind_interrupt_from_rtos(Dma* dma, PicoTask* task) override
        {
            this->bind_interrupt(dma);
            if(!m_task_handler.add_task(task))
                panic("DMA handler: Task overflow!");
        }
        void handle_inst(size_t n) override
        {
            m_task_handler.handle_task(n);
        }
    };



    template<typename T>
    concept IsRtosDmaIsrCallback = IsDmaCallback<T> && requires(T t)
    {
        {t.get_task()} -> std::same_as<PicoTask*>;
    };
    
    #define _DMA_ISR_RTOS_CALLBACK
    #define _DMA_ISR_RTOS_CALLBACK_ASSERT(X) static_assert(IsRtosDmaIsrCallback<X>, "must implement get_task()");


    template<irq_num_rp2350 IRQ_n, size_t N, typename Driver>
    requires IsRtosDmaIsrCallback<Driver>
    class AsyncDmaHandlerWithCallback:  public AbstractDmaHandler<IRQ_n,N> 
    {
        Driver* m_inst{};
        TaskHandler<N> m_task_handler{};
    public:    
        explicit AsyncDmaHandlerWithCallback(irq_handler_t handler)
            : AbstractDmaHandler<IRQ_n, N>(handler)//, m_irqn(irqn)
        {
        }
        inline void bind_rtos_driver(Driver* inst)
        {
            m_inst = inst;
            m_task_handler.add_task(inst->get_task());
            this->bind_interrupt(inst->get_dma_ptr());
        }   
        inline void handle_inst(size_t n) override 
        {
            m_task_handler.handle_task(n);
            assert(m_inst);
            m_inst->callback_from_isr(this->m_dma_channels[n]);
        }
    };
}
                                                          


#endif 