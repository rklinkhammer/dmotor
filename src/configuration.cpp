#include "dmotor/configuration.hpp"
#include "dmotor/registers.hpp"
namespace dmotor {
bool Configuration::access(bool write, uint32_t address, uint32_t data, uint32_t safety_state,
                           uint32_t faults, uint32_t live_faults, uint32_t expected_refresh,
                           uint32_t ring_status, uint32_t producer, uint32_t consumer,
                           uint32_t epoch, uint32_t& read_data, uint32_t& action,
                           uint32_t& argument) {
    using namespace reg;
    read_data = 0;
    action = 0;
    argument = 0;
    const bool enabled = (safety_state & 1) != 0;
    if (!write) {
        switch (address) {
            case WATCHDOG_US:
                read_data = staged_timeout_;
                break;
            case MAX_SPEED_MRPM:
                read_data = staged_speed_;
                break;
            case CONFIG_GENERATION:
                read_data = generation_;
                break;
            case ACTIVE_WATCHDOG_US:
                read_data = active_timeout_;
                break;
            case ACTIVE_MAX_SPEED_MRPM:
                read_data = active_speed_;
                break;
            case REFRESH_SESSION:
                read_data = refresh_session_;
                break;
            case EXPECTED_REFRESH:
                read_data = expected_refresh;
                break;
            case SAFETY_STATUS:
                read_data = safety_state;
                break;
            case FAULTS:
                read_data = faults;
                break;
            default:
                break;
        }
        return true;
    }
    switch (address) {
        case WATCHDOG_US:
            if (enabled) {
                return false;
            }
            staged_timeout_ = data;
            return true;
        case MAX_SPEED_MRPM:
            if (enabled) {
                return false;
            }
            staged_speed_ = data;
            return true;
        case CONFIG_COMMIT:
            if (data != 1 || enabled || (ring_status & 3) || producer != consumer ||
                staged_timeout_ < 1000 || staged_timeout_ > 10000000 || staged_speed_ < 1000 ||
                staged_speed_ > 1000000 || generation_ == UINT32_MAX) {
                return false;
            }
            active_timeout_ = staged_timeout_;
            active_speed_ = staged_speed_;
            ++generation_;
            return true;
        case ENABLE:
            if (data != 1 || enabled || !valid() || safety_state || faults || live_faults ||
                (ring_status & 7) != 2 || epoch == 0) {
                return false;
            }
            action = 1;
            return true;
        case REFRESH_SESSION:
            refresh_session_ = data;
            return true;
        case REFRESH_SEQUENCE:
            if (!enabled || refresh_session_ != epoch || expected_refresh == 0 ||
                data != expected_refresh) {
                return false;
            }
            action = 2;
            argument = data;
            return true;
        case ACK_LOCAL:
            if (data == 0 || (data & ~faults) || (data & live_faults)) {
                return false;
            }
            action = 3;
            argument = data;
            return true;
        case ACK_EXTERNAL:
            if (data != 1 || (safety_state & 4)) {
                return false;
            }
            action = 4;
            return true;
        default:
            return false;
    }
}
uint64_t Configuration::timeout_cycles() const {
    return (static_cast<uint64_t>(active_timeout_) * reg::FABRIC_HZ + 999999) / 1000000;
}
} // namespace dmotor
