#pragma once


#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/intctrl.h>
#include <hardware/sync.h>
#include <limits>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
 

#include "embedded_hal/heapless.hpp"
#include "drivers/interface_async_dma_handler.hpp"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"
#include "drivers/dma.hpp"

 
// unblock threads from interrupts
// should have pointer to thread
// and to Dma to acknowledge interrupt;
// also enable interrupt


// enum class DmaIrq : uint32_t
// {
//     IRQ_0 = DMA_IRQ_0,
//     IRQ_1 = DMA_IRQ_1, 
//     IRQ_2 = DMA_IRQ_2,
//     IRQ_3 = DMA_IRQ_3,
// };
 
using embedded_hal::heapless::Vector;

// namespace interrupt {

template<irq_num_rp2350 IRQ_n, size_t N_Channels>
class DmaHandlerBase: public irq::IDmaHandler
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
    }
public:
    explicit DmaHandlerBase(irq_handler_t handler)
        : m_handler(handler)//, m_irqn(irqn)
    {
    }
    // returns a "ready flag, which will be set to True when it fires"
    bool& bind_interrupt(Dma* dma) override
    {
        size_t n = m_dma_channels.size();
        if(!m_dma_channels.push_back(dma->channel()))
            panic("Bind interrupt: m_dma_channels overflow\n");

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
#define _DMA_ISR_CALLBACK_ASSERT(X) static_assert(HasDmaIsrCallback<X>, "must implement callback_from_isr() & get_dma_ptr()");



template<irq_num_rp2350 IRQ_n, size_t N_Channels, typename Driver>
requires HasDmaIsrCallback<Driver>
class DmaHandlerWithCallback: public DmaHandlerBase<IRQ_n, N_Channels>
{
    Vector<Driver*, N_Channels> m_inst{};
public:    
    explicit DmaHandlerWithCallback(irq_handler_t handler)
        : DmaHandlerBase<IRQ_n, N_Channels>(handler)//, m_irqn(irqn)
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




namespace irq 
{


    template<irq_num_rp2350 IRQ_n, size_t N>
    class AsyncDmaHandlerBase : public DmaHandlerBase<IRQ_n, N>  
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
            : DmaHandlerBase<IRQ_n, N>(handler)//, m_irqn(irqn)
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




    template<irq_num_rp2350 IRQ_n, size_t N>
    class AsyncDmaHandler : public AsyncDmaHandlerBase<IRQ_n, N>, public irq::IAsyncDmaHandler
    {
    public:
        explicit AsyncDmaHandler(irq_handler_t handler)
            : AsyncDmaHandlerBase<IRQ_n, N>(handler)//, m_irqn(irqn)
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
    class AsyncDmaHandlerCallback: public AsyncDmaHandlerBase<IRQ_n, N_Channels>
    {
        Vector<Driver*, N_Channels> m_inst{};
    public:    
        explicit AsyncDmaHandlerCallback(irq_handler_t handler)
            : AsyncDmaHandlerBase<IRQ_n, N_Channels>(handler)//, m_irqn(irqn)
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





// template<irq_num_rp2350 IRQ_n, size_t N>
// class DmaHandler : public DmaHandlerBase<IRQ_n, N> 
// {
//     Vector<bool, N> m_flags{};
// public:
//     explicit DmaHandler(irq_handler_t handler)
//         : DmaHandlerBase<IRQ_n, N>(handler)//, m_irqn(irqn)
//     {
//     }
//     inline void handle_inst(size_t n) override 
//     {
//         m_flags[n] = true;
//     }
//     bool& ready_flag(size_t n = 0)
//     {
//         return m_flags[n];
//     }
// };


}

#define bind_interrupt_DMA_0(irqs, handler_name, n_channels)                                \
    void handler_name();                                                                    \
    [[maybe_unused]] static irq::AsyncDmaHandler<DMA_IRQ_0, n_channels> irqs{handler_name}; \
    void handler_name() {                                                                   \
        irqs.handle_isr();                                                                  \
    }                                                                                       


#define BIND_INTERRUPT(irqs, irq_class, handler_name)     \
    void handler_name();                                  \
    [[maybe_unused]] static irq_class irqs{handler_name}; \
    void handler_name() {                                 \
        irqs.handle_isr();                                                                  \
    }                                                                                       

#define bind_interrupt_DMA_1(X, irqs, handler_name, n_channels)                             \
    void handler_name();                                                                    \
    [[maybe_unused]] static X::AsyncDmaHandler<DMA_IRQ_0, n_channels> irqs{handler_name}; \
    void handler_name() {                                                                   \
        irqs.handle_isr();                                                                  \
    }                                                                                       


#define bind_base_interrupt_DMA_0(irqs, handler_name, n_channels)                             \
    void handler_name();                                                                    \
    [[maybe_unused]] static DmaHandler<DMA_IRQ_0, n_channels> irqs{handler_name}; \
    void handler_name() {                                                                   \
        irqs.handle_isr();                                                                  \
    }                                                                                       

                                                                                  


    