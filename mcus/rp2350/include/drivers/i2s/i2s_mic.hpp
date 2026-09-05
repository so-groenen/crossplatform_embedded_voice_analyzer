#ifndef I2S_MIC_HPP
#define I2S_MIC_HPP

// #include "drivers/i2s/i2s.hpp"
#include "drivers/i2s/i2s.pio.h"
#include "types.hpp"
#include "drivers/pio.hpp"

struct PioI2sConfig 
{
    u32 sample_rate;
    u32 sck_pin;
    u32 ws_pin;
    u32 data_pin;
};

  
class PioI2sMicrophone : public pio::PioBaseProgram
{
public:
    explicit PioI2sMicrophone(const PioI2sConfig& config)  
        : PioBaseProgram(pio0, &i2s_pio_program)
    {
        assert(config.ws_pin == config.sck_pin +1);
        i2s_pio_program_init(m_pio_inst, m_pio_sm, m_offset, config.sample_rate, config.data_pin, config.sck_pin);
    }
};

#endif //I2S_MIC_HPP