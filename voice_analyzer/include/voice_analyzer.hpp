#ifndef VOICE_ANALYZER_HPP
#define VOICE_ANALYZER_HPP


#include "arm_math_types.h"
#include "tiny_dsp.hpp"
#include <concepts>
#include <iterator>

namespace voice_analyzer
{
 

template<typename I2s, typename  RawSampleType>
concept IsI2sMic = requires (I2s mic, std::span<RawSampleType>& buff) 
{
    typename I2s::sample_type;

    {mic.start()}             -> std::same_as<void>;
    {mic.stop()}              -> std::same_as<void>;
    {mic.read_blocking(buff)} -> std::same_as<void>;
    {mic.get_sample_rate()}   -> std::same_as<u32>;
};


#define VOICE_ANALYZER_REGISTER_I2S_TYPE(X) using sample_type = X;


#define VOICE_ANALYZER_I2S_ASSERT(X, T) static_assert(voice_analyzer::IsI2sMic<X, T>,"Voice analyzer - I2S must implement: start(), stop(), read_blocking(spanU32), U32 get_sample_rate().");
 

template<typename Dsp, typename SampleType, typename DspType>
concept IsValidDsp = requires(Dsp dsp, std::span<SampleType> samples, std::span<DspType> in, std::span<DspType> out,  std::span<const DspType> const_src, const size_t right)
{
    typename Dsp::fourier_type;

    {dsp.init()}                                -> std::same_as<bool>;             
    {dsp.perform_fft(in, out)}                  -> std::same_as<void>;
    {dsp.window_inplace(in, const_src)}         -> std::same_as<void>;  
    {dsp.convert_raw_i2s_to_mono(samples, out, right)}-> std::same_as<void>; 
    {dsp.compute_magnitude_squared(const_src, out)}   -> std::same_as<void>;
    {dsp.compute_magnitude(const_src, out)}           -> std::same_as<void>;
};
#define VOICE_ANALYZER_REGISTER_DSP(X) using fourier_type = X;


template<typename T>
concept IsDspFloating32 = IsValidDsp<T, u32, f32>;

template<typename T>
concept IsDspFixedQ15 = IsValidDsp<T, i16, q15_t>;

#define VOICE_ANALYZER_FLOATING32_DSP_ASSERT(X) static_assert(voice_analyzer::IsDspFloating32<X>,"Voice analyzer - IsF32dsp.");
#define VOICE_ANALYZER_Q15_DSP_ASSERT(X) static_assert(voice_analyzer::IsDspFixedQ15<X>,"Voice analyzer - IsF32dsp.");

VOICE_ANALYZER_FLOATING32_DSP_ASSERT(tiny_dsp::RealFloating32FFT);
VOICE_ANALYZER_Q15_DSP_ASSERT(tiny_dsp::RealFFTQ15);

template<size_t FFT_N>
struct VoiceAnalyzerConfig
{
    u32 min_freq          = 50; 
    u32 max_freq          = 1000;
    float noise_threshold = 1.0f;
};



template<size_t FFT_N, IsI2sMic<u32> I2sDriver, IsDspFloating32 Dsp> 
class VoiceAnalyzer
{
    std::array<f32, FFT_N> m_float_samples{};    // formatted i2s float, holding only left (or right) microphone audio
    std::array<f32, FFT_N> m_window_coeffs{};    // Hann window to avoid spectral leakage
    std::array<f32, FFT_N> m_fft_res{};          // real FFT values
    std::array<f32, FFT_N/2> m_mags{};           // FFT magnitudes

    std::span<u32> raw_buff{};                   // Raw I2s audio byte stream (interleaved 32bit Left + 32bit Right)


    I2sDriver& m_i2s;                           // I2s Driver, which interfaces microphone
    Dsp& m_dsp;                                 // Digital Signal Processor for FFT & filtering
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
        m_dsp.convert_raw_i2s_to_mono(raw_buff, m_float_samples);
        m_dsp.window_inplace(m_float_samples, m_window_coeffs);
        m_dsp.perform_fft(m_float_samples, m_fft_res);
    }
public:
    VoiceAnalyzer(I2sDriver& i2s, Dsp& dsp, const VoiceAnalyzerConfig<FFT_N>& config)
      : m_i2s{i2s}, 
        m_dsp(dsp),
        bin_resolution{i2s.get_sample_rate()/(float)FFT_N}, 
        k_min{config.min_freq/(u32)bin_resolution},  
        k_max{config.max_freq/(u32)bin_resolution},
        noise_threshold{config.noise_threshold}
    {
    }
    bool init()
    {
        if(!m_dsp.init())
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
        m_i2s.stop(); 
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

        m_mags[0] = est_freq;
        return est_freq;
    }
};


   

    // template<size_t FFT_N, typename RawI2sType/*SampleType*/, IsI2sMic<RawI2sType> I2sDriver, typename FourierType/*DspDataType*/, typename Dsp> 
    // requires IsValidDsp<Dsp, RawI2sType, FourierType> 
    template <size_t FFT_N, typename I2sDriver, typename Dsp>
    requires IsI2sMic<I2sDriver, typename I2sDriver::sample_type> && IsValidDsp<Dsp, typename I2sDriver::sample_type, typename Dsp::fourier_type>
    class VoiceAnalyzerBase
    {
    protected:

        using RawI2sType  = typename I2sDriver::sample_type;
        using FourierType = typename Dsp::fourier_type;

        std::array<FourierType, FFT_N> m_float_samples{};    // i2s float, holding only left from either ping or pong
        std::array<FourierType, FFT_N> m_window_coeffs{};    // Hann window
#ifdef VOICE_ANALYZER_USE_COMPLEX_FFT
        std::array<T, 2*FFT_N> m_fft_res{};          // fft values
#else
        std::array<FourierType, FFT_N> m_fft_res{};          // fft values
#endif
        std::array<FourierType, FFT_N/2> m_mags{};           // fft magnitudes

        std::span<RawI2sType> raw_buff{};

        I2sDriver& m_i2s;
        Dsp& m_dsp;
 
        float bin_resolution{};
        u32 k_min{};
        u32 k_max{};
        float noise_threshold{};
        inline void acquire_sample()
        {
            m_i2s.read_blocking(raw_buff);
        }
        inline void prepare_sample()
        {
            m_dsp.convert_raw_i2s_to_mono(raw_buff, m_float_samples);
            m_dsp.window_inplace(m_float_samples, m_window_coeffs);
        }
        float estimate_max_magnitude()
        {
            static const u32 search_len = k_max - k_min + 1;
            auto [peak_idx, max_value]  = m_dsp.find_max(std::span{m_mags}.subspan(k_min), search_len);
            if(max_value < this->noise_threshold)
                return 0.0f;

            const u32 peak_bin = k_min + peak_idx;

            f32 d = f32{};
 
            if (peak_bin > k_min && peak_bin < k_max)
            {
                f32 y1 = this->m_mags[peak_bin - 1];
                f32 y2 = this->m_mags[peak_bin];
                f32 y3 = this->m_mags[peak_bin + 1];
                
                f32 denom = y1 - 2.0f * y2 + y3;
                if (fabsf(denom) > 1e-5f) {
                    d = 0.5f * (y1 - y3) / denom;
                }
            }

            const f32 est_freq = (static_cast<f32>(peak_bin) + d) * this->bin_resolution;
            m_mags[0] = (FourierType)est_freq;
            return est_freq;
        }
    public:
        VoiceAnalyzerBase(I2sDriver& i2s, Dsp& dsp, const VoiceAnalyzerConfig<FFT_N>& config)
        : m_i2s{i2s}, 
            m_dsp(dsp),
            bin_resolution{i2s.get_sample_rate()/(float)FFT_N}, 
            k_min{config.min_freq/(u32)bin_resolution},  
            k_max{config.max_freq/(u32)bin_resolution},
            noise_threshold{config.noise_threshold}
        {
        }
        bool init()
        {
            if(!m_dsp.init())
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
        inline auto get_raw_mag() -> std::array<FourierType, FFT_N/2>&
        {
            return m_mags;
        }
        inline auto get_raw_samples() -> std::array<FourierType, FFT_N>&
        {
            return m_float_samples;
        }
        inline auto get_raw_mag_view() -> std::span<FourierType>
        {
            return m_mags;
        }
        inline auto get_raw_samples_view() -> std::span<FourierType>
        {
            return m_float_samples;
        }

        inline auto get_magnitudes_view() -> std::span<const FourierType>
        {
            // compute_magnitudes(squared);
            return std::span{m_mags}.subspan(k_min, k_max - k_min);
        }
        inline void compute_magnitudes(bool squared = true)
        {
            this->acquire_sample();
            this->prepare_sample();
            this->m_dsp.perform_fft(this->m_float_samples, m_fft_res);
            if(squared)
                this->m_dsp.compute_magnitude_squared(m_fft_res, this->m_mags); 
            else  
                this->m_dsp.compute_magnitude(m_fft_res, this->m_mags); 
        }
        float get_peak()
        {
            compute_magnitudes();
            return this->estimate_max_magnitude();
        }
 
    };

// #ifdef VOICE_ANALYZER_USE_16BIT
//     template<size_t FFT_N,  IsI2sMic<i16> I2sDriver, IsDspQ15 Dsp> 
//     using VoiceAnalyzerBaseX = VoiceAnalyzerBase<FFT_N, I2sDriver, i16, q15, Dsp>;    
// #else
//     template<size_t FFT_N, IsI2sMic<u32> I2sDriver, IsDspFloating32 Dsp> 
//     using VoiceAnalyzerBaseX = VoiceAnalyzerBase<FFT_N, I2sDriver, u32, f32, Dsp>;
// #endif
 
}
#endif //VOICE_ANALYZER_HPP