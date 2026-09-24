#ifndef TINY_DSP_HPP
#define TINY_DSP_HPP

#include "arm_math.h"
#include "arm_math_types.h"
#include "dsp/basic_math_functions.h"
#include "dsp/complex_math_functions.h"
#include "dsp/statistics_functions.h"
#include "dsp/transform_functions.h"
#include <cassert>
#include <span>
#include <stdint.h>

namespace voice_analyzer
{
    using u16 = uint16_t;
    using u32 = uint32_t;


    using i16 = int16_t;
    using i32 = int32_t;
    using f32 = float;


 

namespace tiny_dsp
{

    class RealFloating32FFT
    {
        arm_rfft_fast_instance_f32 m_inst{};
        size_t m_fft_size{};
    public:
        using fourier_type = f32;

        explicit RealFloating32FFT(size_t fft_size)
            : m_fft_size(fft_size)
        {
        }
        bool init()
        {
            return arm_rfft_fast_init_f32(&m_inst, static_cast<u16>(m_fft_size)) == ARM_MATH_SUCCESS;
        }
        inline void window_inplace(std::span<f32> src, std::span<const f32> window_coeffs)
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
            mag_out[0]           = fabsf(fft_src[0]);
            mag_out[m_fft_size/2] = fabsf(fft_src[1]); 
            
            // fft_src[0]: The DC component (Frequency bin 0), which is entirely real.
            // fft_src[1]: The Nyquist component (Frequency bin \(N/2\)), which is also entirely real.
            arm_cmplx_mag_squared_f32(fft_src.data() + 2, mag_out.data() + 1, m_fft_size / 2 - 1); // Faster because it does not compute the square root!
        }
        void compute_magnitude(std::span<const f32> fft_src, std::span<f32> mag_out)
        {
            mag_out[0]            = fabsf(fft_src[0]);
            mag_out[m_fft_size/2] = fabsf(fft_src[1]); 
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
                window[n] = 0.5f * (1.0f - arm_cos_f32(2.0f * M_PI * (f32)n / (f32)(m_fft_size - 1)));
            }
        }
        void convert_raw_i2s_to_mono(std::span<u32> raw_i2s_in, std::span<f32> out, const size_t right = 0) // right = 1 for other channel
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



    class RealFFTQ15
    {
        arm_rfft_instance_q15 m_inst{};
        size_t m_fft_size{};
    public:
        using fourier_type = q15_t;

        explicit RealFFTQ15(size_t fft_size)
            : m_fft_size(fft_size)
        {
        }
        bool init()
        {
            return arm_rfft_init_q15(&m_inst, static_cast<u32>(m_fft_size), 0, 1) == ARM_MATH_SUCCESS;
        }
        inline void window_inplace(std::span<q15_t> src, std::span<const q15_t> window_coeffs)
        {
            arm_mult_q15(src.data(), window_coeffs.data(), src.data(), m_fft_size);
        }
        void perform_fft(std::span<q15_t> samples, std::span<q15_t> fft_complex_out)
        {
            assert(fft_complex_out.size() == 2 * samples.size()); // we store complex & real! size = 2 * m_fft_size. 
            arm_rfft_q15(&m_inst, samples.data(), fft_complex_out.data());
        }
        void compute_magnitude_squared(std::span<const q15_t> fft_src, std::span<q15_t> mag_out)
        {
            arm_cmplx_mag_squared_q15(fft_src.data(), mag_out.data(), m_fft_size / 2); // Faster because it does not compute the square root!
        }
        void compute_magnitude(std::span<const q15_t> fft_src, std::span<q15_t> mag_out)
        {
            arm_cmplx_mag_q15(fft_src.data(), mag_out.data(), m_fft_size / 2); // Faster because it does not compute the square root!
        }
        auto find_max(std::span<const q15_t> data, u32 search_len) -> std::pair<u32, q15_t>
        {
            q15_t max_val = 0;
            u32 idx       = 0;
            arm_max_q15(data.data(), search_len, &max_val, &idx);
            return std::pair(idx, max_val);
        }
        void hann(std::span<q15_t> window)
        {
            for (size_t n = 0; n < window.size(); n++) 
            {
                f32 h = 0.5f * (1.0f - cosf(2.0f * M_PI * (f32)n / (f32)(m_fft_size - 1)));
                arm_float_to_q15(&h, &window[n], 1);
            }
        }
        void convert_raw_i2s_to_mono(std::span<q15_t> raw_i2s_in, std::span<q15_t> out, const size_t right = 0) // right = 1 for other channel
        {
            assert(2 * out.size() == raw_i2s_in.size());

            for (size_t i = 0; i < out.size(); i++)
            {
                // raw[2*i] is the Left channel sample (24-bit sign-extended inside the 32-bit word, scaled by 256)
                raw_i2s_in[i] = raw_i2s_in[2 * i + right];
            }
        }
        
    };
} // tiny_dsp

} // voice_analyzer
#endif 