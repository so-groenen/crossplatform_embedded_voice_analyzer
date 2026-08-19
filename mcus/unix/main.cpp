#include <cstdarg>
#include <cstdint>
#include <print>
#include "FreeRTOS/Kernel.hpp"
#include "cpu/cpu.hpp"
#include "embedded_hal/traits.hpp"
#include "driver/led.hpp"
#include "FreeRTOS/Task.hpp" 
#include "projdefs.h"
#include <pthread.h>
#include "interrupts/interrupts.hpp"

#include "stats.hpp"


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
        : FreeRTOS::Task(1, 2024, "unix-test"), m_ms{ms}
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



namespace Test
{
class ButtonTask : public FreeRTOS::Task 
{
    uint16_t m_ms{1000};
public:
    ButtonTask(uint16_t ms)
        : FreeRTOS::Task(1, 2024, "button-task"), m_ms{ms}
    {
    }
    void handle_isr()
    {
        bool awoken_task = false;
        notifyGiveFromISR(awoken_task);
        if(awoken_task)
            FreeRTOS::Kernel::yield();
    }
    void press_button()
    {
        System::console.log(">> button pressed");
        hw_registers[EXTI0_ISRn].write();
    }
    void await()
    {
        System::console.log(">> awaiting isr");
        notifyTake();
    }
protected:
    void taskFunction() override 
    {
        while (true) 
        {
            FreeRTOS::Task::delay(pdMS_TO_TICKS(m_ms));
            press_button();
            await();
        }
    }
};
}

Test::ButtonTask* g_btn_task_ptr{};

void EXTI0_ISR()
{
    System::console.log(">> hello from isr!");
    if(g_btn_task_ptr)
        g_btn_task_ptr->handle_isr();
}


int main() 
{
    constexpr uint16_t pin{16};
    constexpr uint16_t ms_blinky{500};
    constexpr uint16_t ms_unix{2000};

    std::println("Starting FreeRTOS Scheduler...");

    interrupts_init();
    
    CPUTask cpu_task{}; // listens to hardware_events, triggers nvic interrut

    embedded_hal::Test::BlinkyTask led_task{LED{pin}, ms_blinky};
    Test::UnixTask task1{ms_unix};
    Test::ButtonTask btn_task{1000};
    StatsTask stats_task{};
    
    g_btn_task_ptr = &btn_task;
    FreeRTOS::Kernel::startScheduler();
    return 0;
}