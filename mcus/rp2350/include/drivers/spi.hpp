#pragma once
 

#include <cassert>
#include <cstdint>
#include <expected>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/intctrl.h>
#include <hardware/sync.h>

#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>

#include "drivers/interface_dma_irq_handler.hpp"

#include "hardware/spi.h"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"

#include "drivers/dma.hpp"


namespace spi {

enum class SpiError 
{
    WriteFailure,
    ReadFailure,
    SizeMismatch
};

 
struct Config 
{
    uint32_t baudrate{1000 * 1000};
    spi_cpha_t phase{SPI_CPHA_0};
    spi_cpol_t polarity{SPI_CPOL_0};
    spi_order_t order{SPI_MSB_FIRST};
    uint32_t data_bits{8};

    static Config create_default()
    {
        return Config{
            .baudrate = 1000 * 1000,
            .phase = SPI_CPHA_0,
            .polarity = SPI_CPOL_0,
            .order = SPI_MSB_FIRST,
            .data_bits = 8,
        };
    }

};




class Spi
{
protected:
    spi_inst_t* m_spi{};
public:
    explicit Spi(spi_inst_t* spi, uint32_t baudrate = 1000 * 1000)
        : m_spi{spi}
    {    
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
        spi_set_format(m_spi, config.data_bits, config.polarity, config.phase, config.order);
    }
    static void set_config(spi_inst_t* s, const spi::Config& config)
    {
        spi_set_baudrate(s, config.baudrate);
        spi_set_format(s, config.data_bits, config.polarity, config.phase, config.order);
    }
    static void init_from_config(spi_inst_t* s, const spi::Config& config)
    {
        spi_init(s, config.baudrate);
        spi_set_format(s, config.data_bits, config.polarity, config.phase, config.order);
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
 
enum class Mode 
{
    Tx,
    Rx,
};


template<spi::Mode M>
class DmaForSpi : public Dma
{
public:
    explicit DmaForSpi(uint32_t channel)
        : Dma{channel} 
    {
    }
    explicit DmaForSpi(Dma dma)
        : Dma{std::move(dma)} 
    {
    }
    void write_to_spi(spi_inst_t* spi, std::span<uint8_t> tx_buff, bool start) requires (M == spi::Mode::Tx)
    {   
        write(tx_buff, 
            &spi_get_hw(spi)->dr,
            spi_get_dreq(spi, true),
            start);
    }
    void read_from_spi(spi_inst_t* spi, std::span<uint8_t> rx_buff, bool start) requires (M == spi::Mode::Rx)
    {
        read(rx_buff,
            &spi_get_hw(spi)->dr,
            spi_get_dreq(spi, /*is_tx=*/false),
            start);
    }
    // Dma& dma()
    // {
    //     return *this;
    // }
};


namespace base
{
    template<spi::Mode M>
    class SpiDma : public Spi  
    {
    protected:
        DmaForSpi<M> m_dma;
    public:
        explicit SpiDma(spi_inst_t* spi, Dma dma)
            : Spi(spi), m_dma{std::move(dma)} 
        {
        }
        explicit SpiDma(spi_inst_t* spi, uint32_t baudrate, Dma dma)
            : Spi(spi, baudrate), m_dma{std::move(dma)} 
        {
        }
        explicit SpiDma(spi_inst_t* spi, Dma dma, const spi::Config& config)
            : Spi(spi, config), m_dma{std::move(dma)} 
        {
        }
        void write_async(std::span<uint8_t> tx_buff, bool start) requires (M == spi::Mode::Tx)
        {   
            m_dma.write_to_spi(m_spi,
                                tx_buff, 
                                start);
        }
        void read_async(std::span<uint8_t> rx_buff, bool start) requires (M == spi::Mode::Rx)
        {
            m_dma.read_from_spi(m_spi,
                                rx_buff,
                                start);
        }
        Dma& dma()
        {
            return m_dma;
        }
    };



    class SpiDmaFull : public Spi //, public IAsyncReadWrite
    {
    protected:
        DmaForSpi<spi::Mode::Tx> m_dma_tx;
        DmaForSpi<spi::Mode::Rx> m_dma_rx;
    public:
        explicit SpiDmaFull(spi_inst_t* spi, Dma dma_tx, Dma dma_rx)
            : Spi(spi), m_dma_tx{std::move(dma_tx)}, m_dma_rx{std::move(dma_rx)}
        {
        }
        explicit SpiDmaFull(spi_inst_t* spi, uint32_t baudrate, Dma dma_tx, Dma dma_rx)
            : Spi(spi, baudrate), m_dma_tx{std::move(dma_tx)}, m_dma_rx{std::move(dma_rx)} 
        {
        }
        explicit SpiDmaFull(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, const spi::Config& config)
            : Spi(spi, config), m_dma_tx{std::move(dma_tx)}, m_dma_rx{std::move(dma_rx)} 
        {
        }
        void write_async(std::span<uint8_t> tx_buff, bool start) 
        {   
            m_dma_tx.write_to_spi(m_spi,
                                tx_buff, 
                                start);
        }
        void read_async(std::span<uint8_t> rx_buff, bool start) 
        {
            m_dma_rx.read_from_spi(m_spi,
                                rx_buff,
                                start);
        }
        void transfer_async(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff, bool start)
        {
            m_dma_tx.write_to_spi(m_spi,
                                tx_buff, 
                                start);
            m_dma_rx.read_from_spi(m_spi,
                                rx_buff,
                                start);
            if(!start)
                return;

            const uint32_t mask = (1u << m_dma_tx.channel()) | (1u << m_dma_rx.channel());
            dma_start_channel_mask(mask);
        }
        Dma& dma_tx()
        {
            return m_dma_tx;
        }
        Dma& dma_rx()
        {
            return m_dma_rx;
        }
    };

}

 



namespace rtos 
{
    template<spi::Mode M>
    class SpiAsync : public Spi
    {
        PicoTask& m_task;
        DmaForSpi<M> m_dma{};
    public:
        SpiAsync(spi_inst_t* spi, Dma dma, PicoTask* task, ::rtos::IDmaIrqHandler& irq)
            : Spi(spi), m_dma{std::move(dma)}, m_task{*task}//, m_buffer(buffer)
        {
            irq.enable_interrupt_from_rtos(&m_dma, task);
        }
        void await_write(std::span<uint8_t> tx_buff) requires (M == spi::Mode::Tx)
        {   
            m_dma.write_to_spi(m_spi, tx_buff, true);
            m_task.awaitNotifs();
        }
        void await_read(std::span<uint8_t> rx_buff)  requires (M == spi::Mode::Rx)
        {
            m_dma.read_from_spi(m_spi, rx_buff, true);
            m_task.awaitNotifs();
        }
    };


    class SpiAsyncFull : public Spi
    {
        PicoTask& m_task;
        DmaForSpi<spi::Mode::Tx> m_dma_tx;
        DmaForSpi<spi::Mode::Rx> m_dma_rx;
    public:
        SpiAsyncFull(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, PicoTask* task, ::rtos::IDmaIrqHandler& irq)
            : Spi{spi}, m_dma_tx{std::move(dma_tx)}, m_dma_rx{std::move(dma_rx)}, m_task{*task}
        {
            irq.enable_interrupt_from_rtos(&m_dma_tx, task);
            irq.enable_interrupt_from_rtos(&m_dma_rx, task);
        }
        void await_write(std::span<uint8_t> tx_buff) 
        {   
            m_dma_tx.write_to_spi(m_spi, tx_buff, true);
            m_task.awaitNotifs();
        }
        void await_read(std::span<uint8_t> rx_buff) 
        {
            m_dma_rx.read_from_spi(m_spi, rx_buff, true);
            m_task.awaitNotifs();
        }
        void await_transfer(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff)
        {
            m_dma_tx.write_to_spi(m_spi, tx_buff, false);
            m_dma_rx.read_from_spi(m_spi, rx_buff, false);
            const uint32_t mask =  (1u << m_dma_rx.channel()) | (1u << m_dma_tx.channel());
            
            dma_start_channel_mask(mask);
            m_task.awaitNotifs();
        }
    };
}


} // spi