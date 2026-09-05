#ifndef I2S_PICO_HPP
#define I2S_PICO_HPP


#include "drivers/dma.hpp"
#include "drivers/interface_async_dma_handler.hpp"
#include "drivers/pio.hpp"
#include "types.hpp"


template<pio::IsPioProgram P>
requires pio::IsPioReceiver<P>
class SPH0645Mic
{
    Dma m_dma;
    P& m_i2s;
    size_t m_processed_buff{};
    std::span<u32> m_ping_pong[2];
    bool& m_ready_flag;
public:
    SPH0645Mic(P& pio_i2s,
            std::span<u32> ping, 
            std::span<u32> pong,
            Dma dma,
            irq::IDmaHandler& irq)
        : m_i2s(pio_i2s), m_dma{std::move(dma)}, m_ready_flag{irq.bind_interrupt(&m_dma)}
    {
        assert(ping.size() == pong.size());
        m_ping_pong[0] = ping;
        m_ping_pong[1] = pong;

        // if constexpr (M == TransferMode::Rx)
        // {
            setup_rx_mode();
        // }
        // if constexpr (M == TransferMode::Tx)
        // {
            // setup_tx_mode();
        // }
        // irq.bind_interrupt(&m_dma);
        m_dma.enable_channel(true);
    }
 
    void setup_rx_mode() //requires (M == TransferMode::Rx)
    {
        m_dma.read_from_memory(false);
        m_dma.write_to_memory(true);
        m_dma.set_transfer_size<u32>();
        m_dma.set_data_request(m_i2s.get_dreq(false));
        m_dma.config_transfer(
            /*dest:*/ m_ping_pong[0].data(),
            /*src:*/  m_i2s.get_rx_fifo_reg(),
            /*len:*/  m_ping_pong[0].size());
    }
    // void setup_tx_mode() requires (M == TransferMode::Tx)
    // {
    //     m_dma.read_from_memory(true);
    //     m_dma.write_to_memory(false);
    //     m_dma.set_transfer_size<T>();
    //     m_dma.set_data_request(m_i2s.get_dreq(true));
    //     m_dma.config_transfer(
    //         /*dest:*/ m_i2s.get_tx_fifo_reg(),
    //         /*src:*/  m_ping_pong[0].data(), 
    //                   m_ping_pong[0].size());
    // }
    void start()
    {
        m_i2s.start();
        m_dma.start();
    }
    void stop()
    {
        m_i2s.stop();
        m_dma.abort();
    }
    // inline void read_blocking(std::span<T>& rx_buffer, bool& is_ready) requires (M == TransferMode::Rx)
    // {
    //     while(!is_ready)
    //     {
    //         tight_loop_contents();
    //     }
    //     m_processed_buff ^= 1; 
    //     m_dma.set_write_addr(m_ping_pong[m_processed_buff].data(), true);
    //     is_ready  = false;
    //     rx_buffer = m_ping_pong[m_processed_buff ^ 1];
    // }
    inline void read_blocking(std::span<u32>& rx_buffer)// requires (M == TransferMode::Rx)
    {
        while(!m_ready_flag)
        {
            tight_loop_contents();
        }
        m_ready_flag = false;

        m_processed_buff ^= 1; 
        m_dma.set_write_addr(m_ping_pong[m_processed_buff].data(), true);
        rx_buffer = m_ping_pong[m_processed_buff ^ 1];
    }
    inline bool try_read(std::span<u32>& rx_buffer)// requires (M == TransferMode::Rx)
    {
        if(!m_ready_flag)
            return false;

        m_processed_buff ^= 1; 
        m_dma.set_write_addr(m_ping_pong[m_processed_buff].data(), true);
        m_ready_flag  = false;
        rx_buffer = m_ping_pong[m_processed_buff ^ 1];
        return true;
    }

    // inline auto write_blocking(bool& is_ready) requires (M == TransferMode::Tx)
    // {
    //     while(!is_ready)
    //     {
    //         tight_loop_contents();
    //     }
    //     m_processed_buff ^= 1; 
    //     m_dma.set_read_addr(m_ping_pong[m_processed_buff].data(), true);
    //     is_ready = false;
    //     return m_ping_pong[m_processed_buff ^ 1];
    // }
};


#endif