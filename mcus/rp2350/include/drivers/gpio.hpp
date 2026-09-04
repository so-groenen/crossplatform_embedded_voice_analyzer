#ifndef RP2350_GPIO_HPP
#define RP2350_GPIO_HPP

#include "pico/stdlib.h"
#include "embedded_hal/traits.hpp"
#include <hardware/gpio.h>


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

using namespace embedded_hal;

namespace details 
{
    // workaround for freeRTOS cyw43: cache last setting, and return if asked.
    inline bool __cyw43_led_state;




template <bool UseCyw43>
class Output 
{
    uint m_pin{};
    // gpio::Level m_level{};
   
    inline void _put(bool led_on) 
    {
        if constexpr (UseCyw43)
        {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
            // __cyw43_led_state = led_on;
        }
        else  
        {
            gpio_put(m_pin, led_on);
        }
    }
public:
    Output(gpio::Level level = gpio::Level::Low) requires (!UseCyw43)
        : m_pin{DefaultPin}//, m_level{level}
    {
        gpio_init(DefaultPin);
        gpio_set_dir(DefaultPin, GPIO_OUT);
        _put(static_cast<bool>(level));
    }

    Output(gpio::Level level = gpio::Level::Low) requires (UseCyw43)
        : m_pin{CYW43_WL_GPIO_LED_PIN} //, m_level{level}
    {
        cyw43_arch_init();
        _put(static_cast<bool>(level));
    }
    explicit Output(uint pin, gpio::Level level = gpio::Level::Low)  
        : m_pin{pin}//, m_level{level}
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
        
        _put(static_cast<bool>(level));
    }
    void set_high() 
    {
        // m_level = gpio::Level::High;
        _put(static_cast<bool>(gpio::Level::High));
    }
    void set_low() 
    {
        _put(static_cast<bool>(gpio::Level::Low));
    }
    auto get_output_level() const -> gpio::Level 
    {
        if constexpr (UseCyw43)
        {
            // return static_cast<gpio::Level>(__cyw43_led_state);
            return static_cast<gpio::Level>(cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN));
        }
            

        return static_cast<gpio::Level>(gpio_get_out_level(m_pin));
    }
    void set_level(gpio::Level level)
    {
        if(static_cast<bool>(level))
            set_high();
        else 
            set_low();
    }
    bool is_set_high() const
    {
        if constexpr (UseCyw43)
            return cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN);

        return gpio_get_out_level(m_pin);
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
    using Level      = embedded_hal::gpio::Level;
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
