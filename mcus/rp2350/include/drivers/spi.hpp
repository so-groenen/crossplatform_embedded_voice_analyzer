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

#include "interrupts/interface_dma_handlers.hpp"
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





 

template <typename T, IOMode I, TransferMode M>
inline constexpr bool is_spi_implementation = false;

template <typename T, IOMode I, TransferMode M>
concept SpiImpl = is_spi_implementation<T, I, M>;


template<TransferMode M>
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
    io_rw_32* data_register() const
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
    auto write_blocking(const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError> requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        int bytes_written = spi_write_blocking(m_spi, tx_buff.data(), tx_buff.size());
        if(bytes_written != tx_buff.size())
        {
            return std::unexpected{SpiError::WriteFailure};
        }
        return{};
    }
    auto read_blocking(std::span<uint8_t> rx_buff) -> std::expected<void, SpiError> requires (M == TransferMode::Rx || M == TransferMode::RxTx)
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
    bool is_readable() const requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        return spi_is_readable(m_spi);
    }
    bool is_busy() const 
    {
        return spi_is_busy(m_spi);
    }
    bool is_writable() const requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        return spi_is_writable(m_spi);
    }
    auto transfer_blocking(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError>  requires (M == TransferMode::RxTx)
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


// Register this implementation with the concept system
template <typename T, TransferMode M>
inline constexpr bool is_spi_implementation<T, IOMode::Blocking, M> = std::same_as<T, SpiBlocking<M>>; 



template<TransferMode M>
class SpiDma : public SpiBlocking<M> , public DmaDuplex<M>
{

public:

    explicit SpiDma(spi_inst_t* spi, Dma dma_tx, const spi::Config& config) requires (M == TransferMode::Tx)
        : SpiBlocking<M>{spi, config}, DmaDuplex<TransferMode::Tx>{std::move(dma_tx), this->get_dreq(true)}
    {
    }

    explicit SpiDma(spi_inst_t* spi, Dma dma_rx, const spi::Config& config) requires (M == TransferMode::Rx)
        : SpiBlocking<M>{spi, config}, DmaDuplex<TransferMode::Rx>{std::move(dma_rx), this->get_dreq(false)}
    {
    }

    explicit SpiDma(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, const spi::Config& config) requires (M == TransferMode::RxTx)
        : SpiBlocking<M>{spi, config},  DmaDuplex<TransferMode::RxTx>{std::move(dma_tx), std::move(dma_rx), this->get_dreq(true), this->get_dreq(false)}
    {
    }

    inline void write_dma(std::span<const u8> tx_buff, bool start = true) requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {   
        this->write_to_peripheral(this, tx_buff, start);
    }
    inline void read_dma(std::span<u8> rx_buff, bool start = true) requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        this->read_from_peripheral(this, rx_buff, start);
    }

    void transfer_dma(std::span<uint8_t> rx_buff, std::span<const u8> tx_buff, bool start = true) requires (M == TransferMode::RxTx)
    {
        write_dma(tx_buff, 
                  false);
        read_dma(rx_buff,
                 false);

        if(!start)
            return;

        dma_start_channel_mask( (1u << this->dma_tx().channel()) | (1u << this->dma_rx().channel())  );
    }
};



 // Register this implementation with the concept system
template <typename T, TransferMode M>
inline constexpr bool is_spi_implementation<T, IOMode::NonBlocking, M> = std::same_as<T, SpiDma<M>>; 
 


template<IOMode Io, TransferMode M>
class Spi;

template<TransferMode M>
class Spi<IOMode::Blocking, M>
{
public:
    static auto make(spi_inst_t* inst, u32 sck_pin, u32 mosi_pin, u32 miso_pin, const Config& config) -> SpiBlocking<TransferMode::RxTx> requires(M==TransferMode::RxTx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiBlocking<TransferMode::RxTx>{inst, config};
    }   
    
    static auto make(spi_inst_t* inst, u32 sck_pin, u32 mosi_pin, const Config& config) -> SpiBlocking<TransferMode::Tx>  requires(M==TransferMode::Tx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiBlocking<TransferMode::Tx>{inst, config};
    }   
 
    static auto make(spi_inst_t* inst, u32 sck_pin, u32 mosi_pin, const Config& config) -> SpiBlocking<TransferMode::Rx>  requires(M==TransferMode::Rx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiBlocking<TransferMode::Rx>{inst, config};
    }   
};

template<TransferMode M>
class Spi<IOMode::NonBlocking, M> 
{
public:
    
    static auto make(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, u32 miso_pin, Dma dma_tx, Dma dma_rx, const Config& config) -> SpiDma<TransferMode::RxTx> requires(M==TransferMode::RxTx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiDma<TransferMode::RxTx>{inst, std::move(dma_tx), std::move(dma_rx), config};
    }   
 
    static auto make(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, Dma dma_tx, const Config& config) -> SpiDma<TransferMode::Tx> requires(M==TransferMode::Tx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiDma<TransferMode::Tx>{inst, std::move(dma_tx), config};
    }   

 
    static auto make(spi_inst_t* inst, u32 sck_pin, u32 miso_pin, Dma dma_rx, const Config& config) -> SpiDma<TransferMode::Rx> requires(M==TransferMode::Rx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        return SpiDma<TransferMode::Rx>{inst, std::move(dma_rx), config};
    }   
};

} // hardware