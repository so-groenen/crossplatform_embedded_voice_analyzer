

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/dreq.h>
#include <hardware/regs/intctrl.h>
// #include <pico/cyw43_arch.h>
#include <hardware/sync.h>
#include <optional>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
// #include "arm_math_types.h"
#include "arm_math_types.h"
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
#include <type_traits>

#include "drivers/dma.hpp"

#include "dsp/basic_math_functions.h"
#include "dsp/filtering_functions.h"
#include "dsp/support_functions.h"
#include "embedded_hal/heapless.hpp"
#include "embedded_hal/traits.hpp"
#include "stats.hpp"
#include "drivers/countdown_timer.hpp"
#include "drivers/dma.hpp"
#include "tiny_dsp.hpp"
#include "voice_analyzer.hpp"
#include "pico/multicore.h"
#include "types.hpp"
#include "drivers/i2s/i2s_mic.hpp"
#include "drivers/i2s/sph0645_mic.hpp"
#include "interrupts/async_dma_handlers.hpp"


 
 
constexpr size_t N_channel = 1;
 

using I2sMicrophone = SPH0645Mic<PioSph0645Program>;
using I2sIrqHandler = interrupts::DmaHandlerWithCallback<DMA_IRQ_0, N_channel, I2sMicrophone>;


// bind interrupt handler class to interrupt service routine.
BIND_INTERRUPT(i2s_irq, I2sIrqHandler, i2s_handler);
 
 
constexpr size_t FFT_SIZE   = 1024;
constexpr size_t BUFF_SIZE  = 2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end
constexpr u32 SAMPLE_RATE   = 20'000;

static std::array<u32, BUFF_SIZE> PING{};       // raw i2s data, holding left and right
static std::array<u32, BUFF_SIZE> PONG{};       // raw i2s data, holding left and right 
static std::array<q7_t,  FFT_SIZE/2> MAG_Q7{};       // raw i2s data, holding left and right

constexpr size_t FLOAT_MAX_SAMPLE = FFT_SIZE;
constexpr u32 DMA_CH0             = 0;
constexpr u32 DMA_CH1             = 1;


enum class UserMode : u8
{
    Peak = 0,
    Spectrum = 1
};

enum class ModeState : u8
{
    Entering = 0,
    Running = 1,
    Leaving = 2,
};

enum class AnalyzerMode : u32
{
    Start = 0,
    Stop = 1,
    MeasurePeak = 2,
    MeasureSpectrum = 3
};


#include "interrupts/input_handlers.hpp"




using interrupts::InputHandler;

class ButtonDriver
{
    Input& m_btn1;
    pico2_w::Output& m_led;
    i32 m_debounce_us{10'000};
    absolute_time_t m_last1{get_absolute_time()};
    volatile bool m_fired{};
public:   
    template<size_t N>
    using IrqHandler = InputHandler<ButtonDriver, N>;

    
    template<size_t N>
    explicit ButtonDriver(IrqHandler<N>& irq, Input& bt1, pico2_w::Output& led)
        : m_btn1(bt1), m_led(led) 
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
            m_fired = true;
            m_led.toggle();
        }
    }
    inline bool has_fired() const 
    {
        return m_fired;
    }
    inline void acknowledge()
    {
        m_fired = false;
    }

};

 
// Define the MyIRQHandler class which will call the ButtonDriver callback from within the ISR, and register 1 button
using MyIRQHandler = INPUT_IRQ_HANDLER(ButtonDriver, 1);
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

  
namespace multicore 
{
    template<typename T>
    inline auto pop_blocking() -> T
    {
        static_assert(sizeof(T) <= 4, "pop_blocking type must be <= 4 byte");
        if constexpr (std::is_pointer_v<T>)
        {
            return reinterpret_cast<T>(multicore_fifo_pop_blocking_inline());
        }
        else if constexpr (std::is_same_v<T, u32>)
        {
            return multicore_fifo_pop_blocking_inline();
        }
        // else if constexpr (std::is_floating_point_v<T>) 
        // {
        //     u32 raw_val = multicore_fifo_pop_blocking();
        //     T dest      = 0.f;
        //     std::memcpy(&dest, &raw_val, sizeof(f32));
        //     return dest;
        // }
        else 
        {
            return static_cast<T>(multicore_fifo_pop_blocking_inline());
        }  
    }

    template<typename T>
    inline void push_blocking(const T value)
    {
        static_assert(sizeof(T) <= 4, "Push blocking type must be <= 4 byte");
        if constexpr (std::is_pointer_v<T>)
        {
            multicore_fifo_push_blocking_inline(reinterpret_cast<uintptr_t>(value));
        } 
        else if constexpr (std::is_same_v<T, u32>)
        {
            multicore_fifo_push_blocking_inline(value);
        }
        // else if constexpr (std::is_floating_point_v<T>) 
        // {
        //     const T val = value;
        //     u32 dest    = 0;
        //     std::memcpy(&dest, &val, sizeof(u32));
        //     multicore_fifo_push_blocking(dest);
        // }
        else
        {
            // const auto val = std::bit_cast<u32>(value);
            // multicore_fifo_push_blocking(val);
            multicore_fifo_push_blocking_inline(static_cast<u32>(value));
        }
    }

    inline bool rvalid() {return multicore_fifo_rvalid();};
    inline bool wready() {return multicore_fifo_wready();};
    inline void drain() {multicore_fifo_drain();};
}
 
#include "queue.hpp"
std::optional<Queue<AnalyzerMode>> analyzer_mode_queue{};

 


  
void core1_entry() 
{
    btn_irq.enable(true); // enable interrupts on core1!

    // constexpr u32 baudrate = 460'800;     
    constexpr u32 baudrate = 921'600;     
    
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{});
    uart.set_baudrate(baudrate);
    uart.set_hardware_flow_control(true, true);

    static Packet_t<f32, FFT_SIZE/2> magnitude_packet;
    static Single_t<f32> peak_packet;

    peak_packet.header       = {0xAB, 0xBC, 0xCD, 0xDE};
    magnitude_packet.header  = {0xFF, 0xEE, 0xDD, 0xCC};


    constexpr u16 LED_PIN = 15;
    constexpr u16 BTN_PIN1{10};


    pico2_w::Output led_core1{LED_PIN};
    led_core1.set_high();

    Input button(BTN_PIN1, Pull::Up);
    button.enable_interrupts(GPIO_IRQ_EDGE_FALL);

    CountdownTimer<1> tasks{};
    tasks.add_task_ms(250);
 
    constexpr size_t led_task  = 0;
    // constexpr size_t switch_task  = 1;
    
    // Dma dma{};
    // dma.read_from_memory(true);
    // dma.write_to_memory(true);
    // dma.set_transfer_size<f32>();
    // dma.set_data_request(DREQ_FORCE);
 
    ButtonDriver button_driver(btn_irq, button, led_core1);
 

    ModeState mode_state = ModeState::Running;
    UserMode user_mode   = UserMode::Peak;


    multicore::push_blocking(AnalyzerMode::MeasurePeak);
    led_core1.set_high();

    CountdownTimer<1> timer{};
    timer.add_task_ms(250);

    static AnalyzerMode analyzer_mode = AnalyzerMode::MeasurePeak;

    while (true)
    {
        if(button_driver.has_fired())
        {
            button_driver.acknowledge();
            mode_state = ModeState::Leaving;
        }


        switch (user_mode)
        {
            case UserMode::Peak:
                switch (mode_state)
                {
                    case ModeState::Entering:
                    {
                        analyzer_mode = AnalyzerMode::MeasurePeak;
                        analyzer_mode_queue->try_add(analyzer_mode);
                        multicore_fifo_drain();
                        mode_state = ModeState::Running;
                    }
                    case ModeState::Running:
                    {
                        peak_packet.data = multicore::pop_blocking<f32>();
                        
                        uart.dma_tx().await_blocking();                  
                        uart.write_dma(&peak_packet, sizeof(peak_packet));
                    }
                    break;
                    case ModeState::Leaving:
                    {
                        user_mode  = UserMode::Spectrum;
                        mode_state = ModeState::Entering;
                    }
                    break;
                }
                break;

            case UserMode::Spectrum:
                switch (mode_state)
                {
                    case ModeState::Entering:
                    {
                        analyzer_mode = AnalyzerMode::MeasureSpectrum;
                        analyzer_mode_queue->try_add(analyzer_mode);
                        mode_state = ModeState::Running;
                        multicore_fifo_drain();
                    }
                    case ModeState::Running:
                    { 
                        auto raw_mags_ptr = multicore::pop_blocking<const f32*>();
                        std::span<const f32> magnitudes{raw_mags_ptr, FFT_SIZE/2};

                        magnitude_packet.copy_sample(magnitudes);

                        uart.dma_tx().await_blocking();
                        uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
                    }
                    break;
                    case ModeState::Leaving:
                    {
                        user_mode  = UserMode::Peak;
                        mode_state = ModeState::Entering;
                    }
                    break;
                }
                break;
        }
    }
}

using voice_analyzer::tiny_dsp::RealFloating32FFT;

int main() 
{
    stdio_init_all();
    btn_irq.enable(true); 

    sleep_ms(25);
    multicore_reset_core1();
    sleep_ms(25);


    analyzer_mode_queue = Queue<AnalyzerMode>(2, spin_lock_claim_unused(true));


    constexpr u16 CORE0_LED_PIN = 14;
    pico2_w::Output led_core0{CORE0_LED_PIN};

    constexpr Sph0645Config config
    {
        .sample_rate = SAMPLE_RATE, // Word Select frequency: 
        .sck_pin     = 16,          // sck is 32 bit for left and 32bit for right, therefor serial clock is 64 * 16k
        .ws_pin      = 17,          // we clock in Left & right samples 16k per secondes 
        .data_pin    = 18,          // bit sampled at serial clock rate, but only process left (Word Select low)
    };
     
    I2sMicrophone i2s{
        PioSph0645Program{config},
        PING,
        PONG,
        Dma{},
        i2s_irq};

    VOICE_ANALYZER_I2S_ASSERT(I2sMicrophone, u32);
 
    VoiceAnalyzerConfig<FFT_SIZE> analyzer_config = 
    {
        .min_freq        = 50,
        .max_freq        = 1000,
        .noise_threshold = 0.0f
    };

    static RealFloating32FFT dsp{FFT_SIZE};
    static VoiceAnalyzer analyzer{i2s, dsp, analyzer_config};
 
    if(!analyzer.init())
        error_led(led_core0);

 
    CountdownTimer<1> timer{};
    timer.add_task_ms(250);
 
    constexpr size_t led_task  = 0;   
    multicore_launch_core1(core1_entry);
 
    analyzer.start(); 
    AnalyzerMode analyzer_mode = AnalyzerMode::MeasurePeak;


    analyzer_mode = multicore::pop_blocking<AnalyzerMode>();
    while (true) 
    {
        if(timer.is_ready(led_task))
            led_core0.toggle();


        if(!analyzer_mode_queue->is_empty())
        {
            (void)analyzer_mode_queue->try_remove(analyzer_mode);
        }

 
        switch (analyzer_mode)
        {
            case AnalyzerMode::MeasurePeak:
            {
                f32 peak = analyzer.get_peak<true>();
                multicore::push_blocking(peak);
            }
            break;
            
            case AnalyzerMode::MeasureSpectrum:
            {
                analyzer.compute_magnitudes();
                multicore::push_blocking(analyzer.get_raw_mag().data());
            }
            break;

            // case AnalyzerMode::Stop:
            //     analyzer.stop();
            //     multicore_fifo_pop_blocking();
            // case AnalyzerMode::Start:
            //     analyzer.start();
                // break;
            default:
                break;
        }
    }
}

 