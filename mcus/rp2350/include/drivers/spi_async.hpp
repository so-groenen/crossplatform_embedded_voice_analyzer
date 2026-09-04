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

#include "drivers/interface_async_dma_handler.hpp"
#include "hardware/spi.h"
#include "drivers/pico_task.hpp"
#include "drivers/dma.hpp"
#include "types.hpp"
#include "drivers/spi.hpp"

namespace hardware::spi {
 
template<SpiMode M>
class SpiRtos : public SpiDma<M>
{
    PicoTask& m_task;
public:
    SpiRtos(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, PicoTask* task, ::irq::IAsyncDmaHandler& irq, const spi::Config& config) requires (M == SpiMode::RxTx)
        : SpiDma<SpiMode::RxTx>(spi, std::move(dma_tx), std::move(dma_rx), config), m_task{*task}
    {
        irq.bind_interrupt_from_rtos(&this->dma_tx(), task);
        irq.bind_interrupt_from_rtos(&this->dma_rx(), task);
    }

    SpiRtos(spi_inst_t* spi, Dma dma_tx, PicoTask* task, ::irq::IAsyncDmaHandler& irq, const spi::Config& config)  requires (M == SpiMode::Tx)
        : SpiDma<SpiMode::Tx>(spi, std::move(dma_tx), config), m_task{*task}
    {
        irq.bind_interrupt_from_rtos(&this->dma_tx(), task);
    }

    SpiRtos(spi_inst_t* spi, Dma dma_rx, PicoTask* task, ::irq::IAsyncDmaHandler& irq, const spi::Config& config)  requires (M == SpiMode::Rx)
        : SpiDma<SpiMode::Rx>(spi, std::move(dma_rx), config), m_task{*task}
    {
        irq.bind_interrupt_from_rtos(&this->dma_rx(), task);
    }

    void write_await(std::span<const u8> tx_buff)  requires (M == SpiMode::Tx || M == SpiMode::RxTx)
    {   
        this->m_dma_tx->enable_channel(true);                  //need to reset it since transfer can disable it, to avoid double firing!
        this->write_dma(tx_buff);
        m_task.awaitNotifs();
        this->m_dma_tx->enable_channel(false);                  //need to reset it since transfer can disable it, to avoid double firing!
    }
    void read_await(std::span<u8> rx_buff)  requires (M == SpiMode::Rx || M == SpiMode::RxTx)
    {
        this->m_dma_rx->enable_channel(true);                  //need to reset it since transfer can disable it, to avoid double firing!
        this->read_dma(rx_buff);
        m_task.awaitNotifs();
        this->m_dma_rx->enable_channel(false);                  //need to reset it since transfer can disable it, to avoid double firing!
    }
    void transfer_await(std::span<u8> rx_buff, std::span<const u8> tx_buff)  requires (M == SpiMode::RxTx)
    {
        this->m_dma_rx->enable_channel(true);          // Only one interrupt fires!
                                                        // reeset it back in write_awaitZZ
        this->transfer_dma(rx_buff, tx_buff, true);
        m_task.awaitNotifs();                          // wait until dma_rx's irq has fied
        this->m_dma_rx->enable_channel(false);          // Pray that dma_tx has finished and will not fire ..
    }
};


 

template<>
class Spi<IOMode::Rtos> 
{
public:
 
    static auto make(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, u32 miso_pin, Dma dma_tx, Dma dma_rx, PicoTask* task, ::irq::IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<SpiMode::RxTx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiRtos<SpiMode::RxTx>{inst, std::move(dma_tx), std::move(dma_rx), task, irq, config};
    }   

 
    static auto make_tx_only(spi_inst_t* inst, u32 sck_pin,  u32 mosi_pin, Dma dma_tx, PicoTask* task, ::irq::IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<SpiMode::Tx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
        return SpiRtos<SpiMode::Tx>{inst, std::move(dma_tx), task, irq, config};
    }   
 
    static auto make_rx_only(spi_inst_t* inst, u32 sck_pin, u32 miso_pin, Dma dma_rx, PicoTask* task, ::irq::IAsyncDmaHandler& irq, const Config& config) -> SpiRtos<SpiMode::Rx> 
    {
        gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
        gpio_set_function(miso_pin, GPIO_FUNC_SPI);
        return SpiRtos<SpiMode::Rx>{inst, std::move(dma_rx), task, irq, config};
    }   
};
 

static_assert(embedded_hal::blocking::IsBlockingReadWrite<SpiBlocking<SpiMode::RxTx>> && "SpiAsyncFull must implement read & write blocking");
static_assert(embedded_hal::async::IsAsyncReadWrite<SpiRtos<SpiMode::RxTx>> && "SpiAsyncFull must implement read & write async");
static_assert(embedded_hal::async::IsAsyncRead<SpiRtos<SpiMode::Rx>>  && "SpiAsync must implement read");
static_assert(embedded_hal::async::IsAsyncWrite<SpiRtos<SpiMode::Tx>>  && "SpiAsync must implement read");
  

} // hardware