#ifndef I2S_PICO_HPP
#define I2S_PICO_HPP


#include "drivers/async_dma_handler.hpp"
#include "drivers/dma.hpp"
#include "drivers/interface_async_dma_handler.hpp"
#include "drivers/pio.hpp"
#include "types.hpp"
#include <concepts>
#include "voice_analyzer.hpp"

// template<typename T>
// concept IsI2sMic = requires(T t, std::span<u32>& buff)
// {
//     {t.start()}             -> std::same_as<void>;
//     {t.stop()}              -> std::same_as<void>;
//     {t.read_blocking(buff)} -> std::same_as<void>;
//     {t.get_sample_rate()}   -> std::same_as<u32>;
// };


template<typename T>
concept IsI2s = requires(T t)
{
    {t.get_sample_rate()} -> std::same_as<u32>;
};



template<pio::IsPioProgram P>
requires pio::IsPioReceiver<P> && IsI2s<P>
class SPH0645Mic
{
    Dma m_dma;
    P m_i2s;
    // P& m_i2s;
    size_t m_current_buff{};
    std::span<u32> m_ping_pong[2];
    bool& m_ready_flag;

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
public:
    // SPH0645Mic(P& pio_i2s,
    //         std::span<u32> ping, 
    //         std::span<u32> pong,
    //         Dma dma,
    //         irq::IDmaHandler& irq)
    //     : m_i2s(pio_i2s), m_dma{std::move(dma)}, m_ready_flag{irq.bind_interrupt(&m_dma)}
    // {
    //     assert(ping.size() == pong.size());
    //     m_ping_pong[0] = ping;
    //     m_ping_pong[1] = pong;

    //     setup_rx_mode();
    //     m_dma.enable_channel(true);
    // }

    template<irq_num_rp2350 IRQ_n, size_t N_Channels>
    SPH0645Mic(P pio_i2s,
            std::span<u32> ping, 
            std::span<u32> pong,
            Dma dma,
            DmaHandlerWithCallback<IRQ_n, N_Channels, SPH0645Mic<P>>& irq)
        : m_i2s(std::move(pio_i2s)), m_dma{std::move(dma)}, m_ready_flag{irq.bind_driver(this)}
    {
        assert(ping.size() == pong.size());
        m_ping_pong[0] = ping;
        m_ping_pong[1] = pong;

        setup_rx_mode();
        m_dma.enable_channel(true);
    }

    u32 get_sample_rate() const
    {
        return m_i2s.get_sample_rate();
    }

    // Methods needed for DmaHandler
    _DMA_ISR_CALLBACK void callback_from_isr()
    {
        m_current_buff ^= 1; 
        m_dma.set_write_addr(m_ping_pong[m_current_buff].data(), true);
    }
    _DMA_ISR_CALLBACK Dma* get_dma_ptr() 
    {
        return &m_dma;
    }

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
    void change_buffer()
    {
        m_current_buff ^= 1; 
        m_dma.set_write_addr(m_ping_pong[m_current_buff].data(), true);
    }
    inline void read_blocking(std::span<u32>& rx_buffer)// requires (M == TransferMode::Rx)
    {
        while(!m_ready_flag)
        {
            tight_loop_contents();
        }
        m_ready_flag = false;

        // change_buffer();
        rx_buffer = m_ping_pong[m_current_buff ^ 1];
    }
    inline bool try_read(std::span<u32>& rx_buffer)// requires (M == TransferMode::Rx)
    {
        if(!m_ready_flag)
            return false;

        m_current_buff ^= 1; 
        m_dma.set_write_addr(m_ping_pong[m_current_buff].data(), true);
        m_ready_flag  = false;
        rx_buffer = m_ping_pong[m_current_buff ^ 1];
        return true;
    }
};

#include "drivers/i2s/i2s_mic.hpp"
_DMA_ISR_CALLBACK_ASSERT(SPH0645Mic<PioI2sProgram>)
// assert(1);

#endif