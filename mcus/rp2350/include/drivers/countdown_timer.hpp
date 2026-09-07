#ifndef COUNTDOWN_HPP
#define COUNTDOWN_HPP


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
    Vector<absolute_time_t, N> m_next{};
    Vector<uint64_t, N> m_delays{};
public:
    void add_task_us(uint64_t delay_us)
    {
        m_delays.push_back(delay_us);
        m_next.push_back(get_absolute_time());
    }
    void add_task_ms(f32 delay_ms)
    {
        m_delays.push_back(static_cast<uint64_t>(delay_ms * 1000.f) );
        m_next.push_back(get_absolute_time());
    }
    inline bool is_ready(size_t n = 0)
    {
        absolute_time_t now = get_absolute_time();
        i64 diff = absolute_time_diff_us(now, m_next[n]);
        if (diff <= 0) 
        {
            u64 diff_error = (static_cast<u64>(-diff) > m_delays[n]) ? m_delays[n] :  static_cast<u64>(-diff);
            m_next[n]      = delayed_by_us(now, m_delays[n] - diff_error);
            return true;
        }
        return false;
    }
};

// }
#endif  