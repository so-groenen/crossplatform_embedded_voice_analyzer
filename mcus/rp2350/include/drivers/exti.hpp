#pragma once

#include <hardware/gpio.h>
#include <pico/platform/common.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "drivers/pico_task.hpp"

 
class ExtiInput
{
    PicoTask& m_task;
    uint32_t m_mode{GPIO_IRQ_EDGE_FALL};
    uint16_t m_pin{};
    void task_await();
    void set_mode(uint32_t new_mode);
public:
    explicit ExtiInput(uint16_t pin = 0, uint32_t mode = GPIO_IRQ_EDGE_FALL, PicoTask* task = nullptr);
    uint16_t get_pin() const ;
    uint32_t get_mode() const ;
    void wait_for_rising_edge();
    void wait_for_falling_edge();
    void wait_for_any_edge();
    friend class ExtiNotifier;
};
