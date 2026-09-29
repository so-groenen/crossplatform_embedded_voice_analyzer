#ifndef COUNTDOWN_HPP
#define COUNTDOWN_HPP


#include <cstddef>
#include <pico/time.h>
#include <pico/types.h>
#include "embedded_hal/heapless.hpp"
#include "types.hpp"


using embedded_hal::heapless::Vector;

// namespace time
// {

template<size_t N>
class CountdownTimer 
{
    Vector<absolute_time_t, N> m_next_us{};
    Vector<uint64_t, N> m_delays_us{};
public:
    void add_task_us(uint64_t delay_us)
    {
        m_delays_us.push_back(delay_us);
        m_next_us.push_back(get_absolute_time());
    }
    void add_task_ms(f32 delay_ms)
    {
        m_delays_us.push_back(static_cast<uint64_t>(delay_ms * 1000.f) );
        m_next_us.push_back(delayed_by_us(get_absolute_time(), static_cast<uint64_t>(delay_ms * 1000.f)));
        // m_next_us.push_back(get_absolute_time());

    }
    void reset(size_t n = 0)
    {
        m_next_us[n] = delayed_by_us(get_absolute_time(), m_delays_us[n]);
    }
    inline bool is_ready(size_t n = 0)
    {
        absolute_time_t now = get_absolute_time();
        i64 diff = absolute_time_diff_us(now, m_next_us[n]);
        if (diff <= 0) 
        {
            // u64 diff_error = (static_cast<u64>(-diff) > m_delays_us[n]) ? m_delays_us[n] :  static_cast<u64>(-diff);
            // m_next_us[n]   = delayed_by_us(now, m_delays_us[n] - diff_error);
            m_next_us[n]   = delayed_by_us(now, m_delays_us[n]);
            return true;
        }
        return false;
    }
};

// }
#endif  