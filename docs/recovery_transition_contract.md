# Joint recovery and coast-request transition contract — PROPOSED v3

Status: **PROPOSED v3, 2026-11-03, not approved, no code.** v1 responded to
review findings R1/R3; v2 proposed resolutions to **F1–F5**; this revision
resolves the v2-assessment defects **V2-1…V2-4** in
[software_architecture_review_2026_11_03.md](software_architecture_review_2026_11_03.md)
and keeps the I10 (non-latching ~WDO) layering. On adoption this supersedes
the recovery prose in DMOTOR_CONTROL §6.4 and monitor FIRMWARE.md §5.
R4 (PL clock-loss supervision) is **explicitly out of scope** here: a
combinational coast request cannot detect a frozen clock; R4 needs its own
architectural decision (UART progress lease vs narrowed failure model).

**Review status:** the [v2 assessment](software_architecture_review_2026_11_03.md#latest-assessment-recovery-contract-v2)
credits the STOP/coast separation and removal of the original latch-clear
dependency; v3 (this text) incorporates its corrections — epoch/commit
binding (V2-1), outcome reconciliation and dual-session validation (V2-2),
withdrawal of the XDC-PULLUP reset-bias mechanism (V2-3), and derived,
separated timing/invariant rules (V2-4). Still a proposal pending review.

## 1. Architectural resolutions

**A. HB means liveness, never permission** (unchanged from v1): toggled iff
a safety cycle completed with fresh data; keeps toggling in SAFE *and*
FAULT. Faults are reported by permits + UART status, never by stopping HB.

**A′ (F1). Permits are never restored to an unproven PL.** Because HB is
the only mandatory monitor→PL channel on rev A, monitor faults cannot
compel the PL to drop intent in real time. The guarantee is therefore moved
to where the monitor has authority: **the monitor leaves FAULT only through
the recovery exchange (§4), whose request carries a PS attestation —
session identity, outputs disabled, all targets/PI state cleared via the
existing driver reset/session sequence and VERIFIED BY READBACK before
attesting** (V2-2: the existing `DMOTOR_RELEASE_HALT` ioctl and
`Runtime::prepare()` are the extension points — release is not a new
operation; the three acknowledgment writes alone do not clear targets/PI,
so cleanup maps to the session-reset path). **The attestation is backed by
a sustained PS-side recovery hold (V2-1), not a snapshot:** from stage 1
until the explicit `RECOVERY_COMPLETE` transition, command admission is
closed, queued intent is flushed, and outputs stay disabled; new commands
become admissible only after the PS confirms monitor SAFE (§4) and ARM is
observed. No exchange, no SAFE; no SAFE, no ARM; no ARM, no permits; no
RECOVERY_COMPLETE, no command admission. UART thus stays
advisory in operation but is *required equipment for recovery* — a monitor
fault with UART unplugged parks the system in FAULT (coast) until telemetry
is restored. A monitor reset of any duration is covered: BOOT unconditionally sets
`exchange_completed = false` (V2-1 — even though epoch restarts), takes a
fresh session id, and reaching ARMED again requires a new exchange + ARM.
**A PL-only reboot while monitor permits are high is NOT covered** — as
ordered the monitor cannot observe it (B′); it is a documented open case
under R4 with an interim *procedural* rule (not a guarantee): PL/PS
reboots are preceded by operator STOP or PS HALT. On FAULT
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

**B′ (F3, corrected per V2-3). Reset-time behavior, stated honestly.** On
rev A, an unconfigured or absent FPGA does **not** assert COAST_REQ (R17
pulls Q2's base low; I10: "absent FPGA does not inhibit"). v1's
"idles asserted" claim is withdrawn, and **v2's XDC-PULLUP proposal is
withdrawn as a mechanism** (V2-3): configuration-time pull behavior is
governed by PUDC_B and power sequencing, not by a user-design XDC property
alone (UG470), and v2's 2.6 V estimate ignored Q2's base-emitter loading of
that node. The PL failure classes are distinct and individually **open
under R4**: CORE_RESET, fabric reset, reconfiguration, clock loss and power
loss. The motor boards' EN/PH pulldowns govern *undriven* pins only —
nothing here covers frozen actively-driven outputs — so **no coast or
"stationary-safe" guarantee and no stale-permit time bound is claimed for
any PL failure class.** If pre-configuration coast assertion becomes a
requirement, the qualified route is an **external pull-up on the PL
HALT_OUT net** (UG470's recommendation for power-ramp-critical assertion),
designed as a whole bias network (pull-up value, R16, R17, Q2 V_BE and
base-current budget) and bench-validated for the actual Arty Z7 PUDC_B and
supply sequencing — a hardware change to evaluate, not a one-line fix.

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
| T7a | any state → FAULT (ISR class) | nFAULTn↓, ALERT↓, **STOP rising edge (GPIO interrupt)**, HALT_IN↑ | permits dropped + epoch++ in ISR; valid in SAFE/ARM_PENDING/ARMED and mid-exchange (V2-1) | monitor | output ≤100 µs from edge; confirm/latch ≤1 cycle | — |
| T7b | any state → FAULT (task class) | sensor staleness; battery/imbalance persistence | see the derived bounds below the table (V2-4) | monitor | per derivation | — |
| T8 | Monitor FAULT → SAFE | **stages 1–2 complete** (first valid COMMIT; stage 3/ARM happens from SAFE via T5/T6 — table is acyclic, V2-2) | cause cleared; sensors fresh; STOP solid-low; HALT_IN low | monitor | per §4 | remain FAULT; UART loss ⇒ refuse (C) |
| T9 | PS global HALT (any PL state) | `DMOTOR_HALT` | sets PS_HALT latch ⇒ COAST_REQ | PS→PL | PWM zero ≤10 µs; gate inhibit ≤µs (Q2) | release only via §4 stage 1 |
| T10 | PL LOCAL_FAULT clear | existing 3-ack rearm | zero motion intent; targets invalidated; **does not require the exchange** (F2 — ordering fixed: local latches clear first, exchange runs after) | PS | — | latch persists |
| T11 | Either side resets mid-operation | — | peer sees HB loss (T3) or permit drop; **PL death is NOT guaranteed visible to the monitor as-ordered (B′)** — bounded by A′ on next recovery | — | per T3/T7 | no boot-order requirement |
| T12 | Simultaneous HALT_IN + HB expiry | — | recover: T10 → stage 1 → T4 → §4 → T5 | PS then monitor | — | — |

Invariants, split per V2-4 (the v2 blanket form contradicted A′):

- **Monitor permit invariant:** every fault/inhibit entry drops all permits
  within its class bound (T7a ≤100 µs, T7b per derivation) and increments
  the epoch — in every monitor state, including mid-exchange.
- **PL cleanup invariant:** every PL-side transition into an inhibit/fault/
  halt state clears PL motion intent (targets, PI state). A monitor-only
  fault does NOT clear PL intent in real time; intent is instead *proven
  cleared and held cleared* by the A′ recovery hold before permits return.
- Steady ARMED operation is governed by the existing target freshness and
  authorization rules, not this table.

**Derived T7b bounds (V2-4 — named start events, persistence vs detection
vs output):** *staleness*: from the last valid conversion timestamp:
25 ms age threshold + up to 3 further 10 ms cycles (strike confirmation) +
1 cycle output write ⇒ **inhibit ≤65 ms after the last valid conversion**,
assuming the R5 cycle-budget work bounds each cycle at 10 ms (explicitly a
target pending R5, not a measured guarantee). *Battery LVC/imbalance*:
500 ms persistence window (policy, from the first crossing sample) +
1 cycle detection + 1 cycle output ⇒ **inhibit ≤520 ms after the first
qualifying sample**. STOP: the rising edge is ISR-class (T7a); the
solid-low *release* qualification is task-class sampling and gates arming
only. **HB_QUALIFIED lifecycle (V2-4):** set after the first qualified
window; **invalidated by HB_FAULT latch, CORE_RESET, or reconfiguration**;
motion authorization requires `HB_QUALIFIED ∧ ¬HB_FAULT ∧ ¬LOCAL_FAULT ∧
¬PS_HALT`; fault latches always take precedence over qualification status.
**Per-motor DISABLE (V2-4):** immediate output inhibition of that channel
(EN forced inactive within one PL control frame) — never a zero-velocity
PI command; controlled deceleration is exclusively the STOPPING state.

## 4. Recovery exchange (staged; resolves F2)

Stage 1 — PS-side cleanup (no monitor involvement): operator/service review;
`T10` 3-ack rearm clears LOCAL_FAULT; **explicit PS_HALT release operation**
(new driver op — absent from v1, F2) clears the deliberate-halt latch;
`T4` handshake clears HB_FAULT if set. COAST_REQ deasserts when all three
components clear. *Nothing in stage 1 requires the exchange; the exchange
requires stage 1 — strictly acyclic.*

Stage 2 — exchange (UART, authoritative; all frames carry the epoch):
1. PS→M `RECOVER_REQ {epoch?, attestation: outputs_disabled,
   targets_cleared_verified, causes_acked, pl_configured}` — sendable only
   after stage 1 completes and its readback verification passed.
2. M→PS `RECOVER_STATE {epoch, first_cause, sensors_ok, stop_state,
   halt_in_state}` or `NAK {reason}`. The returned `epoch` is the binding
   value for this attempt.
3. PS→M `RECOVER_COMMIT {txn, epoch}` — **the monitor accepts a COMMIT only
   if its epoch equals the monitor's CURRENT epoch** (V2-1): any fault in
   any state (SAFE, ARM_PENDING, ARMED, or mid-exchange) increments the
   epoch inside the same critical section that drops permits, instantly
   invalidating pending transactions, completed-exchange status and ARM
   eligibility; a delayed pre-fault COMMIT is therefore NAKed.
4. **M→PS `COMMIT_ACK {txn, epoch, new_state=SAFE}`**; the monitor enters
   SAFE on first valid COMMIT and re-ACKs duplicates idempotently.
   **Outcome reconciliation (V2-2):** if the PS exhausts COMMIT retries
   without an ACK, the result is UNKNOWN — the PS keeps the recovery hold
   and polls with idempotent `STATE_QUERY` frames until it learns the
   monitor's state and epoch; it never assumes the peer stayed inhibited.
   If the monitor meanwhile faulted (new epoch), the PS restarts at step 1.
   ARM eligibility on the monitor side requires `exchange_completed` for
   the current epoch; `RECOVERY_COMPLETE` on the PS side requires a
   confirmed SAFE with matching txn+epoch.

Stage 3 — physical: ARM press (T5/T6). Never part of the exchange.

Framing (F2, completed per V2-2): HDLC-style — delimiter `0x7E`, escape
`0x7D` (XOR `0x20`), resync on next `0x7E`; decoded frame ≤ 64 B, escaped
wire frame ≤ 130 B, oversize aborts the frame. Fields after unescaping:
`ver(1) | src_session(4) | peer_session_echo(4) | epoch(2) | txn(2) |
seq(2) | type(1) | len(1) | payload(≤44) | crc(2)`. **All multibyte fields
little-endian except the CRC, which is transmitted big-endian**
(CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final
XOR, over ver..payload). 115200 8N1. Sessions are boot-random per side and
established by a `HELLO/HELLO_ACK` discovery exchange after T1/T2
(repeated on mismatch). **A frame is accepted only if BOTH identities
validate (V2-2): `src_session` equals the established peer session AND
`peer_session_echo` equals the receiver's own current session.** Any
session mismatch tears down the established pairing, invalidates in-flight
transactions and `exchange_completed`, and forces re-discovery. `txn` is
bound to the tuple (src_session, peer_session, epoch); the duplicate cache
holds the last 8 txns per pairing and expires with the pairing. `seq` is
16-bit modular with window 8; a duplicate within the window is re-ACKed
without re-execution; beyond the window it is discarded. Timeouts:
100 ms/step, ≤3 attempts; on exhaustion the monitor holds its current
state, the PS keeps the recovery hold and reconciles via `STATE_QUERY`
(§4.4). Boot-order free: frames before discovery are answered only by
HELLO.

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
stale-sensor and battery-threshold detection latencies against the derived
T7b bounds; plus (V2-1) a new fault injected between RECOVER_STATE and
COMMIT (stale COMMIT must NAK on epoch), a fault during ARM_PENDING, a
command sent after attestation but before ARM (must be rejected by the
recovery hold), repeated recovery cycles; (V2-2) lost COMMIT_ACK with
STATE_QUERY reconciliation, a replayed COMMIT from a previous PS boot
against a non-rebooted monitor (src_session rejection), sequence-window
wrap and duplicate-cache expiry on re-discovery; (V2-3) bench
characterization of the PL HALT_OUT net through configuration, reset,
reconfiguration and power-off, with and without a candidate external
pull-up, measuring Q2 base current and ENABLE voltage directly.
