

#include <array>
#include <cstdio>
#include <hardware/irq.h>
#include <hardware/regs/intctrl.h>
// #include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
// #include "arm_math_types.h"
#include "FreeRTOS/Kernel.hpp"
#include "drivers/gpio.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/uart.hpp"
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>
#include <string.h>

#include "drivers/dma.hpp"

#include "embedded_hal/traits.hpp"
#include "projdefs.h"
#include "stats.hpp"
#include "drivers/countdown_timer.hpp"
#include "drivers/dma.hpp"
#include "voice_analyzer.hpp"
#include "drivers/async_dma_handler.hpp"
#include "pico/multicore.h"
#include "types.hpp"
#include "drivers/i2s/i2s_mic.hpp"
#include "drivers/i2s/sph0645_mic.hpp"


 
 
constexpr size_t N_channel = 1;
 

using I2sMicrophoneAsync = SPH0645MicAsync<PioI2sProgram>;
using I2sIrqHandler      = irq::AsyncDmaHandlerCallback<DMA_IRQ_0, N_channel, I2sMicrophoneAsync>;


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
    std::array<u8,2> header;                               
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
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{DMA_CH1});
    uart.set_baudrate(baudrate);


    Packet_t<f32, FFT_SIZE> audio_packet;
    Packet_t<f32, FFT_SIZE/2> magnitude_packet;
    audio_packet.header     = {0xAA, 0xBB};
    magnitude_packet.header = {0xCC, 0xDD};

    constexpr u16 LED_PIN = 15;
    pico2_w::Output led_core1{LED_PIN};

    CountdownTimer<1> tasks{};
    tasks.add_task_ms(250);
 
    constexpr size_t led_task  = 0;
    
    while (true)
    {
        if(tasks.is_ready(led_task))
            led_core1.toggle();

        // u32 ok = multicore_fifo_pop_blocking();
        u32 raw_samples = multicore_fifo_pop_blocking();
        auto raw_samples_ptr = reinterpret_cast<const f32*>(raw_samples);
        
        u32 raw_mags = multicore_fifo_pop_blocking();
        auto raw_mags_ptr = reinterpret_cast<const f32*>(raw_mags);


        audio_packet.copy_sample(std::span{raw_samples_ptr, FFT_SIZE});
        magnitude_packet.copy_sample(std::span{raw_mags_ptr, FFT_SIZE/2});
         
        uart.write_dma(&audio_packet, sizeof(audio_packet));
        uart.dma_tx().await_blocking();
        uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
    }
}

#include <FreeRTOS/Queue.hpp>

static FreeRTOS::Queue<std::span<f32>> s_queue{4};


template<size_t FFT_N>
class VoiceTask : public PicoTask
{
    I2sMicrophoneAsync m_i2s;
    VoiceAnalyzer<FFT_N, I2sMicrophoneAsync> m_analyzer;
public:
    VoiceTask(const PioI2sConfig& i2s_config, const VoiceAnalyzerConfig<FFT_SIZE>& dsp_config)
     :  PicoTask(tskIDLE_PRIORITY +1, 2 * FFT_SIZE, "VoiceTask"),
     m_i2s{PioI2sProgram{i2s_config}, PING, PONG, Dma{}, this, irqs}
      ,m_analyzer{m_i2s, dsp_config}
    {
        m_analyzer.init();
    }
    void taskFunction() override
    {
        constexpr u16 task0_led_pin = 14;
        pico2_w::Output led{task0_led_pin};


        CountdownTimer<1> task{};
        task.add_task_ms(250);

        const auto raw_samples = m_analyzer.get_raw_samples_view();
        const auto raw_mags    = m_analyzer.get_raw_mag_view();

        m_analyzer.start();
        while(true)
        {
            if(task.is_ready())
                led.toggle();
        
            m_analyzer.compute_magnitudes(); // await notifs! & fires interrupt to unblock
            s_queue.sendToBack(raw_samples);
            s_queue.sendToBack(raw_mags);
        }
    }
};


class SenderTask : public PicoTask
{
    UartNonBlocking<TransferMode::Tx>& m_uart;
    Packet_t<f32, FFT_SIZE> audio_packet;
    Packet_t<f32, FFT_SIZE/2> magnitude_packet;

public:
    SenderTask(UartNonBlocking<TransferMode::Tx>& uart)
     : PicoTask(tskIDLE_PRIORITY +1, 2*FFT_SIZE, "SenderTask"), m_uart{uart}
    {
        audio_packet.header     = {0xAA, 0xBB};
        magnitude_packet.header = {0xCC, 0xDD};
    }
    void taskFunction() override
    {
        constexpr u16 task1_pin = 15;
        pico2_w::Output led{task1_pin};

        CountdownTimer<1> tasks{};
        tasks.add_task_ms(250);
        

        while(true)
        {
            if(tasks.is_ready())
                led.toggle();
            
            auto raw_samples = s_queue.receive(portMAX_DELAY);
            
            audio_packet.copy_sample(raw_samples.value());
            m_uart.write_dma(&audio_packet, sizeof(audio_packet));
             
            auto raw_mags    = s_queue.receive(portMAX_DELAY);
            
            magnitude_packet.copy_sample(raw_mags.value());
            m_uart.dma_tx().await_blocking();
            m_uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
        }
    }
};

 


int main() 
{
    stdio_init_all();
     

    constexpr PioI2sConfig i2s_config
    {
        .sample_rate = SAMPLE_RATE, // Word Select frequency: 
        .sck_pin     = 16,          // sck is 32 bit for left and 32bit for right, therefor serial clock is 64 * 16k
        .ws_pin      = 17,          // we clock in Left & right samples 16k per secondes 
        .data_pin    = 18,          // bit sampled at serial clock rate, but only process left (Word Select low)
    };
     
    VOICE_ANALYZER_I2S_ASSERT(I2sMicrophoneAsync);
 
    constexpr VoiceAnalyzerConfig<FFT_SIZE> analyzer_config = 
    {
        .min_freq        = 50,
        .max_freq        = 1000,
        .noise_threshold = 0.0f
    };
    

    constexpr u32 baudrate = 921'600;     
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{});
    uart.set_baudrate(baudrate);

    static VoiceTask<FFT_SIZE> voice_task{i2s_config, analyzer_config};
    static SenderTask sender_task{uart};
    // static FakeSenderTask fake{};
    // static StatsTask stats{};
    FreeRTOS::Kernel::startScheduler();
    while (true) 
    {
        
    }
}

 