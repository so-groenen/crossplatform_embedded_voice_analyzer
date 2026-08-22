
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
 
#include "stats.hpp"
 
  
#include "drivers/dma.hpp"
#include "drivers/dma_irq_handler.hpp"
#include "drivers/spi.hpp"

#define TEST_SIZE 1024

  

 
class DmaEnabledIRQ : public Dma
{
public:
    explicit DmaEnabledIRQ(uint channel, base::IDmaIrqHandler& irq_handler)
        : Dma(channel)
    {
        irq_handler.enable_interrupt(this);
    }
};



constexpr size_t N_CHANNELS = 2;
bind_interrupt_DMA_0(base, dma_irqs, my_handler, N_CHANNELS);
 



int main() 
{
    // Enable UART so we can print status output
    stdio_init_all();
// #if !defined(spi_default) || !defined(PICO_DEFAULT_SPI_SCK_PIN) || !defined(PICO_DEFAULT_SPI_TX_PIN) || !defined(PICO_DEFAULT_SPI_RX_PIN) || !defined(PICO_DEFAULT_SPI_CSN_PIN)
// #warning spi/spi_dma example requires a board with SPI pins
// //     puts("Default SPI pins were not defined");
// #else

    printf("SPI DMA example\n");

//     // Enable SPI at 1 MHz and connect to GPIOs
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

    spi_init(spi_default, 1000 * 1000);
    
    // vvv needs to be called AFTER. calling init again will clear it......
    hw_set_bits(&spi_get_hw(spi_default)->cr1, SPI_SSPCR1_LBM_BITS);
    

    static uint8_t txbuf[TEST_SIZE];
    static uint8_t rxbuf[TEST_SIZE];
    for (uint i = 0; i < TEST_SIZE; ++i) {
        txbuf[i] = rand();
    }

    constexpr uint DMA_CH0{0};
    constexpr uint DMA_CH1{1};
    
    SpiDMA spi_rx{spi_default, Dma{DMA_CH0}};
    SpiDMA spi_tx{spi_default, DmaEnabledIRQ{DMA_CH1, dma_irqs}};





    sleep_ms(3000);
    printf("STARTING NOW\n");
    sleep_ms(1000);
    printf("3\n");
    sleep_ms(1000);
    printf("2\n");
    sleep_ms(1000);
    printf("1\n");
    sleep_ms(1000);
    // We set the outbound DMA to transfer from a memory buffer to the SPI transmit FIFO paced by the SPI TX FIFO DREQ
    // The default is for the read address to increment every element (in this case 1 byte = DMA_SIZE_8)
    // and for the write address to remain unchanged.
    while(true)
    {
        printf("Configure TX DMA\n");
        spi_tx.write_async(txbuf, false);
 
        printf("Configure RX DMA\n");
        spi_rx.read_async(rxbuf, false);

        printf("Starting DMAs...\n");
        // start them exactly simultaneously to avoid races (in extreme cases the FIFO could overflow)

        SpiDMA* instances[] = {&spi_rx, &spi_tx};
        SpiDMA::start_multiple(instances);

        printf("Wait for RX complete...\n");
        spi_rx.dma().await_blocking();


        if (spi_tx.dma().is_busy())
        {
            panic("RX completed before TX");
        }
 
        printf("Done. Checking...");
        for (uint i = 0; i < TEST_SIZE; ++i) {
            if (rxbuf[i] != txbuf[i]) {
                panic("Mismatch at %d/%d: expected %02x, got %02x",
                    i, TEST_SIZE, txbuf[i], rxbuf[i]
                );
            }
        }

        printf("All good\n");
        printf("Next in 3s\n");
        sleep_ms(3000);
    }
    
}