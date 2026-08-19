#ifndef API_HPP
#define API_HPP

#include "rtos_cpp/rtos_cpp.hpp"
#include <concepts>
#include <print>

namespace API
{
template<class T>
concept IsOutput = std::move_constructible<T> && requires (T g) 
{
    {g.set_high()} -> std::same_as<void>;
    {g.set_low()} -> std::same_as<void>;
};

namespace Test
{


void toggle(IsOutput auto& led)
{
    std::println("EmbeddedHal_Test: Set high:");
    led.set_high();
    std::println("EmbeddedHal_Test: Set low:");
    led.set_high();
}

template<IsOutput LedType>
class LedTask : public RtosCpp::Task
{
    LedType m_led;
public:
    LedTask(LedType led)
        : m_led(std::move(led)) 
    {
        std::println("EmbeddedHal_Test: Testing LED Task");
    }
    void run() override
    {
        toggle(m_led);
    }
};

}//Test
}
#endif 