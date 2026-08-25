
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


static PIO pio_inst;
static uint sm_inst;
static uint dma_chan_inst;
 
// Double-buffering variables. Note that raw buffers store interleaved Left & Right samples,
// so they must hold 2 * fft_size words.

static uint32_t arena_memory[4*DSP_FFT_SIZE]; //dma_buffer0 is from [0:2*FFT_SIZE]; then dma_buffer1 is[2*FFT_SIZE:4*FFT_SIZE] to rest
static uint32_t* dma_buffers[2]= {&arena_memory[0], &arena_memory[2*DSP_FFT_SIZE]};

static volatile uint cur_buffer_idx = 0;
static volatile bool new_buffer_ready = false;

// DMA Interrupt Service Routine
static void __isr __time_critical_func(audio_dma_handler)(void) 
{
    uint next_buf = !cur_buffer_idx;
    cur_buffer_idx = next_buf;

    dma_channel_set_write_addr(dma_chan_inst, dma_buffers[next_buf], true);

    new_buffer_ready = true;
    dma_hw->ints0 = 1u << dma_chan_inst;
}


class AudioHAL 
{
    Dma m_dma{};
    PIO pio_inst{pio0};
    uint pio_sm_inst;
public:
    AudioHAL(PIO inst, uint32_t sample_rate, uint data_pin, uint sck_pin)
        : pio_inst(inst), pio_sm_inst(pio_claim_unused_sm(pio_inst, true)) //pio0
    {
        uint offset = pio_add_program(pio_inst, &i2s_pio_program);
        sm_inst     = pio_claim_unused_sm(pio_inst, true);
        i2s_pio_program_init(pio_inst, sm_inst, offset, sample_rate, data_pin, sck_pin);

        //
        m_dma.read_from_memory(false);
        m_dma.write_to_memory(true);
        m_dma.set_data_request(pio_get_dreq(pio_inst, sm_inst, false));
    }
    // void init_buffer(volatile void* write_buff, size_t half_transfer_len)
    // {
    //     m_dma.config_transfer(
    //         /*dest:*/ write_buff,
    //         /*src:*/  &pio_inst->rxf[sm_inst],
    //         /*len:*/  half_transfer_len);
    // }
    // Put in the full array! We set transfer length to be half of it to trigger "half complete interrupts"
    template<std::integral T>
    void init_ping_pong_buff(std::span<T> buffer)
    {
        m_dma.infer_transfer_size(buffer);
        m_dma.config_transfer(
            /*dest:*/ buffer,
            /*src:*/  &pio_inst->rxf[sm_inst],
            /*len:*/  buffer.size()/2);
    }
};

Dma dma{};

int audio_hal_init(uint32_t sample_rate) 
{
    cur_buffer_idx = 0;
    new_buffer_ready = false;

    // Configure PIO
    pio_inst    = pio0; // Use PIO0
    uint offset = pio_add_program(pio_inst, &i2s_pio_program);
    sm_inst     = pio_claim_unused_sm(pio_inst, true);
    audio_hal_program_init(pio_inst, sm_inst, offset, sample_rate, MIC_DATA_PIN, MIC_SCK_PIN);

    // Configure DMA
    dma.set_transfer_size(DMA_SIZE_32);
    dma.read_from_memory(false);
    dma.write_to_memory(true);
    dma.set_data_request(pio_get_dreq(pio_inst, sm_inst, false));
    dma.config_transfer(
        /*dest:*/ dma_buffers[0],
        /*src:*/  &pio_inst->rxf[sm_inst],
        /*len:*/  2*DSP_FFT_SIZE);



 
    // Setup DMA interrupt
    dma_channel_set_irq0_enabled(dma_chan_inst, true);
    irq_set_exclusive_handler(DMA_IRQ_0, audio_dma_handler);
    irq_set_priority(DMA_IRQ_0, 0); // High priority
    irq_set_enabled(DMA_IRQ_0, true);

    return 0;
}

void audio_hal_start(void) {
    cur_buffer_idx = 0;
    new_buffer_ready = false;

    // Reset PIO state machine FIFO
    pio_sm_clear_fifos(pio_inst, sm_inst);
    pio_sm_restart(pio_inst, sm_inst);

    // Re-trigger DMA to write to buffer 0
    dma_channel_set_write_addr(dma_chan_inst, dma_buffers[0], true);

    // Enable state machine to start generating clocks and sampling
    pio_sm_set_enabled(pio_inst, sm_inst, true);
}

void audio_hal_stop(void) {
    pio_sm_set_enabled(pio_inst, sm_inst, false);
    dma_channel_abort(dma_chan_inst);
}

bool audio_hal_is_buffer_ready(void) {
    return new_buffer_ready;
}

const uint32_t* audio_hal_get_raw_buffer(bool block) {
    if (block) {
        while (!new_buffer_ready) {
            tight_loop_contents();
        }
    }
    new_buffer_ready = false;
    // Return the buffer that was just completed.
    // The DMA is currently writing into dma_buffers[cur_buffer_idx].
    // The completed buffer is the other one.
    return dma_buffers[!cur_buffer_idx];
}

void audio_hal_get_samples(float* out_samples, bool block) {
    const uint32_t* raw = audio_hal_get_raw_buffer(block);
    for (size_t i = 0; i < DSP_FFT_SIZE; i++) {
        // raw[2*i] is the Left channel sample (24-bit sign-extended inside the 32-bit word, scaled by 256)
        int32_t signed_sample = (int32_t)raw[2 * i];
        // Convert to float and normalize to [-1.0, 1.0] by dividing by 2^31 (2147483648.0)
        out_samples[i] = (float)signed_sample / 2147483648.0f;
    }
}



int main() 
{
    stdio_init_all();

    // constexpr uint32_t stats_ms{10'000};
    // StatsTask stats{stats_ms};

    // TransferTask transfer_task{};
    // BlinkyTask blinky_task{};
    // ButtonTask button_task{};

    // constexpr uint32_t counter_ms{500};
    // CounterTask counter_task{counter_ms, "counter-tsk"};
    // FreeRTOS::Kernel::startScheduler();
    while(true)
    {
    }
}