#pragma once

#include "FreeRTOS/Mutex.hpp"
#include <concepts>
class ThreadSafeCtx 
{
    FreeRTOS::Mutex mutex{};
public:
    void operator()(std::invocable auto&& fn)
    {
        mutex.lock();
        fn();
        mutex.unlock();
    }
};

// extern ThreadSafeCtx g_thread_safe_ctx;