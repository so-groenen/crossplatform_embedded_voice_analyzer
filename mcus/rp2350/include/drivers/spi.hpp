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

#include "drivers/dma.hpp"




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

// class IAsyncReadWrite 
// {
// public:
//     virtual ~IAsyncReadWrite() = default;
//     virtual void write_async(std::span<uint8_t> buff_to_send, bool start) = 0;
//     virtual void read_async(std::span<uint8_t> rx_buff, bool start) = 0;
// };

class SpiDMA : public SpiRaw //, public IAsyncReadWrite
{
protected:
    Dma m_dma;
public:
    SpiDMA(spi_inst_t* spi, Dma dma)
        : SpiRaw(spi), m_dma{std::move(dma)}//, m_buffer(buffer)
    {
    }
    void write_async(std::span<uint8_t> buff_to_send, bool start)// override
    {   
        m_dma.write(buff_to_send, &spi_get_hw(m_spi)->dr, spi_get_dreq(m_spi, true), start);
    }
    void read_async(std::span<uint8_t> rx_buff, bool start)// override
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
    Dma* dma()
    {
        return &m_dma;
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
 



class SpiRTOS : public SpiDMA 
{
    PicoTask& m_task;
public:
    SpiRTOS(spi_inst_t* spi, Dma dma, PicoTask* task, rtos::IDmaIrqHandler& irq)
        : SpiDMA(spi, std::move(dma)), m_task{*task}//, m_buffer(buffer)
    {
        irq.setup_interrupt(&m_dma, task);
    }
    void await_write(std::span<uint8_t> buff_to_send)  
    {   
        write_async(buff_to_send, true);
        m_task.awaitNotifs();
    }
    void await_read(std::span<uint8_t> rx_buff)  
    {
        read_async(rx_buff, true);
        m_task.awaitNotifs();
    }
     
};


// PicoTask* RX_task{};

// class ReaderTask : public PicoTask
// {
//     SpiRTOS m_reader;
// public:
//     ReaderTask(spi_inst_t* spi, Dma rx_dma, rtos::IDmaIrqHandler& irq)
//         : PicoTask(1, 512, "reader-task"), m_reader(spi, std::move(rx_dma), this, irq)
//     {
//     }
//     void taskFunction() override
//     {
//         static uint8_t BUFFER[512] = {};
//         // for(size_t n = 0; n< 512; n++)
//         // {
//         //     BUFFER[n] = n % 255;
//         // }

//         while(true)
//         {
//             m_reader.await_read(BUFFER);
//         }
//     }
// };

 
// class WriterTask : public PicoTask
// {
//     SpiRTOS m_reader;
// public:
//     WriterTask(spi_inst_t* spi, Dma tx_dma, rtos::IDmaIrqHandler& irq)
//         : PicoTask(1, 512, "writer-task"), m_reader(spi, std::move(tx_dma), this, irq)
//     {
//     }
//     void taskFunction() override
//     {
//         static uint8_t BUFFER[512] = {};
//         for(size_t n = 0; n< 512; n++)
//         {
//             BUFFER[n] = n % 255;
//         }

//         while(true)
//         {
//             m_reader.await_write(BUFFER);
//         }
//     }
// };