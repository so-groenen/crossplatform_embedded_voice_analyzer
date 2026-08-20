#include "drivers/exti_handler.hpp"
#include <cstddef>


ExtiHandler<MAX_EXTI> g_exti_handler{};


void isr_exti_handler()
{
    for(size_t n = 0; n < g_exti_handler.num(); n++)
    {
        g_exti_handler.handle_isr(n);
    }
}