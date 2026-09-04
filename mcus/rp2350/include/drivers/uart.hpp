#ifndef UART_PICO_HPP
#define UART_PICO_HPP

#include "drivers/dma.hpp"
#include "drivers/interface_irq_handler.hpp"
#include "embedded_hal/heapless.hpp"
#include "types.hpp"
#include <cstddef>
#include <hardware/gpio.h>
#include <hardware/regs/intctrl.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/uart.h>
#include <optional>
#include <span>


namespace hardware 
{
 




enum class UartMode : u8
{
    Rx, Tx, RxTx,
};


template<UartMode M>
class UartImplBase
{
    uart_inst_t* m_uart{};
 
public:
    UartImplBase(uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 baudrate = 115200) requires (M == UartMode::RxTx)
        : m_uart(inst)
    {
        uart_init(m_uart, baudrate);
        gpio_set_function(tx_pin, GPIO_FUNC_UART);
        gpio_set_function(rx_pin, GPIO_FUNC_UART);
        uart_set_format(m_uart, 8, 1, UART_PARITY_NONE);
    }
    UartImplBase(uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 baudrate = 115200) requires (M == UartMode::Tx)
        : m_uart(inst)
    {
        uart_init(m_uart, baudrate);
        gpio_set_function(tx_pin, GPIO_FUNC_UART);
        uart_set_format(m_uart, 8, 1, UART_PARITY_NONE);
    }
    UartImplBase(uart_inst_t* inst = uart0, u32 rx_pin = 1, u32 baudrate = 115200) requires (M == UartMode::Rx)
        : m_uart(inst)
    {
        uart_init(m_uart, baudrate);
        gpio_set_function(rx_pin, GPIO_FUNC_UART);
        uart_set_format(m_uart, 8, 1, UART_PARITY_NONE);
    }

    void set_baudrate(u32 baudrate)
    {
        uart_set_baudrate(m_uart, baudrate);
    }
    uart_inst_t* get_instance() const 
    {
        return m_uart;
    }
    io_rw_32* data_register()
    {
        return &uart_get_hw(m_uart)->dr;
    }
    void set_format(uint data_bits, uint stop_bits, uart_parity_t parity)
    {
        uart_set_format(m_uart, data_bits, stop_bits, parity);
    }
    void enable_fifo(bool v)
    {
        uart_set_fifo_enabled(m_uart, v);
    }
    // template<std::integral T>
    void write_blocking(const uint8_t * src, size_t len) requires (M == UartMode::Tx || M == UartMode::RxTx)
    {
        uart_write_blocking(m_uart, src, len);
    }
    void write_blocking(std::span<u8> buff) requires (M == UartMode::Tx || M == UartMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<u16> buff)requires (M == UartMode::Tx || M == UartMode::RxTx) 
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<u32> buff) requires (M == UartMode::Tx || M == UartMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<f32> buff) requires (M == UartMode::Tx || M == UartMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    template<typename T>
    void write_blocking(const T* buff) requires (M == UartMode::Tx || M == UartMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<const u8*>(buff), sizeof(T));
    }


    void read_blocking(uint8_t * dest, size_t len) requires (M == UartMode::Rx || M == UartMode::RxTx)
    {
        uart_read_blocking(m_uart, dest, len);
    }
    void read_blocking(std::span<u8> rx_buff) requires (M == UartMode::Rx || M == UartMode::RxTx)
    {
        uart_read_blocking(m_uart, rx_buff.data(), rx_buff.size_bytes());
    }
    void read_blocking(std::span<u16> rx_buff) requires (M == UartMode::Rx || M == UartMode::RxTx)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    void read_blocking(std::span<u32> rx_buff) requires (M == UartMode::Rx || M == UartMode::RxTx)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    void read_blocking(std::span<f32> rx_buff) requires (M == UartMode::Rx || M == UartMode::RxTx)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    template<typename T>
    void read_blocking(const T* buff) requires (M == UartMode::Rx || M == UartMode::RxTx)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(buff), sizeof(T));
    }

    bool is_readeable() const  requires (M == UartMode::Rx || M == UartMode::RxTx)
    {
        return uart_is_readable(m_uart);
    }
    void enable_interrupt(bool rx_has_data, bool tx_needs_data)
    {
        uart_set_irq_enables(m_uart, rx_has_data, tx_needs_data);
    }
    bool is_writable() const  requires (M == UartMode::Tx || M == UartMode::RxTx)
    {
        return uart_is_writable(m_uart);
    }
    u32 get_dreq_num(bool is_tx)
    {
        return uart_get_dreq_num(m_uart, is_tx);
    }
    
    
    ~UartImplBase()
    {
        uart_deinit(m_uart);
    }
};


template<UartMode M>
class UartImplDma : public UartImplBase<M>
{
    std::optional<Dma> m_dma_tx{std::nullopt};
    std::optional<Dma> m_dma_rx{std::nullopt};
    void set_rx()
    {
        if(m_dma_rx)
        {
            m_dma_tx->read_from_memory(false);
            m_dma_tx->write_to_memory(true);
            m_dma_tx->set_transfer_size<u8>();
            m_dma_tx->set_data_request(this->get_dreq_num(false));
        }
    }
    void set_tx()
    {
        if(m_dma_tx)
        {
            m_dma_tx->read_from_memory(true);
            m_dma_tx->write_to_memory(false);
            m_dma_tx->set_transfer_size<u8>();
            m_dma_tx->set_data_request(this->get_dreq_num(true));
        }
    }
public:
    UartImplDma(Dma dma_tx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 bauderate = 115200) requires (M == UartMode::Tx)
        : m_dma_tx(std::move(dma_tx)), UartImplBase<UartMode::Tx>(inst, tx_pin, bauderate)
    {
        set_tx();
    }
    UartImplDma(Dma dma_rx, uart_inst_t* inst = uart0, u32 rx_pin = 1,  u32 bauderate = 115200) requires (M == UartMode::Rx)
        : m_dma_rx(std::move(dma_rx)), UartImplBase<UartMode::Rx>(inst, rx_pin, bauderate)
    {
        set_rx();
    }
    UartImplDma(Dma dma_tx, Dma dma_rx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 baudrate = 115200) requires (M == UartMode::RxTx)
        : m_dma_tx(std::move(dma_tx)), m_dma_rx(std::move(dma_rx)), UartImplBase<UartMode::RxTx>(inst, tx_pin, rx_pin, baudrate)
    {
        set_tx();
        set_rx();
    }
    Dma& dma_tx() requires (M == UartMode::RxTx || M == UartMode::Tx)
    {
        return m_dma_tx.value();
    }
    Dma& dma_rx() requires (M == UartMode::RxTx || M == UartMode::Rx)
    {
        return m_dma_rx.value();
    }
    inline void send_dma(const volatile void* read_addr, size_t transfer_count) requires (M == UartMode::RxTx || M == UartMode::Tx)
    {
        m_dma_tx->config_transfer(this->data_register(), read_addr, transfer_count, true);
    }
    inline void read_dma(volatile void* write_addr, size_t transfer_count) requires (M == UartMode::RxTx || M == UartMode::Rx)
    {
        m_dma_rx->config_transfer(write_addr, this->data_register(), transfer_count, true);
    }
};


class Uart 
{
public:
    static auto make(uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 bauderate = 115200) -> UartImplBase<UartMode::RxTx> 
    {
        return {inst, tx_pin, rx_pin, bauderate};
    }   
    static auto make(Dma dma_tx, Dma dma_rx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 bauderate = 115200) -> UartImplDma<UartMode::RxTx> 
    {
        return {std::move(dma_tx), std::move(dma_rx), inst, tx_pin, rx_pin, bauderate};
    }  

    static auto make_tx_only(uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 bauderate = 115200) -> UartImplBase<UartMode::Tx> 
    {
        return {inst, tx_pin, bauderate};
    }   
    static auto make_tx_only(Dma dma_tx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 bauderate = 115200) -> UartImplDma<UartMode::Tx> 
    {
        return {std::move(dma_tx), inst, tx_pin, bauderate};
    }   

    static auto make_rx_only(uart_inst_t* inst = uart0, u32 rx_pin = 1, u32 bauderate = 115200) -> UartImplBase<UartMode::Rx> 
    {
        return {inst, rx_pin, bauderate};
    }   
    static auto make_rx_only(Dma dma_tx, uart_inst_t* inst = uart0, u32 rx_pin = 1, u32 bauderate = 115200) -> UartImplDma<UartMode::Rx> 
    {
        return {std::move(dma_tx), inst, rx_pin, bauderate};
    }   
};

 

}

#endif 