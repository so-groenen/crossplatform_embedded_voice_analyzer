#ifndef UART_PICO_HPP
#define UART_PICO_HPP

#include "drivers/dma.hpp"
#include "interrupts/interface_irq_handler.hpp"
#include "embedded_hal/heapless.hpp"
#include "types.hpp"
#include <cstddef>
#include <hardware/gpio.h>
#include <hardware/regs/intctrl.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/uart.h>
#include <optional>
#include <span>


namespace hardware::uart
{
 




 
template<TransferMode M>
class UartBlocking
{
    uart_inst_t* m_uart{};
 
public:
    UartBlocking(uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 baudrate = 115200) requires (M == TransferMode::RxTx)
        : m_uart(inst)
    {
        uart_init(m_uart, baudrate);
        gpio_set_function(tx_pin, GPIO_FUNC_UART);
        gpio_set_function(rx_pin, GPIO_FUNC_UART);
        uart_set_format(m_uart, 8, 1, UART_PARITY_NONE);
    }
    UartBlocking(uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 baudrate = 115200) requires (M == TransferMode::Tx)
        : m_uart(inst)
    {
        uart_init(m_uart, baudrate);
        gpio_set_function(tx_pin, GPIO_FUNC_UART);
        uart_set_format(m_uart, 8, 1, UART_PARITY_NONE);
    }
    UartBlocking(uart_inst_t* inst = uart0, u32 rx_pin = 1, u32 baudrate = 115200) requires (M == TransferMode::Rx)
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
    io_rw_32* data_register() const
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
    void write_blocking(const uint8_t * src, size_t len) requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        uart_write_blocking(m_uart, src, len);
    }
    void write_blocking(std::span<const char> buff) requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<const u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<u8> buff) requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<u16> buff)requires (M == TransferMode::Tx || M == TransferMode::RxTx) 
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<u32> buff) requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<f32> buff) requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    template<typename T>
    void write_blocking(const T* buff) requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        uart_write_blocking(m_uart, reinterpret_cast<const u8*>(buff), sizeof(T));
    }


    void read_blocking(uint8_t * dest, size_t len) requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        uart_read_blocking(m_uart, dest, len);
    }
    void read_blocking(std::span<u8> rx_buff) requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        uart_read_blocking(m_uart, rx_buff.data(), rx_buff.size_bytes());
    }
    void read_blocking(std::span<u16> rx_buff) requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    void read_blocking(std::span<u32> rx_buff) requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    void read_blocking(std::span<f32> rx_buff) requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    template<typename T>
    void read_blocking(const T* buff) requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(buff), sizeof(T));
    }

    bool is_readeable() const  requires (M == TransferMode::Rx || M == TransferMode::RxTx)
    {
        return uart_is_readable(m_uart);
    }
    void enable_interrupt(bool rx_has_data, bool tx_needs_data)
    {
        uart_set_irq_enables(m_uart, rx_has_data, tx_needs_data);
    }
    bool is_writable() const  requires (M == TransferMode::Tx || M == TransferMode::RxTx)
    {
        return uart_is_writable(m_uart);
    }
    u32 get_dreq_num(bool is_tx)
    {
        return uart_get_dreq_num(m_uart, is_tx);
    }
    
    
    ~UartBlocking()
    {
        uart_deinit(m_uart);
    }
};


// template<TransferMode M>
// class UartNonBlocking : public UartBlocking<M>
// {
//     std::optional<Dma> m_dma_tx{std::nullopt};
//     std::optional<Dma> m_dma_rx{std::nullopt};
//     void set_rx()
//     {
//         if(m_dma_rx)
//         {
//             m_dma_tx->read_from_memory(false);
//             m_dma_tx->write_to_memory(true);
//             m_dma_tx->set_transfer_size<u8>();
//             m_dma_tx->set_data_request(this->get_dreq_num(false));
//         }
//     }
//     void set_tx()
//     {
//         if(m_dma_tx)
//         {
//             m_dma_tx->read_from_memory(true);
//             m_dma_tx->write_to_memory(false);
//             m_dma_tx->set_transfer_size<u8>();
//             m_dma_tx->set_data_request(this->get_dreq_num(true));
//         }
//     }
// public:
//     UartNonBlocking(Dma dma_tx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 bauderate = 115200) requires (M == TransferMode::Tx)
//         : m_dma_tx(std::move(dma_tx)), UartBlocking<TransferMode::Tx>(inst, tx_pin, bauderate)
//     {
//         set_tx();
//     }
//     UartNonBlocking(Dma dma_rx, uart_inst_t* inst = uart0, u32 rx_pin = 1,  u32 bauderate = 115200) requires (M == TransferMode::Rx)
//         : m_dma_rx(std::move(dma_rx)), UartBlocking<TransferMode::Rx>(inst, rx_pin, bauderate)
//     {
//         set_rx();
//     }
//     UartNonBlocking(Dma dma_tx, Dma dma_rx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 baudrate = 115200) requires (M == TransferMode::RxTx)
//         : m_dma_tx(std::move(dma_tx)), m_dma_rx(std::move(dma_rx)), UartBlocking<TransferMode::RxTx>(inst, tx_pin, rx_pin, baudrate)
//     {
//         set_tx();
//         set_rx();
//     }
//     Dma& dma_tx() requires (M == TransferMode::RxTx || M == TransferMode::Tx)
//     {
//         return m_dma_tx.value();
//     }
//     Dma& dma_rx() requires (M == TransferMode::RxTx || M == TransferMode::Rx)
//     {
//         return m_dma_rx.value();
//     }
//     inline void write_dma(const volatile void* read_addr, size_t transfer_count) requires (M == TransferMode::RxTx || M == TransferMode::Tx)
//     {
//         m_dma_tx->config_transfer(this->data_register(), read_addr, transfer_count, true);
//     }
//     inline void read_dma(volatile void* write_addr, size_t transfer_count) requires (M == TransferMode::RxTx || M == TransferMode::Rx)
//     {
//         m_dma_rx->config_transfer(write_addr, this->data_register(), transfer_count, true);
//     }
// };


template<TransferMode M>
class UartNonBlocking : public UartBlocking<M>, public DmaDuplex<M>
{
public:
    UartNonBlocking(Dma dma_tx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 bauderate = 115200) requires (M == TransferMode::Tx)
        : DmaDuplex<M>(std::move(dma_tx), this->get_dreq_num(true)), UartBlocking<TransferMode::Tx>(inst, tx_pin, bauderate)
    {
        // set_tx();
    }
    UartNonBlocking(Dma dma_rx, uart_inst_t* inst = uart0, u32 rx_pin = 1,  u32 bauderate = 115200) requires (M == TransferMode::Rx)
        : DmaDuplex<M>(std::move(dma_rx), this->get_dreq_num(false)), UartBlocking<TransferMode::Rx>(inst, rx_pin, bauderate)
    {
        // set_rx();
    }
    UartNonBlocking(Dma dma_tx, Dma dma_rx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 baudrate = 115200) requires (M == TransferMode::RxTx)
        : DmaDuplex<M>(std::move(dma_tx), std::move(dma_rx), this->get_dreq_num(true), this->get_dreq_num(false)), UartBlocking<TransferMode::RxTx>(inst, tx_pin, rx_pin, baudrate)
    {
        // set_tx();
        // set_rx();
    }
    inline void write_dma(std::span<u8> rx_buff) requires (M == TransferMode::RxTx || M == TransferMode::Tx)
    {
        this->write_to_peripheral(this, rx_buff);
    }
    inline void read_dma(std::span<u8> tx_buff) requires (M == TransferMode::RxTx || M == TransferMode::Rx)
    {
        this->read_from_peripheral(this, tx_buff);
    }

    inline void write_dma(std::span<const char> rx_buff) requires (M == TransferMode::RxTx || M == TransferMode::Tx)
    {
        this->write_to_peripheral(this, reinterpret_cast<const u8*>(rx_buff.data()), rx_buff.size());
    }
    inline void read_dma(std::span<char> tx_buff) requires (M == TransferMode::RxTx || M == TransferMode::Rx)
    {
        this->read_from_peripheral(this, reinterpret_cast<u8*>(tx_buff.data()), tx_buff.size());
    }

    inline void write_dma(const volatile void* read_addr, size_t transfer_count) requires (M == TransferMode::RxTx || M == TransferMode::Tx)
    {
        this->write_to_peripheral(this, read_addr, transfer_count);
        // m_dma_tx->config_transfer(this->data_register(), read_addr, transfer_count, true);
    }
    inline void read_dma(volatile void* write_addr, size_t transfer_count) requires (M == TransferMode::RxTx || M == TransferMode::Rx)
    {
        this->read_from_peripheral(this, write_addr, transfer_count);
        // m_dma_rx->config_transfer(write_addr, this->data_register(), transfer_count, true);
    }
};


template<IOMode Io, TransferMode M>
class Uart;


template<TransferMode M>
class Uart<IOMode::Blocking, M>
{
public:
    static auto make(uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 bauderate = 115200) -> UartBlocking<TransferMode::RxTx> 
        requires(M == TransferMode::RxTx)
    {
        return {inst, tx_pin, rx_pin, bauderate};
    }   
 

    static auto make(uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 bauderate = 115200) -> UartBlocking<TransferMode::Tx> 
            requires(M == TransferMode::Tx)
    {
        return {inst, tx_pin, bauderate};
    }   
  

    static auto make(uart_inst_t* inst = uart0, u32 rx_pin = 1, u32 bauderate = 115200) -> UartBlocking<TransferMode::Rx> 
            requires(M == TransferMode::Rx)
    {
        return {inst, rx_pin, bauderate};
    }   
  
};

 

template<TransferMode M>
class Uart<IOMode::NonBlocking, M>
{
public:
 
    static auto make(Dma dma_tx, Dma dma_rx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 rx_pin = 1, u32 bauderate = 115200) -> UartNonBlocking<TransferMode::RxTx> 
            requires(M == TransferMode::RxTx)
    {
        return {std::move(dma_tx), std::move(dma_rx), inst, tx_pin, rx_pin, bauderate};
    }  


    static auto make(Dma dma_tx, uart_inst_t* inst = uart0, u32 tx_pin = 0, u32 bauderate = 115200) -> UartNonBlocking<TransferMode::Tx> 
        requires(M == TransferMode::Tx)
    {
        return {std::move(dma_tx), inst, tx_pin, bauderate};
    }   


    static auto make(Dma dma_tx, uart_inst_t* inst = uart0, u32 rx_pin = 1, u32 bauderate = 115200) -> UartNonBlocking<TransferMode::Rx> 
        requires(M == TransferMode::Rx)
    {
        return {std::move(dma_tx), inst, rx_pin, bauderate};
    }   
};


}

#endif 