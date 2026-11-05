# G1a cleanup proof and recovery admission specification

**Status: source audit, combined PL cleanup and PS verification implemented; actual-HLS regressions and matched hardware/ARM build passed; system qualification open, 2026-11-04.**
Initial audit baseline: `a334d99`. CLN1 integration below is current source;
Robert Klinkhammer's time-separated review is pending. The PL ABI now exposes
cleanup completion; full monitor recovery admission remains unimplemented. Relates to A1–A4/A8, U02 and SYS-07/09 in the
[G1 work package](g1_work_package.md).

## 1. Baseline audit result: reset assertion is not all-context cleanup completion

The audited baseline deliberately has two different reset mechanisms:

- RTL reset cancels scheduler/adapter/transport state and holds output inhibition.
- Explicit HLS commands initialize mailbox and motor application state. Motor
  initialization is lazy: each enabled motor gets RESET_CONTEXT on its first
  adapter invocation after reset. It is not an all-motor sweep while disabled.

Consequently **the PS must not assert `reset_drain_complete` solely from
MONITOR_STATUS.core_reset, a delay, zero PWM or `Runtime::prepare()` success**.
We have not established generated-HLS data-memory reset guarantees. This is a
proof/observability gap, not evidence that stale drive currently resumes: the
lazy initialization and enable/readiness restrictions are important existing defenses.

## 2. Baseline source-level reset chain and visibility

This table records the pre-CLN1 audit. Sections 7–8 describe its implemented
resolution and remaining qualification gaps.

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

## 3. Cleanup proof requirements

The implementation adds inhibited cleanup with observable completion. It does not
weaken v4 attestation to mean “will clear contexts after enabling.” CLN1 offsets
are defined in the generated controller ABI; monitor wire flags are unchanged.

The implementation must:

1. Require all motors disabled, motion admission held, a nonzero fresh PL session
   and stable configuration. Deny ENABLE while cleanup is active. Keep physical
   output inhibition independent of the internal HLS reset-release needed to run
   cleanup commands; do not enable motors merely to make initialization execute.
2. Cancel/reset outstanding scheduler/transport work and establish that old replies
   cannot satisfy new operations. Initialize the atomic mailbox for this session.
3. Explicitly reset all four motor contexts with valid configuration semantics for
   selected/absent channels. Decide how unconfigured channels are cleared without
   weakening configuration validation; zero configuration is permitted for reset only (section 7).
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
establishes all motor contexts were reset. The combined PL operation and MMIO completion record are implemented below.
Actual generated-motor-HLS and system recovery qualification remain required.

## 4. Attestation proof obligations

| Wire flag | Required evidence immediately before request | Current gap |
|---|---|---|
| outputs_disabled | All STATUS enabled bits zero, exclusive ownership, admission hold prevents enable/target admission, no pending software intent | Admission hold/coordinator not implemented; repeated reads alone cannot replace it |
| reset_drain_complete | Current-session/config-bound cleanup completion, all contexts cleared, old transactions/replies excluded | CLN1 combined sweep/completion exists; generated-motor-HLS evidence and coordinator hold still required |
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

## 7. Implemented motor-context primitive

`dmotor_motor_engine_adapter` now accepts an internal `cleanup_start` pulse only
while idle, all motors disabled, session nonzero and no ordinary start. It captures
the complete configuration bank, clears adapter intent/readiness/telemetry and
issues exactly four RESET_CONTEXT calls with authorization false and no TICK.
Each response must contain only the echoed session, with every other word zero.
The fourth valid response emits one `cleanup_done` pulse; `cleanup_busy` covers
the transaction sequence. A changed session/configuration, enable, competing
start/request, transport fault or malformed response fails cleanup. Reset cancels
it. Changes detected on the final response take priority over success.

Absent channels may use zero configuration for **reset only**. The C++ reset path
clears the mailbox/controller and prepares configuration without authorizing motion.
Native tests exercise previously active contexts, reset with zero configuration,
and confirm a subsequent authorized tick still faults with zero duty. Both direct
and copy-context implementations run under UBSan. Ordinary motor startup retains
its explicit reset/TICK readiness path; cleanup does not set any ready bits.

The primitive is now driven by the combined sequencer in section 8. Its pulse alone
is not recovery attestation; the system also requires atomic-mailbox completion,
request/session/configuration binding and a PS admission hold.

[Primitive test evidence](evidence/g1a_cleanup_primitive/tests.log) and
[source identity](evidence/g1a_cleanup_primitive/inputs.json) cover adapter ordering,
final-response races, malformed/failed replies, mid-sweep reset, and native context
semantics. Those primitive tests use a stub. Subsequent actual product-HLS verification is
recorded in section 8. Neither result alone closes G1a.

## 8. Combined CLN1 integration

On each accepted CORE_RESET 1→0 release, the controller increments a fabric-lifetime
request identifier; exhaustion rejects release instead of wrapping. Reset/session/
configuration setup is serialized through the existing AXI interface. While disabled,
`cleanup_sequencer.sv` waits for prerequisites and atomic-mailbox initialization/drain,
holds the scheduler/PWM inhibited, releases only the motor adapter reset and runs
the four-context sweep. It remembers motor completion while awaiting mailbox
quiescence. The active sequence has a bounded cycle timeout; PS bounds the initial
prerequisite wait. Failure/cancellation remains latched until core reset. External inhibition cancels
proof without echoing external HALT as a new local fault; engine/protocol errors,
changed identity, invalid early admission and active-sequence timeout retain local
fault attribution.

Completion is bound to the captured session and full configuration bank, and is
invalidated by reset, changed inputs, new inhibit/fault, enable or a mutating mailbox
request. Enabling consumes the proof without blocking ordinary operation; disabling
later cannot recreate it. Ordinary startup still executes reset/TICK for readiness.
The [CLN1 register map](controller_axi.md#revision-11-disabled-cleanup) exposes pending,
complete, failed, session and request identity. Direct-duty mode reports unsupported;
it cannot claim this all-context proof. Independent output inhibition stays active
while internal reset is released. Protection latches keep their existing ACK rules.

Velocity-mode `Runtime::prepare()` now requires CLN1, waits within its transition
budget, checks incremented request/session and rereads completion after local health
checks. Failure attempts HALT and keeps preparation false. The kernel driver exposes
only read access to CLN1 registers and requires the capability at probe. The exported
controller IP revision is 11; the deployed revision-10 image is not updated by this
source change. Full UART recovery/ARM admission is still absent, and no wire
attestation flag is emitted by this work.

[Combined evidence](evidence/g1a_cleanup_integration/inputs.json) separates sequencer
unit tests, controller/AXI tests with **actual atomic HLS and a stub motor transport**,
PS failure injection, and a native Linux kernel module compilation. That earlier run did not qualify shared motor HLS RTL, routed timing or the
ARM image. The product run below supplies those build/test results; physical
behavior remains unqualified.

The next product run uses `build/product/engine`, not the historical experiment path.
Its native/HLS C tests and C/RTL co-simulation pass (2,449 shared-context transactions).
The full AXI lifecycle bench and CLN1 bench now pass with both actual generated HLS
kernels, including interruption of an accepted context reset, stale-completion
rejection, protection invalidation and request exhaustion. CLN1 is now a mandatory
actual-HLS product-build test. Startup tests poll completion rather than assuming
that 100 clock cycles clear all contexts. Evidence is recorded in
[product build evidence](evidence/g1a_product_build/).

Matched revision-11 hardware/ARM build passed: routed setup slack +2.406 ns,
hold slack +0.024 ns, no DRC errors, ARM driver compilation and five QEMU semantic
executables passed. UART Lite is built in with console support disabled. See
[current system status](system_status.md) for the exported artifact identities. Continue the remaining boundary/race
qualification and A1/A5–A8 admission, endpoint lifetime and recovery outcome work
before using CLN1 as part of a monitor attestation. G1a remains open.
