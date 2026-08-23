#ifndef API_HPP
#define API_HPP

#include "FreeRTOS/Task.hpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <sys/types.h>
 

namespace embedded_hal
{
    namespace gpio
    {
        enum class Level : bool 
        {
            Low = false,
            High = true,
        };

        enum class Pull : uint8_t 
        {
            None = 0,
            Down = 1,
            Up = 2,
        };
    }

    namespace output
    {
        
        template<class T>
        concept IsOutput = std::move_constructible<T> && requires (T g, gpio::Level level) 
        {
            {g.set_high()} -> std::same_as<void>;
            {g.set_low()} -> std::same_as<void>;
            {g.set_level(level)}-> std::same_as<void>;
            {g.get_output_level()}-> std::same_as<gpio::Level>;
            {g.is_set_high()} -> std::same_as<bool>;
            {g.is_set_low()} -> std::same_as<bool>;
            {g.toggle()} -> std::same_as<void>;
        };




}// output


namespace input 
{
    template<class InputType>
    concept IsAsyncInput = std::move_constructible<InputType> && requires (InputType g) 
    {
        // {g.is_awaiting()} -> std::same_as<bool>;
        {g.wait_for_rising_edge()} -> std::same_as<void>;
        {g.wait_for_falling_edge()}-> std::same_as<void>;
        {g.wait_for_any_edge()}-> std::same_as<void>;
        {g.wait_for_high()}-> std::same_as<void>;
        {g.wait_for_low()}-> std::same_as<void>;
    };

    template<class InputType>
    concept IsInput = std::move_constructible<InputType> && requires (InputType g) 
    {
        {g.is_high()} -> std::same_as<bool>;
        {g.is_low()} -> std::same_as<bool>;
        {g.get_level()} -> std::same_as<gpio::Level>;
    };


}

namespace blocking 
{
    enum class SpiError 
    {
        WriteFailure,
        ReadFailure,
        SizeMismatch
    };

    template<class T>
    concept IsBlockingWrite = std::move_constructible<T> && requires (T g, const std::span<uint8_t> tx_buff) 
    {
        {g.write_blocking(tx_buff)} -> std::same_as<std::expected<void, SpiError>>;
    };

    template<class T>
    concept IsBlockingRead = std::move_constructible<T> && requires (T g, std::span<uint8_t> rx_buff) 
    {
        {g.read_blocking(rx_buff)} -> std::same_as<std::expected<void, SpiError>>;
    };

    template<class T>
    concept IsBlockingReadWrite = IsBlockingRead<T> && IsBlockingWrite<T> && requires (T g, const std::span<uint8_t> tx_buff, std::span<uint8_t> rx_buff)
    {
        {g.transfer_blocking(rx_buff, tx_buff)} -> std::same_as<std::expected<void, SpiError>>;
    };




}

namespace async 
{
    
    template<class T>
    concept IsAsyncWrite = std::move_constructible<T> && requires (T g, const std::span<uint8_t> tx_buff) 
    {
        {g.write_await(tx_buff)};
    };

    template<class T>
    concept IsAsyncRead = std::move_constructible<T> && requires (T g, std::span<uint8_t> rx_buff) 
    {
        {g.read_await(rx_buff)};
    };

    template<class T>
    concept IsAsyncReadWrite = IsAsyncWrite<T> && IsAsyncRead<T> && requires (T g, const std::span<uint8_t> tx_buff, std::span<uint8_t> rx_buff)
    {
        {g.transfer_await(rx_buff, tx_buff)} -> std::same_as<void>;
    };


}


}
#endif 