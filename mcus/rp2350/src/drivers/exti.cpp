#include "drivers/exti.hpp"
#include "FreeRTOS/Kernel.hpp"
#include "FreeRTOS/Task.hpp"
#include "drivers/exti_notifier.hpp"
#include "embedded_hal/traits.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>


static constexpr uint32_t all_irq = GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE|GPIO_IRQ_LEVEL_HIGH|GPIO_IRQ_LEVEL_LOW;
 

void ExtiInput::set_mode(uint32_t new_mode)
{
    // irq_set_enabled(IO_IRQ_BANK0, false);
    // gpio_acknowledge_irq(m_pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);
    m_mode = new_mode;
    gpio_set_irq_enabled(m_pin, new_mode, true);
    // irq_set_enabled(IO_IRQ_BANK0, true);
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
    gpio_set_irq_enabled(m_pin, m_mode, false);


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
    // if(m_mode != GPIO_IRQ_EDGE_RISE)
        set_mode(GPIO_IRQ_EDGE_RISE);

    // m_awaiting = true;
    // m_task->awaitNotifs();
    // m_awaiting = false;

    gpio_set_irq_enabled(m_pin, GPIO_IRQ_EDGE_RISE, true);

    // m_awaiting = true;
    m_task->awaitNotifs();
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time

}
void ExtiInput::wait_for_falling_edge()
{
    // if(m_mode != GPIO_IRQ_EDGE_FALL)
    // set_mode(GPIO_IRQ_EDGE_FALL);
    // gpio_acknowledge_irq(m_pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);
    gpio_set_irq_enabled(m_pin, GPIO_IRQ_EDGE_FALL, true);

    // m_awaiting = true;
    m_task->awaitNotifs();
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
    // m_awaiting = false;
}
void ExtiInput::wait_for_any_edge()
{
    // if(m_mode != (GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE))
    // set_mode(GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE);
    gpio_set_irq_enabled(m_pin, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);

    // m_awaiting = true;
    // gpio_set_irq_enabled(m_pin, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);
    // gpio_set_irq_enabled
    // gpio_acknowledge_irq(m_pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);
    // gpio_set_irq_enabled(m_pin, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
    // m_awaiting = false;
}

void ExtiInput::wait_for_high()
{
    // if(m_mode != (GPIO_IRQ_LEVEL_HIGH))
    //     set_mode(GPIO_IRQ_LEVEL_HIGH);

    if(is_high())
        return;

    gpio_set_irq_enabled(m_pin, GPIO_IRQ_LEVEL_HIGH, true);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time
}

void ExtiInput::wait_for_low()
{
    // if(m_mode != (GPIO_IRQ_LEVEL_LOW))
    //     set_mode(GPIO_IRQ_LEVEL_LOW);

    if(is_low())
        return;
    
    gpio_set_irq_enabled(m_pin, GPIO_IRQ_LEVEL_LOW, true);
    m_task->awaitNotifs();                      // Interrupt fires & acknowledged here
    gpio_acknowledge_irq(m_pin, all_irq);        // clear additional interrupt
    gpio_set_irq_enabled(m_pin, all_irq, false); // disable, until next time

    // m_awaiting = true;
    // m_task->awaitNotifs();
    // m_awaiting = false;
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

ExtiInput::ExtiInput(ExtiInput&& other)
    : m_pin{other.m_pin}, m_task{other.m_task}, m_mode{other.m_mode}, m_awaiting(other.m_awaiting)
{

}
ExtiInput& ExtiInput::operator=(ExtiInput&& other)
{
    if(&other != this)
    {
        m_pin = other.m_pin;
        m_task = other.m_task;
        m_mode = other.m_mode;
        m_awaiting = other.m_awaiting;
    }
    return *this;
}
ExtiInput::~ExtiInput()
{

}