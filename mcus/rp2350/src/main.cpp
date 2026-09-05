
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
// #include "arm_math_types.h"
#include "drivers/gpio.hpp"
#include "drivers/uart.hpp"
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/types.h>
#include <ranges>
#include <span>
#include <stddef.h>
#include <string.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "FreeRTOS/Mutex.hpp"
#include "FreeRTOSConfig.h"
#include "counter_task.hpp"
#include "drivers/dma.hpp"

#include "embedded_hal/traits.hpp"
#include "stats.hpp"

#include "drivers/dma.hpp"
#include "tiny_dsp.hpp"
#include "drivers/async_dma_handler.hpp"
#include "types.hpp"

#define Rtos_sleep_ms(X) delay(pdMS_TO_TICKS((X))); 


#include "i2s.pio.h"


 
 
constexpr size_t N_channel = 1;

class I2SDmaIrqHandler : public DmaHandlerBase<DMA_IRQ_0, N_channel>  
{
    bool m_is_ready{};
public:
    explicit I2SDmaIrqHandler(irq_handler_t handler)
        : DmaHandlerBase<DMA_IRQ_0, N_channel>(handler) 
    {
    }
    inline void handle_inst(size_t n) override 
    {
        m_is_ready = true;
    }
    bool& ready_flag()
    {
        return m_is_ready;
    }
};


static void __time_critical_func(i2s_handler)();
I2SDmaIrqHandler irqs{i2s_handler};
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
};

  
constexpr size_t FFT_SIZE   =  2048;
constexpr size_t BUFF_SIZE  =  2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end

static std::array<u32, BUFF_SIZE> PING{};    // raw i2s data
static std::array<u32, BUFF_SIZE> PONG{};    // raw i2s data
static std::array<f32, FFT_SIZE>  SAMPLES{}; // i2s float

static std::array<f32, FFT_SIZE> WINDOW_COEFFS{}; // Hann window
static std::array<f32, FFT_SIZE> FFT_RESULT{};    // fft
static std::array<f32, FFT_SIZE/2> MAGNITUDES{};  // fft magnitudes

constexpr size_t FLOAT_MAX_SAMPLE = FFT_SIZE;
constexpr u32 DMA_CH0 = 0;
constexpr u32 DMA_CH1 = 1;


static tiny_dsp::RealFFT dsp{FFT_SIZE};

#pragma pack(push, 1)
struct Packet_t 
{
    uint8_t header[2];                               
    std::array<float, FFT_SIZE> samples;        
    void swap_sample(std::array<float, FFT_SIZE>& buff)
    {
        samples.swap(buff);
    }
};
#pragma pack(pop)

template<size_t N>
class CountdownTimer 
{
    heapless::Vector<absolute_time_t, N> m_next{};
    heapless::Vector<uint64_t, N> m_delays{};
public:
    void add_task_us(uint64_t delay_us)
    {
        m_delays.push_back(delay_us);
        m_next.push_back(get_absolute_time());
    }
    void add_task_ms(float delay_ms)
    {
        m_delays.push_back( static_cast<uint64_t>(delay_ms * 1000.f) );
        m_next.push_back(get_absolute_time());
    }
    inline bool is_ready(size_t n = 0)
    {
        absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(now, m_next[n]) <= 0) 
        {
            m_next[n] = delayed_by_us(now, m_delays[n]);
            return true;
        }
        return false;
    }
};


void error_led(embedded_hal::output::IsOutput auto& led)
{
    while(true)
    {
        sleep_ms(100);
        led.toggle();
    }
}
using namespace hardware::uart;




int main() 
{
    stdio_init_all();

    constexpr PioI2sConfig config
    {
        .sample_rate = 16'000, // Word Select frequency: 
        .sck_pin     = 16,     // sck is 32 bit for left and 32bit for right, therefor serial clock is 64 * 16k
        .ws_pin      = 17,     // we clock in Left & right samples 16k per secondes 
        .data_pin    = 18,     // bit sampled at serial clock rate, but only process left (Word Select low)
    };

    PioI2sMicrophone pio_mic{config};
    I2SHalfDuplex<TransferMode::Rx, u32, PioI2sMicrophone> i2s{
        pio_mic,
        PING,
        PONG,
        Dma{DMA_CH0},
        irqs};
    constexpr u16 LED_PIN = 15;
    pico2_w::Output led{LED_PIN};
 
    // constexpr u32 baudrate = 921600;
    constexpr u32 baudrate = 250'000;
    constexpr bool is_tx   = true;
    
    // auto uart = hardware::Uart::new_tx_dma(Dma{}, uart0, baudrate);
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{DMA_CH1});
    uart.set_baudrate(baudrate);


    if(dsp.init() != ARM_MATH_SUCCESS)
        error_led(led);
    

    // Packet_t tx_packet;
    // tx_packet.header[0] = 0xAA;
    // tx_packet.header[1] = 0xBB;

 
    std::span<u32> raw_buffer{};


 
    constexpr float bin_resolution = (float)config.sample_rate/FFT_SIZE;
    constexpr u32 k_min            = 50 / bin_resolution;
    constexpr u32 k_max            = 1000 / bin_resolution;

    // float search_max_val = 0.0f;
    uint32_t search_len = k_max - k_min + 1;

   
    heapless::WriteBuffer<64> write_buff{};
    CountdownTimer<1> timer{};
    timer.add_task_ms(500);

    dsp.hann(WINDOW_COEFFS);

    i2s.start(); 
    while (true) 
    {
        if(timer.is_ready())
            led.toggle();
      
        i2s.read_blocking(raw_buffer, irqs.ready_flag());

        dsp.convert_mono_i2s_u32_to_f32(raw_buffer, SAMPLES);
        dsp.window_f32_inplace(SAMPLES, WINDOW_COEFFS);
        dsp.perform_fft(SAMPLES, FFT_RESULT);
        dsp.compute_magnitude_squared(FFT_RESULT, MAGNITUDES); 

        auto [peak_idx, _] = dsp.find_max(std::span{MAGNITUDES}.subspan<k_min>(), search_len);

        u32 peak_bin = k_min + peak_idx;
        f32 est_freq = ((float)peak_bin) * bin_resolution;

        write_buff.write("Got: %f Hz +/- %f\n", est_freq, bin_resolution);
        uart.write_dma(write_buff.get());
         
    }
}

 