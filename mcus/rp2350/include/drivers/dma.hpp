#pragma once


#include <cassert>
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
 
#include "hardware/dma.h"
 
class Dma
{
protected:
    uint m_dma{}; 
    dma_channel_config m_config{};
    static constexpr uint moved_dma_channel{std::numeric_limits<uint>::max()};
public:
    explicit Dma(uint channel)
        : m_dma{channel}, m_config{dma_channel_get_default_config(m_dma)}//, m_dma_irq{dma_irq}
    {
        dma_channel_claim(m_dma);
        channel_config_set_transfer_data_size(&m_config, DMA_SIZE_8);
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

    void write(std::span<uint8_t> buff_to_send, volatile void * write_addr, uint dreq, bool start)
    {
        channel_config_set_dreq(&m_config, dreq);
        // Default:
        channel_config_set_read_increment(&m_config, true);
        channel_config_set_write_increment(&m_config, false);
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
        channel_config_set_read_increment(&m_config, false);
        channel_config_set_write_increment(&m_config, true);
        dma_channel_configure(m_dma, &m_config,
                            rx_buff.data(),            // write address in memory
                            read_addr,                 // read from periph address
                             rx_buff.size(),            // m_spi count (each element is of size transfer_data_size)
                                start);                    // don't start yet
    }
    void start()
    {
        dma_channel_start(m_dma);
    }
    uint channel() const 
    {
        return m_dma;
    }
    void await_blocking()
    {
        dma_channel_wait_for_finish_blocking(m_dma);
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
  