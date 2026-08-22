#pragma once
 

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
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


enum class SpiError 
{
    WriteFailure,
    ReadFailure,
    SizeMismatch
};

namespace spi 
{
    struct Config 
    {
        uint32_t baudrate{1000 * 1000};
        spi_cpha_t phase{SPI_CPHA_0};
        spi_cpol_t polarity{SPI_CPOL_0};

        Config create_default()
        {
            return Config{
                .baudrate = 1000 * 1000,
                .phase = SPI_CPHA_0,
                .polarity = SPI_CPOL_0
            };
        }
    };

}



class Spi
{
protected:
    spi_inst_t* m_spi{};
public:
    explicit Spi(spi_inst_t* spi, uint32_t baudrate = 1000 * 1000)
        : m_spi{spi}
    {
        // spi_init(m_spi, baudrate); // CAN CAUSE MULTIPLE INIT on the same object
    
    }
    explicit Spi(spi_inst_t* spi, const spi::Config& config)
        : m_spi{spi}
    {
        // spi_init(m_spi, config.baudrate);
        spi_set_format(m_spi, 8, config.polarity, config.phase, SPI_MSB_FIRST);
    }
    void set_config(const spi::Config& config)
    {
        spi_set_baudrate(m_spi, config.baudrate);
        spi_set_format(m_spi, 8, config.polarity, config.phase, SPI_MSB_FIRST);
    }
    uint32_t baudrate() const 
    {
        return spi_get_baudrate(m_spi);
    }
    auto write_blocking(const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError>
    {
        int bytes_written = spi_write_blocking(m_spi, tx_buff.data(), tx_buff.size());
        if(bytes_written != tx_buff.size())
        {
            return std::unexpected{SpiError::WriteFailure};
        }
        return{};
    }
    auto read_blocking(std::span<uint8_t> rx_buff) -> std::expected<void, SpiError>
    {
        // 0x00 is dummy values that we are sending, because the way SPI works, as data is shifted in, some is shifted out
        // this is how shift registers work.
        int bytes_read = spi_read_blocking(m_spi, 0x00, rx_buff.data(), rx_buff.size());
        if(bytes_read != rx_buff.size())
        {
            return std::unexpected{SpiError::ReadFailure};
        }
        return{};
    }
    bool is_readable() const 
    {
        return spi_is_readable(m_spi);
    }
    bool is_busy() const 
    {
        return spi_is_busy(m_spi);
    }
    bool is_writable() const
    {
        return spi_is_writable(m_spi);
    }
    auto transfer_blocking(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError>
    {
        if(tx_buff.size() != tx_buff.size())
            return std::unexpected{SpiError::SizeMismatch};

        // 0x00 is dummy values that we are sending, because the way SPI works, as data is shifted in, some is shifted out
        // this is how shift registers work.
        int bytes_read = spi_write_read_blocking(m_spi, tx_buff.data(), rx_buff.data(), rx_buff.size());
        if(bytes_read != rx_buff.size())
        {
            return std::unexpected{SpiError::ReadFailure};
        }
        return{};
    }
    ~Spi()
    {
        // if(m_spi)
        //     spi_deinit(m_spi);
    }
};

// class IAsyncReadWrite 
// {
// public:
//     virtual ~IAsyncReadWrite() = default;
//     virtual void write_async(std::span<uint8_t> buff_to_send, bool start) = 0;
//     virtual void read_async(std::span<uint8_t> rx_buff, bool start) = 0;
// };

class SpiDMA : public Spi //, public IAsyncReadWrite
{
protected:
    Dma m_dma;
public:
    SpiDMA(spi_inst_t* spi, Dma dma)
        : Spi(spi), m_dma{std::move(dma)}//, m_buffer(buffer)
    {
    }
    void write_async(std::span<uint8_t> tx_buff, bool start)// override
    {   
        m_dma.write(tx_buff, 
            &spi_get_hw(m_spi)->dr,
            spi_get_dreq(m_spi, true),
            start);
    }
    void read_async(std::span<uint8_t> rx_buff, bool start)// override
    {
        m_dma.read(rx_buff, 
            &spi_get_hw(m_spi)->dr,
            spi_get_dreq(m_spi, /*is_tx=*/false),
            start);
    }
    void start()
    {
        m_dma.start();
    }
    Dma& dma()
    {
        return m_dma;
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
        irq.enable_interrupt_from_rtos(&m_dma, task);
    }
    void await_write(std::span<uint8_t> tx_buff)  
    {   
        write_async(tx_buff, true);
        m_task.awaitNotifs();
    }
    void await_read(std::span<uint8_t> rx_buff)  
    {
        read_async(rx_buff, true);
        m_task.awaitNotifs();
    }
    void await_transfer(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff)
    {

    }   
};


/// MORE MODERN API 



namespace spi 
{
    enum class Mode 
    {
        Tx,
        Rx,
    };
}

 
template<spi::Mode M>
class SpiDmaHalf : public Spi  
{
protected:
    Dma m_dma;
public:
    explicit SpiDmaHalf(spi_inst_t* spi, Dma dma)
        : Spi(spi), m_dma{std::move(dma)} 
    {
    }
    explicit SpiDmaHalf(spi_inst_t* spi, uint32_t baudrate, Dma dma)
        : Spi(spi, baudrate), m_dma{std::move(dma)} 
    {
    }
    explicit SpiDmaHalf(spi_inst_t* spi, Dma dma, const spi::Config& config)
        : Spi(spi, config), m_dma{std::move(dma)} 
    {
    }
    void write_async(std::span<uint8_t> tx_buff) requires (M == spi::Mode::Tx)
    {   
        m_dma.write(tx_buff, 
            &spi_get_hw(m_spi)->dr,
            spi_get_dreq(m_spi, true),
            true);
    }
    void read_async(std::span<uint8_t> rx_buff) requires (M == spi::Mode::Rx)
    {
        m_dma.read(rx_buff,
            &spi_get_hw(m_spi)->dr,
            spi_get_dreq(m_spi, /*is_tx=*/false),
            true);
    }
    Dma& dma()
    {
        return m_dma;
    }
};


class SpiDmaFull : public Spi //, public IAsyncReadWrite
{
protected:
    Dma m_tx_dma;
    Dma m_rx_dma;
public:
    explicit SpiDmaFull(spi_inst_t* spi, Dma dma_tx, Dma dma_rx)
        : Spi(spi), m_tx_dma{std::move(dma_tx)}, m_rx_dma{std::move(dma_rx)}//, m_buffer(buffer)
    {
    }
    explicit SpiDmaFull(spi_inst_t* spi, uint32_t baudrate, Dma dma_tx, Dma dma_rx)
        : Spi(spi, baudrate), m_tx_dma{std::move(dma_tx)}, m_rx_dma{std::move(dma_rx)}//, m_buffer(buffer)
    {
    }
    explicit SpiDmaFull(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, const spi::Config& config)
        : Spi(spi, config), m_tx_dma{std::move(dma_tx)}, m_rx_dma{std::move(dma_rx)}//, m_buffer(buffer)
    {
    }
    void write_async(std::span<uint8_t> tx_buff)// override
    {   
        m_tx_dma.write(tx_buff, 
            &spi_get_hw(m_spi)->dr,
            spi_get_dreq(m_spi, true),
            true);
    }
    void read_async(std::span<uint8_t> rx_buff)// override
    {
        m_rx_dma.read(rx_buff,
            &spi_get_hw(m_spi)->dr,
            spi_get_dreq(m_spi, /*is_tx=*/false),
            true);
    }
    void transfer_async(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff)
    {
        m_tx_dma.write(tx_buff, 
            &spi_get_hw(m_spi)->dr, 
            spi_get_dreq(m_spi, true),
            false);
        m_rx_dma.read(rx_buff,
            &spi_get_hw(m_spi)->dr,
            spi_get_dreq(m_spi, /*is_tx=*/false),
            false);

        const uint32_t mask = (1u << m_tx_dma.channel()) | (1u << m_rx_dma.channel());
        dma_start_channel_mask(mask);
    }
    Dma& dma_tx()
    {
        return m_tx_dma;
    }
    Dma& dma_rx()
    {
        return m_rx_dma;
    }
};

// 
namespace spi::rtos 
{
    template<spi::Mode M>
    class SpiAsync : public SpiDmaHalf<M>
    {
        PicoTask& m_task;
    public:
        SpiAsync(spi_inst_t* spi, Dma dma, PicoTask* task, ::rtos::IDmaIrqHandler& irq)
            : SpiDmaHalf<M>(spi, std::move(dma)), m_task{*task}//, m_buffer(buffer)
        {
            irq.enable_interrupt_from_rtos(&this->m_dma, task);
        }
        void await_write(std::span<uint8_t> tx_buff) requires (M == spi::Mode::Tx)
        {   
            this->write_async(tx_buff);
            m_task.awaitNotifs();
        }
        void await_read(std::span<uint8_t> rx_buff)  requires (M == spi::Mode::Rx)
        {
            this->read_async(rx_buff);
            m_task.awaitNotifs();
        }
    };

    class SpiAsyncFull : public SpiDmaFull
    {
        PicoTask& m_task;
    public:
        SpiAsyncFull(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, PicoTask* task, ::rtos::IDmaIrqHandler& irq)
            : SpiDmaFull(spi, std::move(dma_tx), std::move(dma_rx)), m_task{*task}
        {
            irq.enable_interrupt_from_rtos(&this->m_rx_dma, task);
            irq.enable_interrupt_from_rtos(&this->m_tx_dma, task);
        }
        void await_write(std::span<uint8_t> tx_buff) 
        {   
            this->write_async(tx_buff);
            m_task.awaitNotifs();
        }
        void await_read(std::span<uint8_t> rx_buff) 
        {
            this->read_async(rx_buff);
            m_task.awaitNotifs();
        }
        void await_transfer(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff)
        {
            transfer_async(rx_buff, tx_buff);
            m_task.awaitNotifs();
        }
    };
}