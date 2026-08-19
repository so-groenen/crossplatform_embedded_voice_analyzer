#include "cpu.hpp"
#include "FreeRTOS/Task.hpp"
#include <cstddef>

NVIC<N_interrupts> nvic{};
std::array<Register, N_interrupts> hw_registers{};