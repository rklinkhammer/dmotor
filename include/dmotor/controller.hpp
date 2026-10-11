#pragma once
#include "dmotor/motion.hpp"
#include "dmotor/velocity.hpp"
namespace dmotor {
struct ControllerConfig {
    MotionConfig motion{};
    // Bounds: dmotor/timing.hpp; acquisition must be shorter than zero timeout.
    uint32_t zero_timeout_cycles = 0, acquisition_timeout_cycles = 0;
    int32_t acquisition_duty_limit_q16 = 0;
};
struct ControllerResult {
    MotionResult motion{};
    VelocityResult velocity{};
    uint32_t faults = 0;
    bool acquiring = false, warming_up = false;
};
ControllerConfig decode_controller_config(const uint32_t configuration[20]);
void encode_controller_result(const ControllerResult& result, uint32_t reply[24]);
class Controller {
  public:
    ControllerResult step(bool reset, bool authorized, bool capture_fault, uint64_t now,
                          const EncoderSample& encoder, int64_t position, uint32_t session,
                          bool command_valid, const Descriptor& command, bool completion_ack,
                          bool immediate_stop, const ControllerConfig& config);

  private:
    friend class Dispatcher;
    ControllerResult step_impl(bool reset, bool authorized, bool capture_fault, uint64_t now,
                               const EncoderSample& encoder, int64_t position, uint32_t session,
                               bool command_valid, const Descriptor& command, bool completion_ack,
                               bool immediate_stop, const ControllerConfig& config,
                               bool command_checked);
    void prepare(const ControllerConfig& config);
    bool matches(const ControllerConfig& config) const;
    static constexpr unsigned config_words = 17;
    uint32_t prepared_words_[config_words] = {};
    PreparedMotionConfig prepared_motion_{};
    bool prepared_ = false, prepared_acquisition_valid_ = false;
    MotionExecutor motion_{};
    VelocityEstimator velocity_{};
    bool qualified_ = false, acquiring_ = false, primed_ = false;
    uint64_t acquisition_start_ = 0;
    uint32_t faults_ = 0, invalid_ = 0;
};
} // namespace dmotor
// Internal hardware-scheduled interface, not software-visible MMIO.
extern "C" void dmotor_controller(bool reset, bool authorized, bool capture_fault, uint64_t now,
                                  uint64_t count, uint64_t last_edge, uint32_t direction_epoch,
                                  uint32_t invalid_transitions, bool negative, int64_t position,
                                  uint32_t session, bool command_valid, const uint32_t command[16],
                                  bool completion_ack, bool immediate_stop,
                                  const uint32_t configuration[20], uint32_t reply[24]);
