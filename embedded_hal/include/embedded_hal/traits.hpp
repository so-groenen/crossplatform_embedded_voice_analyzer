#ifndef API_HPP
#define API_HPP

#include "FreeRTOS/Task.hpp"
#include <concepts>
#include <cstdint>
 
namespace embedded_hal
{
template<class T>
concept IsOutput = std::move_constructible<T> && requires (T g) 
{
    {g.set_high()} -> std::same_as<void>;
    {g.set_low()} -> std::same_as<void>;
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
            : FreeRTOS::Task(1, 2024, "name"), m_led(std::move(led)), m_ms_blink{ms_blink}
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