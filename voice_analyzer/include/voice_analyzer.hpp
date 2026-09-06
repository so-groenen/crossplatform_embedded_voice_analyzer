#ifndef VOICE_ANALYZER_HPP
#define VOICE_ANALYZER_HPP


#include "tiny_dsp.hpp"

namespace voice_analyzer
{

template<typename T>
concept IsI2sMic = requires(T t, std::span<u32>& buff)
{
    {t.start()}             -> std::same_as<void>;
    {t.stop()}              -> std::same_as<void>;
    {t.read_blocking(buff)} -> std::same_as<void>;
    {t.get_sample_rate()}   -> std::same_as<u32>;
};

#define VOICE_ANALYZER_I2S_ASSERT(X) static_assert(voice_analyzer::IsI2sMic<X>,"Voice analyzer - I2S must implement: start(), stop(), read_blocking(spanU32), U32 get_sample_rate().");

template<size_t FFT_N>
struct VoiceAnalyzerConfig
{
    // u32 sample_rate = 20'000;
    u32 min_freq          = 50; 
    u32 max_freq          = 1000;
    float noise_threshold = 1.0f;
};



template<size_t FFT_N, class I2sDriver> 
requires IsI2sMic<I2sDriver> 
class VoiceAnalyzer
{
    std::array<f32, FFT_N> m_float_samples{};    // i2s float, holding only left from either ping or pong
    std::array<f32, FFT_N> m_window_coeffs{};    // Hann window
    std::array<f32, FFT_N> m_fft_res{};          // fft values
    std::array<f32, FFT_N/2> m_mags{};           // fft magnitudes

    std::span<u32> raw_buff{};

    I2sDriver& m_i2s;
    tiny_dsp::RealFFT m_dsp{FFT_N};

    float bin_resolution{};
    u32 k_min{};
    u32 k_max{};
    float noise_threshold{};
    inline void acquire_sample()
    {
        m_i2s.read_blocking(raw_buff);
    }
    inline void compute_fft()
    {
        m_dsp.convert_mono_i2s_u32_to_f32(raw_buff, m_float_samples);
        m_dsp.window_f32_inplace(m_float_samples, m_window_coeffs);
        m_dsp.perform_fft(m_float_samples, m_fft_res);
    }
    // inline void compute_magnitudes(bool squared = true)
    // {
    //     compute_fft();
    //     if(squared)
    //         m_dsp.compute_magnitude_squared(m_fft_res, m_mags); 
    //     else  
    //         m_dsp.compute_magnitude(m_fft_res, m_mags); 
    // }
public:
    VoiceAnalyzer(I2sDriver& i2s, VoiceAnalyzerConfig<FFT_N>& config)
      : m_i2s{i2s}, 
        bin_resolution{i2s.get_sample_rate()/(float)FFT_N}, 
        k_min{config.min_freq/(u32)bin_resolution},  
        k_max{config.max_freq/(u32)bin_resolution},
        noise_threshold{config.noise_threshold}
    {
    }
    bool init()
    {
        if(m_dsp.init() != ARM_MATH_SUCCESS)
            return false;

        m_dsp.hann(m_window_coeffs);
        return true;
    }
    void start()
    {
        m_i2s.start(); 
    }
    void stop()
    {
        m_i2s.start(); 
    }
    inline auto get_raw_mag() -> std::array<f32, FFT_N/2>&
    {
        return m_mags;
    }
    inline auto get_raw_samples() -> std::array<f32, FFT_N>&
    {
        return m_float_samples;
    }
    inline auto get_raw_mag_view() -> std::span<f32>
    {
        return m_mags;
    }
    inline auto get_raw_samples_view() -> std::span<f32>
    {
        return m_float_samples;
    }

    inline auto get_magnitudes_view() -> std::span<const f32>
    {
        // compute_magnitudes(squared);
        return std::span{m_mags}.subspan(k_min, k_max - k_min);
    }
    inline void compute_magnitudes(bool squared = true)
    {
        acquire_sample();
        compute_fft();
        if(squared)
            m_dsp.compute_magnitude_squared(m_fft_res, m_mags); 
        else  
            m_dsp.compute_magnitude(m_fft_res, m_mags); 
    }
    template<bool quad_interpol>
    float get_peak()
    {
        compute_magnitudes();

        static const u32 search_len = k_max - k_min + 1;
        auto [peak_idx, max_value]  = m_dsp.find_max(std::span{m_mags}.subspan(k_min), search_len);
        if(max_value < noise_threshold)
            return 0.0f;

        u32 peak_bin = k_min + peak_idx;


        f32 d = 0.0f;
        if constexpr (quad_interpol)
        {
            if (peak_bin > k_min && peak_bin < k_max)
            {
                f32 y1 = m_mags[peak_bin - 1];
                f32 y2 = m_mags[peak_bin];
                f32 y3 = m_mags[peak_bin + 1];
                
                f32 denom = y1 - 2.0f * y2 + y3;
                if (fabsf(denom) > 1e-5f) {
                    d = 0.5f * (y1 - y3) / denom;
                }
            }
        }

        f32 est_freq = (static_cast<f32>(peak_bin) + d) * bin_resolution;
        return est_freq;
    }
};
}
#endif //VOICE_ANALYZER_HPP