#include "dmotor_sw/ps_device.hpp"
#include "dmotor_sw/ps_engine.hpp"
#include "dmotor/timing.hpp"
#include <chrono>
#include <memory>
namespace dmotor_ps {
namespace {
struct BackendIo : Io {
    dmotor_sw::KernelDevice& device;
    explicit BackendIo(dmotor_sw::KernelDevice& d) : device(d) {}
    uint32_t read(uint32_t a) override {
        return device.read32(a);
    }
    void write(uint32_t a, uint32_t v) override {
        device.write32(a, v);
    }
};
} // namespace
void PsDevice::check() const {
    if (failed_)
        throw dmotor_sw::IoError("PS executor failed; restart explicitly while inhibited");
}
PsDevice::PsDevice(const std::string& path) : device_(path) {
    device_.halt(true);
    if (device_.read32(0x4004) != dmotor::timing::control_period_us)
        throw dmotor_sw::IoError("PS/PL control cadence mismatch");
    // Attach synchronously before starting Runtime. No prepare/enable/HALT release.
    auto io = std::make_shared<BackendIo>(device_);
    auto worker = std::make_shared<Worker>(*io);
    worker->attach();
    thread_ = std::thread([this, io, worker] {
        try {
            while (!stopping_) {
                if (!worker->poll())
                    std::this_thread::sleep_for(std::chrono::microseconds(25));
            }
        } catch (...) {
            failed_ = true;
            try {
                device_.halt(true);
            } catch (...) {
            } // PL frame deadline remains independent.
        }
    });
}
PsDevice::~PsDevice() {
    try {
        device_.halt(true);
    } catch (...) {
    }
    stopping_ = true;
    if (thread_.joinable())
        thread_.join();
}
uint32_t PsDevice::read32(uint32_t a) {
    check();
    return device_.read32(a);
}
void PsDevice::write32(uint32_t a, uint32_t v) {
    check();
    device_.write32(a, v);
}
void PsDevice::fence() {
    check();
    device_.fence();
}
void PsDevice::halt(bool asserted) {
    if (!asserted)
        check();
    device_.halt(asserted);
}
uint64_t PsDevice::now_us() const {
    return device_.now_us();
}
void PsDevice::sleep_us(uint32_t us) {
    check();
    device_.sleep_us(us);
    check();
}
} // namespace dmotor_ps
