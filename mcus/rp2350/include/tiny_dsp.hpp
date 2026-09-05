#ifndef TINY_DSP_HPP
#define TINY_DSP_HPP

#include "arm_math.h"
#include "types.hpp"
#include <cassert>
#include <span>

namespace tiny_dsp 
{

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


}
#endif 