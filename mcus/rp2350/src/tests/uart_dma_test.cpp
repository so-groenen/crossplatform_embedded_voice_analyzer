
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
#include "drivers/async_dma_handler.hpp"
#include "drivers/dma.hpp"

#include "drivers/uart.hpp"


using namespace hardware::uart;



constexpr size_t N_channel = 1;

class UartIrq : public DmaHandlerBase<DMA_IRQ_0, N_channel>  
{
public:
    explicit UartIrq(irq_handler_t handler)
        : DmaHandlerBase<DMA_IRQ_0, N_channel>(handler) 
    {
    }
    inline void handle_inst(size_t n) override 
    {
        printf(">> !! << \n");
    }
};


static void __time_critical_func(my_handler)();
UartIrq irqs{my_handler};
void __time_critical_func(my_handler)()
{
    irqs.handle_isr();
}



constexpr u32 UART_BAUDRATE = 115200;
constexpr u32 DMA_CH0       = 0;

int main() 
{
    stdio_init_all();
    
    auto uart = Uart<IOMode::NonBlocking, TransferMode::Tx>::make(Dma{DMA_CH0});
    uart.set_baudrate(UART_BAUDRATE);
    
    irqs.bind_interrupt(&uart.dma_tx());
    uart.dma_tx().enable_channel(true);

    const char hello_block[] = "hello UART [blocking]\n";
    const char hello_dma[]   = "hello DMA [Non blocking]\n";

    u32 counter = 0;
    while(true)
    {
        uart.write_blocking(std::span{hello_block});
        sleep_ms(1000);
        uart.write_dma(std::span{hello_dma});
        while(uart.dma_tx().is_busy())
        {
            counter++;
        }
        printf("Counted up to: %u \n", counter);
        counter = 0;

        sleep_ms(1000);

    }
}