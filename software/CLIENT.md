# Daemon client library

> Architecture update (2026-11-07): main uses the [1 ms PS motor executor](../DMOTOR_ARCH.md) with independent PL capture/PWM/protection and a matched PSF1 driver/daemon. Client contracts, commissioning-first scope, U0 evidence, the hardware pause and pending live recovery are unchanged.

**All applications that communicate with `dmotord` must use `dmotor_client`.**
Do not copy socket code, serialize `Request` yourself, or use the driver to bypass
the daemon. `dmotor-maint` is the first consumer. Independent protocol test peers
may deliberately construct packets to test malformed input/output.

The separate UIO inspection/encoder utilities are inhibited board diagnostics, not
daemon clients. They cannot coexist with the production driver binding and are
not templates for new control applications. The daemon itself owns `KernelDevice`
and `Runtime`; it does not call itself through the client library.

## Build and use

The Linux C++14 static library exports the standalone header
`dmotor_sw/client.hpp` and CMake target `dmotor::client`. It has no dependency on
Runtime, PL headers or the kernel backend. In a source build use
`target_link_libraries(my_app PRIVATE dmotor::client)` after adding the software
project. With an installed SDK:

```cmake
find_package(dmotorClient CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE dmotor::client)
```

```sh
cmake -S software -B build/ps-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/ps-release -j4
cmake --install build/ps-release --prefix "$PWD/build/ps-sdk"
# Configure an external application with -DCMAKE_PREFIX_PATH="$PWD/build/ps-sdk"
```

EDF staging includes the library and package metadata. The Yocto recipe provides
`dmotor-client-dev` (header/CMake package) and `dmotor-client-staticdev` (archive,
depends on the development package). Include both in the target SDK when building
external ARM clients. Native-built archives cannot be used for ARM targets. The
maintenance executable links the archive; no new shared runtime library is needed.

A minimal status-only application (daemon must already be running):

```cpp
#include <dmotor_sw/client.hpp>
#include <iostream>

int main() {
    try {
        dmotor_sw::Client controller; // Default /run/dmotor/control.sock; admission ping.
        const auto state = controller.status();
        if (!state.okay) {
            std::cerr << state.message << '\n';
            return 1;
        }
        std::cout << "Applied sequence: " << state.applied << '\n';
        // Explicit quit obtains a HALT acknowledgment; the destructor cannot.
        return controller.quit().okay ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

Connecting does not prepare or enable motion. Even a status-only client acquires
the daemon's sole-client slot; this is not a concurrent observer API.

## Calls and result semantics

| Method | Meaning |
|---|---|
| `ping()` | Renew the daemon client lease explicitly |
| `status()` | Read status, measured milliRPM and signed position counts |
| `prepare()` | Explicit global PL preparation, not completed monitor/ARM recovery |
| `enable(mask)` | Request enable of prepared channels; readiness is asynchronous |
| `velocity(mask, targets_mrpm)` | Set four signed milliRPM values, selected by mask |
| `stop(mask)` | Controlled STOP; current runtime requires rearm afterward |
| `disable(mask)` | Disable selected motors and remove intent |
| `halt()` | Global HALT; connection stays open |
| `duty(value_q16)` | Signed duty, ±65536 = ±100%; daemon must be in direct-duty mode |
| `inject_target_timeout()`, `inject_watchdog_timeout()` | Explicit inhibited commissioning tests |
| `quit()` | Request acknowledged global HALT, then close; check `okay` |
| `disconnect()` | Close immediately without waiting for acknowledgment |

Every command returns `ClientResult`: `okay`, raw `status`, global `applied`
sequence and a bounded decoded `message`. Only `status()` populates
`measured_mrpm[4]` and `position_counts[4]`. Those numbers retain the service's
existing limitations: it does not yet return full telemetry validity/fault detail.
Zero fields on other replies are not observations. `okay` acknowledges handler
completion, not physical motion, target application or electrical safety.

A daemon rejection returns `okay=false`; the daemon attempts HALT and the
connection remains usable for explicit inspection/recovery. It is not an exception.
Invalid local masks or duty ranges throw `std::invalid_argument` before sending
anything and do not change daemon state. The daemon remains authoritative for
configuration, readiness, per-motor limits and commissioning policy.

Transport failure, deadline expiry or malformed response throws `ClientError` and
closes the connection. The operation's outcome may be unknown. Schema 1 has no
request IDs, so a timeout permanently invalidates that connection; late replies
cannot be consumed by subsequent calls. There is no automatic command replay,
reconnect, prepare or enable. Reconnecting requires a new Client and explicit
lifecycle decisions. `connected()` reports local descriptor state, not live peer
health; failures are detected by subsequent calls.

## Ownership, deadlines and lease

A Client is noncopyable/nonmovable and has one serialized caller. Do not call it
concurrently, from signal handlers, or share it with forked children. It owns a
nonblocking, close-on-exec SOCK_SEQPACKET socket; send/receive waits use one absolute
monotonic deadline per request and handle EINTR without extending that deadline.
It performs an admission ping during construction to detect refused ownership.

`ClientOptions` defaults to 250 ms for ordinary requests and 2000 ms for prepare,
which can spend 1500 ms waiting for monitor health. Both values must be positive
and fit the platform poll timeout. These are host waiting budgets, not real-time
execution guarantees; choose them against commissioned server timing.

No background heartbeat thread runs. Service the application event loop and call
`ping()` at `Client::recommended_ping_ms` (100 ms) intervals while idle; the daemon's
current lease is 500 ms. Ordinary requests also renew the lease. A stalled caller
must not be hidden by a separate always-running ping worker. Keep slow application
work off that event loop. A synchronous prepare is a special daemon lifecycle
operation that renews the runtime lease when it completes.

The destructor only closes, allowing the daemon's ownership-loss handling to
assert HALT. It cannot confirm HALT and does not throw or block for a reply. Use
`halt()` or `quit()` explicitly and check the result when confirmation is needed.
A failed quit/transport error must never be reported as confirmed HALT.

## Validation and extension

Native tests run every typed operation against an independent protocol peer,
check signed position extremes, rejection vs transport failure, prepare's longer
deadline, malformed/oversized/truncated replies, disconnect and timeout closure.
Existing daemon/maintenance process tests exercise the library against the real
service with a test-only device. An installed-consumer test builds through
`find_package` using only the public header and exported library.

Add future daemon operations to this library and its tests before exposing them
in applications. Keep `service_protocol.hpp` internal to daemon/client transport
and protocol tests; it is deliberately not installed as an application API.
A wire change requires a schema/version plan, not merely another wrapper method.
See the [controller programming guide](../docs/controller_programming.md) for
lifecycle, units and current monitor recovery limitations.

## Monitor recovery status

`Client::recovery_status()` reports monitor admission independently of PL readiness.
The current daemon explicitly returns `RecoveryState::unavailable` and
`admission_open=false`; `prepare()` remains PL-only. Older daemons reject this
schema-2 query and the client disconnects without retry. See [the G1a API contract](../docs/g1a_client_status.md).
