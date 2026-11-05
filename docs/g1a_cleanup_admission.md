# G1a cleanup proof and recovery admission specification

**Status: source audit and proposed implementation contract, 2026-11-04.**
Prepared against implementation baseline `a334d99`; Robert Klinkhammer's
time-separated review is pending. No recovery admission or PL ABI is changed by
this increment. Relates to A1–A4/A8, U02 and SYS-07/09 in the
[G1 work package](g1_work_package.md).

## 1. Audit result: reset assertion is not all-context cleanup completion

The current design deliberately has two different reset mechanisms:

- RTL reset cancels scheduler/adapter/transport state and holds output inhibition.
- Explicit HLS commands initialize mailbox and motor application state. Motor
  initialization is lazy: each enabled motor gets RESET_CONTEXT on its first
  adapter invocation after reset. It is not an all-motor sweep while disabled.

Consequently **the current PS must not assert `reset_drain_complete` solely from
MONITOR_STATUS.core_reset, a delay, zero PWM or `Runtime::prepare()` success**.
We have not established generated-HLS data-memory reset guarantees. This is a
proof/observability gap, not evidence that stale drive currently resumes: the
lazy initialization and enable/readiness restrictions are important existing defenses.

## 2. Source-level reset chain and visibility

| Boundary | Observed implementation | Proof available / limit |
|---|---|---|
| Local AXI/reset | `rtl/controller_axi.sv`: CORE_RESET writes require disabled channels and nonzero session; HB11 MONITOR_STATUS bit 3 exposes the reset level | Confirms register level, not downstream completed cleanup; core_reset starts high after fabric reset |
| Motor wrapper | `rtl/motor_io.sv`: integrated core reset is `reset || core_reset || direct_active`; direct-duty state/PWM also have reset paths | Removes live RTL drive intent; HALT/protective latches have separate reset/ACK semantics |
| Scheduler | `rtl/shared_scheduler.sv`: reset clears issued/frame tags, staged/committed duty and FSM; normal cancellation uses DRAIN for an accepted call | Global reset cancellation differs from peer-preserving cancellation/drain; do not use DRAIN comments as proof of a global cleanup handshake |
| Motor adapter | `rtl/integrated_control.sv` resets adapter on core reset or engine_reset; `rtl/motor_engine_adapter.sv` clears initialized/ready/target/sequence/result state | First invocation for a context issues RESET_CONTEXT before TICK; no invocation occurs just to sweep disabled motors |
| HLS transport | `rtl/shared_engine_transport.sv`: reset clears transport FSM/result bank and drives HLS ap_rst | Control reset exists; source inspection alone does not establish which synthesized static memories ap_rst resets |
| HLS application state | `experiments/mailbox/shared_mailbox.cpp`: static four-context array; `experiments/mailbox/mailbox.cpp`: explicit reset assigns a fresh VelocityMailbox and resets its controller | A completed per-context reset call is the source-level cleanup operation. Exact prepared/synthesized sources must match this source before using its guarantee |
| Atomic mailbox | `rtl/atomic_mailbox_arbiter.sv` clears initialized on reset, then schedules OP_RESET for a nonzero session and checks reply; `src/atomic_mailbox.cpp` resets all mailbox fields | Initialization is separate from the reset level. Arbiter quiescent exists internally but is unconnected at `rtl/mailbox_control.sv`; there is no exposed combined cleanup-complete token |
| PS prepare | `software/src/runtime.cpp`: config/session readback, monitor ACK/qualification, release/reset operations and mailbox identity checks | Does not perform an explicit motor-context sweep or verify a combined drained/completed token; DMB1 identity alone is not that proof |

The local AXI interface itself is not reset by CORE_RESET. Software must serialize
requests, finish each response, and not pipeline old atomic commands across cleanup.
The current exclusive kernel owner supports serialization, but accepted AXI/HLS
transactions still need integration coverage across reset edges/backpressure.

No minimum CPU sleep is derived here. A synchronous RTL reset requires active
controller clock edges; proving downstream HLS reset/initialization duration requires
the exact generated implementation. A stopped PL clock remains R4, not solved by a
status bit or software wait. Existing monitor qualification may remain active while
the motor core is held reset; preserve that property to avoid circular recovery.

## 3. Proposed proof mechanism — explicit disabled cleanup

**Recommendation for review:** add an explicit inhibited cleanup operation and an
observable completion record, rather than weakening v4's attestation to mean only
“will clear contexts after enabling.” Do not allocate MMIO offsets or wire flags
until the operation and schema impact are reviewed.

The implementation must:

1. Require all motors disabled, motion admission held, a nonzero fresh PL session
   and stable configuration. Deny ENABLE while cleanup is active. Keep physical
   output inhibition independent of the internal HLS reset-release needed to run
   cleanup commands; do not enable motors merely to make initialization execute.
2. Cancel/reset outstanding scheduler/transport work and establish that old replies
   cannot satisfy new operations. Initialize the atomic mailbox for this session.
3. Explicitly reset all four motor contexts with valid configuration semantics for
   selected/absent channels. Decide how unconfigured channels are cleared without
   weakening configuration validation; this remains a design question.
4. Collect actual completion acknowledgments, detect transport/engine errors, and
   expose completion bound to a software-visible request identity/session and
   configuration identity. Clear validity on a new cleanup, reset, session/config
   change, enable, or a new protection event; no counter may silently wrap.
5. Keep output inhibition and PS admission hold after completion. Software verifies
   disabled/session/config/causes and completion together, rechecks before RECOVER_REQ,
   and again before opening admission after fresh ARMED confirmation.

A verified eager reset guarantee with equivalent observability could replace this
mechanism, but requires generated-HLS/RTL evidence for every context and transaction
boundary. Merely exposing existing `quiescent` or `busy==0` is insufficient: neither
establishes all motor contexts were reset. This recommendation is not implemented.

## 4. Attestation proof obligations

| Wire flag | Required evidence immediately before request | Current gap |
|---|---|---|
| outputs_disabled | All STATUS enabled bits zero, exclusive ownership, admission hold prevents enable/target admission, no pending software intent | Admission hold/coordinator not implemented; repeated reads alone cannot replace it |
| reset_drain_complete | Current-session/config-bound cleanup completion, all contexts cleared, old transactions/replies excluded | No combined observable proof or eager all-context sweep; proposed §3 work required |
| causes_cleared | Local fault/HALT/blocked status clear, HB qualified with no HB fault, required cleanup completion still valid | Readable status exists; coordinator must order checks and invalidate proof on new events |

Compose flags only after all obligations succeed. A later fault must invalidate
completion/ARM eligibility and keep admission closed. GPIO release, ACK, configure,
readback and fence failures leave an unknown/failed outcome: attempt global HALT,
retain the hold and report the error, never retry an enable or repeat a commit blindly.
Endpoint session/epoch freshness remains separate A5–A7 work; a PL session token
cannot substitute for peer lifetime identity.

## 5. Proposed admission table (A1/A8)

This specifies desired behavior during a future recovery hold, not current daemon
behavior. Internal coordinator methods and application calls must remain distinct.

| Operation | While recovery hold is active |
|---|---|
| Connect/disconnect, constructor/destructor | Connect cannot release hold; disconnect/destruction inhibits/abandons ownership; no implicit recovery |
| ping / Runtime heartbeat | May maintain client lease only; cannot renew recovery proof, remove hold or generate targets |
| status / capture | Read-only bounded observation; expose recovery state and failure/unknown outcome distinctly; error never opens admission |
| prepare / explicit recover request | Operator-controlled entry into one recovery attempt; reject concurrent attempts; existing PL-only prepare must not be exposed as full recovery success |
| enable, velocity, duty | Reject; do not enqueue for later replay; request rejection cannot imply completion |
| stop | Reject as unavailable while outputs disabled; never use it to release hold |
| disable | Permit only an idempotent inhibit; if it changes cleanup assumptions, invalidate the active attempt |
| halt / quit / ownership loss | Abort attempt, assert global HALT, clear queued intent and keep hold; quit closes ownership |
| timeout injection | Reject during recovery; commissioning policy cannot bypass recovery hold |
| Runtime tick / diagnostics | Process protective deadlines and coordinator progression with bounded work; no ordinary target publication while held |
| RECOVERY_COMPLETE (internal) | Only after verified current cleanup, fresh matching ARMED challenge/session/epoch/transaction and local prerequisites; opens admission for new explicit enable/commands only |

Do not let a generic successful client reply mean RECOVERY_COMPLETE. Define versioned
recovery-state/status semantics before implementing A8. Current schema-1 Client.prepare
continues to mean PL preparation; this audit makes no wire-compatible behavior change.

## 6. Verification performed and remaining cases

[Adapter regression](../tests/motor_engine_adapter_tb.sv) uses a **transport stub**.
It checks all four contexts receive explicit reset before their first subsequent
operation, reset clears adapter state, no context-reset commands execute while
reset is held, and interrupted SEND/WAIT_REPLY/FINISH cannot emit a late completion.
A [negative control](evidence/g1a_reset_audit/negative_control.log) that skips
RESET_CONTEXT fails as expected. This is evidence for adapter ordering only, not HLS memory clearing, complete AXI
cleanup, board timing or an approved attestation mechanism.

[Recorded CTest run](evidence/g1a_reset_audit/tests.log): the new adapter regression,
existing shared-scheduler regression and HB-qualifier regression passed using
Icarus 12.0. [Input identity](evidence/g1a_reset_audit/inputs.json) identifies the
sources, commands and tool package. The cached Icarus path was unavailable; the
tool was extracted into a temporary directory without changing system packages.
Generated HLS RTL was not available at the expected experiment path, so generated
HLS reset/PI clearing was **not** tested or declared passed.

Required next cases after the cleanup mechanism is selected: reset during actual
HLS context read/write and final reply; atomic COMMIT/BEGIN/RETIRE with backpressure;
new fault/session/config changes at every cleanup boundary; failed context reset;
counter exhaustion; disabled output enforcement throughout cleanup and stale
completion rejection. Add an assertion for each attestation obligation, plus
end-to-end actual-HLS integration before closing A2–A4 or G1a.
