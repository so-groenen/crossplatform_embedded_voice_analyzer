#pragma once


#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/intctrl.h>
#include <hardware/sync.h>
#include <limits>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
 

#include "embedded_hal/heapless.hpp"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "drivers/pico_task.hpp"
#include "drivers/dma.hpp"

 
// unblock threads from interrupts
// should have pointer to thread
// and to Dma to acknowledge interrupt;
// also enable interrupt

 
namespace base
{
    class IDmaIrqHandler
    {
    public:
        virtual ~IDmaIrqHandler() = default;
        // virtual void enable_interrupt() = 0;
        virtual void enable_interrupt(Dma* dma) = 0;
        virtual void handle_isr() = 0;
    };
}

namespace rtos 
{
    class IDmaIrqHandler //: public base::IDmaIrqHandler
    {
    public:
        virtual ~IDmaIrqHandler() = default;
        // virtual void enable_interrupt() = 0;
        virtual void setup_interrupt(Dma* dma, PicoTask* task) = 0;
    };
}

    