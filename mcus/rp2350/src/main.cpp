
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <print>
#include "pico/stdlib.h"
#include "embedded_hal/traits.hpp"
#include "drivers/gpio.hpp"
 



void test_gpio()
{
    using namespace pico2_w;


    WifiOutput led{};
    led.set_level(Level::High);
    led.set_level(Level::Low);
    led.toggle();

    assert(led.is_set_high());
    assert(led.get_out_put_level() == Level::High);


    constexpr uint16_t ms_blinky{125};
    embedded_hal::output::Test::BlinkyTask ledTask{led, ms_blinky};

    printf("GPIO TEST OK \n");
}

int main() 
{
    stdio_init_all();
    printf("STARTING SCHEDULER\n");

    test_gpio();
    FreeRTOS::Kernel::startScheduler();

    while(1)
    {
        configASSERT(0);
    }
}