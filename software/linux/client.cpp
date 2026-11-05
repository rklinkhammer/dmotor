#include "dmotor_sw/client.hpp"
#include "dmotor_sw/service_protocol.hpp"
#include <chrono>
#include <cerrno>
#include <climits>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace dmotor_sw {
constexpr unsigned Client::recommended_ping_ms;
static_assert(Client::recommended_ping_ms == client_ping_ms,
              "Client/service ping cadence mismatch");
namespace {
using Clock = std::chrono::steady_clock;
void wait_ready(int fd, short events, Clock::time_point deadline) {
    for (;;) {
        const auto remaining = deadline - Clock::now();
        if (remaining <= Clock::duration::zero())
            throw ClientError("Service response timeout; HALT not confirmed");
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
        const int timeout = static_cast<int>(ms < INT_MAX ? ms + 1 : INT_MAX);
        pollfd p{fd, events, 0};
        const int result = ::poll(&p, 1, timeout);
        if (result < 0 && errno == EINTR)
            continue;
        // Read a final queued reply even when the peer has also closed (quit).
        if (result > 0 && (p.revents & events))
            return;
        if (result == 0)
            continue;
        throw ClientError("Service disconnected; HALT not confirmed");
    }
}
void valid_mask(uint32_t mask) {
    if (!mask || (mask & ~15U))
        throw std::invalid_argument("Motor mask must be 1..15");
}
uint32_t code(Operation operation) {
    return static_cast<uint32_t>(operation);
}
} // namespace
Client::Client(const std::string& path, ClientOptions options) : options_(options) {
    sockaddr_un address{};
    if (path.empty() || path.find('\0') != std::string::npos)
        throw std::invalid_argument("Invalid socket path");
    if (path.size() >= sizeof(address.sun_path))
        throw std::invalid_argument("Socket path too long");
    if (!options.response_timeout_ms || options.response_timeout_ms > INT_MAX ||
        !options.prepare_timeout_ms || options.prepare_timeout_ms > INT_MAX)
        throw std::invalid_argument("Invalid client deadline");
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    fd_ = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd_ < 0)
        throw ClientError("Cannot create daemon socket");
    try {
        const auto deadline = Clock::now() + std::chrono::milliseconds(options.response_timeout_ms);
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            if (errno != EINPROGRESS)
                throw ClientError("Cannot connect to dmotord");
            wait_ready(fd_, POLLOUT, deadline);
            int error = 0;
            socklen_t size = sizeof(error);
            if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error)
                throw ClientError("Cannot connect to dmotord");
        }
        if (!ping().okay)
            throw ClientError("Daemon refused client admission");
    } catch (...) {
        disconnect();
        throw;
    }
}
Client::~Client() {
    disconnect();
}
void Client::disconnect() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
ClientResult Client::exchange(uint32_t operation, uint32_t mask,
                              const std::array<int32_t, 4>& values) {
    if (!connected())
        throw ClientError("Client disconnected; HALT not confirmed");
    Request request{};
    request.operation = static_cast<Operation>(operation);
    request.mask = mask;
    for (unsigned i = 0; i < 4; ++i)
        request.velocity[i] = values[i];
    const auto budget = request.operation == Operation::prepare ? options_.prepare_timeout_ms
                                                                : options_.response_timeout_ms;
    const auto deadline = Clock::now() + std::chrono::milliseconds(budget);
    try {
        for (;;) {
            wait_ready(fd_, POLLOUT, deadline);
            const auto sent = ::send(fd_, &request, sizeof(request), MSG_NOSIGNAL);
            if (sent < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                continue;
            if (sent != sizeof(request))
                throw ClientError("Service send failed; HALT not confirmed");
            break;
        }
        Response response{};
        for (;;) {
            wait_ready(fd_, POLLIN, deadline);
            const auto received = ::recv(fd_, &response, sizeof(response), MSG_TRUNC);
            if (received < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                continue;
            if (received != sizeof(response) || response.schema != service_schema ||
                response.okay > 1 || !std::memchr(response.message, '\0', sizeof(response.message)))
                throw ClientError("Invalid service response; HALT not confirmed");
            break;
        }
        ClientResult result{};
        result.okay = response.okay != 0;
        result.status = response.status;
        result.applied = response.applied;
        result.message = response.message;
        if (request.operation == Operation::status) {
            for (unsigned i = 0; i < 4; ++i) {
                result.measured_mrpm[i] = response.measured[i];
                const uint64_t p =
                    (uint64_t{response.position_hi[i]} << 32) | response.position_lo[i];
                result.position_counts[i] = p <= uint64_t{INT64_MAX}
                                                ? static_cast<int64_t>(p)
                                                : -1 - static_cast<int64_t>(UINT64_MAX - p);
            }
        }
        return result;
    } catch (...) {
        // Schema 1 has no request IDs. A late reply must never match a later call.
        disconnect();
        throw;
    }
}
ClientResult Client::ping() {
    return exchange(code(Operation::ping));
}
ClientResult Client::status() {
    return exchange(code(Operation::status));
}
ClientResult Client::prepare() {
    return exchange(code(Operation::prepare));
}
ClientResult Client::enable(uint32_t mask) {
    valid_mask(mask);
    return exchange(code(Operation::enable), mask);
}
ClientResult Client::velocity(uint32_t mask, const std::array<int32_t, 4>& values) {
    valid_mask(mask);
    return exchange(code(Operation::velocity), mask, values);
}
ClientResult Client::stop(uint32_t mask) {
    valid_mask(mask);
    return exchange(code(Operation::stop), mask);
}
ClientResult Client::disable(uint32_t mask) {
    valid_mask(mask);
    return exchange(code(Operation::disable), mask);
}
ClientResult Client::halt() {
    return exchange(code(Operation::halt));
}
ClientResult Client::duty(int32_t value) {
    if (value < -65536 || value > 65536)
        throw std::invalid_argument("Duty outside signed Q16 range");
    return exchange(code(Operation::duty), 0, {{value, 0, 0, 0}});
}
ClientResult Client::inject_target_timeout() {
    return exchange(code(Operation::inject_target));
}
ClientResult Client::inject_watchdog_timeout() {
    return exchange(code(Operation::inject_watchdog));
}
ClientResult Client::quit() {
    const auto result = exchange(code(Operation::quit));
    disconnect();
    return result;
}
} // namespace dmotor_sw
