#ifndef LED_HPP
#define LED_HPP

#include "embedded_hal/api.hpp"
#include <cstdint>
#include <limits>
#include <print>

constexpr uint16_t INACTIVE_PIN = std::numeric_limits<uint16_t>::max();

class LED 
{
    uint16_t m_pin{INACTIVE_PIN};
public:
    explicit LED(uint16_t pin)
        : m_pin(pin)
    {
        std::println("Init LED at pin {}", m_pin);
    }
    LED(const LED&) = delete;
    LED& operator=(const LED&) = delete;

    LED(LED&& other) noexcept : m_pin(other.m_pin) 
    {
        other.m_pin = INACTIVE_PIN; // Leave original in a safe, inactive state
    }
    
    LED& operator=(LED&& other) noexcept 
    {
        if (this != &other) 
        {
            m_pin = other.m_pin;
            other.m_pin = INACTIVE_PIN;
        }
        return *this;
    }
    

    void set_high()
    {
        std::println("LED Is On");
    }
    void set_low()
    {
        std::println("LED Is Off");
    }

    ~LED()
    {   

    }
};

static_assert(API::IsOutput<LED>, "mus be Output");


#endif