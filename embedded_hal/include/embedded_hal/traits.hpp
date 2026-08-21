#ifndef API_HPP
#define API_HPP

#include "FreeRTOS/Task.hpp"
#include <concepts>
#include <cstdint>
#include <sys/types.h>
 

namespace embedded_hal
{
    namespace gpio
    {
        enum class Level : bool 
        {
            Low = false,
            High = true,
        };

        enum class Pull : uint8_t 
        {
            None = 0,
            Down = 1,
            Up = 2,
        };
    }

    namespace output
    {
        
        template<class T>
        concept IsOutput = std::move_constructible<T> && requires (T g, gpio::Level level) 
        {
            {g.set_high()} -> std::same_as<void>;
            {g.set_low()} -> std::same_as<void>;
            {g.set_level(level)}-> std::same_as<void>;
            {g.get_output_level()}-> std::same_as<gpio::Level>;
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
}// output


namespace input 
{
    template<class InputType>
    concept IsExtiInput = std::move_constructible<InputType> && requires (InputType g) 
    {
        {g.is_awaiting()} -> std::same_as<bool>;
        {g.wait_for_rising_edge()} -> std::same_as<void>;
        {g.wait_for_falling_edge()}-> std::same_as<void>;
        {g.wait_for_any_edge()}-> std::same_as<void>;
        {g.wait_for_high()}-> std::same_as<void>;
        {g.wait_for_low()}-> std::same_as<void>;
    };

    template<class InputType>
    concept IsInput = std::move_constructible<InputType> && requires (InputType g) 
    {
        {g.is_high()} -> std::same_as<bool>;
        {g.is_low()} -> std::same_as<bool>;
        {g.get_level()} -> std::same_as<gpio::Level>;
    };


}



}
#endif 