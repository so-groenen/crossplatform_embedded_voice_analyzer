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
#include "drivers/spi.hpp"
#include "embedded_hal/traits.hpp"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"

#include "drivers/dma.hpp"
#include "types.hpp"


namespace spi {

using namespace embedded_hal::blocking;
using embedded_hal::blocking::SpiError;
 


// Dma class wrapper for SPI: it features a convenience function to write to / from spi registers


template<TransferMode M>
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
    void write_to_spi(spi_inst_t* spi, const std::span<uint8_t> tx_buff, bool start) requires (M == TransferMode::Tx)
    {   
        write(tx_buff, 
            &spi_get_hw(spi)->dr,
            spi_get_dreq(spi, true),
            start);
    }
    void read_from_spi(spi_inst_t* spi, std::span<uint8_t> rx_buff, bool start) requires (M == TransferMode::Rx)
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
    template<TransferMode M>
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
        void write_async(std::span<uint8_t> tx_buff, bool start) requires (M == TransferMode::Tx)
        {   
            m_dma.write_to_spi(m_spi,
                                tx_buff, 
                                start);
        }
        void read_async(std::span<uint8_t> rx_buff, bool start) requires (M == TransferMode::Rx)
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
        DmaForSpi<TransferMode::Tx> m_dma_tx;
        DmaForSpi<TransferMode::Rx> m_dma_rx;
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
    template<TransferMode M>
    class SpiAsync : public Spi
    {
        PicoTask& m_task;
        DmaForSpi<M> m_dma{};
    public:
        SpiAsync(spi_inst_t* spi, Dma dma, PicoTask* task, ::irq::IAsyncDmaHandler& irq)
            : Spi(spi), m_dma{std::move(dma)}, m_task{*task}//, m_buffer(buffer)
        {
            irq.bind_interrupt_from_rtos(&m_dma, task);
        }
        void write_await(const std::span<uint8_t> tx_buff) requires (M == TransferMode::Tx)
        {   
            m_dma.write_to_spi(m_spi, tx_buff, true);
            m_task.awaitNotifs();
        }
        void read_await(std::span<uint8_t> rx_buff)  requires (M == TransferMode::Rx)
        {
            m_dma.read_from_spi(m_spi, rx_buff, true);
            m_task.awaitNotifs();
        }
    };


    class SpiAsyncFull : public Spi
    {
        PicoTask& m_task;
        DmaForSpi<TransferMode::Tx> m_dma_tx;
        DmaForSpi<TransferMode::Rx> m_dma_rx;
    public:
        SpiAsyncFull(spi_inst_t* spi, Dma dma_tx, Dma dma_rx, PicoTask* task, ::irq::IAsyncDmaHandler& irq)
            : Spi{spi}, m_dma_tx{std::move(dma_tx)}, m_dma_rx{std::move(dma_rx)}, m_task{*task}
        {
            irq.bind_interrupt_from_rtos(&m_dma_tx, task);
            irq.bind_interrupt_from_rtos(&m_dma_rx, task);
        }
        void write_await(const std::span<uint8_t> tx_buff) 
        {   
            m_dma_tx.enable_channel(true);                  //need to reset it since transfer can disable it, to avoid double firing!
            m_dma_tx.write_to_spi(m_spi, tx_buff, true);
            m_task.awaitNotifs();
        }
        void read_await(std::span<uint8_t> rx_buff) 
        {
            m_dma_rx.read_from_spi(m_spi, rx_buff, true);
            m_task.awaitNotifs();
        }
        void transfer_await(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff)
        {
            m_dma_tx.enable_channel(false);                // Disable so that only one interrupt fire!
                                                           // reeset it back in write_awaitZZ
            m_dma_tx.write_to_spi(m_spi, tx_buff, false);
            m_dma_rx.read_from_spi(m_spi, rx_buff, false);

            const uint32_t mask =  (1u << m_dma_rx.channel()) | (1u << m_dma_tx.channel());
            
            dma_start_channel_mask(mask);
            m_task.awaitNotifs();
            // m_task.awaitNotifs();
        }
    };


    static_assert(embedded_hal::blocking::IsBlockingReadWrite<SpiAsyncFull> && "SpiAsyncFull must implement read & write blocking");
    static_assert(embedded_hal::async::IsAsyncReadWrite<SpiAsyncFull> && "SpiAsyncFull must implement read & write async");
    static_assert(embedded_hal::async::IsAsyncRead<SpiAsync<TransferMode::Rx>> && "SpiAsync must implement read");
    static_assert(embedded_hal::async::IsAsyncWrite<SpiAsync<TransferMode::Tx>> && "SpiAsync must implement read");
}



} // spi