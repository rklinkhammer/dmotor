# G1a post-pairing recovery model

Status: implemented for host review, 2026-11-05. This is a deterministic executable
model of part of [recovery v4](recovery_transition_contract.md), not an adopted
replacement protocol or a live recovery coordinator. Robert Klinkhammer's
separate evidence review remains pending. Relates to A1/A6/A7/A8 and SYS-07/09.

## Scope and boundaries

`software/include/dmotor_sw/recovery_model.hpp` and `software/src/recovery_model.cpp`
are compiled only into `dmotor-recovery-model-tests`. They are not linked into the
daemon/runtime, installed, or connected to UART, MMIO, GPIO or motor commands.
`software/sources.json` includes them so staged builds remain reproducible.
The existing client/daemon schema and PL-only meaning of `prepare()` are unchanged.

Inputs represent already-established facts, not proof that the model performs the
necessary hardware/storage operations. Before integration, an adapter must:

- Allocate fresh durable endpoint sessions while inhibited; complete mutual
  HELLO confirmation. Construct a new model for a new pairing/lifetime, never
  reconstruct the same lifetime with counters silently reset.
- Validate framing, both sessions, increasing reply sequences and exact duplicate
  contents. The typed `Reply` is downstream of these checks; the codec alone does
  not perform them. Mismatched reply identities/challenges cannot change state.
- Hold motion admission independently of PS HALT, disable all motors and discard
  queued intent. Supply a verified CLN1 request/session, complete configuration
  identity, disabled/drained/causes-cleared evidence. No unconditional cleanup
  flags are generated here. `configuration` is an opaque caller-owned identity,
  not a proposed new hardware checksum/register.
- Send returned transmissions using the shared wire schema; a transmission returned
  by `poll()` counts as handed off. UART write failure, local fault, ownership loss,
  explicit HALT/quit, or detected peer restart must call `abort()` and assert HALT.
  Process these events before replies/local confirmation in the same event batch.
- Recheck local prerequisites immediately before final confirmation and perform
  the actual admission transition atomically with fault/ownership handling.
  `halt_required()` is an obligation to inhibit, not a GPIO operation; false never
  commands an automatic release. No old motion command is stored by this model.

The model does not implement ESP32 physical ARM/debounce, permit logic, persistent
storage, discovery, wire replay protection, client leases, or a public recovery API.
It cannot qualify a real recovery exchange on its own.

## Transition rules exercised

| State/event | Result and admission |
|---|---|
| Explicit begin after verified cleanup | RECOVER_REQ; hold retained; reject concurrent attempts |
| Matching RECOVER_STATE, SAFE or FAULT with no active causes | Capture epoch (including legal initial epoch zero), send exact-tuple COMMIT |
| Matching COMMIT_ACK, even ARMED | Send a fresh challenged STATE_QUERY; no admission yet |
| Lost COMMIT_ACK after three transmissions | Query the same transaction; never assume rollback or automatically start another recovery |
| Challenged reply confirms same completed tuple in SAFE/ARM_PENDING | Wait while disabled; caller may explicitly query again with a new challenge |
| Challenged ARMED reply for same completed tuple | Require final local cleanup recheck; still no admission |
| Matching local proof within 100 ms of accepted ARMED reply | Complete; permit only new explicit commands subject to normal Runtime checks |
| Request exhausted | Failed, request HALT, retain hold |
| Query exhausted | UNKNOWN, request HALT, retain hold; delayed replies cannot reopen admission |
| New fault, abort, changed epoch/completed tuple/PL session, failed local proof | Failed, request HALT, retain hold |
| UART silence after complete | Does not itself revoke admission; local protection/ownership checks still apply |

Each logical request gets at most three transmissions, 100 ms apart. Retries keep
the same sequence, tuple, cleanup identity and challenge; new queries allocate a
new challenge/sequence. Expiry wins at the 100 ms reply boundary even if `poll()`
has not yet processed that deadline. No transaction/sequence/challenge counter
wraps; exhausted counters fail closed. Host timestamps must be monotonic; backward
movement and deadline arithmetic exhaustion fail closed.

SAFE/ARM_PENDING waiting has no automatic motion or hidden retry. `query_arm()`
starts one bounded query; silence while waiting retains the hold indefinitely.
The final-local-check 100 ms bound and explicit query scheduling are **model design
choices for review**, not measured timing requirements added to the wire contract.
The model requires increasing PL session and CLN1 request identities across attempts
within one instance. Fabric-counter reset requires a newly allocated endpoint lifetime
and new model, rather than reuse of an old proof.

During the hold, ping/status/capture and inhibit operations are admissible; none
opens admission. Enable/velocity/duty/STOP/timeout injection are rejected. Prepare
may initiate a new attempt only outside an active exchange; from COMPLETE the caller
first enters a new hold via `abort()`, performs fresh cleanup, then calls `begin()`.
Disable must invalidate the attempt if it changes the cleanup assumptions. These
are requirements for the future adapter, not newly deployed Runtime behavior.

## Evidence and next work

[Tests](../software/tests/recovery_model_test.cpp) cover direct success, lost ACK
with SAFE/ARM_PENDING/ARMED outcomes, fresh/stale challenges, late replies, query
exhaustion, incorrect identities, each cleanup flag, changed local proof,
abort in pre-admission phases, initial epoch zero and clock boundaries. The full
native PS suite passes with UBSan enabled. A negative control that opens admission
on COMMIT_ACK fails the hold assertion. See [recorded evidence](evidence/g1a_recovery_model/inputs.json).

This is PS post-pairing coverage, not a runnable two-endpoint simulation. Counter
exhaustion guards exist. The isolated [pairing/replay link](g1a_session_link.md)
now tests duplicate-cache behavior, both handshake orders and peer lifetime
changes. End-to-end counter boundaries, boot ordering, fast monitor restart and
physical held-ARM cases still need the joint endpoint/storage/monitor models. The isolated [PS journal allocator](g1a_endpoint_sessions.md) now supplies an A5
candidate; target/ESP32 durability is still unqualified. The [wire adapter](g1a_recovery_link.md) now connects the link to this model
in scripted exchanges; durable allocation hookup remains open. The
[behavioral monitor simulator](g1a_monitor_simulator.md) now extends joint coverage.
A1's real command queue/hold integration and A8's client schema remain open.
