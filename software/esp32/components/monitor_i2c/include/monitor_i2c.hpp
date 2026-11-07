#pragma once
#include "acquisition.hpp"
#include "driver/i2c_master.h"
namespace dmotor_monitor {
struct EspClock final : Clock { Us now() override; };
class EspBus final : public Bus {
    std::array<i2c_master_dev_handle_t, 4> devices_;
public:
    // Handles must belong to a dedicated synchronous bus (queue depth zero),
    // configured inhibited at 100 kHz / addresses 0x40..0x43. No other caller.
    explicit EspBus(std::array<i2c_master_dev_handle_t, 4> devices) : devices_(devices) {}
    bool read(unsigned device, std::uint8_t reg, int timeout_ms,
              std::uint16_t& value) override;
};
}
