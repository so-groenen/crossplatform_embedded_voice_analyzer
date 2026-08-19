#include <cstdarg>
#include <print>
#include "FreeRTOS/Task.hpp" 
#include "FreeRTOS/Mutex.hpp"
#include <pthread.h>


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

namespace System {
    Console console{};

};

 

class ExampleTask : public FreeRTOS::Task 
{
public:
    ExampleTask(UBaseType_t priority, uint16_t stackDepth, const char* name)
        : FreeRTOS::Task(priority, stackDepth, name)
        {
            std::println("Posix FreeRtos task created");
        }

protected:
    void taskFunction() override 
    {
        while (true) 
        {
            System::console.log("Hello from FreeRTOS C++ Task on Unix!");
            FreeRTOS::Task::delay(pdMS_TO_TICKS(1000));
        }
    }
};

int main() 
{
    std::println("Starting FreeRTOS Scheduler...");
    ExampleTask task(1, 2048, "TestTask");

    FreeRTOS::Kernel::startScheduler();
    return 0;
}