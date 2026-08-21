#pragma once

#include <hardware/gpio.h>
#include <pico/platform/common.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "drivers/interface_exti_notifier.hpp"
#include "drivers/pico_task.hpp"
#include "embedded_hal/traits.hpp"

 
using Level = embedded_hal::gpio::Level;
// using Pull  = embedded_hal::gpio::Level

class ExtiInput
{
    uint16_t m_pin{};
    uint32_t m_mode{};
    PicoTask* m_task{};
    bool m_awaiting{};
    Level m_level{};
public:
    explicit ExtiInput(uint16_t pin, uint32_t mode, PicoTask* task, IExtiNotifier& irq);
    void set_mode(uint32_t new_mode);
    uint16_t pin() const;
    uint32_t mode() const;

    // implement ExtiInput
    bool is_awaiting() const;
    void wait_for_rising_edge();
    void wait_for_falling_edge();
    void wait_for_any_edge();
    void wait_for_high(); // todo: add to hal
    void wait_for_low();  // todo: add to hal

    // implement Input
    bool is_high() const;
    bool is_low() const;
};

