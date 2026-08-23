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



 

void ExtiInput::set_mode(uint32_t new_mode)
{
    irq_set_enabled(IO_IRQ_BANK0, false);
    gpio_set_irq_enabled(m_pin, m_mode, false);
    m_mode = new_mode;
    gpio_set_irq_enabled(m_pin, m_mode, true);
    irq_set_enabled(IO_IRQ_BANK0, true);


}


ExtiInput::ExtiInput(uint16_t pin, Pull pull, PicoTask* task, IExtiNotifier& notifier, uint32_t mode)
    : m_pin{pin}, m_task{task}, m_mode{mode}
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);

    switch (pull)
    {
        case Pull::Up:
            gpio_pull_up(pin);
            break;
        case Pull::Down:
            gpio_pull_down(pin);
            break;
        case Pull::None:
            gpio_set_pulls(pin, false, false);
    }

    notifier.bind_interrupt_from_rtos(this, task);

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

    if(is_high())
        return;

    m_awaiting = true;
    m_task->awaitNotifs();
    m_awaiting = false;
}

void ExtiInput::wait_for_low()
{
    if(m_mode != (GPIO_IRQ_LEVEL_LOW))
        set_mode(GPIO_IRQ_LEVEL_LOW);

    if(is_low())
        return;
    
    m_awaiting = true;
    m_task->awaitNotifs();
    m_awaiting = false;
}


bool ExtiInput::is_high() const
{
    return gpio_get(m_pin); 
}
bool ExtiInput::is_low() const
{
    return !gpio_get(m_pin);
}

Level ExtiInput::get_level() const
{
    return (static_cast<embedded_hal::gpio::Level>(gpio_get(m_pin)));
}
