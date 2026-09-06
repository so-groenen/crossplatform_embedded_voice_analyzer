#pragma once
 

#include <cassert>
 
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
#include "hardware/spi.h"
#include "drivers/pico_task.hpp"
#include "drivers/dma.hpp"
#include "types.hpp"
#include "drivers/spi.hpp"

namespace hardware::spi 
{

    using namespace interrupts;

template<TransferMode M>
class SpiRtos : public SpiDma<M>
{
    PicoTask& m_task;
public:
    SpiRtos(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, PicoTask* task, IAsyncDmaHandler& irq, const spi::Config& config) requires (M == TransferMode::RxTx)
        : SpiDma<TransferMode::RxTx>(spi, std::move(dma_tx), std::move(dma_rx), config), m_task{*task}
    {
        irq.bind_interrupt_from_rtos(&this->dma_tx(), task);
        irq.bind_interrupt_from_rtos(&this->dma_rx(), task);
    }

    SpiRtos(spi_inst_t* spi, Dma dma_tx, PicoTask* task, IAsyncDmaHandler& irq, const spi::Config& config)  requires (M == TransferMode::Tx)
        : SpiDma<TransferMode::Tx>(spi, std::move(dma_tx), config), m_task{*task}
    {
        irq.bind_interrupt_from_rtos(&this->dma_tx(), task);
    }

    SpiRtos(spi_inst_t* spi, Dma dma_rx, PicoTask* task, IAsyncDmaHandler& irq, const spi::Config& config)  requires (M == TransferMode::Rx)
        : SpiDma<TransferMode::Rx>(spi, std::move(dma_rx), config), m_task{*task}
    {
        irq.bind_interrupt_from_rtos(&this->dma_rx(), task);
    }

    void write_await(std::span<const u8> tx_buff)  requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {   
        this->m_dma_tx->enable_channel(true);                  //need to reset it since transfer can disable it, to avoid double firing!
        this->write_dma(tx_buff);
        m_task.awaitNotifs();
        this->m_dma_tx->enable_channel(false);                  //need to reset it since transfer can disable it, to avoid double firing!
    }
    void read_await(std::span<u8> rx_buff)  requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        this->m_dma_rx->enable_channel(true);                  //need to reset it since transfer can disable it, to avoid double firing!
        this->read_dma(rx_buff);
        m_task.awaitNotifs();
        this->m_dma_rx->enable_channel(false);                  //need to reset it since transfer can disable it, to avoid double firing!
    }
    void transfer_await(std::span<u8> rx_buff, std::span<const u8> tx_buff)  requires (M == TransferMode::RxTx)
    {
        this->m_dma_rx->enable_channel(true);          // Only one interrupt fires!
                                                        // reeset it back in write_awaitZZ
        this->transfer_dma(rx_buff, tx_buff, true);
        m_task.awaitNotifs();                          // wait until dma_rx's irq has fied
        this->m_dma_rx->enable_channel(false);          // Pray that dma_tx has finished and will not fire ..
    }
};


template <typename T, TransferMode M>
inline constexpr bool is_spi_implementation<T, IOMode::Rtos, M> = std::same_as<T, SpiRtos<M>>; 


// template<>
// class Spi<IOMode::Rtos> 
// {
// public:
 
//     static auto make(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, u32 miso_pin, Dma dma_tx, Dma dma_rx, PicoTask* task, IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<TransferMode::RxTx> 
//     {
//         gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
//         gpio_set_function(miso_pin, GPIO_FUNC_SPI);
//         gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
//         return SpiRtos<TransferMode::RxTx>{inst, std::move(dma_tx), std::move(dma_rx), task, irq, config};
//     }   

 
//     static auto make_tx_only(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, Dma dma_tx, PicoTask* task, IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<TransferMode::Tx> 
//     {
//         gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
//         gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
//         return SpiRtos<TransferMode::Tx>{inst, std::move(dma_tx), task, irq, config};
//     }   
 
//     static auto make_rx_only(spi_inst_t* inst, u32 sck_pin, u32 miso_pin, Dma dma_rx, PicoTask* task, IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<TransferMode::Rx> 
//     {
//         gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
//         gpio_set_function(miso_pin, GPIO_FUNC_SPI);
//         return SpiRtos<TransferMode::Rx>{inst, std::move(dma_rx), task, irq, config};
//     }   
// };

template<TransferMode M>
class Spi<IOMode::Rtos, M> 
{
public:
 
    static auto make(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, u32 miso_pin, Dma dma_tx, Dma dma_rx, PicoTask* task, IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<TransferMode::RxTx> 
        requires(M == TransferMode::RxTx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiRtos<TransferMode::RxTx>{inst, std::move(dma_tx), std::move(dma_rx), task, irq, config};
    }   

 
    static auto make_tx_only(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, Dma dma_tx, PicoTask* task, IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<TransferMode::Tx> 
            requires(M == TransferMode::Tx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiRtos<TransferMode::Tx>{inst, std::move(dma_tx), task, irq, config};
    }   
 
    static auto make_rx_only(spi_inst_t* inst, u32 sck_pin, u32 miso_pin, Dma dma_rx, PicoTask* task, IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<TransferMode::Rx> 
            requires(M == TransferMode::Rx)
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        return SpiRtos<TransferMode::Rx>{inst, std::move(dma_rx), task, irq, config};
    }   
};


static_assert(embedded_hal::blocking::IsBlockingReadWrite<SpiBlocking<TransferMode::RxTx>> && "SpiAsyncFull must implement read & write blocking");
static_assert(embedded_hal::async::IsAsyncReadWrite<SpiRtos<TransferMode::RxTx>> && "SpiAsyncFull must implement read & write async");
static_assert(embedded_hal::async::IsAsyncRead<SpiRtos<TransferMode::Rx>>  && "SpiAsync must implement read");
static_assert(embedded_hal::async::IsAsyncWrite<SpiRtos<TransferMode::Tx>>  && "SpiAsync must implement read");
  

} // hardware