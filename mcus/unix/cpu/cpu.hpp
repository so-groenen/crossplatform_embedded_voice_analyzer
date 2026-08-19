#ifndef CPU_HPP
#define CPU_HPP

#include "FreeRTOS/Task.hpp"
#include "projdefs.h"
#include <cstddef>
#include <array>
#include <ranges>
#include "hw_registers.hpp"
#include "nvic.hpp"
   
 
constexpr size_t N_interrupts{8};

extern NVIC<N_interrupts> nvic;
extern std::array<Register, N_interrupts> hw_registers;


class CPUTask : public FreeRTOS::Task 
{
public:
    CPUTask()
        : FreeRTOS::Task(0, 2024, "cpu-task")
    {
    }

protected:
    void taskFunction() override 
    {
        while (true) 
        {
            for(auto [IRQn, reg] : std::views::enumerate(hw_registers))
            {
                if(reg.read())
                {
                    delay(pdMS_TO_TICKS(1000));
                    nvic.interrupt(IRQn);
                }
            }
            delay(pdMS_TO_TICKS(100));
        }
    }
};

#endif