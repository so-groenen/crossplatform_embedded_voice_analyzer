
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/regs/intctrl.h>
#include <hardware/spi.h>
#include <hardware/sync.h>
#include <hardware/uart.h>
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
#include "types.hpp"

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
    }
    bool& ready_flag()
    {
        return m_is_ready;
    }
};


static void __time_critical_func(i2s_handler)();
I2SDmaIrqHandler<DmaIrq::IRQ_0, 1> irqs{i2s_handler};
void __time_critical_func(i2s_handler)()
{
    irqs.handle_isr();
}

struct PioI2sConfig 
{
    u32 sample_rate;
    u32 sck_pin;
    u32 ws_pin;
    u32 data_pin;
};

#include "drivers/pio.hpp"
#include "drivers/i2s/i2s.hpp"
 
class PioI2sMicrophone : public pio::PioBaseProgram
{
public:
    explicit PioI2sMicrophone(const PioI2sConfig& config)  
        : PioBaseProgram(pio0, &i2s_pio_program)
    {
        assert(config.ws_pin == config.sck_pin +1);
        i2s_pio_program_init(m_pio_inst, m_pio_sm, m_offset, config.sample_rate, config.data_pin, config.sck_pin);
    }
    io_ro_32* get_rx_fifo_reg() const 
    {
        return &m_pio_inst->rxf[m_pio_sm];
    }
};

 

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
    void enable_fifo(bool v)
    {
        uart_set_fifo_enabled(m_uart, v);
    }
    // template<std::integral T>
    // void write_blocking(std::span<const T> buff)
    // {
    //     uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    // }
    // template<std::floating_point T>
    void write_blocking(std::span<f32> buff)
    {
        uart_write_blocking(m_uart, reinterpret_cast<u8*>(buff.data()), buff.size_bytes());
    }
    template<typename T>
    void write_blocking(const T& buff)
    {
        uart_write_blocking(m_uart, reinterpret_cast<const u8*>(&buff), sizeof(T));
    }
};

constexpr size_t FFT_SIZE   =  2048;
constexpr size_t BUFF_SIZE  =  2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end

static std::array<u32, BUFF_SIZE> PING{};
static std::array<u32, BUFF_SIZE> PONG{};
static std::array<f32, FFT_SIZE>  SAMPLES{};

constexpr size_t FLOAT_MAX_SAMPLE = 6;

#pragma pack(push, 1)
struct Packet_t {
    uint8_t header[2];                               // Sync bytes: 0xAA, 0xBB
    std::array<float, FLOAT_MAX_SAMPLE> samples;        // 4 floats (16 bytes)
    void set_sample(std::array<float, FLOAT_MAX_SAMPLE>& buff)
    {
        samples.swap(buff);
    }
} ;
#pragma pack(pop)

 



int main() 
{
    stdio_init_all();

    PioI2sConfig config
    {
        .sample_rate = 16'000,
        .sck_pin     = 20,
        .ws_pin      = 21,
        .data_pin    = 22,
    };

    PioI2sMicrophone pio_mic{config};
    I2SHalfDuplex<TransferMode::Rx, u32, PioI2sMicrophone> i2s{
        pio_mic,
        PING,
        PONG,
        irqs};
    
    // i2s.start(); 
    
    // std::span<u32> buffer{};
    
    Uart uart{};
    std::array<f32, 4> arr = {42, 69, 67, 666};

    Packet_t tx_packet;
    tx_packet.header[0] = 0xAA;
    tx_packet.header[1] = 0xBB;

    // Dummy loop variables
    float counter = 0.0f;
    std::array<float, FLOAT_MAX_SAMPLE> rx_buff{};
    while (true) 
    {
        // Populate payload
        for(auto& value : rx_buff)
        {
            value = 1.0f - 2*(float)rand() / RAND_MAX;
        }
        tx_packet.set_sample(rx_buff);
        uart.write_blocking(tx_packet);

        sleep_ms(1000); // 10Hz transmission rate
    }
}

 