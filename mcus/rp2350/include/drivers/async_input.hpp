#pragma once

#include <hardware/gpio.h>
#include <pico/platform/common.h>
// #include "pico/stdlib.h"
// #include "embedded_hal/traits.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "drivers/interface_async_input_handler.hpp"
#include "drivers/pico_task.hpp"
#include "embedded_hal/traits.hpp"

 
using Level = embedded_hal::gpio::Level;
using Pull  = embedded_hal::gpio::Pull;

class AsyncInput
{
    PicoTask* m_task{};
    uint32_t m_mode{};
    uint16_t m_pin{};
    bool m_awaiting{};
public:
    explicit AsyncInput(uint16_t pin, Pull pull, PicoTask* task, IAsyncInputHandler& irq, uint32_t mode = GPIO_IRQ_EDGE_FALL);
    void set_mode(uint32_t new_mode);
    uint16_t pin() const;
    // uint32_t mode() const;

    // implement AsyncInput
    // bool is_awaiting() const;
    void wait_for_rising_edge();
    void wait_for_falling_edge();
    void wait_for_any_edge();
    void wait_for_high(); 
    void wait_for_low(); 

    // implement Input
    bool is_high() const;
    bool is_low() const;
    Level get_level() const;
    
    AsyncInput(const AsyncInput& other) = delete;
    AsyncInput& operator=(AsyncInput& other) = delete;
    
    AsyncInput(AsyncInput&& other);
    AsyncInput& operator=(AsyncInput&& other);
    ~AsyncInput();

    static constexpr uint32_t all_irq = GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE|GPIO_IRQ_LEVEL_HIGH|GPIO_IRQ_LEVEL_LOW;
};

static_assert(embedded_hal::input::IsInput<AsyncInput>, "must implement Input traits");
static_assert(embedded_hal::input::IsAsyncInput<AsyncInput>, "must implement Exti traits");

