#include "drivers/async_input.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "drivers/async_input_handler.hpp"
#include "embedded_hal/traits.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include "drivers/input.hpp"

 

AsyncInput::AsyncInput(uint16_t pin, Pull pull, PicoTask* task, IAsyncInputHandler& notifier)
    : Input{pin, pull}, m_task{task}
{
    notifier.bind_interrupt_from_rtos(this, task);
    gpio_set_irq_enabled(m_pin, all_irq, false);
}

void AsyncInput::wait_for_rising_edge()
{
    gpio_set_irq_enabled(m_pin, GPIO_IRQ_EDGE_RISE, true);
    m_task->awaitNotifs();
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time

}
void AsyncInput::wait_for_falling_edge()
{
    gpio_set_irq_enabled(m_pin, GPIO_IRQ_EDGE_FALL, true);
    m_task->awaitNotifs();
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
    // m_awaiting = false;
}
void AsyncInput::wait_for_any_edge()
{
    gpio_set_irq_enabled(m_pin, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
}

void AsyncInput::wait_for_high()
{
    if(is_high())
        return;

    gpio_set_irq_enabled(m_pin, GPIO_IRQ_LEVEL_HIGH, true);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
}

void AsyncInput::wait_for_low()
{
    if(is_low())
        return;
    
    gpio_set_irq_enabled(m_pin, GPIO_IRQ_LEVEL_LOW, true);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
}

 