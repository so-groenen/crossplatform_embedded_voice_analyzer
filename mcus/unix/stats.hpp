#ifndef STATS_HPP
#define STATS_HPP


/**
 * @file stats.c
 * @brief FreeRTOS Task & Memory Statistics Monitor
 * @author STM32World <lth@stm32world.com>
 * @date 2026
 *
 * Copyright (c) 2026 STM32World <lth@stm32world.com>
 *
 * Periodically extracts FreeRTOS task status, runtime statistics, and heap status.
 *
 */



#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
// #include "task.h" 
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#define STATS_INTERVAL_MS 5000 // 10000ms = 10 seconds

namespace System 
{
    extern Console console;
}


template<typename T>
class SmartMalloc
{
    T* m_data{nullptr};
public:
    SmartMalloc(size_t count)
        : m_data{ reinterpret_cast<T*>(pvPortMalloc(count * sizeof(T)))}
    {   
    }
    SmartMalloc(const SmartMalloc& other) = delete;
    SmartMalloc& operator=(const SmartMalloc& other) = delete;
    
    SmartMalloc(SmartMalloc&& other)
        : m_data{other.m_data}
    {
        other.m_data = nullptr;
    }
    explicit operator bool() const
    {
        return m_data != nullptr;
    }
    const T& operator[](size_t n) const
    {
        return m_data[n];
    }
    T& operator[](size_t n)
    {
        return m_data[n];
    }
    SmartMalloc& operator=(SmartMalloc&& other)
    {
        if(other != this)
        {
            m_data       = other.m_data;
            other.m_data = nullptr;
        }
        return *this;
    }
    T* data() const 
    {
        return m_data;
    }

    ~SmartMalloc()
    {
        if(m_data)
            vPortFree(m_data);
    }
};

class StatsTask : public FreeRTOS::Task
{
public:
    StatsTask()
    :     FreeRTOS::Task(tskIDLE_PRIORITY, configMINIMAL_STACK_SIZE, "stats-task")
    {}
    void taskFunction() override
    {
        static std::array<uint32_t, 16> u32LastTaskRunTimes{};
 
        while (true) 
        {
            delayUntil(pdMS_TO_TICKS(STATS_INTERVAL_MS));
            
            UBaseType_t uxTaskCount = FreeRTOS::Kernel::getNumberOfTasks();
            auto pxTaskStatusArray  = SmartMalloc<TaskStatus_t>(uxTaskCount);

            if (pxTaskStatusArray) 
            {
                uint32_t ulTotalRunTime;

                uxTaskCount = uxTaskGetSystemState(pxTaskStatusArray.data(), uxTaskCount, &ulTotalRunTime);
 

                System::console.do_locked([]
                {
                    std::println("\n==================== SYSTEM STATS ====================");
                    std::println("Free Heap           : {} bytes", xPortGetFreeHeapSize());
                    std::println("Minimum Ever Free   : {} bytes", xPortGetMinimumEverFreeHeapSize());
                    std::println("------------------------------------------------------");
                    std::println("Task          State  Prio  Stack  Num                 ");
                    // printf("Task          State  Prio  Stack  Num  Affinity  %% Core\n");
                    std::println("------------------------------------------------------");
                });
                

                    uint32_t u32TotalCoreUsagePct = 0;

                    for (UBaseType_t i = 0; i < uxTaskCount; i++) 
                    {
                        // Task index bounds safety
                        UBaseType_t tNum = pxTaskStatusArray[i].xTaskNumber;

                        // Convert state enum to character flag
                        char cState = 'X';
                        switch (pxTaskStatusArray[i].eCurrentState) 
                        {
                            case eRunning:
                                cState = 'G';
                                break;
                            case eInvalid:
                                cState = 'I';
                                break;
                            case eReady:
                                cState = 'R';
                                break;
                            case eBlocked:
                                cState = 'B';
                                break;
                            case eSuspended:
                                cState = 'S';
                                break;
                            case eDeleted:
                                cState = 'D';
                                break;
                            default:
                                break;
                        }
 
                        System::console.log("{:<13}      {}   {:>2}   {:>5}   {:>2}",
                            pxTaskStatusArray[i].pcTaskName,
                            cState, 
                            pxTaskStatusArray[i].uxCurrentPriority,
                            pxTaskStatusArray[i].usStackHighWaterMark,
                            pxTaskStatusArray[i].xTaskNumber
                            /*,pxTaskStatusArray[i].uxCoreAffinityMask*/
                            /*,u32CorePct*/ );
                    }
                    System::console.log("------------------------------------------------------\n");
 

            }
        }
    }
};


#endif