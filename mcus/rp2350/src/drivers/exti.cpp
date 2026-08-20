#include "drivers/exti.hpp"
#include "drivers/exti_handler.hpp"
#include <array>
#include <cstddef>
#include <cstdio>

 
void ExtiInput::task_await() 
{
    m_task.notifyTake();
}
void ExtiInput::set_mode(uint32_t new_mode)
{
    irq_set_enabled(IO_IRQ_BANK0, false);
    gpio_set_irq_enabled(m_pin, m_mode, false);
    m_mode = new_mode;
    gpio_set_irq_enabled(m_pin, m_mode, false);
    irq_set_enabled(IO_IRQ_BANK0, false);
}


ExtiInput::ExtiInput(uint16_t pin, uint32_t mode, PicoTask* task)
    : m_pin{pin}, m_task{*task}
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);


    set_mode(mode);
    printf("hello \n");
    // record the exti input here !!!
    g_exti_handler(this);
}
uint16_t ExtiInput::get_pin() const 
{
    return m_pin;
}
uint32_t ExtiInput::get_mode() const 
{
    return m_mode;
}
void ExtiInput::wait_for_rising_edge()
{
    if(m_mode != GPIO_IRQ_EDGE_RISE)
        set_mode(GPIO_IRQ_EDGE_RISE);
    task_await();
}
void ExtiInput::wait_for_falling_edge()
{
    if(m_mode != GPIO_IRQ_EDGE_FALL)
        set_mode(GPIO_IRQ_EDGE_FALL);
    task_await();
}
void ExtiInput::wait_for_any_edge()
{
    if(m_mode != (GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE))
        set_mode(GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE);
    task_await();
}

