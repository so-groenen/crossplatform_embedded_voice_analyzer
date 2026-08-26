#ifndef I2S_PICO_HPP
#define I2S_PICO_HPP


#include "drivers/dma.hpp"
#include "drivers/interface_async_dma_handler.hpp"
#include "drivers/pio.hpp"


template<TransferMode M, std::integral T, pio::IsPioProgram P>
requires pio::PioValidateMethod<P,M>
class I2SHalfDuplex
{
    Dma m_dma{};
    P& m_pio{};
    size_t m_processed_buff{};
    std::span<T> m_ping_pong[2];
public:
    I2SHalfDuplex(P& pio_,
            std::span<T> ping, 
            std::span<T> pong,
            irq::IDmaHandler& irq)
        : m_pio(pio_)
    {
        assert(ping.size() == pong.size());
        m_ping_pong[0] = ping;
        m_ping_pong[1] = pong;

        if constexpr (M == TransferMode::Rx)
        {
            setup_rx_mode();
        }
        if constexpr (M == TransferMode::Tx)
        {
            setup_tx_mode();
        }

        irq.bind_interrupt(&m_dma);
    }
    void setup_rx_mode() requires (M == TransferMode::Rx)
    {
        m_dma.read_from_memory(false);
        m_dma.write_to_memory(true);
        m_dma.set_data_request(m_pio.get_dreq(false));
        m_dma.config_transfer(
            /*dest:*/ m_ping_pong[0],
            /*src:*/  m_pio.get_rx_fifo_reg());
    }
    void setup_tx_mode() requires (M == TransferMode::Tx)
    {
        m_dma.read_from_memory(true);
        m_dma.write_to_memory(false);
        m_dma.set_data_request(m_pio.get_dreq(true));
        m_dma.config_transfer(
            /*dest:*/ m_ping_pong[0],
            /*src:*/  m_pio.get_tx_fifo_reg());
    }
    void start()
    {
        m_pio.start();
        m_dma.start();
    }
    void stop()
    {
        m_pio.stop();
        m_dma.abort();
    }
    void read_blocking(std::span<T> rx_buffer, bool& is_ready)
    {
        while(!is_ready)
        {
            tight_loop_contents();
        }
        m_processed_buff ^= 1; 
        m_dma.set_write_addr(m_ping_pong[m_processed_buff].data(), true);
        is_ready = false;
        rx_buffer = m_ping_pong[m_processed_buff ^ 1];
    }
    inline auto write_blocking(bool& is_ready)
    {
        while(!is_ready)
        {
            tight_loop_contents();
        }
        m_processed_buff ^= 1; 
        m_dma.set_read_addr(m_ping_pong[m_processed_buff].data(), true);
        is_ready = false;
        return m_ping_pong[m_processed_buff ^ 1];
    }
};
 

#endif