

#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
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
    Spectrum = 1,
    DeepSleep = 2,
    Pause = 3,
};

enum class ModeState : u8
{
    Entering = 0,
    Running = 1,
    Leaving = 2,
};

enum class AnalyzerMode : u32
{
    Idle = 0,
    Start = 1,
    Stop = 2,
    MeasurePeak = 3,
    MeasureSpectrum = 4
};


#include "interrupts/input_handlers.hpp"




using interrupts::InputHandler;

class ButtonDriver
{
    Input& m_btn_mode_change;
    Input& m_btn_pause;
    pico2_w::Output& m_led;
    i32 m_debounce_us{};
    absolute_time_t m_last1{get_absolute_time()};
    absolute_time_t m_last2{get_absolute_time()};
    volatile bool m_fired{};
    bool m_is_mode_change{};
public:   
    template<size_t N>
    using IrqHandler = InputHandler<ButtonDriver, N>;

    
    template<size_t N>
    explicit ButtonDriver(IrqHandler<N>& irq, Input& mode_btn, Input& pause_btn, pico2_w::Output& led, i32 debounce_us = 10'000)
        : m_btn_mode_change(mode_btn), m_btn_pause(pause_btn), m_led(led), m_debounce_us{debounce_us}
    {
        irq.bind_driver(this);
        irq.bind_interrupt(&m_btn_mode_change);
        irq.bind_interrupt(&m_btn_pause);
    }
    _ISR_INPUT_CALLBACK void callback_from_isr(u16 pin)
    {
        // assert(pin = m_btn_mode_change.pin());
        auto now  = get_absolute_time(); 

        if(pin == m_btn_mode_change.pin())
        {
            if(absolute_time_diff_us(m_last1, now) > m_debounce_us)
            {
                m_fired = true;
                m_led.toggle();
                m_last1 = now;
                m_is_mode_change = true;

            }
        }

        if(pin == m_btn_pause.pin())
        {
            if(absolute_time_diff_us(m_last2, now) > m_debounce_us)
            {
                m_fired = true;
                // m_led.toggle();
                m_last2 = now;
                m_is_mode_change = false;
            }
        }

    }
    bool is_mode_change_event() const 
    {
        return m_is_mode_change;
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
using MyIRQHandler = INPUT_IRQ_HANDLER(ButtonDriver, 2);
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
        // else if constexpr (std::is_same_v<T, u32>)
        // {
        //     return multicore_fifo_pop_blocking_inline();
        // }
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
    inline void push_timeout_us(const T value, u64 time_out)
    {
        if constexpr (std::is_pointer_v<T>)
        {
            multicore_fifo_push_timeout_us(reinterpret_cast<uintptr_t>(value), time_out);
        } 
        else
        {
            multicore_fifo_push_timeout_us(static_cast<u32>(value), time_out);
        }
    }

    template<typename T>
    inline void pop_timeout_us(T& value, u64 time_out)
    {
        // if constexpr (std::is_pointer_v<T>)
        // {
        //     multicore_fifo_pop_timeout_us(time_out, reinterpret_cast<u32*>(value));
        // } 
        // else
        // {
            multicore_fifo_pop_timeout_us(time_out, static_cast<u32*>(&value));
        // }
    }

    template<typename T>
    inline void push_blocking(const T value)
    {
        static_assert(sizeof(T) <= 4, "Push blocking type must be <= 4 byte");
        if constexpr (std::is_pointer_v<T>)
        {
            multicore_fifo_push_blocking_inline(reinterpret_cast<uintptr_t>(value));
        } 
        // else if constexpr (std::is_same_v<T, u32>)
        // {
        //     multicore_fifo_push_blocking_inline(value);
        // }
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
using voice_analyzer::tiny_dsp::RealFloating32FFT;

#include "queue.hpp"
std::optional<Queue<AnalyzerMode>> s_analyzer_mode_queue{};

 
// std::atomic<bool> core1_sleep_ready(false);


class AtomicFlag
{
    std::atomic<bool> m_flag{false};
public:
    explicit AtomicFlag(bool val = false)
     : m_flag(val)
    {
    }
    inline void signal()
    {
        m_flag.store(true, std::memory_order_release);
    }
    inline void await_signal()
    {
        while(!m_flag.load(std::memory_order_acquire))
            tight_loop_contents();
    }
    inline void acknowledge_signal()
    {
        m_flag.store(false, std::memory_order_release);
    }
};

static AtomicFlag s_core1_state_changed{};

#include "repeating_timers.hpp"

using namespace timers;

void core1_entry() 
{
    i2s_irq.enable(true);

    constexpr i32 LED_BLINK_MS  = 125;
    constexpr u16 CORE0_LED_PIN = 14;
    pico2_w::Output led_core1{CORE0_LED_PIN};

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
        error_led(led_core1);

 
    BasicTimer<Reapating> timer{};

    if(!timer.start_ms(LED_BLINK_MS))
        panic("Could not init timer \n");

  
    analyzer.start(); 


    AnalyzerMode analyzer_mode = AnalyzerMode::Start;

    auto await_command = []{ while(s_analyzer_mode_queue->is_empty())tight_loop_contents();};
    
    await_command();

    while (true) 
    {
        if(!s_analyzer_mode_queue->is_empty())
        {
            // analyzer_mode = s_analyzer_mode_queue->try_remove().value();
            s_analyzer_mode_queue->try_remove(&analyzer_mode);
            multicore::drain();
            s_core1_state_changed.signal();
        }
        if(timer.has_fired())
        {
            timer.acknowledge();
            led_core1.toggle();
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


            case AnalyzerMode::Stop:
                analyzer.stop();
                // // i2s_irq.enable(false);
                // while(!timer.has_fired())   // avoid spurious pending timer interrupts which would disturb the _wfe by waiting and directly cancelling futur ones.
                //     tight_loop_contents();
                // timer.acknowledge();

                // if(!timer.deactivate())
                //     panic("Could not cancel timer");
                 
                led_core1.set_low();
                __wfe();

                analyzer_mode = AnalyzerMode::Idle;
                [[fallthrough]];

            case AnalyzerMode::Idle:
                break;
  
            case AnalyzerMode::Start:

                // if(!timer.start_ms(LED_BLINK_MS))
                //     panic("Could not re-init timer");
                await_command();
                // i2s_irq.enable(true);
                analyzer.start();
                break;

            default:
                break;
        }
    }
}

 
 
#include <pico/low_power.h>


class AppState
{
    UserMode m_mode   = UserMode::Peak;
    ModeState m_state = ModeState::Running;
public:
    AppState() = default;
    inline UserMode user_mode() const {return m_mode;}
    inline ModeState state() const {return m_state;}
    void set_state(UserMode mode, ModeState state)
    {
        m_mode = mode;
        m_state = state;
    }
    void set_to_enter()
    {
        m_state = ModeState::Entering;
    }
    void set_to_run()
    {
        m_state = ModeState::Running;
    }
    void set_to_leave()
    {
        m_state = ModeState::Leaving;
    }
    void transition_to(UserMode new_mode)
    {
        m_mode = new_mode;
        set_to_enter();
    }
};



class StateSynchronizer
{
    Queue<AnalyzerMode>& m_analyzer_mode_queue;
    AtomicFlag& m_core1_state;
public:
    explicit StateSynchronizer(Queue<AnalyzerMode>& queue, AtomicFlag& flag)
     : m_analyzer_mode_queue{queue}, m_core1_state{flag}
    {
    }
    void signal_core1(AnalyzerMode mode)
    {
        m_analyzer_mode_queue.try_add(mode);
    }
    void sev() // send event ARM instruction
    {
        __sev();
    }
    void await_and_acknowledge_core1_state_change()
    {
        m_core1_state.await_signal();
        m_core1_state.acknowledge_signal();
    }
};





class AppStateManager : public StateSynchronizer , public AppState
{
public:
    explicit AppStateManager(Queue<AnalyzerMode>& queue, AtomicFlag& flag)
     : StateSynchronizer(queue, flag)
    {
    }
};



template<std::invocable Fn>
void discard_some(Fn func, size_t N)
{
    for(size_t n = 0; n < N; n++)
        (void)func();
}


class IVoiceAppMode 
{
    public:
    virtual ~IVoiceAppMode() = default;
    virtual void enter() = 0;
    virtual void run() = 0;
    virtual void leave() = 0;
};

class PeakMode final : public IVoiceAppMode
{
public:
    Single_t<f32> peak_packet;
private:
    StateSynchronizer& m_app_state_manager;
    UartNonBlocking<TransferMode::Tx>& m_uart;
    static inline f32 get_peaks()
    {
        return multicore::pop_blocking<f32>();
    } 
public:
    size_t samples_to_discard = 6;
    explicit PeakMode(AppStateManager& state_manager, UartNonBlocking<TransferMode::Tx>& uart, size_t _samples_to_discard = 6)
      : m_app_state_manager{state_manager}, m_uart(uart), samples_to_discard(_samples_to_discard)
    {

    }
    void enter() override
    {
        m_app_state_manager.signal_core1(AnalyzerMode::MeasurePeak);
        m_app_state_manager.await_and_acknowledge_core1_state_change();
        discard_some(get_peaks, samples_to_discard);
    }
    void run() override
    {
        peak_packet.data = get_peaks(); 
        m_uart.dma_tx().await_blocking();                  
        m_uart.write_dma(&peak_packet, sizeof(peak_packet));
    }
    void leave() override
    {
        
    }
};

template<size_t FFT_SIZE>
class MagnitudeMode final : public IVoiceAppMode
{
public:
    Packet_t<f32, FFT_SIZE/2> magnitude_packet;
private:
    StateSynchronizer& m_state_sync;
    UartNonBlocking<TransferMode::Tx>& m_uart;
    static inline const f32* get_mags_ptr()
    {
        return  multicore::pop_blocking<const f32*>();
    } 
public:
    size_t samples_to_discard = 6;
    explicit MagnitudeMode(StateSynchronizer& state_sync, UartNonBlocking<TransferMode::Tx>& uart, size_t _samples_to_discard = 6)
      : m_state_sync{state_sync}, m_uart(uart), samples_to_discard(_samples_to_discard)
    {

    }
    void enter() override
    {
        m_state_sync.signal_core1(AnalyzerMode::MeasureSpectrum);
        m_state_sync.await_and_acknowledge_core1_state_change();
        discard_some(get_mags_ptr, samples_to_discard);
    }
    void run() override
    {
        magnitude_packet.copy_sample(std::span{get_mags_ptr(), FFT_SIZE/2});

        m_uart.dma_tx().await_blocking();
        m_uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
    }
    void leave() override
    {
        
    }
};


class PauseMode final : public IVoiceAppMode
{
private:
    StateSynchronizer& m_state_sync;
    UartNonBlocking<TransferMode::Tx>& m_uart;
 
public:
    explicit PauseMode(StateSynchronizer& state_manager, UartNonBlocking<TransferMode::Tx>& uart)
      : m_state_sync{state_manager}, m_uart(uart) 
    {
    }
    void enter() override
    {
        m_uart.dma_tx().await_blocking();          
        m_uart.wait_blocking_tx(); 
        
        m_state_sync.signal_core1(AnalyzerMode::Stop);
        m_state_sync.await_and_acknowledge_core1_state_change();
    }
    void run() override
    {
        // __wfi();
    }
    void leave() override
    {
        m_state_sync.signal_core1(AnalyzerMode::Start);
        m_state_sync.sev();
        m_state_sync.await_and_acknowledge_core1_state_change();
    }
};



int main() 
{
    stdio_init_all();
    btn_irq.enable(true); 

    s_analyzer_mode_queue = Queue<AnalyzerMode>(8, spin_lock_claim_unused(true));
 
    sleep_ms(10);
 


    AppStateManager app_state_manager(s_analyzer_mode_queue.value(), s_core1_state_changed);
    app_state_manager.set_state(UserMode::Peak, ModeState::Entering); 


    constexpr u32 baudrate = 921'600;      
    // constexpr u32 baudrate = 250'000;     

    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{});
    // auto uart = Uart<IOMode::Blocking, TransferMode::Tx>::make(uart0);
    uart.set_baudrate(baudrate);
    uart.set_hardware_flow_control(true, true);

    // static Packet_t<f32, FFT_SIZE/2> magnitude_packet;
    // static Single_t<f32> peak_packet;

    // peak_packet.header       = {0xAB, 0xBC, 0xCD, 0xDE};
    // magnitude_packet.header  = {0xFF, 0xEE, 0xDD, 0xCC};

    // BasicTimer<Reapating> timer{};
    // if(!timer.init_timer_ms(500))
    //     panic("Could not init timer \n");


    constexpr u16 LED_PIN = 15;
    constexpr u16 BTN_PIN1{10};
    constexpr u16 BTN_PIN2{21};


    pico2_w::Output led_core0{LED_PIN};
    led_core0.set_high();

    Input button_mode_change(BTN_PIN1, Pull::Up);
    button_mode_change.enable_interrupts(GPIO_IRQ_EDGE_FALL);

    Input button_pause(BTN_PIN2, Pull::Up);
    button_pause.enable_interrupts(GPIO_IRQ_EDGE_FALL);


    constexpr i32 debounce_us = 100'000; 
    ButtonDriver button_driver(btn_irq, button_mode_change, button_pause, led_core0, debounce_us);
 
    led_core0.set_high();
 
    UserMode next_mode{app_state_manager.user_mode()};
    multicore_launch_core1(core1_entry);

    constexpr size_t samples_to_discard = 6;
    // auto get_mag_pointers = []() -> const f32* {return multicore::pop_blocking<const f32*>();};
    // auto get_freq_peaks   = []() -> f32 {return multicore::pop_blocking<f32>();};

    // PeakPrinter peak_printer{3};
    // MagPrinter mag_printer{3};

    static PauseMode pause_mode{app_state_manager, uart};
    static PeakMode peak_mode{app_state_manager, uart, samples_to_discard};
    static MagnitudeMode<FFT_SIZE> mag_mode{app_state_manager, uart, samples_to_discard};
    
    peak_mode.peak_packet.header      = {0xAB, 0xBC, 0xCD, 0xDE};
    mag_mode.magnitude_packet.header  = {0xFF, 0xEE, 0xDD, 0xCC};

    IVoiceAppMode* app_mode_ptr = &peak_mode; 
    app_mode_ptr->enter();
    while (true)    
    {
        if(button_driver.has_fired())
        {
            button_driver.acknowledge();
            static IVoiceAppMode* last_mode_ptr{app_mode_ptr};
            if(app_mode_ptr != &pause_mode)
                last_mode_ptr = app_mode_ptr;

            app_mode_ptr->leave();
            if(button_driver.is_mode_change_event())
            {
                app_mode_ptr = (last_mode_ptr == &peak_mode)? static_cast<IVoiceAppMode*>(&mag_mode) : &peak_mode;
            }
            else
            {
                app_mode_ptr = (app_mode_ptr == &pause_mode)? last_mode_ptr : &pause_mode;
            }
            app_mode_ptr->enter();


            // static UserMode last_mode{};
            // if(app_state_manager.user_mode() != UserMode::Pause)
            //     last_mode = app_state_manager.user_mode();

            // if(button_driver.is_mode_change_event())
            // {
            //     next_mode = (last_mode == UserMode::Peak)? UserMode::Spectrum : UserMode::Peak;
            // }
            // else
            // {
            //     next_mode = (app_state_manager.user_mode() == UserMode::Pause)? last_mode : UserMode::Pause;
            // }
            // app_state_manager.set_to_leave();
        }
        app_mode_ptr->run();


        // switch (app_state_manager.user_mode())
        // {
        //     case UserMode::Peak:
        //         switch (app_state_manager.state())
        //         {
        //             case ModeState::Entering:
        //                 app_state_manager.signal_core1(AnalyzerMode::MeasurePeak);
        //                 app_state_manager.await_and_acknowledge_core1_state_change();
        //                 app_state_manager.set_to_run();
        //                 discard_some(get_freq_peaks, samples_to_discard);
        //                 [[fallthrough]];
        //             case ModeState::Running:
        //                 // peak_printer.print_some(get_freq_peaks());
        //                 peak_packet.data = get_freq_peaks(); 
        //                 uart.dma_tx().await_blocking();                  
        //                 uart.write_dma(&peak_packet, sizeof(peak_packet));
        //                 break;
        //             case ModeState::Leaving:
        //                 app_state_manager.transition_to(next_mode);
        //                 break;
        //         }
        //         break;

        //     case UserMode::Spectrum:
        //         switch (app_state_manager.state())
        //         {
        //             case ModeState::Entering:
        //                 app_state_manager.signal_core1(AnalyzerMode::MeasureSpectrum);
        //                 app_state_manager.await_and_acknowledge_core1_state_change();
        //                 app_state_manager.set_to_run();
        //                 discard_some(get_mag_pointers, samples_to_discard);
        //                 [[fallthrough]];
        //             case ModeState::Running:
        //             // { 
        //                 // To be replaced with SPI - LCD stuff!
        //                 // dispatch data via UART to PySerial 
 
        //                 // auto raw_mags_ptr = get_mag_pointers();
        //                 // std::span<const f32> magnitudes{raw_mags_ptr, FFT_SIZE/2};
        //                 // mag_printer.print_some(MagPrinter::mean(std::span{get_mag_pointers(), FFT_SIZE/2}));
        //                 magnitude_packet.copy_sample(std::span{get_mag_pointers(), FFT_SIZE/2});

        //                 uart.dma_tx().await_blocking();
        //                 uart.write_dma(&magnitude_packet, sizeof(magnitude_packet));
        //                 // // }
        //                 break;
        //             case ModeState::Leaving:
        //                 app_state_manager.transition_to(next_mode); 
        //                 break;
        //         }
        //         break;
            

        //     case UserMode::Pause:
        //         switch (app_state_manager.state())
        //         {
        //             case ModeState::Entering:
        //             {
        //                 uart.dma_tx().await_blocking();          
        //                 uart.wait_blocking_tx(); 
                        
        //                 app_state_manager.signal_core1(AnalyzerMode::Stop);
        //                 app_state_manager.await_and_acknowledge_core1_state_change();
        //                 app_state_manager.set_to_run();
        //                 // printf("Pause! \n");
        //             }
        //             [[fallthrough]];
                    
        //             case ModeState::Running:
        //             { 
        //                 __wfi();
        //             }
        //             break;

        //             case ModeState::Leaving:
        //             {
        //                 app_state_manager.signal_core1(AnalyzerMode::Start);
        //                 app_state_manager.sev();
        //                 app_state_manager.await_and_acknowledge_core1_state_change();
        //                 app_state_manager.transition_to(next_mode);
        //             }
        //             break;
        //         }
        //         break;
            

        //     default:
        //         break;

        // }
    }
}