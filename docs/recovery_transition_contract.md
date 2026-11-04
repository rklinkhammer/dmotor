# Joint recovery and coast-request transition contract — PROPOSED v4

Status: **PROPOSED v4, 2026-11-03; design corrections, no implementation or
physical qualification.** Reviewed against v3 at dmotor `d1295a3`. This is the
single current recovery design; prior review findings remain in the
[review record](software_architecture_review_2026_11_03.md). The earlier
claim that v3 had resolved every issue is replaced by the explicit remaining
gates below. Changes here do not approve a powered commissioning release.

The ESP32-S3 firmware, host-testable safety core, UART protocol and tests **will
be included in the dmotor repository**, under [software/esp32](../software/esp32/README.md).
Linux software remains under `software/`; PL integration remains under `rtl/`
and `platforms/`. `motor-interface-integrated` owns PCB and assembly artifacts,
not a second firmware implementation or competing recovery specification.

## 1. Authority and physical behavior

**HB proves completed monitoring work, not permission to move.** The monitor
may emit HB in SAFE or FAULT if required measurements are fresh and the cycle
completed within its budget. A latched operational fault alone does not stop HB;
missing/stale measurements or a failed cycle do. Permits are low outside ARMED.
A monitor-only fault need not clear PL intent immediately: UART fault events
are best-effort during operation. Recovery must clear and hold PL intent before
permits can return. UART is required equipment for boot arming and recovery.

**COAST_REQ is global.** It drives the existing monitor HALT_IN/Q2 gate and
asserts for PS deliberate HALT, PL local protective faults or HB-loss latch:
`COAST_REQ = PS_HALT OR LOCAL_FAULT OR HB_FAULT`. Cause attribution remains
separate from outward inhibition. Ordinary monitor faults do not echo through
this expression; monitor permits already inhibit the drivers.

| Operation | Output behavior on rev A |
|---|---|
| Global HALT | PWM inhibited; COAST_REQ asserted; monitor hardware gate drops all permits |
| Selected-motor controlled STOP | Existing STOPPING deceleration, then READY with EN inactive; no COAST_REQ; peers continue |
| Selected-motor DISABLE | Immediate channel output inhibit through the independent safety path; no ramp or zero-velocity PI command; no COAST_REQ |
| READY / disabled EN with monitor permit high | PH/EN braking, not physical coast; report this explicitly |
| Physical STOP switch | Passive all-board coast; monitor records the event and invalidates recovery eligibility |

Per-motor permit hardware exists, but this contract grants only a global monitor
arm/fault policy; it defines no per-motor coast API. Global HALT is the supported
operator request for coast. These semantics retain the PL lifecycle
DISABLED/READY/RUNNING/STOPPING/HALTED. Direct-duty STOP retains its separate
immediate-disable behavior. Neither zero PWM nor zero target confirms coast.

**Reset and clock-loss limitations remain open (R4).** An absent/unconfigured
FPGA does not reliably assert the monitor coast line on the ordered board.
EN/PH pulldowns apply to undriven pins, not frozen active outputs. CORE_RESET,
fabric reset, reconfiguration, clock loss and power loss must be tested as
separate cases. There is no asserted shutdown-time bound for PL-only failure.
A monitor recovery interlock cannot supply that bound. XDC PULLUP is not a
pre-configuration mitigation; configuration pulls depend on PUDC_B and supply
sequencing. An external bias network is only a candidate hardware change, not
a qualified solution for all failure classes. Planned reconfiguration requires
physical STOP held throughout; PS HALT alone may disappear with configuration.
Closing R4 requires independent PL-progress supervision or an explicitly adopted
narrower failure model. This revision does neither silently.

## 2. State and invariants

Monitor states: BOOT, SAFE, ARM_PENDING, ARMED, FAULT. SAFE means permits low,
not permission to arm. BOOT initializes `exchange_completed=false`, clears
pending transactions and obtains a fresh endpoint session. It cannot restore
saved motion/arming state. All faults are checked as levels as well as edges.

A newly observed fault event in any state atomically drops permits, advances
`recovery_epoch`, invalidates pending/completed exchanges and ARM eligibility,
and records cause bits. Repeated sampling of the same continuously asserted
cause does not endlessly advance the epoch; a newly asserted cause or a new
fault episode does. The ISR owns immediate inhibit and event capture; the task
owns recovery transitions. Both use one synchronization discipline. Fault wins
against COMMIT, ARM and permit writes in the same critical section. New faults
in FAULT still invalidate the current transaction.

PL latches are LOCAL_FAULT, HB_FAULT and PS_HALT. HB_QUALIFIED is false on reset,
CORE_RESET or HB loss. Initially require 500 ms of qualified HB before motion;
the first-edge observation timeout is 1 s from qualifier startup, not from motor
enable. Thereafter a 50 ms absence latches HB_FAULT. These are proposed timing
values, not measured bounds. Clear HB_FAULT only by explicit PS acknowledgment
with the qualified window satisfied; expiration wins simultaneous clear.
A separate monitor qualifier must continue observing HB while the motor core is
held reset so recovery cannot wait on a clock/reset domain it has disabled.

The PS recovery hold closes command admission, flushes all queued intent and
keeps every motor disabled through cleanup, exchange and physical ARM. It is
not implemented by leaving PS_HALT asserted throughout, because COAST_REQ must
be released for the monitor exchange. Admission remains closed independently.
No client heartbeat, reconnect, old command or automatic retry may remove it.

Monitor permit inhibition is immediate by its detection path; a monitor-only
fault does **not** imply immediate PL state cleanup. PL inhibit immediately gates
outputs and invalidates intent; reset/session recovery subsequently clears PI
and engine state before attestation. Do not claim acknowledgment writes or a
single status read prove unexposed internal state was cleared.

## 3. Acyclic recovery and motion admission

1. **Enter recovery hold.** Operator requests recovery. PS asserts HALT, disables
   all channels and discards queued commands. Monitor drops permits and records
   any new HALT edge even in SAFE/ARM_PENDING. A monitor fault remains latched
   until the exchange, but a healthy monitoring task may continue HB.
2. **Clean PL state without requiring monitor permission.** Use the existing
   reset/drain/session/configuration sequence, then acknowledge cleared local
   causes, release deliberate PS HALT and acknowledge qualified HB loss. Reuse
   `DMOTOR_RELEASE_HALT` and `Runtime::prepare()` as implementation extension
   points, not as already-complete implementations of this contract. New local
   faults abort recovery. COAST_REQ deasserts once its sources clear. Motors
   remain disabled and admission closed. Verify readable session/configuration,
   disabled status and drained/reset completion. If the ABI cannot establish a
   required condition, add observable completion or a verified reset guarantee
   before implementing attestation; do not invent PI readback.
3. **Discover and begin one exchange.** Establish peer sessions as in §4. PS
   sends RECOVER_REQ with its current PL session and verified cleanup flags.
   Monitor accepts only in SAFE or FAULT, with permits low, healthy inputs,
   STOP release qualified and HALT_IN low. It captures the current epoch and
   returns RECOVER_STATE. Unresolved local faults refuse the request.
4. **Commit the captured epoch.** PS sends RECOVER_COMMIT for the exact pending
   tuple (both endpoint sessions, epoch, transaction). Monitor rechecks all
   recovery preconditions atomically with commit. It enters SAFE, marks the
   tuple completed and sends COMMIT_ACK. Stale, unknown, superseded or faulted
   tuples cannot commit. Duplicate COMMIT may repeat the recorded reply but
   cannot clear a newer fault or execute again.
5. **Reconcile uncertainty while disabled.** A lost COMMIT_ACK means UNKNOWN
   to PS, not presumed FAULT. PS keeps the hold and queries the exact tuple.
   Replies identify the completed tuple, current epoch and state. Monitor may
   be SAFE, ARM_PENDING or ARMED by then; an ARMED reply may confirm the same
   completed transaction without requiring PS to have seen an earlier SAFE.
   New epoch/session invalidates the attempt. No automatic new recovery starts.
   Query polling is bounded (100 ms, three attempts); failure reports UNKNOWN
   and asserts global HALT, retaining the hold until a new explicit recovery.
6. **Physical ARM and RECOVERY_COMPLETE.** ARM requires an observed released
   button after commit, a debounced press then debounced release (20 ms each),
   and ten consecutive healthy 10 ms checks with HALT_IN low. A held button
   across fault/recovery is not eligible. Any failure invalidates eligibility
   and drops permits. PS issues a fresh STATE_QUERY challenge and accepts only
   its matching reply confirming ARMED, the same completed tuple and epoch.
   It then rechecks local PL prerequisites and transitions RECOVERY_COMPLETE.
   Admission opens for a **new explicit enable and new command**, with no replay
   of pre-recovery intent. Before this transition UART loss retains the hold;
   after it UART telemetry loss alone does not trip active motion.

Monitor permits can be high before the PS knows ARM completed; this is safe only
because the PS recovery hold keeps outputs disabled. A subsequent monitor fault
revokes permits even if a late ARMED reply reaches PS. Restoring those permits
always requires another verified cleanup/exchange. Peer restart detected during
recovery invalidates completion. PL-only failure while already running remains
R4, not a guarantee supplied by this protocol.

## 4. Protocol freshness, framing and failure rules

One pending transaction per endpoint; no concurrent recovery. Endpoint sessions
identify a **service/firmware lifetime**, separately from the PL engine session.
Use persistent monotonic 64-bit endpoint session IDs allocated before discovery;
if unique allocation/integrity cannot be established, refuse arming. Do not reuse
an old session on process restart or counter exhaustion. NVS/session allocation
occurs only inhibited. No active counter wraps: epoch, txn and seq are 32-bit;
exhaustion enters inhibited maintenance and requires a fresh session. This avoids
ambiguity from modular sequence windows and evicted duplicate-cache entries.

Discovery is explicit HELLO/HELLO_ACK while inhibited. HELLO uses peer echo zero
and grants no recovery/ARM authority. Discovery must confirm both identities
before accepting control frames. During an established pairing, mismatched or
malformed frames are discarded and logged; they cannot replace the pairing or
clear state. An explicit re-pair operation first inhibits and invalidates all
transactions. A valid active-peer restart discovery request is treated as a
request to inhibit/re-pair, never permission to move. Define its implementation
with the same fail-closed behavior; arbitrary old HELLO frames cannot authorize.

All accepted control frames must match both established sessions. Transaction
identity is (PS session, monitor session, monitor epoch, PS txn). RECOVER_REQ
has no authoritative epoch until RECOVER_STATE returns it; COMMIT must match
both the captured and current epoch. PS retains its recovery hold throughout.
For each direction, accept increasing seq only; an exact duplicate may return a
cached response without mutation. Lower or changed-content duplicate sequences
are rejected. Retain current/completed transaction outcomes until invalidated;
a cache miss must never cause a duplicate mutation to re-execute.

Frames: `0x7E` boundaries, `0x7D` escaping with XOR `0x20`, resynchronize on a
boundary and reject malformed escapes/lengths. After unescaping:
`ver:u8 | src_session:u64 | peer_echo:u64 | epoch:u32 | txn:u32 | seq:u32 |
type:u8 | len:u8 | payload[0..64] | crc:u16`.
All multibyte fields little-endian except CRC big-endian. CRC-16/CCITT-FALSE:
poly 0x1021, initial 0xFFFF, no reflection/final XOR, over ver through payload.
Decoded maximum is 97 bytes; escaped maximum including two boundaries is
196 bytes. UART: 115200 8N1. Fixed storage; reject oversize before copying.

Messages: HELLO, HELLO_ACK, RECOVER_REQ, RECOVER_STATE, RECOVER_COMMIT,
COMMIT_ACK, STATE_QUERY, STATE_REPLY, NAK, and advisory FAULT_EVT/telemetry.
STATE_QUERY/REPLY must carry a fresh PS challenge and the completed tuple/state;
attestation includes PL session, outputs-disabled, reset/drain-complete and
causes-cleared flags. A reply never serves as a motion command. Numerical type
IDs, exact payload layouts, fault-bit allocation and golden CRC/escape vectors
must be frozen in a shared schema in dmotor before firmware implementation;
this document defines behavior, not a completed wire ABI.

Timeout: 100 ms per step, at most three transmissions of the same logical
request. Retransmissions preserve identity and content. Exhaustion does not
assume rollback of a committed monitor state: use step 5 above, then fail closed.
Remote replies cannot renew the maintenance-client lease or FPGA watchdog.

## 5. Timing and remaining implementation gates

All values here are proposed acceptance targets. ISR inhibit target: ≤100 µs
from a qualified nFAULT/ALERT/STOP/HALT edge to permit clear; measure with actual
ISR/cache/flash conditions. Level polling catches already-active sources and
missed edges; STOP polling must clear permits in the same cycle, not wait for
release debounce. Fault detection is enabled in every state after safe GPIO
initialization; no boot exception may raise permits.

R5 must establish the 10 ms absolute cycle budget. With a 25 ms sensor-age limit,
three stale observations and at most one subsequent output cycle, 65 ms from
last valid conversion is a conservative proposed bound only if cycle and sample
age bounds are proven. Drop permits in the detecting cycle where possible.
HB cannot toggle after stale/failed/over-budget cycles. For battery LVC, the
initial 500 ms persistence plus two 10 ms scheduling allowances gives a proposed
520 ms bound from the first qualifying sample, excluding front-end sample delay.
Imbalance persistence is a separate unresolved threshold-policy decision; do
not silently assign the LVC duration to it. The external 2.25 s watchdog bound
and proposed 500 ms task-WDT reset remain separate from these detection budgets.

DISABLE uses the independent safety gate without waiting for a control frame,
HLS completion or PWM period; qualify bus-command-to-inhibit latency separately.
Controlled STOP uses its existing ramp/settle deadline. Do not label either a
measured electrical coast response without connector/DRVOFF observations.

Open implementation gates: R2 pin map/PL qualifier/ABI; R4 independent PL-failure
response; R5 acquisition budget; R6 pinned IDF and ISR/flash isolation; R7 bounded
Linux logging; R8 validated configuration/thresholds. Repository placement is
settled; ESP32 firmware is still unimplemented. A runnable shared simulation and
scope evidence are required before closing system findings R1/R3.

## 6. Required verification

Host state-machine/protocol tests must cover both boot orders, monitor reset
shorter than the HB timeout, session allocation failure, held ARM, faults in
SAFE/ARM_PENDING/FAULT and between every recovery message, stale COMMIT after
epoch change, queued commands during hold, duplicate/mutated/reordered frames,
old sender and receiver sessions, counter exhaustion, lost ACK followed by
STATE_REPLY in SAFE or ARMED, query exhaustion, malformed/oversize frames, and
no automatic retry/restart. Replayed discovery cannot authorize movement.

RTL/integration tests must cover software HALT to outward COAST_REQ, qualified
HB startup/expiry/clear races, CORE_RESET recovery, selected-motor STOP with peers
running, immediate DISABLE, late HLS results and a new fault during cleanup.
Hardware checks observe PWM, COAST_REQ, permits and DRVOFF together. Characterize
configuration/reset/power/clock-loss cases separately; evaluate any candidate
external bias network with measured base current and ENABLE voltage. There is
no XDC-only pre-configuration qualification path. Test stdout backpressure,
I2C faults, monitor hang and watchdog recovery without reset before powered use.
