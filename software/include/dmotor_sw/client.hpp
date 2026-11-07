#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace dmotor_sw {
// Transport/protocol failure: outcome may be unknown; the connection is closed.
class ClientError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
struct ClientOptions {
    uint32_t response_timeout_ms = 250;
    // PL prepare can wait 1.5 s for monitor health, unlike ordinary commands.
    uint32_t prepare_timeout_ms = 2000;
};
// This is monitor recovery admission, independent of PL readiness or enable bits.
enum class RecoveryState : uint32_t {
    unavailable = 0,
    held,
    pairing,
    recovering,
    waiting_arm,
    verifying,
    admitted,
    failed,
    unknown
};
struct RecoveryStatus {
    RecoveryState state = RecoveryState::unavailable;
    bool admission_open = false;
};
struct ClientResult {
    bool okay = false;
    uint32_t status = 0;
    uint32_t applied = 0;
    // Populated only by status(); other operations do not return observations.
    std::array<int32_t, 4> measured_mrpm{};
    std::array<int64_t, 4> position_counts{};
    std::string message;
};
// Linux local daemon client. Single owner, serialized calls, no background thread.
// No automatic rearm, retry, reconnect or lease renewal. Constructor performs ping.
class Client {
  public:
    static constexpr unsigned recommended_ping_ms = 100;
    explicit Client(const std::string& path = "/run/dmotor/control.sock",
                    ClientOptions options = {});
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(Client&&) = delete;

    ClientResult ping();
    ClientResult status();
    ClientResult prepare(); // PL-only; never implies completed monitor recovery.
    // Schema-2 read-only query. Older daemons disconnect: no fallback or retry.
    RecoveryStatus recovery_status();
    ClientResult enable(uint32_t mask);
    ClientResult velocity(uint32_t mask, const std::array<int32_t, 4>& targets_mrpm);
    ClientResult stop(uint32_t mask);
    ClientResult disable(uint32_t mask);
    ClientResult halt();
    ClientResult duty(int32_t duty_q16);
    ClientResult inject_target_timeout();
    ClientResult inject_watchdog_timeout();
    // Acknowledged all-motor HALT and disconnect. Check result.okay.
    ClientResult quit();
    // Close only; daemon handles ownership loss. Does not confirm physical HALT.
    void disconnect() noexcept;
    bool connected() const noexcept {
        return fd_ >= 0;
    }

  private:
    ClientResult exchange(uint32_t operation, uint32_t mask = 0,
                          const std::array<int32_t, 4>& values = {},
                          RecoveryStatus* recovery = nullptr);
    int fd_ = -1;
    ClientOptions options_;
};
} // namespace dmotor_sw
