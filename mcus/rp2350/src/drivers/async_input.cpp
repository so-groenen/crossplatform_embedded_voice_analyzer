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


static constexpr uint32_t all_irq = GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE|GPIO_IRQ_LEVEL_HIGH|GPIO_IRQ_LEVEL_LOW;
 

void AsyncInput::set_mode(uint32_t new_mode)
{
    // irq_set_enabled(IO_IRQ_BANK0, false);
    // gpio_acknowledge_irq(m_pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);
    m_mode = new_mode;
    gpio_set_irq_enabled(m_pin, new_mode, true);
    // irq_set_enabled(IO_IRQ_BANK0, true);
}


AsyncInput::AsyncInput(uint16_t pin, Pull pull, PicoTask* task, IAsyncInputHandler& notifier, uint32_t mode)
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
// bool AsyncInput::is_awaiting() const
// {
//     return m_awaiting;
// }
uint16_t AsyncInput::pin() const 
{
    return m_pin;
}
// uint32_t AsyncInput::mode() const 
// {
//     return m_mode;
// }
void AsyncInput::wait_for_rising_edge()
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
void AsyncInput::wait_for_falling_edge()
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
void AsyncInput::wait_for_any_edge()
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

void AsyncInput::wait_for_high()
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

void AsyncInput::wait_for_low()
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


bool AsyncInput::is_high() const
{
    return gpio_get(m_pin); 
}
bool AsyncInput::is_low() const
{
    return !gpio_get(m_pin);
}

Level AsyncInput::get_level() const
{
    return (static_cast<embedded_hal::gpio::Level>(gpio_get(m_pin)));
}

AsyncInput::AsyncInput(AsyncInput&& other)
    : m_pin{other.m_pin}, m_task{other.m_task}, m_mode{other.m_mode}, m_awaiting(other.m_awaiting)
{

}
AsyncInput& AsyncInput::operator=(AsyncInput&& other)
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
AsyncInput::~AsyncInput()
{

}