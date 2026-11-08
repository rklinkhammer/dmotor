#pragma once
#include "monitor_cycle.hpp"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
namespace dmotor_monitor {
// Physical polling adapter. Does not replace required STOP/HALT/fault ISR path.
class EspMonitorInputs final : public MonitorInputs {
    adc_oneshot_unit_handle_t unit_=nullptr;
    unsigned population_;
public:
    explicit EspMonitorInputs(unsigned population):population_(population) {}
    bool start();
    bool adc(unsigned channel,int& raw) override;
    uint32_t faults() override;
};
} // namespace dmotor_monitor
