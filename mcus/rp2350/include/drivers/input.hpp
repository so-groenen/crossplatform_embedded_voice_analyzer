#pragma once

#include <hardware/gpio.h>
#include <pico/platform/common.h>
#include "embedded_hal/traits.hpp"

 
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
    bool is_high() const;
    bool is_low() const;
    Level get_level() const;
    
    Input(const Input& other) = delete;
    Input& operator=(Input& other) = delete;
    
    Input(Input&& other);
    Input& operator=(Input&& other);
    ~Input();
};



static_assert(embedded_hal::input::IsInput<Input>, "Input must implement Input traits");
 
