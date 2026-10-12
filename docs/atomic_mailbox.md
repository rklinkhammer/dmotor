# Atomic velocity batch protocol

Revision 0.18's first implementation milestone is a standalone C++ HLS protocol
block, separate from the retained full-profile ring peripheral. It provides the
new batch publication/activation contract and a portable host reference adapter.
It is not yet connected to the shared motor engine, AXI bridge, output commit,
watchdogs or target-age timers.

## Contract

`abi/mailbox.json` defines the distinct `DMB1` identity, development schema 1,
register offsets and rejection codes. `tools/generate_mailbox_abi.py` produces
C++ and SystemVerilog constants and the [register map](batch_registers.md).
The driver verifies identity, schema, channel count and trusted session before
writing. The ring register map is unchanged. This is not a frozen release ABI.

The four-channel `AtomicMailbox` has mutable staging, one immutable pending batch,
one reserved in-flight batch, and applied telemetry. Staging requires session,
sequence, motor mask and generation/signed milliRPM for every selected motor.
Unselected fields are ignored. Only aligned full-word writes are accepted.

A valid full-word COMMIT attempt consumes staging validity whether accepted or
rejected; all required fields must be written again. Invalid byte strobes or
misaligned accesses have no staging effect. Acceptance requires nonzero valid
mask/session, matching per-motor generations, authorization, configured speed bounds,
and a strictly increasing nonzero sequence. Invalid commits preserve pending and
active state. Last rejection is retained until reset or a newer rejection.

A newer valid batch may replace pending only for the identical motor mask.
Overlapping or disjoint different masks are rejected while the single slot is
occupied. A new pending batch may be accepted while an earlier batch is in flight;
it cannot alter that reserved batch. Sequence wrap is rejected; session reuse
requires externally inhibited/drained reset.

`begin_tick` revalidates ownership, range and authorization, and requires all
selected motors ready. Temporary unready status holds pending intact. Invalid
ownership/authorization drops the entire pending batch. One call reserves the
whole batch with a common 64-bit effective tick; application is not yet reported.

`retire` requires matching session/sequence, still-valid admission, and successful
completion of every selected calculation. It atomically updates global and
selected per-motor applied sequences/timestamps. Wrong or duplicate retirement
cannot change applied state or consume a different in-flight batch. A failed
matching retirement invalidates newer intersecting pending work as well.

`invalidate(mask)` discards entire pending/in-flight batches intersecting the mask,
and clears staging validity conservatively because the new staging mask may not
yet have been written. It preserves historical applied telemetry. The wrapper
must separately inhibit outputs, close admission, latch the cause, and clear or
reset algorithm state as required. No accepted/pending/applied status is a claim
that the physical motor achieved its speed or position.

## Integration responsibilities

`BatchAdmission` comes from trusted validated configuration and safety state;
software cannot write it through this register map. Its ready mask expresses
algorithm/lifecycle readiness separately from authorization. During stop/fault,
the wrapper removes admission and invalidates work before evaluating publication
or retirement. Expiration/HALT priority on the same fabric clock is still an
integration obligation, not proven by serialized function calls.

The internal `dmotor_atomic_mailbox` HLS top exercises reset, write, read,
begin, retire and invalidate transactions. It is a scheduled handshake interface,
not an AXI top. Only one call is in flight. Whole-frame PWM commit, four-motor
scheduler timing, target-age renewal and late-result suppression must be wired
before deployment. Per-motor applied tick records supply the future target-age
logic; this module does not implement that independent timer.

`batch_host.hpp` supplies publication and coherent applied-snapshot helpers over
an injected transport. The platform must serialize writers, implement ordered
32-bit device accesses, surface read/write bus errors, and provide a real device
write-completion/order fence. It is a reference driver interface, not a Linux UIO
mapping or kernel driver. Snapshot reads validate global applied sequence and
session before/after reading mask/timestamp; a changed generation requires retry.
Per-motor telemetry uses the same enclosing sequence/session check. No destructive
reads or software-writable trusted session are provided.

Upstream bus draining, cold reset, token reuse, enable/rearm, coherent live
configuration and actual hardware inhibit remain explicit platform obligations.
Standalone C++ tests do not establish AXI stale-write isolation.

## Verification and measurement

All 25 tests in the current native/RTL regression pass. Native protocol tests under UBSan cover partial accesses/staging, all-or-none
validation, same-mask replacement, disjoint/overlap rejection, stale/reordered
sequences, unsigned sequence exhaustion, unavailable motors, generation changes
before activation/retirement, wrong/duplicate/failed retirement, stop/fault
invalidation, reset and host snapshot races. All 60 combinations of 15 nonempty
motor masks and four invalidated channels are checked. Selected per-motor applied
telemetry and unchanged unselected channels are checked separately.

HLS C simulation and Verilog co-simulation pass. The HLS test compares all 16 response words over 1,165 transactions against the
C++ protocol reference. Native assertions provide the semantic checks; the HLS
comparison checks generated-RTL equivalence, not an independently written protocol.

Vitis 2026.1, xc7z020clg400-1, 20 MHz synthesis estimates: **6,249 LUT, 4,507 FF,
0 DSP, 0 BRAM18K**, 52-cycle maximum latency (2.60 us), 53-cycle maximum interval,
19.860 ns estimated critical path. This includes the standalone internal test
interface and separate protocol state. Do not add it to the shared-engine estimate
as a final integrated utilization: future integration can replace existing mailbox
state, and needs additional protection/bus/output logic. No route/board timing
claim is made.

```sh
python3 tools/generate_mailbox_abi.py --check
cmake -S . -B build/atomic_protocol
cmake --build build/atomic_protocol -j
ctest --test-dir build/atomic_protocol --output-on-failure
python3 tools/vitis_check.py --kernel atomic
```

Icarus is required for existing RTL regressions. Evidence is retained in
`docs/evidence/atomic_mailbox/`. The next integration milestone is independent
per-motor target-age protection, followed by the shared scheduler and common PWM
commit. The previous ring/profile implementation remains available throughout.
