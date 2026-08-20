
#include <cstdint>
#include <cstdio>
#include "pico/stdlib.h"
#include "embedded_hal/traits.hpp"

 


#ifdef CYW43_WL_GPIO_LED_PIN
    #include "pico/cyw43_arch.h"
#endif

int pico_led_init(void) 
{
#ifdef CYW43_WL_GPIO_LED_PIN
    return cyw43_arch_init();
#else
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

void pico_set_led(bool led_on) {
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#endif
}

class Cyw43Led 
{
public:
    explicit Cyw43Led()
    {
        pico_led_init();
    }
    void set_high() 
    {
        pico_set_led(true);
    }
    void set_low() 
    {
        pico_set_led(false);
    }
};

static_assert(embedded_hal::IsOutput<Cyw43Led>, "Cyw43Led must implement traits");
 

int main() 
{
    stdio_init_all();
    printf("STARTING SCHEDULER\n");

    constexpr uint16_t ms_blinky{250};
    embedded_hal::Test::BlinkyTask ledTask{Cyw43Led{}, ms_blinky};

    FreeRTOS::Kernel::startScheduler();

    while(1)
    {
        configASSERT(0);
    }
}