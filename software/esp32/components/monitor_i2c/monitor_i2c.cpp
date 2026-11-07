#include "monitor_i2c.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#if CONFIG_FREERTOS_HZ != 1000
#error "Operational acquisition adapter requires 1000 Hz ticks; diagnostic config is separate"
#endif
namespace dmotor_monitor {
Us EspClock::now() { return esp_timer_get_time(); }
bool EspBus::read(unsigned device, std::uint8_t reg, int wait,
                  std::uint16_t& value) {
    if (device >= devices_.size() || !devices_[device] || wait <= 0 || wait > 2 ||
        pdMS_TO_TICKS(wait) == 0) return false;
    std::uint8_t bytes[2] = {};
    if (i2c_master_transmit_receive(devices_[device], &reg, 1, bytes, 2, wait) != ESP_OK)
        return false;
    value = static_cast<std::uint16_t>((static_cast<unsigned>(bytes[0]) << 8) | bytes[1]);
    return true;
}
}
