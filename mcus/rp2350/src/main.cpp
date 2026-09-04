
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

#include "arm_math.h"

#include "i2s.pio.h"


 
 
 
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
        m_is_ready = true;
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
};

 

void convert(std::span<f32> out, std::span<u32> in)
{
    assert(2 * out.size() == in.size());

    for (size_t i = 0; i < out.size(); i++)
    {
        // raw[2*i] is the Left channel sample (24-bit sign-extended inside the 32-bit word, scaled by 256)
        i32 signed_sample = (i32)in[2 * i];
        // Convert to float and normalize to [-1.0, 1.0] by dividing by 2^31 (2147483648.0)
        out[i] = (float)signed_sample / 2147483648.0f; 
    }
}
 


constexpr size_t FFT_SIZE   =  2048;
constexpr size_t BUFF_SIZE  =  2*FFT_SIZE;    // LEFT + RIGHT Channel, we only keep one left in the end

static std::array<u32, BUFF_SIZE> PING{};
static std::array<u32, BUFF_SIZE> PONG{};
static std::array<f32, FFT_SIZE>  SAMPLES{};

constexpr size_t FLOAT_MAX_SAMPLE = FFT_SIZE;

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

 
static std::array<f32, FFT_SIZE> WINDOW_COEFFS{};
static std::array<f32, FFT_SIZE> FFT_RESULT{};
static std::array<f32, FFT_SIZE/2> MAGNITUDES{};

class RealFFT
{
    arm_rfft_fast_instance_f32 m_inst{};
    size_t m_fft_size{};
public:
    explicit RealFFT(size_t fft_size)
        : m_fft_size(fft_size)
    {
    }
    arm_status init()
    {
        return arm_rfft_fast_init_f32(&m_inst, static_cast<u16>(m_fft_size));
    }
    inline void window_f32_inplace(std::span<f32> src, std::span<const f32> window_coeffs)
    {
        arm_mult_f32(src.data(), window_coeffs.data(), src.data(), m_fft_size);
    }
    void perform_fft(std::span<f32> samples, std::span<f32> fft_out)
    {
        arm_rfft_fast_f32(&m_inst, samples.data(), fft_out.data(), 0);
    }
    void perform_inverse_fft(std::span<f32> fourier_in, std::span<f32> time_out)
    {
        arm_rfft_fast_f32(&m_inst, fourier_in.data(), time_out.data(), 1);
    }
    void compute_magnitude_squared(std::span<const f32> fft_src, std::span<f32> mag_out)
    {
        mag_out[0] = fabsf(fft_src[0]);
        arm_cmplx_mag_squared_f32(fft_src.data() + 2, mag_out.data() + 1, m_fft_size / 2 - 1); // Faster because it does not compute the square root!
    }
    void compute_magnitude(std::span<const f32> fft_src, std::span<f32> mag_out)
    {
        mag_out[0] = fabsf(fft_src[0]);
        arm_cmplx_mag_f32(fft_src.data() + 2, mag_out.data() + 1, m_fft_size / 2 - 1); // Faster because it does not compute the square root!
    }
    auto find_max(std::span<const f32> data, u32 search_len) -> std::pair<u32, f32>
    {
        f32 max_val = 0.0f;
        u32 idx     = 0;
        arm_max_f32(data.data(), search_len, &max_val, &idx);
        return std::pair(idx, max_val);
    }
    void hann(std::span<f32> window)
    {
        for (size_t n = 0; n < window.size(); n++) 
        {
            window[n] = 0.5f * (1.0f - cosf(2.0f * M_PI * (f32)n / (f32)(m_fft_size - 1)));
        }
    }
    void convert_mono_i2s_u32_to_f32(std::span<u32> raw_i2s_in, std::span<f32> out, const size_t right = 0) // right = 1 for other channel
    {
        assert(2 * out.size() == raw_i2s_in.size());

        for (size_t i = 0; i < out.size(); i++)
        {
            // raw[2*i] is the Left channel sample (24-bit sign-extended inside the 32-bit word, scaled by 256)
            auto signed_sample = static_cast<i32>(raw_i2s_in[2 * i + right]);
            // Convert to float and normalize to [-1.0, 1.0] by dividing by 2^31 (2147483648.0)
            out[i] = (float)signed_sample / 2147483648.0f; 
        }
    }
    
};

static RealFFT dsp{FFT_SIZE};

 
using hardware::Uart;

int main() 
{
    stdio_init_all();

    PioI2sConfig config
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
        irqs};

    Dma dma{};  
    // constexpr u32 baudrate = 921600;
    constexpr u32 baudrate = 115200;
    constexpr bool is_tx   = true;
    
    // auto uart = hardware::Uart::new_tx_dma(Dma{}, uart0, baudrate);
    
    auto uart = Uart::make_tx_only(Dma{}, uart0);

    uart.set_baudrate(baudrate);

    if(dsp.init() != ARM_MATH_SUCCESS)
        return -1;

 

    Packet_t tx_packet;
    tx_packet.header[0] = 0xAA;
    tx_packet.header[1] = 0xBB;

    // std::array<float, FLOAT_MAX_SAMPLE> rx_buff{};


 
    // // Dummy loop variables
    // float counter = 0.0f;
    // std::array<float, FLOAT_MAX_SAMPLE> rx_buff{};
    // char hello[] = "hello, world?!\n";

    std::span<u32> buffer{};

    i2s.start(); 

    u32 count = 0;

    float bin_resolution = (float)config.sample_rate/FFT_SIZE;
    u32 k_min = 50 / bin_resolution;
    u32 k_max = 1000 / bin_resolution;

    float search_max_val = 0.0f;
    // uint32_t peak_idx = 0;
    uint32_t search_len = k_max - k_min + 1;

    const char* h = "hello \n";
    while (true) 
    {
        // i2s.read_blocking(buffer, irqs.ready_flag());

        // dsp.convert_mono_i2s_u32_to_f32(buffer, SAMPLES);
        // dsp.window_f32_inplace(SAMPLES, WINDOW_COEFFS);
        // dsp.perform_fft(SAMPLES, FFT_RESULT);
        // dsp.compute_magnitude_squared(FFT_RESULT, MAGNITUDES); 

        // auto [peak_idx, _] = dsp.find_max(std::span{MAGNITUDES}.subspan(k_min), search_len);

        // u32 peak_bin = k_min + peak_idx;
        // f32 est_freq = ((float)peak_bin) * bin_resolution;

        // // printf("Got: %f Hz +/- %f\n", est_freq, bin_resolution);
        // // // Populate payload  
        // // for(auto& value : SAMPLES)
        // // {
        // //     value = 1.0f - 2*(float)rand() / RAND_MAX;
        // // }
        // // constexpr u32 factor = 1000;
        // // // std::ranges::for_each(SAMPLES, [factor](float x) { x *= factor; });

        tx_packet.swap_sample(SAMPLES); 
        // printf("Got: %f, %f, %f \n", tx_packet.samples[0], tx_packet.samples[1], tx_packet.samples[2]);

        // if(buffer[0] || buffer[2])
        // {
        //     int32_t signed_sample = (int32_t)buffer[0];
        //     auto signed_f         = (float)signed_sample / 2147483648.0f; 
        //     // dma.config_transfer(uart.data_register(), &tx_packet, sizeof(tx_packet), true);
        //     printf("%lu; %d; %f \n", buffer[0], signed_sample, signed_f);
        // }
        uart.send_dma(h, strlen(h) + 1);
        
        // // tx_packet.swap_sample(SAMPLES);
        // if(uart.is_writable() && (count % 10 == 0) && !dma.is_busy())
        // if(dma.is_busy())
 

            // uart.write_blocking(&tx_packet);

        sleep_ms(1000); // 10Hz transmission rate
    }
}

 