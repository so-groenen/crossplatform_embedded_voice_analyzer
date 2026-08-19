#ifndef LED_HPP
#define LED_HPP

#include "FreeRTOS/Mutex.hpp"
#include "embedded_hal/traits.hpp"
#include <cstdint>
#include <limits>
#include <print>

constexpr uint16_t INACTIVE_PIN = std::numeric_limits<uint16_t>::max();

 

namespace System 
{
    class Console 
    {
        FreeRTOS::Mutex mutex{}; 
    public:
        template <typename... Args>
        void log(std::format_string<Args...> fmt, Args&&... args)
        {
            mutex.lock();
            std::println(fmt, std::forward<Args>(args)...);
            mutex.unlock();
        }
    };


    inline Console console{};
};


 

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
        System::console.log("LED Is On");
    }
    void set_low()
    {
        System::console.log("LED Is Off");
    }

    ~LED()
    {   

    }
};

static_assert(embedded_hal::IsOutput<LED>, "mus be Output");


#endif