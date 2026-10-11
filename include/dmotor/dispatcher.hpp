#pragma once
#include "dmotor/controller.hpp"
namespace dmotor {
// Serialized bus services and autonomous tick services share ONE ring owner.
class Dispatcher {
  public:
    void service(bool reset, const uint32_t request[4], bool tick, bool authorized, bool cancel,
                 uint32_t cause, bool stop, bool capture_fault, bool drained, uint64_t now,
                 const EncoderSample& encoder, int64_t position, const ControllerConfig& config,
                 uint32_t reply[40]);

  private:
    CommandRing ring_{};
    Controller controller_{};
    ControllerResult last_{};
    bool ack_ = false, cancel_pending_ = false, stop_pending_ = false;
    bool tick_authorized_ = false;
    uint32_t faults_ = 0;
};
} // namespace dmotor
extern "C" void dmotor_dispatcher(bool reset, const uint32_t request[4], bool tick, bool authorized,
                                  bool cancel, uint32_t cause, bool stop, bool capture_fault,
                                  bool drained, uint64_t now, uint64_t count, uint64_t last_edge,
                                  uint32_t direction_epoch, uint32_t invalid_transitions,
                                  bool negative, int64_t position, const uint32_t configuration[20],
                                  uint32_t reply[40]);
