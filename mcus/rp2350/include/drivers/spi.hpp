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
 

#include "drivers/interface_dma_irq_handler.hpp"
#include "embedded_hal/heapless.hpp"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"

#include "drivers/dma_raw.hpp"




class SpiRaw
{
protected:
    spi_inst_t* m_spi{};
public:
    explicit SpiRaw(spi_inst_t* spi)
        : m_spi{spi}
    {
    }
    
};


class SpiDMA : public SpiRaw
{
    DmaRaw m_dma;
public:
    SpiDMA(spi_inst_t* spi, DmaRaw dma)
        : SpiRaw(spi), m_dma{std::move(dma)}//, m_buffer(buffer)
    {
    }
    void write_async(std::span<uint8_t> buff_to_send, bool start)
    {   
        m_dma.write(buff_to_send, &spi_get_hw(m_spi)->dr, spi_get_dreq(m_spi, true), start);
    }
    void read_async(std::span<uint8_t> rx_buff, bool start)
    {
        m_dma.read(rx_buff, &spi_get_hw(m_spi)->dr, spi_get_dreq(m_spi, /*is_tx=*/false), start);
    }
    void start()
    {
        m_dma.start();
    }
    uint channel() const 
    {
        return m_dma.channel();
    }
    void await_blocking()
    {
        return m_dma.await_blocking();
    }
    bool is_busy() const 
    {
        return m_dma.is_busy();
    }
    static void start_multiple(std::span<SpiDMA*> instances)
    {
        uint32_t mask = 0;
        for(const auto& spi : instances)
        {
            mask |= (1u << spi->m_dma.channel());
        }
        dma_start_channel_mask(mask);
    }
};
 

 