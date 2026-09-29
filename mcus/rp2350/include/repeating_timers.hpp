#ifndef REPEATING_TIMERS_HPP
#define REPEATING_TIMERS_HPP

#include "types.hpp"
#include <pico/time.h>
#include <string.h>

namespace timers
{


class AbstractRepeatingTimer  
{
    repeating_timer_t m_timer;
    static bool wrapper_callback(repeating_timer_t* rt)
    {
        auto self = reinterpret_cast<AbstractRepeatingTimer*>(rt->user_data);
        return self->timer_callback();
    }
public:
    explicit AbstractRepeatingTimer()
    {
    }
    bool init_timer_ms(i32 delay_ms)
    {
        memset(&m_timer, 0, sizeof(repeating_timer_t)); 
        return add_repeating_timer_ms(delay_ms, AbstractRepeatingTimer::wrapper_callback, reinterpret_cast<void*>(this), &m_timer);
    }
    bool init_timer_us(i32 delay_us)
    {
        memset(&m_timer, 0, sizeof(repeating_timer_t));
        return add_repeating_timer_us(delay_us, AbstractRepeatingTimer::wrapper_callback, reinterpret_cast<void*>(this), &m_timer);
    }
    bool cancel()
    {
        return cancel_repeating_timer(&m_timer);
    }
    virtual bool timer_callback() = 0;
    

    AbstractRepeatingTimer(const AbstractRepeatingTimer& other) = delete;
    AbstractRepeatingTimer& operator=(const AbstractRepeatingTimer& other) = delete;

    AbstractRepeatingTimer(AbstractRepeatingTimer&& other)
        : m_timer(other.m_timer)
    {}
    AbstractRepeatingTimer& operator=(AbstractRepeatingTimer&& other)
    {
        if(&other != this)
        {
            this->m_timer = other.m_timer;
        }
        return *this;
    }
    virtual ~AbstractRepeatingTimer() = default;
};


    // template<bool ShouldRepeat>
    // class BasicTimer final : public AbstractRepeatingTimer 
    // {
    //     bool m_fired{};
    // public:
    //     BasicTimer(bool repeat = true)
    //     {
    //     }
    //     bool timer_callback() override
    //     {
    //         m_fired = true;
    //         return ShouldRepeat;
    //     }
    //     bool has_fired() const 
    //     {
    //         return m_fired;
    //     }
    //     void acknowledge()
    //     {
    //         m_fired = false;
    //     }
    // };

    template<bool ShouldRepeat>
    class BasicTimer final : private AbstractRepeatingTimer 
    {
        bool m_fired{};
        bool m_is_active{};
        bool m_deactivated{};
    public:
        BasicTimer()
        {
        }
        bool start_ms(i32 ms)
        {
            if(!m_is_active)
            {
                m_is_active   = this->init_timer_ms(ms);
                m_deactivated = false;
            }
            return m_is_active;
        }
        bool timer_callback() override
        {
            if(m_deactivated)
            {
                m_is_active = false;
                return false;
            }

            m_fired = true;
            return ShouldRepeat;
        }
        bool deactivate()
        {
            m_deactivated = true;
            if(m_is_active)
            {
                m_is_active = false;
                return this->cancel();
            }
            return true;
        }
        bool has_fired() const 
        {
            return m_fired;
        }
        void acknowledge()
        {
            m_fired = false;
        }
    };

    inline constexpr bool Reapating = true; 
    inline constexpr bool NonReapating = false;
}

#endif