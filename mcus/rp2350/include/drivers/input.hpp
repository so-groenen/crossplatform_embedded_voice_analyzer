#pragma once

#include <hardware/gpio.h>
#include <pico/platform/common.h>
#include "embedded_hal/traits.hpp"
#include "types.hpp"

 
using Level = embedded_hal::gpio::Level;
using Pull  = embedded_hal::gpio::Pull;


class Input
{
protected:
    uint16_t m_pin{};
public:
    explicit Input(uint16_t pin, Pull pull);
    uint16_t pin() const;

    // implement Input
    bool is_high() const
    {
        return gpio_get(m_pin);
    }
    bool is_low() const
    {
        return !is_high();
    }
    Level get_level() const
    {
        return static_cast<Level>(is_high());
    }

    // interrupt handling
    void enable_interrupts(u32 event_mask)
    {
        gpio_set_irq_enabled(m_pin, event_mask, true);
    } 
    void disable_interrupts(u32 event_mask = all_irq)
    {
        gpio_set_irq_enabled(m_pin, event_mask, false);
    } 
    
    Input(const Input& other) = delete;
    Input& operator=(Input& other) = delete;
    
    Input(Input&& other);
    Input& operator=(Input&& other);
    ~Input();

    static constexpr u32 all_irq = GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE|GPIO_IRQ_LEVEL_HIGH|GPIO_IRQ_LEVEL_LOW;
};



static_assert(embedded_hal::input::IsInput<Input>, "Input must implement Input traits");
 
