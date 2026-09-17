

#include <array>
#include <cstdio>
#include <cstdlib>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/dreq.h>
#include <hardware/regs/intctrl.h>
// #include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
// #include "arm_math_types.h"
#include "drivers/gpio.hpp"
#include "drivers/input.hpp"
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
BIND_INTERRUPT(i2s_irq, I2sIrqHandler, i2s_handler);
 
 
constexpr size_t FFT_SIZE   = 1024;
constexpr size_t BUFF_SIZE  = 2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end
constexpr u32 SAMPLE_RATE   = 20'000;

static std::array<u32, BUFF_SIZE> PING{};       // raw i2s data, holding left and right
static std::array<u32, BUFF_SIZE> PONG{};       // raw i2s data, holding left and right 

constexpr size_t FLOAT_MAX_SAMPLE = FFT_SIZE;
constexpr u32 DMA_CH0             = 0;
constexpr u32 DMA_CH1             = 1;


enum class UserMode : u8
{
    Peak = 0,
    Spectrum = 1
};


#include "interrupts/input_handlers.hpp"

class ButtonDriver
{
    Input& m_btn1;
    pico2_w::Output& m_led;
    i32 m_debounce_us{10'000};
    absolute_time_t m_last1{get_absolute_time()};
    volatile UserMode& m_mode;
public:
    INPUT_HANDLER_CLASS(ButtonDriver, irq, Input& bt1, pico2_w::Output& led, volatile UserMode& mode)
        : m_btn1(bt1), m_led(led), m_mode(mode)
    {
        irq.bind_driver(this);
        irq.bind_interrupt(&m_btn1);
    }
    _ISR_INPUT_CALLBACK void callback_from_isr(u16 pin)
    {
        assert(pin = m_btn1.pin());
        auto now  = get_absolute_time(); 
        if(absolute_time_diff_us(m_last1, now) > m_debounce_us)
        {
            m_mode = (m_mode == UserMode::Peak)? UserMode::Spectrum : UserMode::Peak;
            m_led.toggle();
        }
    }
};

// Define the MyIRQHandler class which will call the ButtonDriver callback from within the ISR, and register 1 button
using MyIRQHandler = INPUT_IRQ_HANDLER(ButtonDriver, 1);
 
// Create a irq instance of fthe MyIRQHandler class, and bind it to the low level C ISR. 
BIND_INTERRUPT(btn_irq, MyIRQHandler, handler)  

 

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

#pragma pack(push, 1)
template<typename T>
struct Single_t 
{
    std::array<u8,4> header;                               
    T data;
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

 
template<typename T>
inline auto multicore_span_tx(const std::span<T>& span_data)
{
    auto raw_data = reinterpret_cast<u32>(span_data.data());
    auto len      = static_cast<u32>(span_data.size());
    multicore_fifo_push_blocking(raw_data);
    multicore_fifo_push_blocking(len);

}

template<typename T>
inline auto multicore_span_rx() -> std::span<T>
{
    auto raw_data = multicore_fifo_pop_blocking();
    auto len      = multicore_fifo_pop_blocking();

    return std::span<T>{reinterpret_cast<T*>(raw_data), static_cast<size_t>(len)};
}


void core1_entry() 
{
    btn_irq.enable(true); // enable interrupts on core1!

    // constexpr u32 baudrate = 250'000;     
    constexpr u32 baudrate = 921'600;     
    // 
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{});
    uart.set_baudrate(baudrate);

    static Packet_t<f32, FFT_SIZE/2> magnitude_packet;
    static Single_t<f32> peak_packet;

    peak_packet.header       = {0xAB, 0xBC, 0xCD, 0xDE};
    magnitude_packet.header  = {0xFF, 0xEE, 0xDD, 0xCC};


    constexpr u16 LED_PIN = 15;
    constexpr u16 BTN_PIN1{10};


    pico2_w::Output led_core1{LED_PIN};
    Input button(BTN_PIN1, Pull::Up);
    button.enable_interrupts(GPIO_IRQ_EDGE_FALL);

    CountdownTimer<2> tasks{};
    tasks.add_task_ms(250);
    tasks.add_task_ms(2000);

    constexpr size_t led_task  = 0;
    constexpr size_t switch_task  = 1;
    
    Dma dma{};
    dma.read_from_memory(true);
    dma.write_to_memory(true);
    dma.set_transfer_size<f32>();
    dma.set_data_request(DREQ_FORCE);
 
    volatile UserMode mode = UserMode::Peak;
    ButtonDriver button_driver(btn_irq, button, led_core1, mode);

    while (true)
    {
         
        const UserMode local_mode = mode;
        // u32 raw_mags = multicore_fifo_pop_blocking();
        // auto raw_mags_ptr = reinterpret_cast<const f32*>(raw_mags);
        // if(tasks.is_ready(switch_task))
        //     mode = static_cast<UserMode>(static_cast<u8>(mode) ^ 1);
        

        auto mags = multicore_span_rx<f32>();
        peak_packet.data = mags[0];
        magnitude_packet.copy_sample(mags);
        // dma.config_transfer(magnitude_packet.samples.data(), mags.data(), mags.size());
        // dma.await_blocking();

        uart.dma_tx().await_blocking();
        switch (local_mode)
        {
            case UserMode::Peak:
                uart.write_dma(&peak_packet, sizeof(peak_packet));
                break;
            case UserMode::Spectrum:
                uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
                break;
        }
    }
}

 

int main() 
{
    stdio_init_all();
    // irq_set_enabled(IO_IRQ_BANK0, false);

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
        i2s_irq};

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
        // analyzer.compute_magnitudes();
        analyzer.get_peak<true>();

        // auto raw_samples_ptr = reinterpret_cast<uintptr_t>(analyzer.get_raw_samples().data());
        // auto raw_mag_ptr     = reinterpret_cast<uintptr_t>(analyzer.get_raw_mag().data());

        multicore_span_tx<f32>(analyzer.get_raw_mag());
        // multicore_fifo_push_blocking(raw_samples_ptr);
        // multicore_fifo_push_blocking(raw_mag_ptr);
    }
}

 