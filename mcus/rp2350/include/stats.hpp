
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/platform/common.h>
#include <pico/time.h>
#include <stddef.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "thread_safe_ctx.hpp"
 
#include "projdefs.h" 


  


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
    uint32_t m_interval_ms{5000};
public:
    StatsTask(uint32_t interval_ms = 5'000)
    :     FreeRTOS::Task(tskIDLE_PRIORITY +1, 512, "stats-task"), m_interval_ms{interval_ms}
    {}
    void taskFunction() override
    {
        static std::array<uint32_t, 16> u32LastTaskRunTimes{};
 
        while (true) 
        {
            delayUntil(pdMS_TO_TICKS(m_interval_ms));
            
            UBaseType_t uxTaskCount = FreeRTOS::Kernel::getNumberOfTasks();
            auto pxTaskStatusArray  = SmartMalloc<TaskStatus_t>(uxTaskCount);

            if (pxTaskStatusArray) 
            {
                uint32_t ulTotalRunTime;

                uxTaskCount = uxTaskGetSystemState(pxTaskStatusArray.data(), uxTaskCount, &ulTotalRunTime);
 

                // g_thread_safe_ctx([]{
                    printf("\n==================== SYSTEM STATS ====================\n");
                    printf("Free Heap           : %zu bytes\n", xPortGetFreeHeapSize());
                    printf("Minimum Ever Free   : %zu bytes\n", xPortGetMinimumEverFreeHeapSize());
                    printf("------------------------------------------------------\n");
                    printf("Task          State  Prio  Stack  Num  Affinity  %% Core\n");
                    printf("------------------------------------------------------\n");
                // });
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
                    
                    // g_thread_safe_ctx([&]{
                        printf("%-13s %c     %2lu    %5u  %2lu  \n", 
                            pxTaskStatusArray[i].pcTaskName,
                            cState, pxTaskStatusArray[i].uxCurrentPriority,
                            pxTaskStatusArray[i].usStackHighWaterMark, 
                            pxTaskStatusArray[i].xTaskNumber
                        );
                    // });
                            // pxTaskStatusArray[i].pcTaskName,
                            // cState, 
                            // pxTaskStatusArray[i].uxCurrentPriority,
                            // pxTaskStatusArray[i].usStackHighWaterMark,
                            // pxTaskStatusArray[i].xTaskNumber
                            // /*,pxTaskStatusArray[i].uxCoreAffinityMask*/
                            // /*,u32CorePct*/ );
                    }
                    printf("------------------------------------------------------\n");
 

            }
        }
    }
};