#ifndef  EMB_HAL_TESTS_HPP
#define  EMB_HAL_TESTS_HPP

#include "traits.hpp"



namespace embedded_hal::tests 
{

namespace blocking
{
    using namespace embedded_hal::blocking;

    template<class T>
    requires (IsBlockingReadWrite<T>)
    class TransferApp
    {   T m_spi;
    public:
        TransferApp(T spi)
            : m_spi(std::move(spi))
        {
        }
        static void check_buffs(std::span<uint8_t> rbuff, std::span<uint8_t> tbuff)
        {
            printf("Done. Checking...");
            for (uint i = 0; i < rbuff.size(); ++i) {
                if (rbuff[i] != rbuff[i]) {
                    panic("Mismatch at %d/%d: expected %02x, got %02x",
                        i, rbuff.size(), tbuff[i], rbuff[i]
                    );
                }
            } 
            printf("All good!\n");
        }
        static void randomize_buff(std::span<uint8_t> rbuff){
            for (uint i = 0; i < rbuff.size(); ++i){
                rbuff[i] = rand();
            }
        }
        void run()
        {
            constexpr size_t test_size{1024};
            static uint8_t txbuf[test_size];
            static uint8_t rxbuf[test_size];
            while(true)
            {
                printf("-- spi inplace transfer -- \n");
                randomize_buff(txbuf);
                auto res = m_spi.transfer_blocking(rxbuf, txbuf);
                if(!res)
                    panic("transfer error!");
                check_buffs(rxbuf, txbuf);
                FreeRTOS::Task::delay(pdMS_TO_TICKS(3000));
            }
        }
    };
}//blocking



namespace async
{
    using namespace embedded_hal::async;

    template<class T>
    requires (IsAsyncReadWrite<T>)
    class AsyncTransferApp
    {   T& m_spi;
    public:
        AsyncTransferApp(T& spi)
            : m_spi(spi)
        {
        }
        static void check_buffs(std::span<uint8_t> rbuff, std::span<uint8_t> tbuff)
        {
            printf("Done. Checking...");
            for (uint i = 0; i < rbuff.size(); ++i) {
                if (rbuff[i] != rbuff[i]) {
                    panic("Mismatch at %d/%d: expected %02x, got %02x",
                        i, rbuff.size(), tbuff[i], rbuff[i]
                    );
                }
            } 
            printf("All good!\n");
        }
        static void randomize_buff(std::span<uint8_t> rbuff){
            for (uint i = 0; i < rbuff.size(); ++i){
                rbuff[i] = rand();
            }
        }
        void run()
        {
            constexpr size_t test_size{1024};
            static uint8_t txbuf[test_size];
            static uint8_t rxbuf[test_size];
            while(true)
            {
                printf("-- spi async dma transfer -- \n");
                randomize_buff(txbuf);
                m_spi.transfer_await(rxbuf, txbuf);

                check_buffs(rxbuf, txbuf);
                FreeRTOS::Task::delay(pdMS_TO_TICKS(3000));


                printf("-- spi async try blocking transfer -- \n");
                randomize_buff(txbuf);
                m_spi.transfer_blocking(rxbuf, txbuf);
                check_buffs(rxbuf, txbuf);
                FreeRTOS::Task::delay(pdMS_TO_TICKS(3000));
            }
        }
    };
}//blocking


namespace output
{
    using namespace embedded_hal::output;

    template<IsOutput LedType>
    class BlinkyApp
    {
        LedType& m_led;
        uint32_t m_ms_blink;
    public:
        BlinkyApp(LedType& led, uint32_t ms_blink)
            : m_led(led), m_ms_blink{ms_blink}
        {
        }
        void run()
        {
            while (true)
            {
                m_led.set_high();
                FreeRTOS::Task::delay(pdMS_TO_TICKS(m_ms_blink));
                m_led.set_low();
                FreeRTOS::Task::delay(pdMS_TO_TICKS(m_ms_blink));       
            }
        }
    };
}//outout



namespace input
{
    using namespace embedded_hal::input;

    template<IsInput Button>
    class ButtonAnyEdge
    {
        Button& m_button;
    public:
        ButtonAnyEdge(Button& button)
            : m_button(button)
        {
        }
        void run()
        {
            while (true)
            {
                m_button.wait_for_any_edge();
                puts("Click!");
                FreeRTOS::Task::delay(pdMS_TO_TICKS(10));       
            }
        }
    };

    template<IsInput Button, output::IsOutput Led>
    class ButtonFallBlinkApp
    {
        Button& m_button;
        Led& m_led;
    public:
        ButtonFallBlinkApp(Button& button, Led& led)
            : m_button(button), m_led(led)
        {
        }
        void run()
        {
            while (true)
            {
                m_button.wait_for_falling_edge();
                m_led.toggle();
                puts("Click!");
                FreeRTOS::Task::delay(pdMS_TO_TICKS(10));       
            }
        }
    };

    template<IsInput Button, output::IsOutput Led>
    class ButtonRaiseBlinkApp
    {
        Button& m_button;
        Led& m_led;
    public:
        ButtonRaiseBlinkApp(Button& button, Led& led)
            : m_button(button), m_led(led)
        {
        }
        void run()
        {
            while (true)
            {
                m_button.wait_for_rising_edge();
                m_led.toggle();
                puts("Click!");
                FreeRTOS::Task::delay(pdMS_TO_TICKS(10));       
            }
        }
    };
}//input

 



}




#endif//