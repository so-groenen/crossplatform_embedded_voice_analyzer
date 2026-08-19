#include <cstdarg>
#include <cstdint>
#include <print>
#include "embedded_hal/traits.hpp"
#include "driver/led.hpp"
#include "FreeRTOS/Task.hpp" 
#include <pthread.h>




namespace System 
{
    extern Console console;
};

 
namespace Test
{
class UnixTask : public FreeRTOS::Task 
{
    uint16_t m_ms{1000};
public:
    UnixTask(uint16_t ms)
        : m_ms{ms}
    {
        std::println("Posix FreeRtos task created");
    }

protected:
    void taskFunction() override 
    {
        while (true) 
        {
            System::console.log("Hello from FreeRTOS C++ Task on Unix!");
            FreeRTOS::Task::delay(pdMS_TO_TICKS(m_ms));
        }
    }
};
}

int main() 
{
    constexpr uint16_t pin{16};
    constexpr uint16_t ms_blinky{500};
    constexpr uint16_t ms_unix{2000};

    std::println("Starting FreeRTOS Scheduler...");

    Test::UnixTask task1{ms_unix};
    embedded_hal::Test::BlinkyTask led_task{LED{pin}, ms_blinky};

    FreeRTOS::Kernel::startScheduler();
    return 0;
}