
#pragma once
#include "drivers/pico_task.hpp"
#include "thread_safe_ctx.hpp"

class CounterTask : public PicoTask
{
    uint32_t m_delay{};
public:
    CounterTask(uint32_t delay, const char* name)
      : PicoTask(tskIDLE_PRIORITY+1, 256, name), m_delay{delay}
    {
    }

    void taskFunction() override
    {
        while(true)
        {
            static uint32_t counter = 0;
            // g_thread_safe_ctx([&]
            // {
                printf("Hello (delay[%u ms])! counter = %u \n", m_delay, counter);
            // });
            counter++;
            delay(pdMS_TO_TICKS(m_delay));
        }
    }
};
