#pragma once
#include "dmotor_sw/device.hpp"
#include "dmotor/controller_registers.hpp"
namespace dmotor_sw {
// Scope owns only encoder reset release; never configures or enables a motor.
class EncoderSession {
public:
    explicit EncoderSession(Device& device) : device_(device) {
        namespace reg = dmotor::controller_reg;
        if (device_.read32(reg::IDENTITY) != reg::PROFILE_ID ||
            (device_.read32(reg::STATUS) & 255U))
            throw IoError("Encoder check requires a disabled, unconfigured DMI2 controller");
        if (device_.read32(reg::SESSION) == 0) {
            device_.write32(reg::SESSION, 1);
            device_.fence();
            if (device_.read32(reg::SESSION) != 1) throw IoError("Cannot establish encoder session");
        }
        device_.write32(reg::CORE_RESET, 1);
        device_.fence();
        active_ = true;
        try { device_.write32(reg::CORE_RESET, 0); device_.fence(); }
        catch (...) { try { close(); } catch (...) {} throw; }
    }
    EncoderSession(const EncoderSession&) = delete;
    EncoderSession& operator=(const EncoderSession&) = delete;
    ~EncoderSession() { try { close(); } catch (...) {} }
    void close() {
        if (!active_) return;
        device_.write32(dmotor::controller_reg::CORE_RESET, 1);
        device_.fence();
        active_ = false;
    }
private:
    Device& device_;
    bool active_ = false;
};
}
