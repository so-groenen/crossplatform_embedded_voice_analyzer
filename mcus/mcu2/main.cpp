#include "rtos/rtos.h"
#include "rtos_cpp/rtos_cpp.hpp"

#include <cstdint>
#include <print>
#include <stdint.h>


void hello(void* value)
{
    uint64_t v = reinterpret_cast<uint64_t>(value);
    std::println("Value is {}", v);
}


class HelloTask : public RtosCpp::Task
{
    uint32_t m_number{};
public: 
    HelloTask(uint32_t number)
      : m_number(number)
    {
    }
    void run() override 
    {
        std::println("Value is {}", m_number);
    }
};

int main()
{
    std::println("Hello from mcu2");
    static HelloTask my_task{42};

    RtosCpp::startSchedueler();
}
