#include "drivers/async_input.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "interrupts/async_input_handler.hpp"
#include "embedded_hal/traits.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include "drivers/input.hpp"

 

AsyncInput::AsyncInput(uint16_t pin, Pull pull, PicoTask* task, interrupts::IAsyncInputHandler& notifier)
    : Input{pin, pull}, m_task{task}
{
    notifier.bind_interrupt_from_rtos(this, task);
    disable_interrupts();
}

void AsyncInput::wait_for_rising_edge()
{
    enable_interrupts(GPIO_IRQ_EDGE_RISE);
    m_task->awaitNotifs();
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time

}
void AsyncInput::wait_for_falling_edge()
{
    enable_interrupts(GPIO_IRQ_EDGE_FALL);
    m_task->awaitNotifs();
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
    // m_awaiting = false;
}
void AsyncInput::wait_for_any_edge()
{
    enable_interrupts(GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
}

void AsyncInput::wait_for_high()
{
    if(is_high())
        return;

    enable_interrupts(GPIO_IRQ_LEVEL_HIGH);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
}

void AsyncInput::wait_for_low()
{
    if(is_low())
        return;

    enable_interrupts(GPIO_IRQ_LEVEL_LOW);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
}

 