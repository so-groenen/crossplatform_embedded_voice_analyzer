#ifndef IRQ_RTOS_HANDLER_HPP
#define IRQ_RTOS_HANDLER_HPP


 
 
#include "embedded_hal/heapless.hpp"
#include <hardware/irq.h>
#include <hardware/structs/dma.h>
#include "interrupts/dma_handlers.hpp"

using embedded_hal::heapless::Vector;

namespace interrupts 
{

    namespace details 
    {
        // We don't expose this class.
        
        template<irq_num_rp2350 IRQ_n, size_t N>
        class AsyncDmaHandlerBase : public DmaHandler<IRQ_n, N>  
        {
        protected:
            Vector<PicoTask*, N> m_tasks{};
        private:
            inline void handle_task(size_t n) 
            {
                if(this->m_tasks[n])
                {
                    // #ifdef DEBUG
                    // printf(" >>> Hello from RTOS DMA Handler!\n");
                    // #endif 
                    bool xHigherPriorityTaskWoken{};
                    this->m_tasks[0]->notifyGiveFromISR(xHigherPriorityTaskWoken);
                    if(xHigherPriorityTaskWoken)
                        FreeRTOS::Kernel::yield();
                }
            }
        public:
            explicit AsyncDmaHandlerBase(irq_handler_t handler)
                : DmaHandler<IRQ_n, N>(handler)//, m_irqn(irqn)
            {
            }
            void handle_isr() override
            {   
                for(size_t n = 0; n < this->m_dma_channels.size(); n++)
                {
                    if(this->is_event(n))
                    {
                        this->m_ready_flags[n] = true;
                        this->acknowledge_isr(n);
                        this->handle_inst(n);
                        handle_task(n);
                    }
                }
            }
        };
    }



    template<irq_num_rp2350 IRQ_n, size_t N>
    class AsyncDmaHandler : public details::AsyncDmaHandlerBase<IRQ_n, N>, public IAsyncDmaHandler
    {
    public:
        explicit AsyncDmaHandler(irq_handler_t handler)
            : details::AsyncDmaHandlerBase<IRQ_n, N>(handler)//, m_irqn(irqn)
        {
        }
        void bind_interrupt_from_rtos(Dma* dma, PicoTask* task) override
        {
            this->bind_interrupt(dma);
            if(!this->m_tasks.push_back(task))
                panic("DMA handler: Task overflow!");
        }
    };



    template<typename T>
    concept IsRtosDmaIsrCallback = HasDmaIsrCallback<T> && requires(T t)
    {
        {t.get_task()} -> std::same_as<PicoTask*>;
    };
    
    #define _DMA_ISR_RTOS_CALLBACK
    #define _DMA_ISR_RTOS_CALLBACK_ASSERT(X) static_assert(IsRtosDmaIsrCallback<X>, "must implement get_task()");


    template<irq_num_rp2350 IRQ_n, size_t N_Channels, typename Driver>
    requires IsRtosDmaIsrCallback<Driver>
    class AsyncDmaHandlerWithCallback: public details::AsyncDmaHandlerBase<IRQ_n, N_Channels>
    {
        Vector<Driver*, N_Channels> m_inst{};
    public:    
        explicit AsyncDmaHandlerWithCallback(irq_handler_t handler)
            : details::AsyncDmaHandlerBase<IRQ_n, N_Channels>(handler)//, m_irqn(irqn)
        {
        }
        inline bool& bind_rtos_driver(Driver* inst)
        {
            m_inst.push_back(inst);
            this->m_tasks.push_back(inst->get_task());
            return this->bind_interrupt(inst->get_dma_ptr());
        }   
        inline void handle_inst(size_t n) override 
        {
            m_inst[n]->callback_from_isr();
        }
    };
}
                                                          


#endif 