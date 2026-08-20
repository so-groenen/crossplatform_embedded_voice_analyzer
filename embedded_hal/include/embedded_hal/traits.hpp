#ifndef API_HPP
#define API_HPP

#include "FreeRTOS/Task.hpp"
#include <concepts>
#include <cstdint>
 
namespace embedded_hal::output
{
    enum class Level : bool 
    {
        Low = false,
        High = true,
    };

    template<class T>
    concept IsOutput = std::move_constructible<T> && requires (T g, Level level) 
    {
        {g.set_high()} -> std::same_as<void>;
        {g.set_low()} -> std::same_as<void>;
        {g.set_level(level)}-> std::same_as<void>;
        {g.get_out_put_level()}-> std::same_as<Level>;
        {g.is_set_high()} -> std::same_as<bool>;
        {g.is_set_low()} -> std::same_as<bool>;
        {g.toggle()} -> std::same_as<void>;
    };



namespace Test
{

    template<IsOutput LedType>
    class BlinkyTask : public FreeRTOS::Task
    {
        LedType m_led;
        uint16_t m_ms_blink;
    public:
        BlinkyTask(LedType led, uint16_t ms_blink)
            : FreeRTOS::Task(1, 2024, "blinky-test"), m_led(std::move(led)), m_ms_blink{ms_blink}
        {
        }
        void taskFunction() override
        {
            while (true)
            {
                m_led.set_high();
                FreeRTOS::Task::delay(pdMS_TO_TICKS(m_ms_blink));
                m_led.set_low();
                FreeRTOS::Task::delay(pdMS_TO_TICKS(m_ms_blink));       
            }

        }
    };

}//Test
}
#endif 