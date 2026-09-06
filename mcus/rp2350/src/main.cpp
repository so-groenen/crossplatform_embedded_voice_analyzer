

#include <array>
#include <cstdio>
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
#include "voice_analyzer.hpp"
#include "drivers/async_dma_handler.hpp"
#include "types.hpp"
#include "drivers/i2s/i2s_mic.hpp"
#include "drivers/i2s/sph0645_mic.hpp"


 

 

 
 
constexpr size_t N_channel = 1;
 

static void __time_critical_func(i2s_handler)();
DmaHandlerBase<DMA_IRQ_0, N_channel>  irqs{i2s_handler};
void __time_critical_func(i2s_handler)()
{
    irqs.handle_isr();
}


 
 
constexpr size_t FFT_SIZE   =  1024;
constexpr size_t BUFF_SIZE  =  2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end

static std::array<u32, BUFF_SIZE> PING{};       // raw i2s data, holding left and right
static std::array<u32, BUFF_SIZE> PONG{};       // raw i2s data, holding left and right 

constexpr size_t FLOAT_MAX_SAMPLE = FFT_SIZE;
constexpr u32 DMA_CH0             = 0;
constexpr u32 DMA_CH1             = 1;


// static tiny_dsp::RealFFT dsp{FFT_SIZE};

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
    void copy_sample(const std::array<T, N>& buff)
    {
        samples = buff;
    }
    void copy_sample(std::span<const T, N> buff)
    {
        std::copy(buff.begin(), buff.end(), samples.data());
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
  

constexpr u32 SAMPLE_RATE{20'000};

using voice_analyzer::VoiceAnalyzerConfig;
using voice_analyzer::VoiceAnalyzer;


int main() 
{
    stdio_init_all();

    constexpr u16 LED_PIN = 15;
    pico2_w::Output led{LED_PIN};

    constexpr PioI2sConfig config
    {
        .sample_rate = SAMPLE_RATE, // Word Select frequency: 
        .sck_pin     = 16,          // sck is 32 bit for left and 32bit for right, therefor serial clock is 64 * 16k
        .ws_pin      = 17,          // we clock in Left & right samples 16k per secondes 
        .data_pin    = 18,          // bit sampled at serial clock rate, but only process left (Word Select low)
    };
    PioI2sProgram pio_i2s_program{config};
    
    SPH0645Mic i2s{
        pio_i2s_program,
        PING,
        PONG,
        Dma{DMA_CH0},
        irqs};


    VoiceAnalyzerConfig<FFT_SIZE> analyzer_config = 
    {
        .min_freq        = 50,
        .max_freq        = 1000,
        .noise_threshold = 0.0f
    };

    static VoiceAnalyzer analyzer{i2s, analyzer_config};
    if(!analyzer.init())
        error_led(led);



 
    constexpr u32 baudrate = 921'600;
     
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{DMA_CH1});
    uart.set_baudrate(baudrate);


    Packet_t<float, FFT_SIZE> audio_packet;
    Packet_t<float, FFT_SIZE/2> magnitude_packet;
    audio_packet.header     = {0xAA, 0xBB};
    magnitude_packet.header = {0xCC, 0xDD};

 
    CountdownTimer<2> timer{};
    timer.add_task_ms(500);
    timer.add_task_ms(250);

    constexpr size_t led_task  = 0;
    constexpr size_t peak_task = 1;
    // dsp.hann(WINDOW_COEFFS);

    const auto raw_mags    = analyzer.get_raw_mag_view();
    const auto raw_samples = analyzer.get_raw_samples_view();

    
    analyzer.start();
    while (true) 
    {
        if(timer.is_ready(led_task))
            led.toggle();
        
        // static constexpr bool use_quad_interpol = true;
        // float peak = analyzer.get_peak<use_quad_interpol>();
        // if(timer.is_ready(peak_task))
        //     printf("Peak: %f \n", peak);
        analyzer.compute_magnitudes();

        audio_packet.copy_sample(raw_samples);
        magnitude_packet.copy_sample(raw_mags);
         
        uart.write_dma(&audio_packet, sizeof(audio_packet));
        uart.dma_tx().await_blocking();
        uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
    }
}

 