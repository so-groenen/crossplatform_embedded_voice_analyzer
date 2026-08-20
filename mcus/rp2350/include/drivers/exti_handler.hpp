#pragma once


#include "drivers/exti.hpp"
#include "array"
#include "drivers/exti_notifier.hpp"
#include <cstddef>

void isr_exti_handler();

constexpr size_t MAX_EXTI{8};


template<size_t N>
class ExtiHandler
{
    std::array<ExtiNotifier, N> notifier{};
    size_t n_num{};
public:
    explicit ExtiHandler()
        : n_num(0)
    {
    }
    void operator()(ExtiInput* input)
    {
        notifier[n_num++].set_input(input);
        gpio_add_raw_irq_handler(input->get_pin(), isr_exti_handler);
    }
    size_t num() const 
    {
        return n_num;
    }
    void handle_isr(size_t n)
    {
        notifier[n].handle_exti();
        notifier[n].notifyGiveFromISR();
    } 
};

extern ExtiHandler<MAX_EXTI> g_exti_handler;