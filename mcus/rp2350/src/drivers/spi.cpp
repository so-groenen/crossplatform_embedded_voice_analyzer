#include "drivers/spi.hpp"
 
namespace hardware::spi {



Spi::Spi(spi_inst_t* spi, uint32_t baudrate)
    : m_spi{spi}
{    
}
Spi::Spi(spi_inst_t* spi, const spi::Config& config)
    : m_spi{spi}
{
    // spi_init(m_spi, config.baudrate);
    // init_from_config(m_spi, config);
    spi_set_format(m_spi, 8, config.polarity, config.phase, SPI_MSB_FIRST);
}
void Spi::set_config(const spi::Config& config)
{
    spi_set_baudrate(m_spi, config.baudrate);
    spi_set_format(m_spi, config.data_bits, config.polarity, config.phase, config.order);
}
void Spi::set_config(spi_inst_t* s, const spi::Config& config)
{
    spi_set_baudrate(s, config.baudrate);
    spi_set_format(s, config.data_bits, config.polarity, config.phase, config.order);
}
void Spi::init_from_config(spi_inst_t* s, const spi::Config& config)
{
    spi_init(s, config.baudrate);
    spi_set_format(s, config.data_bits, config.polarity, config.phase, config.order);
}

uint32_t Spi::baudrate() const 
{
    return spi_get_baudrate(m_spi);
}
auto Spi::write_blocking(const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError>
{
    int bytes_written = spi_write_blocking(m_spi, tx_buff.data(), tx_buff.size());
    if(bytes_written != tx_buff.size())
    {
        return std::unexpected{SpiError::WriteFailure};
    }
    return{};
}
auto Spi::read_blocking(std::span<uint8_t> rx_buff) -> std::expected<void, SpiError>
{
    // 0x00 is dummy values that we are sending, because the way SPI works, as data is shifted in, some is shifted out
    // this is how shift registers work.
    int bytes_read = spi_read_blocking(m_spi, 0x00, rx_buff.data(), rx_buff.size());
    if(bytes_read != rx_buff.size())
    {
        return std::unexpected{SpiError::ReadFailure};
    }
    return{};
}
bool Spi::is_readable() const 
{
    return spi_is_readable(m_spi);
}
bool Spi::is_busy() const 
{
    return spi_is_busy(m_spi);
}
bool Spi::is_writable() const
{
    return spi_is_writable(m_spi);
}
auto Spi::transfer_blocking(std::span<uint8_t> rx_buff, const std::span<uint8_t> tx_buff) -> std::expected<void, SpiError>
{
    if(tx_buff.size() != tx_buff.size())
        return std::unexpected{SpiError::SizeMismatch};

    // 0x00 is dummy values that we are sending, because the way SPI works, as data is shifted in, some is shifted out
    // this is how shift registers work.
    int bytes_read = spi_write_read_blocking(m_spi, tx_buff.data(), rx_buff.data(), rx_buff.size());
    if(bytes_read != rx_buff.size())
    {
        return std::unexpected{SpiError::ReadFailure};
    }
    return{};
}
Spi::~Spi()
{
    // if(m_spi)
    //     spi_deinit(m_spi);
}
static_assert(IsBlockingReadWrite<Spi>, "Spi must implement read & write blocking");
};
 

 

 