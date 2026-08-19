#include "rtos_cpp/rtos_cpp.hpp"
#include "embedded_hal/api.hpp"
#include "driver/led.hpp"

#include <cstdint>
#include <print>
#include <stdint.h>

 

class HelloTask : public RtosCpp::Task
{
    uint32_t m_number{};
    LED led{5};
public: 
    HelloTask(uint32_t number)
      : m_number(number)
    {
    }
    void run() override 
    {
        std::println("Value is {}", m_number);
        API::Test::toggle(led);
    }
};

int main()
{
    constexpr uint16_t pin{16};
    
    static API::Test::LedTask my_task{ LED{pin} };
    
    RtosCpp::startSchedueler();
}
