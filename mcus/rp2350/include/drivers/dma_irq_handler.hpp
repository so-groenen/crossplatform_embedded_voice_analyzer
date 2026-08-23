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
#include "drivers/interface_dma_irq_handler.hpp"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"
#include "drivers/dma.hpp"

 
// unblock threads from interrupts
// should have pointer to thread
// and to Dma to acknowledge interrupt;
// also enable interrupt


enum class DmaIrq : uint32_t
{
    IRQ_0 = DMA_IRQ_0,
    IRQ_1 = DMA_IRQ_1, 
    IRQ_2 = DMA_IRQ_2,
    IRQ_3 = DMA_IRQ_3,
};
 
using embedded_hal::heapless::Vector;


 

   
namespace base   
{

template<DmaIrq IRQ_n, size_t N_Channels>
class DmaIrqHandler: public base::IDmaIrqHandler
{
public:
    irq_handler_t m_handler{};
    Vector<uint, N_Channels> m_dma_channels{};
    // std::array<Dma*, 5> m_dma_insts
    bool is_event(size_t n) const
    {
        if constexpr (IRQ_n == DmaIrq::IRQ_0)
        {
            return(dma_hw->ints0 & (1u << m_dma_channels[n]));
        }
        else if constexpr (IRQ_n == DmaIrq::IRQ_1)
        {
            return(dma_hw->ints1 & (1u << m_dma_channels[n]));

        }
        else if constexpr (IRQ_n == DmaIrq::IRQ_2)
        {
            return(dma_hw->ints2 & (1u << m_dma_channels[n]));

        }
        else if constexpr (IRQ_n == DmaIrq::IRQ_3)
        {
            return(dma_hw->ints3 & (1u << m_dma_channels[n]));
        }
    }
    void acknowledge_isr(size_t n)
    {
        if constexpr (IRQ_n == DmaIrq::IRQ_0)
        {
            dma_hw->ints0 = (1u << m_dma_channels[n]);
        }
        else if constexpr (IRQ_n == DmaIrq::IRQ_1)
        {
            dma_hw->ints1 = (1u << m_dma_channels[n]);
        }
        else if constexpr (IRQ_n == DmaIrq::IRQ_2)
        {
            dma_hw->ints2 = (1u << m_dma_channels[n]);
        }
        else if constexpr (IRQ_n == DmaIrq::IRQ_3)
        {
            dma_hw->ints3 = (1u << m_dma_channels[n]);
        }
    }
    explicit DmaIrqHandler(irq_handler_t handler)
        : m_handler(handler)//, m_irqn(irqn)
    {
    }
    void bind_interrupt(Dma* dma) override
    {
        m_dma_channels.push_back(dma->channel());
        if constexpr (IRQ_n == DmaIrq::IRQ_1)
            dma->set_irqn(1); // By default we use IRQ_0!
        dma->enable_channel(true);

        // Register our callback function to the CPU vector table for DMA_IRQ_0 etc...
        irq_set_exclusive_handler(static_cast<irq_num_t>(IRQ_n), m_handler);
        irq_set_enabled(static_cast<irq_num_t>(IRQ_n), true);
    }
    void handle_isr() override
    {   
        for(size_t n = 0; n < this->m_dma_channels.size(); n++)
        {
            if(this->is_event(n))
            {
                printf(">>>> Hello From Interrupt \n");
                this->acknowledge_isr(n);
            }
        }
    }
};
}




namespace rtos 
{
template<DmaIrq IRQ_n, size_t N>
class DmaIrqHandler : public base::DmaIrqHandler<IRQ_n, N> , public rtos::IDmaIrqHandler
{
    Vector<PicoTask*, N> m_tasks{};
public:
    explicit DmaIrqHandler(irq_handler_t handler)
        : base::DmaIrqHandler<IRQ_n, N>(handler)//, m_irqn(irqn)
    {
    }
    void bind_interrupt_from_rtos(Dma* dma, PicoTask* task) override
    {
        this->bind_interrupt(dma);
        if(!m_tasks.push_back(task))
            panic("DMA handler: Task overflow!");
        if (m_tasks.size() != this->m_dma_channels.size())
            panic("DMA handler: Task vs DMA channel mismatch!");
    }
    void handle_isr() override
    {   
        for(size_t n = 0; n < this->m_dma_channels.size(); n++)
        {
            if(this->is_event(n))
            {
                this->acknowledge_isr(n);
                if(this->m_tasks[0])
                {
                    printf(" >>> Hello from RTOS DMA Handler!\n");
                    bool xHigherPriorityTaskWoken{};
                    this->m_tasks[0]->notifyGiveFromISR(xHigherPriorityTaskWoken);
                    if(xHigherPriorityTaskWoken)
                        FreeRTOS::Kernel::yield();
                }
            }
        }
    }
 
};
}

#define bind_interrupt_DMA_0(X, irqs, handler_name, n_channels)                             \
    void handler_name();                                                                    \
    [[maybe_unused]] static X::DmaIrqHandler<DmaIrq::IRQ_0, n_channels> irqs{handler_name}; \
    void handler_name() {                                                                   \
        irqs.handle_isr();                                                                  \
    }                                                                                       

#define bind_interrupt_DMA_1(X, irqs, handler_name, n_channels)                             \
    void handler_name();                                                                    \
    [[maybe_unused]] static X::DmaIrqHandler<DmaIrq::IRQ_1, n_channels> irqs{handler_name}; \
    void handler_name() {                                                                   \
        irqs.handle_isr();                                                                  \
    }                                                                                       


    