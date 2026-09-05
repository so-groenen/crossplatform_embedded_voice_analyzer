
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/regs/intctrl.h>
#include <hardware/sync.h>
#include <limits>
#include <pico/cyw43_arch.h>
#include <pico/platform/common.h>
#include <pico/platform/panic.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <stddef.h>
#include "drivers/dma.hpp"

#include "drivers/uart.hpp"


using namespace hardware::uart;
constexpr u32 UART_BAUDRATE = 115200;
constexpr u32 DMA_CH0       = 0;

int main() 
{
    stdio_init_all();
    
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{DMA_CH0});
    uart.set_baudrate(UART_BAUDRATE);


    const char hello_block[] = "hello UART [blocking]\n";
    const char hello_dma[]   = "hello DMA [Non blocking]\n";

    u32 counter = 0;
    while(true)
    {
        uart.write_blocking(std::span{hello_block});
        sleep_ms(1000);
        uart.send_dma(std::span{hello_dma});
        while(uart.dma_tx().is_busy())
        {
            counter++;
        }
        printf("Counted up to: %u \n", counter);
        counter = 0;

        sleep_ms(1000);

    }
}