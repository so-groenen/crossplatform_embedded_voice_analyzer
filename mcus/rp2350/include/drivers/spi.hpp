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

#include "drivers/interface_async_dma_handler.hpp"
#include "embedded_hal/traits.hpp"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"

#include "drivers/dma.hpp"


namespace spi {

using namespace embedded_hal::blocking;
using embedded_hal::blocking::SpiError;

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
 

static_assert(IsBlockingReadWrite<Spi>, "Spi must implement read & write blocking");
 

} // spi