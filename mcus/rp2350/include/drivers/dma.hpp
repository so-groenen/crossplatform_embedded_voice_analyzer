#pragma once


#include <cassert>
#include <concepts>
#include <cstdint>
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
#include <type_traits>
 
#include "hardware/dma.h"
 
class Dma
{
protected:
    uint m_dma; 
    dma_channel_config m_config{};
    uint m_irqn_index{}; // NOT vector index, but if O or 1. Irq_n, can be reset by the handler
    static constexpr uint moved_dma_channel{std::numeric_limits<uint>::max()};
public:
    explicit Dma(uint channel)
        : m_dma{channel}, m_config{dma_channel_get_default_config(m_dma)}//, m_dma_irq{dma_irq}
    {
        dma_channel_claim(m_dma);
        set_transfer_size<uint8_t>();
    }
    explicit Dma()
        : m_dma{static_cast<uint>(dma_claim_unused_channel(true))}, m_config{dma_channel_get_default_config(m_dma)}
    {
        set_transfer_size<uint8_t>();
    }
    
    Dma(const Dma& other) = delete;
    Dma& operator=(Dma& other) = delete;

    Dma(Dma&& other)
        : m_dma{other.m_dma}, m_config{other.m_config}//, m_dma_irq{other.m_dma_irq}
    {
        // make it faulty to avoid freeing!
        other.m_dma = moved_dma_channel;
    }
    Dma& operator=(Dma&& other)
    {
        if(&other != this)
        {
            m_dma       = other.m_dma;
            other.m_dma = moved_dma_channel;

            m_config  = other.m_config;
            // m_dma_irq = other.m_dma_irq;
        }
        return *this;
    }
    void read_from_memory(bool v)
    {
        channel_config_set_read_increment(&m_config, v);
    }
    void set_data_request(uint dreq)
    {
        channel_config_set_dreq(&m_config, dreq);
    }
    void config_transfer(volatile void* write_addr, const volatile void* read_addr, size_t transfer_count)
    {
        dma_channel_configure(m_dma, &m_config,
                                write_addr,           // write address
                                read_addr,  // read address from memory
                                transfer_count,  // element count (each element is of size transfer_data_size)
                                false);     
    }
    template<std::integral T>
    void config_transfer(std::span<T> write_buff, const std::span<T> read_buff)
    {
        dma_channel_configure(m_dma, &m_config,
                                write_buff.data(),           // write address
                                read_buff.data(),            // read address from memory
                                read_buff.size(),       // element count (each element is of size transfer_data_size)
                                false);     
        
        set_transfer_size<T>();
    }
    // We read in memory FROM a buffer and send it to a register pointed to by write_addr
    template<std::integral T>
    void config_transfer(volatile void* write_addr, const std::span<T> read_buff)
    {
        dma_channel_configure(m_dma, &m_config,
                                write_addr,           // write address
                                read_buff.data(),            // read address from memory
                                read_buff.size(),       // element count (each element is of size transfer_data_size)
                                false);     
        set_transfer_size<T>();
    }
    // We read from a register and write in memory to a buffer and set the length of that buffer
    template<std::integral T>
    void config_transfer(std::span<T> write_buff, const volatile void* read_addr)
    {
        dma_channel_configure(m_dma, &m_config,
                                write_buff.data(),           // write address
                                read_addr,            // read address from memory
                                write_buff.size(),       // element count (each element is of size transfer_data_size)
                                false);     
        
        set_transfer_size<T>();
    }
    void write_to_memory(bool v)
    {
        channel_config_set_write_increment(&m_config, v);
    }
    void write(std::span<uint8_t> buff_to_send, volatile void * write_addr, uint dreq, bool start)
    {
        channel_config_set_dreq(&m_config, dreq);
        // read from memory and send/WRITE to peripheral
        read_from_memory(true); 
        write_to_memory(false);
        dma_channel_configure(m_dma, &m_config,
                                write_addr,           // write address
                                buff_to_send.data(),  // read address from memory
                                buff_to_send.size(),  // element count (each element is of size transfer_data_size)
                                start);     
    }
    // read from read_addr (for ex Spi-data register) and write into rx_buffer
    void read(std::span<uint8_t> rx_buff, const volatile void * read_addr, uint dreq, bool start)
    {
        channel_config_set_dreq(&m_config, dreq);
        // read from peripheral register and write to memory
        read_from_memory(false); 
        write_to_memory(true);
        dma_channel_configure(m_dma, &m_config,
                            rx_buff.data(),            // write address in memory
                            read_addr,                 // read from periph address
                             rx_buff.size(),            
                            start);                     
    }
    inline void set_write_addr(volatile void* write_addr, bool start)
    {
        dma_channel_set_write_addr(m_dma, write_addr, start);   
    }
  
    inline void set_read_addr(const void* read_addr, bool start)
    {
    dma_channel_set_read_addr(m_dma, read_addr, start);   
    }

    void abort()
    {
        dma_channel_abort(m_dma);
    }
    inline void start()
    {
        dma_channel_start(m_dma);
    }
    uint channel() const 
    {
        return m_dma;
    }
    template<std::integral T>
    void set_transfer_size()
    {
        if constexpr (sizeof(T) == 4) 
        {
            channel_config_set_transfer_data_size(&m_config, DMA_SIZE_32);
        }
        else if constexpr (sizeof(T) == 2) 
        {
            channel_config_set_transfer_data_size(&m_config, DMA_SIZE_16);
        }
        else if constexpr (sizeof(T) == 1) 
        {
            channel_config_set_transfer_data_size(&m_config, DMA_SIZE_8);
        }
        else  
        {
            panic("DMA Infer Sized Not implemented");
        }
    }
 
    void await_blocking()
    {
        dma_channel_wait_for_finish_blocking(m_dma);
    }

    // interrupt handling:
    void enable_channel(bool value)
    {
        dma_irqn_set_channel_enabled(m_irqn_index, m_dma, value);
    }
    // default is zero, and IRQ1 is rarely used.
    void set_irqn(uint irqn_index)
    {
        m_irqn_index = irqn_index;
    }
    bool is_busy() const 
    {
        return dma_channel_is_busy(m_dma);
    }
    ~Dma()
    {
        if(m_dma != moved_dma_channel)
            dma_channel_unclaim(m_dma);
    }
    static void start_multiple(std::span<Dma*> instances)
    {
        uint32_t mask = 0;
        for(const auto& s : instances)
        {
            mask |= (1u << s->m_dma);
        }
        dma_start_channel_mask(mask);
    }
};
  