

#include <array>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
// #include "arm_math_types.h"
#include "drivers/gpio.hpp"
#include "drivers/uart.hpp"
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>
#include <string.h>

#include "drivers/dma.hpp"

#include "embedded_hal/traits.hpp"
#include "stats.hpp"
#include "drivers/countdown_timer.hpp"
#include "drivers/dma.hpp"
#include "tiny_dsp.hpp"
#include "drivers/async_dma_handler.hpp"
#include "types.hpp"

#define Rtos_sleep_ms(X) delay(pdMS_TO_TICKS((X))); 


 

 
 
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


#include "drivers/i2s/i2s_mic.hpp"
#include "drivers/i2s/sph0645_mic.hpp"
 
 
constexpr size_t FFT_SIZE   =  1024;
constexpr size_t BUFF_SIZE  =  2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end

static std::array<u32, BUFF_SIZE> PING{};       // raw i2s data, holding left and right
static std::array<u32, BUFF_SIZE> PONG{};       // raw i2s data, holding left and right
static std::array<f32, FFT_SIZE>  SAMPLES{};    // i2s float, holding only left from either ping or pong

static std::array<f32, FFT_SIZE> WINDOW_COEFFS{}; // Hann window
static std::array<f32, FFT_SIZE> FFT_RESULT{};    // fft values
static std::array<f32, FFT_SIZE/2> MAGNITUDES{};  // fft magnitudes

constexpr size_t FLOAT_MAX_SAMPLE = FFT_SIZE;
constexpr u32 DMA_CH0 = 0;
constexpr u32 DMA_CH1 = 1;


static tiny_dsp::RealFFT dsp{FFT_SIZE};

#pragma pack(push, 1)
template<typename T, size_t N>
struct Packet_t 
{
    std::array<u8,2> header;                               
    std::array<T, N> samples;        
    void swap_sample(std::array<T, N>& buff)
    {
        samples.swap(buff);
    }
    void copy_ptr(std::array<T, N>& buff)
    {
        samples.data() = buff.data();
    }
};

#pragma pack(pop)



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
        .sample_rate = 20'000, // Word Select frequency: 
        .sck_pin     = 16,     // sck is 32 bit for left and 32bit for right, therefor serial clock is 64 * 16k
        .ws_pin      = 17,     // we clock in Left & right samples 16k per secondes 
        .data_pin    = 18,     // bit sampled at serial clock rate, but only process left (Word Select low)
    };

    PioI2sMicrophone i2s_mic{config};
    SPH0645Mic i2s{
        i2s_mic,
        PING,
        PONG,
        Dma{DMA_CH0},
        irqs};

    constexpr u16 LED_PIN = 15;
    pico2_w::Output led{LED_PIN};
 
    constexpr u32 baudrate = 921'600;
    // constexpr u32 baudrate = 250'000;
     
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{DMA_CH1});
    uart.set_baudrate(baudrate);


    if(dsp.init() != ARM_MATH_SUCCESS)
        error_led(led);
    

    Packet_t<float, FFT_SIZE> audio_packet;
    Packet_t<float, FFT_SIZE/2> magnitude_packet;
    
    audio_packet.header     = {0xAA, 0xBB};
    magnitude_packet.header = {0xCC, 0xDD};

 
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
      
        i2s.read_blocking(raw_buffer);

        dsp.convert_mono_i2s_u32_to_f32(raw_buffer, SAMPLES);
        dsp.window_f32_inplace(SAMPLES, WINDOW_COEFFS);
        dsp.perform_fft(SAMPLES, FFT_RESULT);

        audio_packet.swap_sample(SAMPLES);
        uart.write_dma(&audio_packet, sizeof(audio_packet));

        dsp.compute_magnitude_squared(FFT_RESULT, MAGNITUDES); 
        auto [peak_idx, _] = dsp.find_max(std::span{MAGNITUDES}.subspan<k_min>(), search_len);
        
        
        magnitude_packet.swap_sample(MAGNITUDES);
        uart.dma_tx().await_blocking();
        uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
        
        // u32 peak_bin = k_min + peak_idx;
        // f32 est_freq = ((float)peak_bin) * bin_resolution;

        // write_buff.write("Got: %f Hz +/- %f\n", est_freq, bin_resolution);
        // uart.write_dma(write_buff.get());
         
    }
}

 