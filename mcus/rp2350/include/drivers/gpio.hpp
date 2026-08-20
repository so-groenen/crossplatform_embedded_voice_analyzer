#ifndef RP2350_GPIO_HPP
#define RP2350_GPIO_HPP

#include "pico/stdlib.h"
#include "embedded_hal/traits.hpp"


#ifdef CYW43_WL_GPIO_LED_PIN
    #include "pico/cyw43_arch.h"
#endif
 
// Main inspiration:
// https://docs.embassy.dev/embassy-stm32/0.6.0/stm32c011d6/gpio/struct.Output.html

#ifdef CYW43_WL_GPIO_LED_PIN
    constexpr uint DefaultPin = CYW43_WL_GPIO_LED_PIN;
#else
    constexpr uint DefaultPin = PICO_DEFAULT_LED_PIN;
#endif


namespace details 
{
using _Level = embedded_hal::output::Level;

template <bool UseCyw43>
class Output 
{
    uint m_pin{};
    _Level m_level{};
   
    inline void _put(bool led_on) 
    {
        if constexpr (UseCyw43)
        {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
        }
        else  
        {
            gpio_put(m_pin, led_on);
        }
    }

public:
    Output(embedded_hal::output::Level level = _Level::Low) requires (!UseCyw43)
        : m_pin{DefaultPin}, m_level{level}
    {
        gpio_init(DefaultPin);
        gpio_set_dir(DefaultPin, GPIO_OUT);
    }

    Output(_Level level = _Level::Low) requires (UseCyw43)
        : m_pin{CYW43_WL_GPIO_LED_PIN}, m_level{level}
    {
        cyw43_arch_init();
    }
    explicit Output(uint pin, _Level level = _Level::Low)  
        : m_pin{pin}, m_level{level}
    {
        if constexpr (UseCyw43)
        {
            if(pin == DefaultPin)
            {  
                cyw43_arch_init();
            }
            else 
            {
                gpio_init(pin);
                gpio_set_dir(pin, GPIO_OUT);
            }
        }
        else 
        {
            gpio_init(pin);
            gpio_set_dir(pin, GPIO_OUT);
        }
        set_level(m_level);
    }
    void set_high() 
    {
        m_level = _Level::High;
        _put(static_cast<bool>(m_level));
    }
    void set_low() 
    {
        m_level = _Level::Low;
        _put(static_cast<bool>(m_level));
    }
    auto get_out_put_level() const -> _Level 
    {
        return m_level;
    }
    void set_level(_Level level)
    {
        if(static_cast<bool>(level))
            set_high();
        else 
            set_low();
    }
    bool is_set_high() const
    {
        return m_level == _Level::High;
    }
    bool is_set_low() const
    {
        return !is_set_high();
    } 
    void toggle()
    {
        is_set_high() ? set_low() : set_high();
    }
};


static_assert(embedded_hal::output::IsOutput<Output<true>>, "Output must implement traits");

}
 
#ifdef CYW43_WL_GPIO_LED_PIN
namespace pico2_w 
{
    using Level      = embedded_hal::output::Level;
    using WifiOutput = details::Output<true>;
    using Output     = details::Output<false>;
}
#else
namespace pico2 
{
    using Level      = embedded_hal::output::Level;
    using Output     = details::Output<false>;
}
#endif






#endif // RP2350_GPIO_HPP
