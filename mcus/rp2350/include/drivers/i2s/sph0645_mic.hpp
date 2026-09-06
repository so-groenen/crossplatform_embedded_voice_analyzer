#ifndef I2S_PICO_HPP
#define I2S_PICO_HPP


#include "interrupts/async_dma_handlers.hpp"
#include "drivers/dma.hpp"
// #include "drivers/interface_async_dma_handler.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/pio.hpp"
#include "types.hpp"
#include <concepts>
#include "voice_analyzer.hpp"


template<typename T>
concept IsI2s = requires(T t)
{
    {t.get_sample_rate()} -> std::same_as<u32>;
};


template<pio::IsPioProgram P>
requires pio::IsPioReceiver<P> && IsI2s<P>
class AbstractI2sDriver
{
protected:
    Dma m_dma;
    P m_i2s;
    size_t m_current_buff{};
    std::span<u32> m_ping_pong[2];

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
    AbstractI2sDriver(P pio_i2s,
            std::span<u32> ping, 
            std::span<u32> pong,
            Dma dma)
        : m_i2s(std::move(pio_i2s)), m_dma{std::move(dma)}
    {
        assert(ping.size() == pong.size());
        m_ping_pong[0] = ping;
        m_ping_pong[1] = pong;

        setup_rx_mode();
    }

    u32 get_sample_rate() const
    {
        return m_i2s.get_sample_rate();
    }

    _DMA_ISR_CALLBACK void callback_from_isr()
    {
        m_current_buff ^= 1; 
        m_dma.set_write_addr(m_ping_pong[m_current_buff].data(), true);
    }
    _DMA_ISR_CALLBACK Dma* get_dma_ptr() 
    {
        return &m_dma;
    }
 
    virtual void read_blocking(std::span<u32>& rx_buffer) = 0;
 
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
};



template<pio::IsPioProgram P>
requires pio::IsPioReceiver<P> && IsI2s<P>
class SPH0645Mic : public AbstractI2sDriver<P>
{
    bool& m_ready_flag;
public:
    template<irq_num_rp2350 IRQ_n, size_t N_Channels>
    SPH0645Mic(P pio_i2s,
            std::span<u32> ping, 
            std::span<u32> pong,
            Dma dma,
            interrupts::DmaHandlerWithCallback<IRQ_n, N_Channels, SPH0645Mic<P>>& irq)
        : AbstractI2sDriver<P>{std::move(pio_i2s), ping, pong, std::move(dma)}, 
            m_ready_flag{irq.bind_driver(this)}
    {
        this->m_dma.enable_channel(true);
    }
    inline void read_blocking(std::span<u32>& rx_buffer) override
    {
        while(!m_ready_flag)
        {
            tight_loop_contents();
        }
        m_ready_flag = false;
        rx_buffer = this->m_ping_pong[this->m_current_buff ^ 1];
    }
 
};


template<pio::IsPioProgram P>
requires pio::IsPioReceiver<P> && IsI2s<P>
class SPH0645MicAsync : public AbstractI2sDriver<P>
{
    // bool& m_ready_flag;
    PicoTask* m_task{};
public:
    template<irq_num_rp2350 IRQ_n, size_t N_Channels>
    SPH0645MicAsync(P pio_i2s,
            std::span<u32> ping, 
            std::span<u32> pong,
            Dma dma,
            PicoTask* task,
            interrupts::AsyncDmaHandlerWithCallback<IRQ_n, N_Channels, SPH0645MicAsync<P>>& irq)
        : AbstractI2sDriver<P>{std::move(pio_i2s), ping, pong, std::move(dma)}, 
            m_task{task}
    {
        irq.bind_rtos_driver(this);
        this->m_dma.enable_channel(true);
    }

    _DMA_ISR_RTOS_CALLBACK auto get_task() -> PicoTask*
    {
        return m_task;
    }

    _DMA_ISR_RTOS_CALLBACK  inline void read_blocking(std::span<u32>& rx_buffer) override
    {
        // while(!m_ready_flag)
        // {
        //     tight_loop_contents();
        // }
        m_task->awaitNotifs();
        // m_ready_flag = false;
        rx_buffer = this->m_ping_pong[this->m_current_buff ^ 1];
    }
 
};


#include "drivers/i2s/i2s_mic.hpp"
_DMA_ISR_CALLBACK_ASSERT(SPH0645Mic<PioI2sProgram>)
// assert(1);

#endif