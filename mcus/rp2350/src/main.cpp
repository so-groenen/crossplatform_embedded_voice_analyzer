
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/regs/intctrl.h>
#include <hardware/spi.h>
#include <hardware/sync.h>
#include <limits>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "FreeRTOS/Mutex.hpp"
#include "FreeRTOSConfig.h"
#include "counter_task.hpp"
#include "drivers/dma.hpp"
#include "drivers/async_input.hpp"
#include "drivers/gpio.hpp"
#include "drivers/interface_async_dma_handler.hpp"
#include "drivers/spi_async.hpp"
#include "embedded_hal/traits.hpp"
#include "embedded_hal/tests.hpp"
#include "hardware/dma.h"
#include "drivers/pico_task.hpp"
#include "stats.hpp"

#include "drivers/dma.hpp"
#include "drivers/spi.hpp"
#include "drivers/async_dma_handler.hpp"

#define Rtos_sleep_ms(X) delay(pdMS_TO_TICKS((X))); 


#include "i2s.pio.h"


#define DSP_FFT_SIZE 1024


// static PIO pio_inst;
// static uint sm_inst;
// static uint dma_chan_inst;
 
// Double-buffering variables. Note that raw buffers store interleaved Left & Right samples,
// so they must hold 2 * fft_size words.

static uint32_t arena_memory[4*DSP_FFT_SIZE]; //dma_buffer0 is from [0:2*FFT_SIZE]; then dma_buffer1 is[2*FFT_SIZE:4*FFT_SIZE] to rest
static uint32_t* dma_buffers[2]= {&arena_memory[0], &arena_memory[2*DSP_FFT_SIZE]};

static volatile uint cur_buffer_idx = 0;
static volatile bool new_buffer_ready = false;

// // DMA Interrupt Service Routine
// static void __isr __time_critical_func(audio_dma_handler)(void) 
// {
//     uint next_buf = !cur_buffer_idx;
//     cur_buffer_idx = next_buf;

//     dma_channel_set_write_addr(dma_chan_inst, dma_buffers[next_buf], true);

//     new_buffer_ready = true;
//     dma_hw->ints0 = 1u << dma_chan_inst;
// }


 
template<DmaIrq IRQ_n, size_t N>
class I2SDmaIrqHandler : public DmaHandlerBase<IRQ_n, N>  
{
    bool m_is_ready{};
public:
    explicit I2SDmaIrqHandler(irq_handler_t handler)
        : DmaHandlerBase<IRQ_n, N>(handler) 
    {
    }
    inline void handle_inst(size_t n) override 
    {
        m_is_ready   = true;
        // printf("Hello I2S\n");
    }
    bool& ready_flag()
    {
        return m_is_ready;
    }
};

struct PioI2sConfig 
{
    uint32_t sample_rate;
    uint32_t sck_pin;
    uint32_t ws_pin;
    uint32_t data_pin;
};
class PioBaseProgram
{
protected:
    PIO m_pio_inst{pio0};
    uint m_pio_sm;
    uint m_offset{};
public:
    PioBaseProgram(PIO inst, const pio_program* program)
        : m_pio_inst{inst}, m_pio_sm(pio_claim_unused_sm(m_pio_inst, true)), m_offset(pio_add_program(m_pio_inst, program))
    {
        // uint m_offset = pio_add_program(m_pio_inst, program);
    }
    PIO pio_inst() const 
    {
        return m_pio_inst;
    }
    uint pio_sm() const 
    {
        return m_pio_sm;
    }
    io_ro_32* get_tx_fifo_reg() 
    {
        return &m_pio_inst->rxf[m_pio_sm];
    }
    io_ro_32* get_rx_fifo_reg() 
    {
        return &m_pio_inst->rxf[m_pio_sm];
    }
    uint get_dreq( bool is_tx)
    {
        return pio_get_dreq(m_pio_inst, m_pio_sm, is_tx);
    }
    void start()
    {
        pio_sm_clear_fifos(m_pio_inst, m_pio_sm);
        pio_sm_restart(m_pio_inst, m_pio_sm);
        pio_sm_set_enabled(m_pio_inst, m_pio_sm, true);
    }
    void stop()
    {
        pio_sm_set_enabled(m_pio_inst, m_pio_sm, false);
    } 
};

class PioI2sProgram : public PioBaseProgram
{
public:
    PioI2sProgram(const PioI2sConfig& config)
        : PioBaseProgram(pio0, &i2s_pio_program)
    {
        assert(config.ws_pin == config.sck_pin +1);
        i2s_pio_program_init(m_pio_inst, m_pio_sm, m_offset, config.sample_rate, config.data_pin, config.sck_pin);
    }
};

template<typename T>
concept IsPioI2S = requires (T pio, bool is_tx) {
    {pio.start()} -> std::same_as<void>;
    {pio.stop()} -> std::same_as<void>;
    {pio.get_tx_fifo_reg()} -> std::same_as<io_ro_32*>;
    {pio.get_rx_fifo_reg()} -> std::same_as<io_ro_32*>;
    {pio.get_dreq(is_tx)} -> std::same_as<uint>;
};


template<std::integral T, IsPioI2S P>
class I2S_RX 
{
    Dma m_dma{};
    P& m_pio{};
    size_t m_processed_buff{};
    std::span<T> m_ping_pong[2];
public:
    I2S_RX(P& pio_,
            // const I2S_Config& config, 
            std::span<T> ping, 
            std::span<T> pong,
            base::IDmaHandler& irq)
        : m_pio(pio_) //, m_pio_sm(pio_claim_unused_sm(m_pio_inst, true))
    {
        // assert(config.ws_pin == config.sck_pin +1);
        assert(ping.size() == pong.size());

        // uint offset = pio_add_program(m_pio_inst, &i2s_pio_program);
        // i2s_pio_program_init(m_pio_inst, m_pio_sm, offset, config.sample_rate, config.data_pin, config.sck_pin);

        m_dma.read_from_memory(false);
        m_dma.write_to_memory(true);
        m_dma.set_data_request(m_pio.get_dreq(false));
        m_dma.config_transfer(
            /*dest:*/ ping,
            /*src:*/  m_pio.get_rx_fifo_reg());
        m_dma.set_transfer_size<T>();
        m_ping_pong[0] = ping;
        m_ping_pong[1] = pong;
        irq.bind_interrupt(&m_dma);
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
    inline auto read_blocking(bool& is_ready) -> std::span<T>
    {
        while(!is_ready)
        {
            tight_loop_contents();
        }
        m_processed_buff ^= 1; 
        m_dma.set_write_addr(m_ping_pong[m_processed_buff].data(), true);
        is_ready = false;
        return m_ping_pong[m_processed_buff ^ 1];
    }
};


// int audio_hal_init(uint32_t sample_rate) 
// {
//     cur_buffer_idx = 0;
//     new_buffer_ready = false;

//     // Configure PIO
//     pio_inst    = pio0; // Use PIO0
//     uint offset = pio_add_program(pio_inst, &i2s_pio_program);
//     sm_inst     = pio_claim_unused_sm(pio_inst, true);
//     audio_hal_program_init(pio_inst, sm_inst, offset, sample_rate, MIC_DATA_PIN, MIC_SCK_PIN);

//     // Configure DMA
// Dma dma{};
//     dma.set_transfer_size(DMA_SIZE_32);
//     dma.read_from_memory(false);
//     dma.write_to_memory(true);
//     dma.set_data_request(pio_get_dreq(pio_inst, sm_inst, false));
//     dma.config_transfer(
//         /*dest:*/ dma_buffers[0],
//         /*src:*/  &pio_inst->rxf[sm_inst],
//         /*len:*/  2*DSP_FFT_SIZE);

 
//     return 0;
// }

// void audio_hal_start(void) {
//     cur_buffer_idx = 0;
//     new_buffer_ready = false;

//     // Reset PIO state machine FIFO
//     pio_sm_clear_fifos(pio_inst, sm_inst);
//     pio_sm_restart(pio_inst, sm_inst);

//     // Re-trigger DMA to write to buffer 0
//     dma_channel_set_write_addr(dma_chan_inst, dma_buffers[0], true);

//     // Enable state machine to start generating clocks and sampling
//     pio_sm_set_enabled(pio_inst, sm_inst, true);
// }

// void audio_hal_stop(void) {
//     pio_sm_set_enabled(pio_inst, sm_inst, false);
//     dma_channel_abort(dma_chan_inst);
// }

// bool audio_hal_is_buffer_ready(void) {
//     return new_buffer_ready;
// }

// const uint32_t* audio_hal_get_raw_buffer(bool block) {
//     if (block) {
//         while (!new_buffer_ready) {
//             tight_loop_contents();
//         }
//     }
//     new_buffer_ready = false;
//     // Return the buffer that was just completed.
//     // The DMA is currently writing into dma_buffers[cur_buffer_idx].
//     // The completed buffer is the other one.
//     return dma_buffers[!cur_buffer_idx];
// }

// void audio_hal_get_samples(float* out_samples, bool block) {
//     const uint32_t* raw = audio_hal_get_raw_buffer(block);
//     for (size_t i = 0; i < DSP_FFT_SIZE; i++) {
//         // raw[2*i] is the Left channel sample (24-bit sign-extended inside the 32-bit word, scaled by 256)
//         int32_t signed_sample = (int32_t)raw[2 * i];
//         // Convert to float and normalize to [-1.0, 1.0] by dividing by 2^31 (2147483648.0)
//         out_samples[i] = (float)signed_sample / 2147483648.0f;
//     }
// }

void convert(std::span<float> out, std::span<uint32_t> in)
{
    assert(2 * out.size() == in.size());

    for (size_t i = 0; i < out.size(); i++)
    {
        // raw[2*i] is the Left channel sample (24-bit sign-extended inside the 32-bit word, scaled by 256)
        int32_t signed_sample = (int32_t)in[2 * i];
        // Convert to float and normalize to [-1.0, 1.0] by dividing by 2^31 (2147483648.0)
        out[i] = (float)signed_sample / 2147483648.0f; 
    }
}

// void audio_hal_get_samples(float* out_samples, bool block) {
//     const uint32_t* raw = audio_hal_get_raw_buffer(block);
//     for (size_t i = 0; i < DSP_FFT_SIZE; i++) {
//         // raw[2*i] is the Left channel sample (24-bit sign-extended inside the 32-bit word, scaled by 256)
//         int32_t signed_sample = (int32_t)raw[2 * i];
//         // Convert to float and normalize to [-1.0, 1.0] by dividing by 2^31 (2147483648.0)
//         out_samples[i] = (float)signed_sample / 2147483648.0f;
//     }
// }


static void __time_critical_func(i2s_handler)();
I2SDmaIrqHandler<DmaIrq::IRQ_0, 1> irq{i2s_handler};
void __time_critical_func(i2s_handler)()
{
    irq.handle_isr();
}

constexpr size_t FFT_SIZE   =  2048;
constexpr size_t BUFF_SIZE  =  2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end
constexpr size_t TOTAL_SIZE =  2 * BUFF_SIZE; // Ping buffer + Pong buffer
static uint32_t ARENA[TOTAL_SIZE]; //dma_buffer0 is from [0:2*FFT_SIZE]; then dma_buffer1 is[2*FFT_SIZE:4*FFT_SIZE] to rest
static float    SAMPLES[FFT_SIZE];

int main() 
{
    stdio_init_all();

    std::span ping{ARENA,             BUFF_SIZE};
    std::span pong{ARENA + BUFF_SIZE, BUFF_SIZE};
    // std::span samples{SAMPLES};
    std::array<std::span<uint32_t>,2> ping_pong = {ping, pong};

    PioI2sConfig config
    {
        .sample_rate = 16'000,
        .sck_pin     = 20,
        .ws_pin      = 21,
        .data_pin    = 22,
    };

    PioI2sProgram pio_{config};
    I2S_RX i2s{
        pio_,
        ping,
        pong,
        irq};
    
    i2s.start(); 

    while(true)
    {
        std::span<uint32_t> raw_data = i2s.read_blocking(irq.ready_flag());
        printf("PING! \n");
        std::span<uint32_t> _raw_data = i2s.read_blocking(irq.ready_flag());
        printf("PONG! \n");
        
        // if(irq.ready_flag())
        // {
        //     curr ^= 1;
        //     dma.set_write(ping_pong[curr].data(), true);
        //     irq.ready_flag() = false;
        //     // curr? printf("Ping!\n") : printf("pong!\n");
        // }

        // tight_loop_contents();
        // sleep_ms(1000);
        // printf("Hello I2S \n");
        // auto raw_buff = i2s.await_buff_blocking();
        // convert(samples, raw_buff);
    }
}