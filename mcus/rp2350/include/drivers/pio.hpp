#pragma once

#include <hardware/pio.h>
#include <concepts>
#include "types.hpp"


namespace pio 
{

class PioBaseProgram
{
protected:
    PIO m_pio_inst{pio0};
    uint m_pio_sm;
    uint m_offset{};
public:
    PioBaseProgram(PIO inst, const pio_program* program)
        : m_pio_inst{inst}, m_pio_sm(pio_claim_unused_sm(m_pio_inst, true)), m_offset(pio_add_program(m_pio_inst, program))
    {
        // uint m_offset = pio_add_program(m_pio_inst, program);
    }
    PIO pio_inst() const 
    {
        return m_pio_inst;
    }
    u32 pio_sm() const 
    {
        return m_pio_sm;
    }
    io_ro_32* get_tx_fifo_reg() 
    {
        return &m_pio_inst->txf[m_pio_sm];
    }
    io_ro_32* get_rx_fifo_reg() 
    {
        return &m_pio_inst->rxf[m_pio_sm];
    }
    u32 get_dreq( bool is_tx)
    {
        return pio_get_dreq(m_pio_inst, m_pio_sm, is_tx);
    }
    void start()
    {
        pio_sm_clear_fifos(m_pio_inst, m_pio_sm);
        pio_sm_restart(m_pio_inst, m_pio_sm);
        pio_sm_exec(m_pio_inst, m_pio_sm, pio_encode_jmp(m_offset));

        pio_sm_set_enabled(m_pio_inst, m_pio_sm, true);
    }
    void stop()
    {
        pio_sm_set_enabled(m_pio_inst, m_pio_sm, false);
    } 
};


template<class T>
concept IsPioProgram = requires (T pio, bool is_tx) {
    {pio.start()}           -> std::same_as<void>;
    {pio.stop()}            -> std::same_as<void>;
    {pio.get_dreq(is_tx)}   -> std::same_as<u32>;
};

template<typename T>
concept IsPioReceiver = IsPioProgram<T> && requires(T pio) {
    {pio.get_rx_fifo_reg() } -> std::same_as<io_ro_32*>;
};

template<typename T>
concept IsPioTransmitter = IsPioProgram<T> && requires(T pio) {
    {pio.get_tx_register() } -> std::same_as<io_ro_32*>;
};

template<typename T, TransferMode M>
concept PioValidateMethod = 
    (M == TransferMode::Rx && IsPioReceiver<T>) || 
    (M == TransferMode::Tx && IsPioTransmitter<T>);

}