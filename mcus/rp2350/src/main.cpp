

#include <array>
#include <cstdio>
#include <cstdlib>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/regs/dreq.h>
#include <hardware/regs/intctrl.h>
// #include <pico/cyw43_arch.h>
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
#include <stdint.h>
#include <string.h>

#include "drivers/dma.hpp"

#include "embedded_hal/traits.hpp"
#include "stats.hpp"
#include "drivers/countdown_timer.hpp"
#include "drivers/dma.hpp"
#include "voice_analyzer.hpp"
#include "pico/multicore.h"
#include "types.hpp"
#include "drivers/i2s/i2s_mic.hpp"
#include "drivers/i2s/sph0645_mic.hpp"
#include "interrupts/async_dma_handlers.hpp"


 
 
constexpr size_t N_channel = 1;
 

using I2sMicrophone = SPH0645Mic<PioI2sProgram>;
using I2sIrqHandler = interrupts::DmaHandlerWithCallback<DMA_IRQ_0, N_channel, I2sMicrophone>;


// bind interrupt handler class to interrupt service routine.
BIND_INTERRUPT(irqs, I2sIrqHandler, i2s_handler);
 
 
constexpr size_t FFT_SIZE   = 1024;
constexpr size_t BUFF_SIZE  = 2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end
constexpr u32 SAMPLE_RATE   = 20'000;

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
    std::array<u8,4> header;                               
    std::array<T, N> samples;        
    void copy_sample(const std::array<T, N>& buff)
    {
        samples = buff;
    }
    void copy_sample(std::span<const T> buff)
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
  


using voice_analyzer::VoiceAnalyzerConfig;
using voice_analyzer::VoiceAnalyzer;

 
void core1_entry() 
{

    // constexpr u32 baudrate = 250'000;     
    constexpr u32 baudrate = 921'600;     
    // 
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{});
    uart.set_baudrate(baudrate);


    static Packet_t<f32, FFT_SIZE> audio_packet;
    static Packet_t<f32, FFT_SIZE/2> magnitude_packet;
    audio_packet.header     = {0xAA, 0xBB, 0xCC, 0xDD};
    magnitude_packet.header = {0xAB, 0xBC, 0xCD, 0xDE};

    constexpr u16 LED_PIN = 15;
    pico2_w::Output led_core1{LED_PIN};

    CountdownTimer<1> tasks{};
    tasks.add_task_ms(250);
 
    constexpr size_t led_task  = 0;
    
 
    while (true)
    {
        if(tasks.is_ready(led_task))
            led_core1.toggle();
 
        u32 raw_samples = multicore_fifo_pop_blocking();
        auto raw_samples_ptr = reinterpret_cast<const f32*>(raw_samples);
        
        u32 raw_mags = multicore_fifo_pop_blocking();
        auto raw_mags_ptr = reinterpret_cast<const f32*>(raw_mags);

 
        magnitude_packet.copy_sample(std::span{raw_mags_ptr, FFT_SIZE/2});
        audio_packet.copy_sample(std::span{raw_samples_ptr, FFT_SIZE});

        uart.write_dma(&audio_packet, sizeof(audio_packet));
        uart.dma_tx().await_blocking();
        uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
    }
}

 

int main() 
{
    stdio_init_all();
    sleep_ms(25);
    multicore_reset_core1();
    sleep_ms(25);

    constexpr u16 CORE0_LED_PIN = 14;
    pico2_w::Output led_core0{CORE0_LED_PIN};

    constexpr PioI2sConfig config
    {
        .sample_rate = SAMPLE_RATE, // Word Select frequency: 
        .sck_pin     = 16,          // sck is 32 bit for left and 32bit for right, therefor serial clock is 64 * 16k
        .ws_pin      = 17,          // we clock in Left & right samples 16k per secondes 
        .data_pin    = 18,          // bit sampled at serial clock rate, but only process left (Word Select low)
    };
     
    I2sMicrophone i2s{
        PioI2sProgram{config},
        PING,
        PONG,
        Dma{},
        irqs};

    VOICE_ANALYZER_I2S_ASSERT(I2sMicrophone);
 
    VoiceAnalyzerConfig<FFT_SIZE> analyzer_config = 
    {
        .min_freq        = 50,
        .max_freq        = 1000,
        .noise_threshold = 0.0f
    };

    static VoiceAnalyzer analyzer{i2s, analyzer_config};
    if(!analyzer.init())
        error_led(led_core0);

 
    CountdownTimer<1> timer{};
    timer.add_task_ms(250);
 
    constexpr size_t led_task  = 0;   
    multicore_launch_core1(core1_entry);
 
    analyzer.start(); 
    while (true) 
    {
        if(timer.is_ready(led_task))
            led_core0.toggle();
        
        // static constexpr bool use_quad_interpol = true;
        // float peak = analyzer.get_peak<use_quad_interpol>();
        // if(timer.is_ready(peak_task))
        //     printf("Peak: %f \n", peak);
        analyzer.compute_magnitudes();

        auto raw_samples_ptr = reinterpret_cast<uintptr_t>(analyzer.get_raw_samples().data());
        auto raw_mag_ptr     = reinterpret_cast<uintptr_t>(analyzer.get_raw_mag().data());

        multicore_fifo_push_blocking(raw_samples_ptr);
        multicore_fifo_push_blocking(raw_mag_ptr);
    }
}

 