

#include <array>
#include <cstddef>
#include <cstdio>
#include <hardware/irq.h>
#include <hardware/regs/intctrl.h>
// #include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
// #include "arm_math_types.h"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "interrupts/async_dma_handlers.hpp"
#include "drivers/gpio.hpp"
#include "drivers/pico_task.hpp"
#include "drivers/uart.hpp"
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>
#include <string.h>
#include <sys/cdefs.h>

#include "drivers/dma.hpp"

#include "embedded_hal/traits.hpp"
#include "interrupts/dma_handlers.hpp"
#include "projdefs.h"
#include "stats.hpp"
#include "drivers/countdown_timer.hpp"
#include "drivers/dma.hpp"
#include "voice_analyzer.hpp"
#include "interrupts/async_dma_handlers.hpp"
#include "pico/multicore.h"
#include "types.hpp"
#include "drivers/i2s/i2s_mic.hpp"
#include "drivers/i2s/sph0645_mic.hpp"




// class IRepeatingTimer
// {
//     struct repeating_timer m_timer;
//     i32 m_delay_ms;
// public:
//     IRepeatingTimer(i32 delay_ms)
//      : m_delay_ms{delay_ms}
//     {
//     }
//     void start()
//     {
//         add_repeating_timer_ms(m_delay_ms, IRepeatingTimer::runner, reinterpret_cast<void*>(this), &m_timer);
//     }
//     virtual bool timer_callback() = 0;
//     bool cancel()
//     {
//         return cancel_repeating_timer(&m_timer);
//     }
//     ~IRepeatingTimer() = default;
// private:
//     static bool runner(repeating_timer* timer_ptr)
//     {
//         auto inst = reinterpret_cast<IRepeatingTimer*>(timer_ptr->user_data);
//         return inst->timer_callback();
//     }
// };


using interrupts::IAsyncDmaHandler;
using interrupts::AsyncDmaHandler;
using interrupts::AsyncDmaHandlerWithCallback;
using interrupts::DmaHandlerWithCallback;

constexpr size_t num_channels = 1;
 

using I2sMicrophoneAsync = SPH0645MicAsync<PioI2sProgram>;
using I2sIrqHandler      = AsyncDmaHandlerWithCallback<DMA_IRQ_0, num_channels, I2sMicrophoneAsync>;
BIND_INTERRUPT(irqs, I2sIrqHandler, i2s_handler);
 


// using UartIrqHandler = AsyncDmaHandler<DMA_IRQ_1, num_channels>;
// BIND_INTERRUPT(irq_uart, UartIrqHandler, uart_handler);




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

 


// class LedTimer : public IRepeatingTimer
// {
//     pico2_w::Output led{};
// public:
//     LedTimer(u16 led_pin, i32 delay_ms)
//      : IRepeatingTimer(delay_ms), led{led_pin}
//     {}
//     bool timer_callback() override
//     {
//         led.toggle();
//         return true;
//     }
// };


#include <FreeRTOS/Queue.hpp>

static FreeRTOS::Queue<std::span<f32>> s_queue{4};


template<size_t FFT_N>
class VoiceTask : public PicoTask
{
    I2sMicrophoneAsync m_i2s;
    VoiceAnalyzer<FFT_N, I2sMicrophoneAsync> m_analyzer;
public:
    VoiceTask(const PioI2sConfig& i2s_config, const VoiceAnalyzerConfig<FFT_SIZE>& dsp_config)
     :  PicoTask(tskIDLE_PRIORITY + 2, 2 * FFT_SIZE, "VoiceTask"),
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


// class AsyncWriter : public UartNonBlocking<TransferMode::Tx>
// {
//     PicoTask* m_task{};
// public:
//     explicit AsyncWriter(Dma dma, PicoTask* task, IAsyncDmaHandler& irqs)
//      : UartNonBlocking<TransferMode::Tx>(std::move(dma)), m_task(task)
//     {
//         irqs.bind_interrupt_from_rtos(&this->dma_tx(), m_task);
//     }
//     inline void write_async(const volatile void* read_addr, size_t transfer_count)
//     {
//         this->m_dma_tx->enable_channel(true);
//         this->write_dma(read_addr, transfer_count);
//         m_task->awaitNotifs();
//         this->m_dma_tx->enable_channel(false);
//     }
// };



class UartCallbackWriter : public UartNonBlocking<TransferMode::Tx>
{
    bool m_trans_allowed{};
    std::span<const u8> m_data{};
public:
    template<size_t N>
    explicit UartCallbackWriter(Dma dma, DmaHandlerWithCallback<DMA_IRQ_1, N, UartCallbackWriter>& irqs)
     : UartNonBlocking<TransferMode::Tx>(std::move(dma), uart0)
    {
        irqs.bind_driver(this);
        this->dma_tx().enable_channel(true);
    }

    void enable_transfer_from_isr()
    {
        m_trans_allowed = true;
    }
    void set_isr_transfer_data(const void* addr, size_t bytes)
    {
        m_data = std::span<const u8>{reinterpret_cast<const u8*>(addr), bytes};
    }
    _DMA_ISR_CALLBACK void callback_from_isr(uint dma_chan)
    {
        assert(this->dma_tx().channel() == dma_chan);
        
        if(!m_trans_allowed)
            return;

        this->write_dma(m_data);
        m_trans_allowed = false;
    }
    _DMA_ISR_CALLBACK  Dma* get_dma_ptr()
    {
        return &this->dma_tx();
    } 
};


using UartIrqHandler      =  interrupts::DmaHandlerWithCallback<DMA_IRQ_1, 1, UartCallbackWriter>;
BIND_INTERRUPT(irqs_uart, UartIrqHandler, uart_handler);

constexpr u32 baudrate = 921'600;     


class SenderTask : public PicoTask
{
    // UartNonBlocking<TransferMode::Tx>& m_uart;
    Packet_t<f32, FFT_SIZE> audio_packet;
    Packet_t<f32, FFT_SIZE/2> magnitude_packet;
    // AsyncWriter m_writer;
    UartCallbackWriter m_writer;
public:
    SenderTask(UartNonBlocking<TransferMode::Tx>& uart)
     : PicoTask(tskIDLE_PRIORITY +1, 2*FFT_SIZE, "SenderTask"), // m_uart{uart}, 
    //  m_writer{Dma{}, this, irq_uart},
     m_writer{Dma{}, irqs_uart} // uses default uart0
    {
        m_writer.set_baudrate(baudrate);

        audio_packet.header     = {0xAA, 0xBB};
        magnitude_packet.header = {0xCC, 0xDD};
    }
    void taskFunction() override
    {
        constexpr u16 task1_pin = 15;
        pico2_w::Output led{task1_pin};

        CountdownTimer<1> tasks{};
        tasks.add_task_ms(250);
        m_writer.set_isr_transfer_data(&magnitude_packet, sizeof(magnitude_packet)); 

        while(true)
        {
            if(tasks.is_ready())
                led.toggle();
            
            auto raw_samples = s_queue.receive(portMAX_DELAY);
            auto raw_mags    = s_queue.receive(portMAX_DELAY);

            audio_packet.copy_sample(raw_samples.value());
            magnitude_packet.copy_sample(raw_mags.value());

            m_writer.write_dma(&audio_packet, sizeof(audio_packet));
            m_writer.enable_transfer_from_isr();   // will automatically transfer magnitude_packet from ISR after first finishes. It is single shot, so needs to be renabled.
            
        }
    }
};

 
//     bool callback_func(__unused repeating_timer_t* t)
//     {
//         static pico2_w::Output led{15};
//         led.toggle();
//         printf("hello \n");
//         return true;
//     }
 
// class TimerTask : FreeRTOS::Task
// {
//     LedTimer led_timer{14, 250};
// public:
//     TimerTask()
//      : FreeRTOS::Task(1, 128, "my task")
//     {

//     }
//     void taskFunction() override
//     {
//         led_timer.start();
//         while(true)
//         {
//         }
//     }
// };
// bool repeating_timer_callback(struct repeating_timer *t) {
//     printf("Tick\n");
//     return true; 
// }
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
    

    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{});
    uart.set_baudrate(baudrate);

    static VoiceTask<FFT_SIZE> voice_task{i2s_config, analyzer_config};
    static SenderTask sender_task{uart};
    // static FakeSenderTask fake{};
    // static StatsTask stats{};

    // LedTimer timer{1000};
    // TimerTask timer_task{};
    FreeRTOS::Kernel::startScheduler();
    while (true) 
    {
        tight_loop_contents();
    }
}

 