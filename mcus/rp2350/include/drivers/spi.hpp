#pragma once
 

#include <cassert>
#include <cstdint>
#include <expected>
#include <hardware/address_mapped.h>
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


namespace hardware::spi {

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
    
    // Spi(const Spi& other)
    //     : m_spi{other.m_spi}
    // {}
    static Spi create(spi_inst_t* spi, uint32_t pin_sck, uint32_t pin_mosi, uint32_t pin_miso, const spi::Config& config)
    {
        gpio_set_function(pin_sck,  GPIO_FUNC_SPI);
        gpio_set_function(pin_miso, GPIO_FUNC_SPI);
        gpio_set_function(pin_mosi, GPIO_FUNC_SPI);
        return Spi{spi, config};
    }
    static Spi create(spi_inst_t* spi, uint32_t pin_sck, uint32_t pin_mosi, uint32_t pin_miso)
    {
        auto config = Config::create_default();
        gpio_set_function(pin_sck,  GPIO_FUNC_SPI);
        gpio_set_function(pin_miso, GPIO_FUNC_SPI);
        gpio_set_function(pin_mosi, GPIO_FUNC_SPI);
        return Spi{spi, config};
    }
    static Spi create_tx_only(spi_inst_t* spi, uint32_t pin_sck, uint32_t pin_mosi, const spi::Config& config)
    {
        gpio_set_function(pin_sck,  GPIO_FUNC_SPI);
        gpio_set_function(pin_mosi, GPIO_FUNC_SPI);
        return Spi{spi, config};
    }
    static Spi create_rx_only(spi_inst_t* spi, uint32_t pin_sck, uint32_t pin_miso, const spi::Config& config)
    {
        gpio_set_function(pin_sck,  GPIO_FUNC_SPI);
        gpio_set_function(pin_miso, GPIO_FUNC_SPI);
        return Spi{spi, config};
    }


    void set_config(const spi::Config& config);
    static void set_config(spi_inst_t* s, const spi::Config& config);
    static void init_from_config(spi_inst_t* s, const spi::Config& config);
    uint32_t baudrate() const ;
    void enable_loop_back(bool v)
    {
        v? hw_set_bits(&spi_get_hw(spi_default)->cr1, SPI_SSPCR1_LBM_BITS)
        : hw_clear_bits(&spi_get_hw(spi_default)->cr1, SPI_SSPCR1_LBM_BITS);
    }
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
 

} // hardware