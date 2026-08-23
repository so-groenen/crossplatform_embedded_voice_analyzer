#pragma once

#include <hardware/gpio.h>
#include <pico/platform/common.h>
#include "drivers/input.hpp"
#include "drivers/interface_async_input_handler.hpp"
#include "drivers/pico_task.hpp"
#include "embedded_hal/traits.hpp"


class AsyncInput : public Input
{
    PicoTask* m_task{};
public:
    explicit AsyncInput(uint16_t pin, Pull pull, PicoTask* task, IAsyncInputHandler& irq);

    // implement AsyncInput
    void wait_for_rising_edge();
    void wait_for_falling_edge();
    void wait_for_any_edge();
    void wait_for_high(); 
    void wait_for_low(); 

    static constexpr uint32_t all_irq = GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE|GPIO_IRQ_LEVEL_HIGH|GPIO_IRQ_LEVEL_LOW;
};



static_assert(embedded_hal::input::IsInput<AsyncInput>, "must implement Input traits");
static_assert(embedded_hal::input::IsAsyncInput<AsyncInput>, "must implement Exti traits");

