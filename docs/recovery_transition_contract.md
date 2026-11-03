# Joint recovery and coast-request transition contract — PROPOSED

Status: **PROPOSED 2026-11-03, not approved, no code.** Responds to review
findings R1 (software HALT never reaches the monitor; coast vs brake) and R3
(circular recovery; contradictory UART authority) in
[software_architecture_review_2026_11_03.md](software_architecture_review_2026_11_03.md),
and folds in the MAX706R's non-latching ~WDO (board ISSUES.md I10). On
adoption this supersedes the recovery prose in DMOTOR_CONTROL §6.4 and
monitor FIRMWARE.md §5; both already point here.

**Follow-up review:** this draft does not yet close R1/R3. See
[findings F1–F5](software_architecture_review_2026_11_03.md#follow-up-status-and-review-of-the-proposed-recovery-contract)
for unresolved fault visibility, recovery ordering, reset bias, STOP scope and
transition/protocol details. The behavior below remains proposed, not implemented.

## 1. Architectural resolutions (the decisions that break the knots)

**A. HB means liveness, never permission.** HB is toggled iff the monitor
firmware completed a safety cycle with all sensor data inside its freshness
limits (FIRMWARE §4). It keeps toggling in `SAFE` and in `FAULT` with
permits low. A latched fault is reported by permits-low + UART status —
**not by stopping HB**. This removes the R3 deadlock at its root: the PL
qualifier can re-qualify a healthy monitor while the monitor still refuses
motion, so neither side waits on the other's motion state.
*Consequence:* HB loss now means exactly one thing — monitor
dead/hung/stale — which is also what the MAX706R gate acts on.

**B. The outward PL line is a COAST REQUEST, not a fault echo.** The
monitor-facing `HALT_OUT` (J7, monitor `HALT_IN`, hardware Q2 inhibit)
asserts for conditions where the PL *wants the world coasting*, and never
for conditions that originated at the monitor (no echo loop):

| Condition | Asserts outward COAST_REQ? | Rationale |
|---|---|---|
| PS deliberate HALT (`halt_async` via `DMOTOR_HALT`) | **yes** | R1: a software stop must produce coast, not brake |
| PL local latched fault (`aggregated`), watchdog/target-age fault | **yes** | fault ⇒ outputs unsafe ⇒ coast |
| PL reset / unconfigured / `CORE_RESET` | **yes** (inactive-high bias: line idles asserted until PL drives it) | fail-safe default |
| HB-loss latch (monitor-origin evidence) | **yes** | harmless toward a dead monitor; no loop because HB is independent of HALT_IN |
| Monitor-origin external HALT echo (`halt_detected`, `external_latches`) | **no** | preserves the no-echo rule; cause attribution stays truthful |
| READY, idle, PWM = 0 without fault | **no** | quiescent ≠ coast request |

Physical semantics (R1's READY/STOP/DISABLE question): **READY** = permits
may be high, EN = 0, DRVOFF low ⇒ *brake* is acceptable as a commanded,
attributed state; **STOP/DISABLE/HALT** = COAST_REQ asserted ⇒ monitor
drops permits ⇒ DRVOFF high ⇒ *coast*. Braking is therefore only ever a
deliberate, reported state, never the accidental result of a stop.

**C. UART is advisory in operation, authoritative only inside a recovery
exchange.** Loss of UART during normal operation degrades telemetry only
(existing decision). Loss *during an explicit recovery exchange* refuses
that recovery attempt: both sides return to their safe holding states.

**D. Non-latching ~WDO is compensated in layers** (unchanged from I10):
500 ms IDF task WDT (reset → hi-Z → RN1 pulldowns) under the 1.0 s minimum
external window; boot writes permits low before the first HB toggle; the PL
HB-loss latch clears only by PS handshake, never by HB resuming.

## 2. State definitions

Monitor (owner: ESP32 firmware): `BOOT → SAFE → ARM_PENDING → ARMED →
FAULT(latched, first cause)`. HB runs in every state after BOOT self-test
passes, including FAULT. Permits high only in ARMED.

PL (owner: RTL + driver): `RESET → READY → RUN`, orthogonal latches
`LOCAL_FAULT` (aggregated/watchdog) and `HB_FAULT` (qualifier). COAST_REQ
is combinational per §1B. PWM authorization requires READY/RUN with both
latches clear.

Hardware gate (owner: physics): `ENABLE = ~WDO_ok AND NOT HALT_IN`; permits
reach the boards only while ENABLE is high. Survives all software states.

## 3. Transition table

| # | From → To | Trigger | Preconditions | Owner | Timeout | On failure |
|---|---|---|---|---|---|---|
| T1 | Monitor BOOT → SAFE | self-test pass | pins configured permits-low first; I2C scan matches population mask; NVS valid | monitor | 2 s | stay BOOT, no HB, gate stays closed |
| T2 | PL RESET → READY | bitstream + driver init | outputs inhibited; COAST_REQ asserted through reset | PL/driver | — | remain RESET (COAST_REQ held) |
| T3 | PL: HB_FAULT set | no qualified HB edge within window (initial 50 ms; first-ever sighting required ≤ 1 s after READY) | — | PL qualifier | — | latch; COAST_REQ asserts |
| T4 | PL: HB_FAULT clear | **PS handshake write** after software review | qualified HB present ≥ 500 ms; zero motion intent retained | PS (Linux) | — | latch persists; resumed HB alone NEVER clears (D) |
| T5 | Monitor SAFE → ARM_PENDING | ARM press edge | no latched fault; STOP_SENSE solid low N=10 cycles; packs in window; imbalance < 0.3 V; population mask satisfied | monitor | — | ignore press |
| T6 | ARM_PENDING → ARMED | re-verify T5 for 10 consecutive cycles | HALT_IN deasserted (else gate blocks anyway) | monitor | 200 ms | → SAFE |
| T7 | ARMED → FAULT | any: nFAULTn↓, ALERT↓, STOP non-low sample, HALT_IN↑, sensor stale 3-strike, pack threshold | — | monitor (ISR latch + cycle confirm) | ≤ 1 cycle (10 ms); ISR path ≤ 100 µs | permits were dropped in ISR regardless |
| T8 | Monitor FAULT → SAFE | operator STOP-cycle + fault cause cleared + **recovery exchange** (§4) completed | HALT_IN deasserted; HB still running (A) | monitor | exchange: 3 × 100 ms | remain FAULT; UART loss ⇒ refuse (C) |
| T9 | PS HALT (any PL state) | `DMOTOR_HALT` ioctl | — | PS → PL | PWM zero ≤ 10 µs; COAST_REQ same cycle | monitor drops permits (hw ≤ µs via Q2, fw ≤ 1 cycle); coast confirmed, not brake (B) |
| T10 | PL LOCAL_FAULT clear | existing explicit rearm (3 acks) **plus** recovery exchange if COAST_REQ was asserted | zero motion intent; targets stale-invalidated | PS | — | latch persists |
| T11 | Either side resets mid-operation | watchdogs / power | peer sees: HB loss (T3) or COAST_REQ/permit drop (T7) | — | per T3/T7 | both sides land in safe holding states independently — **no ordering requirement between boots** |
| T12 | Simultaneous HALT_IN + HB expiry | — | both latches set independently; recovery requires T4 then T8, in that order | PS then monitor | — | — |

Invariants: every row leaves *zero motion intent* behind; no row is an
automatic retry loop; any precondition failure lands in the safe
predecessor state, never an intermediate.

## 4. Recovery exchange (the only authoritative UART use)

Frame (both directions): `0x7E | ver(1) | session(4) | seq(2) | type(1) |
len(1) | payload | CRC16-CCITT(0x1021, init 0xFFFF, over ver..payload)`,
115200 8N1, max frame 64 B, little-endian. `session` = boot-random per
side; a changed peer session invalidates any exchange in progress
(replay/reset detection — CRC alone is not freshness).

Sequence (monitor FAULT → SAFE with PL participation):
1. PS → monitor `RECOVER_REQ {causes_acknowledged}` — PS has reviewed the
   fault log (UART telemetry) and cleared its own latches per T4/T10.
2. Monitor → PS `RECOVER_STATE {first_cause, sensors_ok, stop_state}` —
   refuses (`NAK {reason}`) if cause persists, sensors stale, or STOP
   not solid-low.
3. PS → monitor `RECOVER_COMMIT {seq echo}` ⇒ monitor enters SAFE (not
   ARMED — physical ARM still required, T5/T6).
4. Any step: 100 ms timeout, ≤ 3 attempts, duplicate frames idempotent by
   `seq`; UART loss or session change ⇒ both sides abandon, monitor stays
   FAULT, PS reports. Boot-order independence: either side may initiate
   discovery after T1/T2; frames before T1 are discarded unanswered.

## 5. Required tests (from R1/R3, to be automated in the joint harness)

Software HALT from active motion and from READY (observe PWM, COAST_REQ,
permits, DRVOFF — assert coast, not brake); client exit / service death /
explicit disable; both boot orders; reset of either processor mid-ARMED;
held ARM through a fault; delayed, duplicated and replayed ACKs; UART
unplugged during operation (advisory) vs during exchange (refusal);
simultaneous HALT + HB expiry (T12); WDO trip and recovery-without-reset
(I10 scenario) proving no motion without T4 + T5/T6.
