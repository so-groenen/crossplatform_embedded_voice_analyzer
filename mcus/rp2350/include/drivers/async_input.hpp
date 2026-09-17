#pragma once

#include <hardware/gpio.h>
#include <pico/platform/common.h>
#include "drivers/input.hpp"
#include "interrupts/interface_input_handlers.hpp"
#include "drivers/pico_task.hpp"
#include "embedded_hal/traits.hpp"


class AsyncInput : public Input
{
    PicoTask* m_task{};
public:
    explicit AsyncInput(uint16_t pin, Pull pull, PicoTask* task, interrupts::IAsyncInputHandler& irq);

    // implement AsyncInput
    void wait_for_rising_edge();
    void wait_for_falling_edge();
    void wait_for_any_edge();
    void wait_for_high(); 
    void wait_for_low(); 

};



static_assert(embedded_hal::input::IsInput<AsyncInput>, "must implement Input traits");
static_assert(embedded_hal::input::IsAsyncInput<AsyncInput>, "must implement Exti traits");

