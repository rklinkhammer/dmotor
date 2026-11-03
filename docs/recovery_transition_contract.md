# Joint recovery and coast-request transition contract — PROPOSED v2

Status: **PROPOSED v2, 2026-11-03, not approved, no code.** v1 responded to
review findings R1/R3; this revision proposes resolutions to defects **F1–F5**
in [software_architecture_review_2026_11_03.md](software_architecture_review_2026_11_03.md)
and keeps the I10 (non-latching ~WDO) layering. On adoption this supersedes
the recovery prose in DMOTOR_CONTROL §6.4 and monitor FIRMWARE.md §5.
R4 (PL clock-loss supervision) is **explicitly out of scope** here: a
combinational coast request cannot detect a frozen clock; R4 needs its own
architectural decision (UART progress lease vs narrowed failure model).

**Review status:** the [v2 assessment](software_architecture_review_2026_11_03.md#latest-assessment-recovery-contract-v2)
credits the STOP/coast separation and removal of the original latch-clear
dependency, but leaves epoch/commit handling, reset bias and timing claims open.
The design below remains a proposal requiring those corrections.

## 1. Architectural resolutions

**A. HB means liveness, never permission** (unchanged from v1): toggled iff
a safety cycle completed with fresh data; keeps toggling in SAFE *and*
FAULT. Faults are reported by permits + UART status, never by stopping HB.

**A′ (F1). Permits are never restored to an unproven PL.** Because HB is
the only mandatory monitor→PL channel on rev A, monitor faults cannot
compel the PL to drop intent in real time. The guarantee is therefore moved
to where the monitor has authority: **the monitor leaves FAULT only through
the recovery exchange (§4), whose request carries a PS attestation —
session identity, outputs disabled, all targets/PI state cleared.** No
exchange, no SAFE; no SAFE, no ARM; no ARM, no permits. UART thus stays
advisory in operation but is *required equipment for recovery* — a monitor
fault with UART unplugged parks the system in FAULT (coast) until telemetry
is restored. A monitor reset too fast for the PL's 50 ms HB window is
harmless for the same reason: it boots into SAFE with permits low and a new
session id, and reaching ARMED again requires the exchange + ARM. On FAULT
entry the monitor also emits an unsolicited best-effort `FAULT_EVT` frame
(attribution fast path, not relied upon).

**B (F4). COAST_REQ is a GLOBAL emergency line; controlled stops never use
it.** The single Q2 gate inhibits all four motors at once, so COAST_REQ
asserts only for conditions that are already global:

| Condition | COAST_REQ | Rationale |
|---|---|---|
| PS deliberate global HALT (`DMOTOR_HALT`) | **yes** (latched until explicit PS release, §4 stage 1) | R1: a software emergency stop must coast, not brake |
| PL latched local fault (aggregated), watchdog/target-age fault | **yes** | fault ⇒ outputs unproven ⇒ coast |
| PL HB-loss latch | **yes** | no echo risk: HB is independent of HALT_IN |
| Monitor-origin external HALT echo | **no** | no-echo rule preserved; attribution stays truthful |
| **Controlled STOP (any subset of motors)** | **no** | DMOTOR_ARCH §7 deceleration runs under PI (STOPPING), then holds zero-drive; coasting peers mid-motion is the harm, not the cure |
| READY / zero-drive idle | **no** | commanded, attributed state |

Per-motor "disable" on rev A is zero-target + EN=0 (brake/zero-drive at
that motor); **true coast exists only globally** (monitor permits or
COAST_REQ). An operator "coast everything" is expressed as the PS global
HALT. PL state names map to the existing lifecycle (F4): contract RESET ≙
unconfigured/DISABLED, READY ≙ READY (zero-drive, brake permissible and
attributed), RUN ≙ RUNNING/STOPPING (STOPPING retains PWM authority for
deceleration), HALTED ≙ HALTED (COAST_REQ asserted). READY-after-STOP stays
braking unless the operator escalates to global HALT — both states are
explicit, satisfying F4's "both must be expressible".

**B′ (F3). Reset-time bias, as ordered vs desired.** On rev A, an
unconfigured or absent FPGA does **not** assert COAST_REQ (R17 pulls Q2's
base low; XDC uses PULLDOWN; I10 documents "absent FPGA does not inhibit").
v1's "idles asserted" claim is withdrawn. The as-ordered reset behavior is:
PL reset ⇒ PWM ceases, motor-board EN/PH pulldowns give zero drive
(brake-class, stationary-safe) while permits remain whatever the monitor
holds. Mitigations, layered: (i) the A′ interlock bounds the stale-permit
window — permits can only be high if the monitor reached ARMED, and any
monitor fault/STOP revokes them independently of the PL; (ii) **proposed
one-line rework for bring-up qualification: set the PL HALT_OUT pin's IOB
to `PULLUP` in the XDC** — during/before configuration the internal pull-up
(≈14 k) against R16+R17 biases Q2's base ≈2.6 V ⇒ COAST_REQ asserted while
unconfigured (verify the divider on hardware; powered-off FPGA still does
not assert, which remains an R4-class residual). T2/T11 below state only
the as-ordered guarantee; the PULLUP upgrade is a tracked option.

**C. UART: advisory in operation; required and authoritative for the
recovery exchange** (sharpened from v1 per A′).

**D. Non-latching ~WDO layering** (unchanged, I10): 500 ms task WDT under
the 1.0 s minimum external window; boot writes permits low before first HB;
PL HB-loss latch clears only by PS handshake (T4), never by HB resuming.

## 2. State definitions

Monitor: `BOOT → SAFE → ARM_PENDING → ARMED → FAULT(latched, first cause)`.
HB runs in every state after BOOT self-test. Permits high only in ARMED.
`recovery_epoch` counter increments on each FAULT entry; ARM requires an
exchange completed for the current epoch (F1).

PL: existing lifecycle `DISABLED/READY/RUNNING/STOPPING/HALTED` plus
orthogonal latches `LOCAL_FAULT`, `HB_FAULT`, `PS_HALT` (deliberate,
explicitly released), and status flag `HB_QUALIFIED` (set on first
qualified HB after configuration; **motion authorization requires
HB_QUALIFIED, not merely "no HB_FAULT" — an initial grace period never
authorizes motion** (F5)). `COAST_REQ = PS_HALT ∨ LOCAL_FAULT ∨ HB_FAULT`
(combinational, excludes monitor-origin inputs).

Hardware gate: `ENABLE = WDO_ok ∧ ¬HALT_IN`; survives all software states.

## 3. Transition table

Event definitions (F5): *ARM press* = debounced (20 ms) falling edge
preceded by an observed released state **after** the current epoch's
exchange completed — a button held through recovery cannot rearm; *STOP
solid-low* = N=10 consecutive 10 ms samples reading low; *STOP unsafe* =
any single sample not low (assert immediately, release deliberately).

| # | From → To | Trigger | Preconditions | Owner | Bound | On failure |
|---|---|---|---|---|---|---|
| T1 | Monitor BOOT → SAFE | self-test pass | permits written low first; I2C scan = population mask; NVS valid; new session id | monitor | 2 s | hold BOOT, no HB |
| T2 | PL unconfigured → DISABLED | configuration + driver init | outputs inhibited from first clock; **as ordered, COAST_REQ is NOT asserted pre-configuration (B′)** | PL/driver | — | hold DISABLED |
| T3 | PL: HB_FAULT set | no qualified edge in window (50 ms run; first sighting ≤1 s after enable) | HB_QUALIFIED gates motion until first sighting (F5) | PL qualifier | — | latch; COAST_REQ |
| T4 | PL: HB_FAULT clear | PS handshake write after review | qualified HB ≥500 ms; zero motion intent | PS | — | resumed HB alone never clears (D) |
| T5 | Monitor SAFE → ARM_PENDING | ARM press (defined above) | no latched fault; STOP solid-low; packs in window; imbalance <0.3 V; mask satisfied; **exchange completed for current epoch** (F1) | monitor | — | ignore |
| T6 | ARM_PENDING → ARMED | T5 conditions re-verified 10 cycles | HALT_IN low | monitor | 200 ms | → SAFE |
| T7a | ARMED → FAULT (ISR class) | nFAULTn↓, ALERT↓, STOP sample ≠ low, HALT_IN↑ | permits dropped in ISR | monitor | ≤100 µs inhibit; ≤1 cycle confirm/latch | — |
| T7b | ARMED → FAULT (task class) | sensor stale 3-strike; battery/imbalance threshold crossing | detection bound: ≤3 cycles (30 ms) stale; ≤1 cycle after the crossing sample (F5) | monitor | ≤40 ms inhibit worst case | — |
| T8 | Monitor FAULT → SAFE | recovery exchange stages 1–3 complete (§4) | cause cleared; sensors fresh; STOP solid-low; HALT_IN low | monitor | per §4 | remain FAULT; UART loss ⇒ refuse (C) |
| T9 | PS global HALT (any PL state) | `DMOTOR_HALT` | sets PS_HALT latch ⇒ COAST_REQ | PS→PL | PWM zero ≤10 µs; gate inhibit ≤µs (Q2) | release only via §4 stage 1 |
| T10 | PL LOCAL_FAULT clear | existing 3-ack rearm | zero motion intent; targets invalidated; **does not require the exchange** (F2 — ordering fixed: local latches clear first, exchange runs after) | PS | — | latch persists |
| T11 | Either side resets mid-operation | — | peer sees HB loss (T3) or permit drop; **PL death is NOT guaranteed visible to the monitor as-ordered (B′)** — bounded by A′ on next recovery | — | per T3/T7 | no boot-order requirement |
| T12 | Simultaneous HALT_IN + HB expiry | — | recover: T10 → stage 1 → T4 → §4 → T5 | PS then monitor | — | — |

Invariant (scoped per F5): **every transition into or out of an
inhibit/fault/recovery state leaves zero motion intent**; during steady
ARMED operation, intent admissibility is governed by the existing target
freshness/authorization rules, not this table.

## 4. Recovery exchange (staged; resolves F2)

Stage 1 — PS-side cleanup (no monitor involvement): operator/service review;
`T10` 3-ack rearm clears LOCAL_FAULT; **explicit PS_HALT release operation**
(new driver op — absent from v1, F2) clears the deliberate-halt latch;
`T4` handshake clears HB_FAULT if set. COAST_REQ deasserts when all three
components clear. *Nothing in stage 1 requires the exchange; the exchange
requires stage 1 — strictly acyclic.*

Stage 2 — exchange (UART, authoritative):
1. PS→M `RECOVER_REQ {attestation: outputs_disabled, targets_cleared,
   causes_acked, pl_configured}` — sendable only after stage 1.
2. M→PS `RECOVER_STATE {epoch, first_cause, sensors_ok, stop_state,
   halt_in_state}` or `NAK {reason}` (cause persists / stale / STOP / HALT_IN
   still high).
3. PS→M `RECOVER_COMMIT {txn}`.
4. **M→PS `COMMIT_ACK {txn, new_state=SAFE}`** (F2: PS must learn the
   outcome); monitor enters SAFE on sending it. Lost ACK: PS retransmits
   COMMIT; monitor re-ACKs idempotently by `txn` without re-execution.

Stage 3 — physical: ARM press (T5/T6). Never part of the exchange.

Framing (F2 completions): HDLC-style — delimiter `0x7E`, escape `0x7D`
(XOR `0x20`), resync on next `0x7E`. Fields after unescaping:
`ver(1) | src_session(4) | peer_session_echo(4) | txn(2) | seq(2) | type(1)
| len(1) | payload(≤48) | crc(2)`. CRC-16/CCITT-FALSE (poly 0x1021, init
0xFFFF, no reflection, no final XOR) over ver..payload, transmitted
big-endian. 115200 8N1. Sessions are boot-random per side; **a frame is
accepted only if `peer_session_echo` matches the receiver's current
session** — delayed frames from an earlier peer boot are rejected
regardless of seq (F2). `seq` is 16-bit modular, window 8, duplicates
re-ACKed not re-executed; `txn` binds one recovery attempt end-to-end.
Timeouts: 100 ms/step, ≤3 attempts, then both sides abandon (monitor stays
FAULT, PS reports). Boot-order free: pre-T1 frames are discarded silently.

## 5. Required tests

All v1 cases, plus (F1) monitor fault with UART disconnected and a live
host target — assert permits stay low and no path to ARMED exists until
UART restored and exchange completes; short monitor reset (< 50 ms HB gap)
with stale PL intent — assert A′ blocks rearm; (F2) lost COMMIT_ACK,
replayed COMMIT from a previous boot session, sequence wrap; (F3)
unconfigured-FPGA bench measurement of HALT_OUT bias with and without the
XDC PULLUP option, FPGA powered-off case recorded as residual; (F4)
selected-motor STOP while peers run — assert no COAST_REQ and peers
unaffected; STOP-then-escalate-to-HALT; (F5) motion attempt before first
HB qualification (must refuse), held ARM through recovery (must not rearm),
stale-sensor and battery-threshold detection latencies against T7b bounds.
