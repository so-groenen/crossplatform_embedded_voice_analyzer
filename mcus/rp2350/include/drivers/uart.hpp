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
#include <span>


namespace hardware 
{


class Uart 
{
    uart_inst_t* m_uart{};
public:
    Uart(uart_inst_t* inst = uart0, u32 baudrate = 115200, u32 tx_pin = 0, u32 rx_pin = 1)
        : m_uart(inst)
    {
        uart_init(m_uart, baudrate);
        gpio_set_function(tx_pin, GPIO_FUNC_UART);
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
    void write_blocking(const uint8_t * src, size_t len)
    {
        uart_write_blocking(m_uart, src, len);
    }
    void write_blocking(std::span<u8> buff)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<u16> buff)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<u32> buff)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    void write_blocking(std::span<f32> buff)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    template<typename T>
    void write_blocking(const T* buff)
    {
        uart_write_blocking(m_uart, reinterpret_cast<const u8*>(buff), sizeof(T));
    }


    void read_blocking(uint8_t * dest, size_t len)
    {
        uart_read_blocking(m_uart, dest, len);
    }
    void read_blocking(std::span<u8> rx_buff)
    {
        uart_read_blocking(m_uart, rx_buff.data(), rx_buff.size_bytes());
    }
    void read_blocking(std::span<u16> rx_buff)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    void read_blocking(std::span<u32> rx_buff)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    void read_blocking(std::span<f32> rx_buff)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(rx_buff.data()), rx_buff.size_bytes());
    }
    template<typename T>
    void read_blocking(const T* buff)
    {
        uart_read_blocking(m_uart, reinterpret_cast<u8*>(buff), sizeof(T));
    }

    bool is_readeable() const 
    {
        return uart_is_readable(m_uart);
    }
    void enable_interrupt(bool rx_has_data, bool tx_needs_data)
    {
        uart_set_irq_enables(m_uart, rx_has_data, tx_needs_data);
    }
    bool is_writable() const 
    {
        return uart_is_writable(m_uart);
    }
    u32 get_dreq_num(bool is_tx)
    {
        return uart_get_dreq_num(m_uart, is_tx);
    }
    void init_dma(Dma& dma, bool is_tx)
    {
        if(is_tx)
        {
            dma.read_from_memory(true);
            dma.write_to_memory(false);
            dma.set_transfer_size<u8>();
            dma.set_data_request(get_dreq_num(is_tx));
        }
        else  
        {
            dma.read_from_memory(false);
            dma.write_to_memory(true);
            dma.set_transfer_size<u8>();
            dma.set_data_request(get_dreq_num(is_tx));
        }
    }
    inline void send_dma(Dma& dma, const volatile void* read_addr, size_t transfer_count)
    {
        dma.config_transfer(data_register(), read_addr, transfer_count, true);
    }
    inline void read_dma(Dma& dma, volatile void* write_addr, size_t transfer_count)
    {
        dma.config_transfer(write_addr, data_register(), transfer_count, true);
    }
    ~Uart()
    {
        uart_deinit(m_uart);
    }
};

template<typename T, size_t N>
class UartIrqWriter  
{
    irq_handler_t m_handler{};
    embedded_hal::heapless::RingBuffer<T, N> m_ring_buffer{};
    Uart* m_uart{};
public:
    UartIrqWriter(irq_handler_t handler)
        : m_handler(handler)
    {
    }
    void handle_isr()
    {
        while (m_uart->is_writable() && !m_ring_buffer.empty()) 
        {
            T data = m_ring_buffer.pop();
            m_uart->write_blocking(&data);
        }
    }
    inline void write(char c)
    {
        m_ring_buffer.push(c);
    }
    void bind_interrupt(Uart* uart)
    {
        m_uart = uart;
        enum irq_num_rp2350 irq = (m_uart->get_instance() == uart0)? UART0_IRQ : UART1_IRQ;
        irq_set_exclusive_handler(irq, m_handler);
        irq_set_enabled(irq, true);
        // m_ready_flags.push_back(false);
    }
};


}

#endif 