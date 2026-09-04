#pragma once
 

#include <cassert>
#include <cstdint>
#include <expected>
#include <hardware/address_mapped.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/intctrl.h>
#include <hardware/sync.h>

#include <optional>
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
#include "types.hpp"


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



enum class SpiMode 
{
    Tx, Rx, RxTx,
};

template<SpiMode M>
class SpiBlocking
{
protected:
    spi_inst_t* m_spi{};
public:
    explicit SpiBlocking(spi_inst_t* spi, u32 baudrate = 1000 * 1000)
        : m_spi{spi}
    {
        spi_init(m_spi, baudrate);
    }
    explicit SpiBlocking(spi_inst_t* spi, const spi::Config& config)
        : m_spi{spi}
    {
        spi_init(m_spi, config.baudrate);
        // init_from_config(m_spi, config);
        spi_set_format(m_spi, 8, config.polarity, config.phase, SPI_MSB_FIRST);
    }
     
 
    void enable_loop_back(bool v)
    {
        v? hw_set_bits(&spi_get_hw(m_spi)->cr1, SPI_SSPCR1_LBM_BITS)
        : hw_clear_bits(&spi_get_hw(m_spi)->cr1, SPI_SSPCR1_LBM_BITS);
    }
    void set_config(const spi::Config& config)
    {
        spi_set_baudrate(m_spi, config.baudrate);
        spi_set_format(m_spi, config.data_bits, config.polarity, config.phase, config.order);
    }
    io_rw_32* data_register()
    {
        return &spi_get_hw(m_spi)->dr;
    }
    u32 get_dreq(bool is_tx)
    {
        return spi_get_dreq(m_spi, is_tx);
    }
    u32 get_baudrate() const 
    {
        return spi_get_baudrate(m_spi);
    }
    auto write_blocking(const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError> requires (M == SpiMode::Tx || M == SpiMode::RxTx)
    {
        int bytes_written = spi_write_blocking(m_spi, tx_buff.data(), tx_buff.size());
        if(bytes_written != tx_buff.size())
        {
            return std::unexpected{SpiError::WriteFailure};
        }
        return{};
    }
    auto read_blocking(std::span<uint8_t> rx_buff) -> std::expected<void, SpiError> requires (M == SpiMode::Rx || M == SpiMode::RxTx)
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
    bool is_readable() const requires (M == SpiMode::Rx || M == SpiMode::RxTx)
    {
        return spi_is_readable(m_spi);
    }
    bool is_busy() const 
    {
        return spi_is_busy(m_spi);
    }
    bool is_writable() const requires (M == SpiMode::Tx || M == SpiMode::RxTx)
    {
        return spi_is_writable(m_spi);
    }
    auto transfer_blocking(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError>  requires (M == SpiMode::RxTx)
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
    ~SpiBlocking()
    {
        // if(m_spi)
        //     spi_deinit(m_spi);
    }   
};
 

template<SpiMode M>
class SpiDma : public SpiBlocking<M>
{
protected:
    std::optional<Dma> m_dma_tx{std::nullopt};
    std::optional<Dma> m_dma_rx{std::nullopt};
private:
    void init_dma_tx()
    {
        if(!m_dma_tx)
            return;

        m_dma_tx->set_data_request(this->get_dreq(true));
        m_dma_tx->set_transfer_size<u8>();
        m_dma_tx->write_to_memory(false);
        m_dma_tx->read_from_memory(true);
    }
    void init_dma_rx()
    {
        if(!m_dma_rx)
            return;

        m_dma_rx->set_data_request(this->get_dreq(false));
        m_dma_rx->set_transfer_size<u8>();
        m_dma_rx->write_to_memory(true);
        m_dma_rx->read_from_memory(false);
    }
public:

    explicit SpiDma(spi_inst_t* spi, Dma dma_tx, const spi::Config& config) requires (M == SpiMode::Tx)
        : SpiBlocking<M>{spi, config}, m_dma_tx{std::move(dma_tx)}
    {
        init_dma_tx();
    }

    explicit SpiDma(spi_inst_t* spi, Dma dma_rx, const spi::Config& config) requires (M == SpiMode::Rx)
        : SpiBlocking<M>{spi, config}, m_dma_rx{std::move(dma_rx)}
    {
        init_dma_rx();
    }

    explicit SpiDma(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, const spi::Config& config) requires (M == SpiMode::RxTx)
        : SpiBlocking<M>{spi, config}, m_dma_tx{std::move(dma_tx)}, m_dma_rx{std::move(dma_rx)}
    {
        init_dma_rx();
        init_dma_tx();
    }

    Dma& dma_tx() requires (M == SpiMode::Tx || M == SpiMode::RxTx)
    {
        return m_dma_tx.value();
    }
    Dma& dma_rx() requires (M == SpiMode::Rx || M == SpiMode::RxTx)
    {
        return m_dma_rx.value();
    }
    void write_dma(std::span<const u8> tx_buff, bool start = true) requires (M == SpiMode::Tx || M == SpiMode::RxTx)
    {   
        m_dma_tx->config_transfer(this->data_register(),
                                  tx_buff.data(),
                                  tx_buff.size(), 
                                  start);
    }
    void read_dma(std::span<u8> rx_buff, bool start = true) requires (M == SpiMode::Rx || M == SpiMode::RxTx)
    {
        m_dma_rx->config_transfer(rx_buff.data(),
                                  this->data_register(),
                                  rx_buff.size(), 
                                  start);
    }

    void transfer_dma(std::span<uint8_t> rx_buff, std::span<const u8> tx_buff, bool start = true) requires (M == SpiMode::RxTx)
    {
        write_dma(tx_buff, 
                  false);
        read_dma(rx_buff,
                 false);

        if(!start)
            return;

        dma_start_channel_mask( (1u << m_dma_tx->channel()) | (1u << m_dma_rx->channel())  );
    }
};



 

template<IOMode Io>
class Spi;
 

template<>
class Spi<IOMode::Blocking> 
{
public:
    static auto make(spi_inst_t* inst, u32 sck_pin, u32 mosi_pin, u32 miso_pin, const Config& config) -> SpiBlocking<SpiMode::RxTx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiBlocking<SpiMode::RxTx>{inst, config};
    }   
    
    static auto make_tx_only(spi_inst_t* inst, u32 sck_pin, u32 mosi_pin, const Config& config) -> SpiBlocking<SpiMode::Tx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiBlocking<SpiMode::Tx>{inst, config};
    }   
 
    static auto make_rx_only(spi_inst_t* inst, u32 sck_pin, u32 mosi_pin, const Config& config) -> SpiBlocking<SpiMode::Tx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiBlocking<SpiMode::Tx>{inst, config};
    }   
   
};


template<>
class Spi<IOMode::NonBlocking> 
{
public:
    
    static auto make(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, u32 miso_pin, Dma dma_tx, Dma dma_rx, const Config& config) -> SpiDma<SpiMode::RxTx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiDma<SpiMode::RxTx>{inst, std::move(dma_tx), std::move(dma_rx), config};
    }   
 
    static auto make_tx_only(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, Dma dma_tx, const Config& config) -> SpiDma<SpiMode::Tx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiDma<SpiMode::Tx>{inst, std::move(dma_tx), config};
    }   

 
    static auto make_rx_only(spi_inst_t* inst, u32 sck_pin, u32 miso_pin, Dma dma_rx, const Config& config) -> SpiDma<SpiMode::Rx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        return SpiDma<SpiMode::Rx>{inst, std::move(dma_rx), config};
    }   
};
 

} // hardware