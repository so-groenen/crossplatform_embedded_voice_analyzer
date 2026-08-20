#ifndef INT_HPP
#define INT_HPP

#include "cpu/cpu.hpp"
#include <cstddef>

// External Interrupt (raising / falling edge etc)
void EXTI0_ISR();
constexpr size_t EXTI0_ISRn = 0;

inline void interrupts_init()
{
    nvic.set_vector(EXTI0_ISRn, EXTI0_ISR);
}

#endif