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
    explicit Spi(spi_inst_t* spi, uint32_t baudrate = 1000 * 1000);
    explicit Spi(spi_inst_t* spi, const spi::Config& config);
    
    void set_config(const spi::Config& config);
    static void set_config(spi_inst_t* s, const spi::Config& config);
    static void init_from_config(spi_inst_t* s, const spi::Config& config);
    uint32_t baudrate() const ;

    // blocking IO
    auto write_blocking(const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError>;
    auto read_blocking(std::span<uint8_t> rx_buff) -> std::expected<void, SpiError>;
    auto transfer_blocking(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError>;

    // state
    bool is_readable() const;
    bool is_busy() const;
    bool is_writable() const;

    ~Spi();

};
 

static_assert(IsBlockingReadWrite<Spi>, "Spi must implement read & write blocking");
 

} // spi