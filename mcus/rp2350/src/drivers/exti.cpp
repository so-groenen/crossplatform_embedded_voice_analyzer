#include "drivers/exti.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "drivers/exti_notifier.hpp"
#include "embedded_hal/traits.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <hardware/gpio.h>



static inline embedded_hal::gpio::Level to_level(bool high)
{
    return high? embedded_hal::gpio::Level::High : embedded_hal::gpio::Level::Low;
}

void ExtiInput::set_mode(uint32_t new_mode)
{
    irq_set_enabled(IO_IRQ_BANK0, false);
    gpio_set_irq_enabled(m_pin, m_mode, false);
    m_mode = new_mode;
    gpio_set_irq_enabled(m_pin, m_mode, true);
    irq_set_enabled(IO_IRQ_BANK0, true);


}


ExtiInput::ExtiInput(uint16_t pin, uint32_t mode, PicoTask* task, IExtiNotifier& notifier)
    : m_pin{pin}, m_task{task}, m_mode{mode}
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);

    notifier.enable(this,task);

    m_level = to_level(gpio_get_out_level(m_pin));
}
bool ExtiInput::is_awaiting() const
{
    return m_awaiting;
}
uint16_t ExtiInput::pin() const 
{
    return m_pin;
}
uint32_t ExtiInput::mode() const 
{
    return m_mode;
}
void ExtiInput::wait_for_rising_edge()
{
    if(m_mode != GPIO_IRQ_EDGE_RISE)
        set_mode(GPIO_IRQ_EDGE_RISE);
    // task_await();
    m_awaiting = true;
    m_task->awaitNotifs();
    m_awaiting = false;
}
void ExtiInput::wait_for_falling_edge()
{
    if(m_mode != GPIO_IRQ_EDGE_FALL)
        set_mode(GPIO_IRQ_EDGE_FALL);
    m_awaiting = true;
    m_task->awaitNotifs();
    m_awaiting = false;
}
void ExtiInput::wait_for_any_edge()
{
    if(m_mode != (GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE))
        set_mode(GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE);

    m_awaiting = true;
    m_task->awaitNotifs();
    m_awaiting = false;
}

void ExtiInput::wait_for_high()
{
    if(m_mode != (GPIO_IRQ_LEVEL_HIGH))
        set_mode(GPIO_IRQ_LEVEL_HIGH);

    m_level = to_level(gpio_get_out_level(m_pin));
    if(static_cast<bool>(m_level))
        return;

    m_awaiting = true;
    m_task->awaitNotifs();
    m_awaiting = false;
}

void ExtiInput::wait_for_low()
{
    if(m_mode != (GPIO_IRQ_LEVEL_LOW))
        set_mode(GPIO_IRQ_LEVEL_LOW);

    m_level = to_level(gpio_get_out_level(m_pin));
    if(!static_cast<bool>(m_level))
        return;
    
    m_awaiting = true;
    m_task->awaitNotifs();
    m_awaiting = false;
}


bool ExtiInput::is_high() const
{
    return gpio_get_out_level(m_pin); 
}
bool ExtiInput::is_low() const
{
    return !is_high(); 
}