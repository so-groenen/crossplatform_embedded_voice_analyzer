
#include "drivers/input.hpp"
#include <cassert>
#include <cstdint>
#include <hardware/gpio.h>



Input::Input(uint16_t pin, Pull pull)
    : m_pin{pin}
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
}
uint16_t Input::pin() const 
{
    return m_pin;
} 


Input::Input(Input&& other)
    : m_pin{other.m_pin}
{

}
Input& Input::operator=(Input&& other)
{
    if(&other != this)
    {
        m_pin = other.m_pin;
    }
    return *this;
}
Input::~Input()
{

} 