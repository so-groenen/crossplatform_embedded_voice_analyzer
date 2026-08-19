
#ifndef REGISTERS_HPP
#define REGISTERS_HPP

#include "FreeRTOS/Semaphore.hpp"


class Register 
{
    FreeRTOS::BinarySemaphore m_sem{};
public:
    void write()
    {
        m_sem.give();
    }
    bool read() const
    {
        return m_sem.take(1);
    }
};

#endif