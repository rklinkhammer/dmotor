# DMOTOR external motor-control circuitry — per-motor PCB restart

Revision: 2026-10-30 (2). This supersedes the same-day first revision and
re-evaluates the complete external circuitry after the user's restart decision.
The previous two-motor interface board — including its LM393 IPROPI
comparators, LM4040 reference, MCP130 supervisor, HCT arm-latch logic and the
optocoupled two-board peer interlock — is **superseded and archived unchanged**
in `motor-interface-integrated/archive/2026-10-30-before-per-motor-restart/`.
Numeric values marked *initial* are starting points, not qualified limits.

## 1. Resolved user inputs (2026-10-30)

- **Each motor controller on its own PCB**: four identical single-driver
  boards, each carrying one Pololu DRV8263H #5238 carrier.
- **The external microcontroller handles all fault detection and protection
  circuitry** and provides the heartbeat (HB) to the Zynq. Discrete
  comparators and protection logic are eliminated; monitoring collapses into
  microcontroller resources and external I2C devices.
- **One microcontroller preferred.** ESP32 if its resources suffice; a Teensy
  board is the accepted alternative otherwise. Section 4 shows a plain ESP32
  is marginal and recommends the **ESP32-S3**; **Teensy 4.1** is the fallback.
- **Current sensing: both** an INA226 per motor board and IPROPI into
  microcontroller ADC channels.
- **FPGA-initiated coast goes via the microcontroller**: FPGA HALT_OUT is a
  monitor input; the monitor drops the DRVOFF permits (millisecond-scale).
  The FPGA independently zeroes its own PWM, as before.
- Battery: **two 2S LiPo packs in series** (4S, ~12.0–16.8 V) feeding the
  external 12 V regulator; **per-pack** voltage monitoring, low precision
  acceptable. UART telemetry from the monitor into the Zynq PL.
- ARM/STOP handling: the user requested a recommendation — see section 6.3.

## 2. Architecture overview

```text
                 ┌──────────────── Zynq (FPGA control plane) ───────────────┐
                 │ PWM/DIR out, QA/QB capture, PI control, HB qualifier,    │
                 │ fault latch + PWM inhibit, UART telemetry (advisory)     │
                 └───┬PWM/DIR/QA/QB×4      HB▲  UART▲        HALT_OUT┬──────┘
                     │                       │     │                 ▼
┌── motor PCB ×4 ──┐ │          ┌────────── central monitor (ESP32-S3) ─────┐
│ DRV8263H carrier ◄─┘          │ firmware: arm latch, fault aggregation,   │
│ INA226 + shunt   ◄── I2C ────►│ thresholds, DRVOFF permits, HB, telemetry │
│ IPROPI RC filter ──── ADC ───►│ ADC: 4×IPROPI + 2×battery taps            │
│ nFAULT pull-up   ──── GPIO ──►│ battery INA226 + shunt (I2C)              │
│ DRVOFF pull-up (coast default)◄─ permit GPIO ×4                           │
└──────────────────┘            └───────────────────────────────────────────┘
          STOP (NC) hardwired in series with every DRVOFF pull-low return
```

The Zynq keeps the entire motion-control plane (PWM, direction, encoder
capture, control loops, watchdogs, target aging) per [DMOTOR_ARCH.md](DMOTOR_ARCH.md).
The monitor owns the protection/monitoring plane. The two meet at exactly
three signals: HB (monitor→PL), HALT_OUT (PL→monitor) and the advisory UART.

## 3. Per-motor PCB

Detailed requirements and net intent:
`motor-interface-integrated/motor-board/README.md`. Summary — each board
contains only what cannot be collapsed into the monitor or an I2C device:

- DRV8263H #5238 carrier (PH/EN mode, SLEEP to local 5 V).
- INA226 + shunt in the board's 12 V feed. A0/A1 address straps make the four
  boards distinct on one I2C bus. The INA226's open-drain ALERT output is the
  **hardware threshold element that replaces the LM393 comparator**: its limit
  register asserts ALERT after one conversion without firmware in the loop.
  All five ALERTs wire-OR onto one monitor interrupt line.
- IPROPI, RC-filtered, to a monitor ADC channel (coarse per-motor telemetry).
- DRVOFF pulled up locally (**coast is the default state**); a small-signal
  MOSFET pulls DRVOFF low to permit drive, gated by the monitor and returned
  through the series STOP loop.
- nFAULT pull-up, routed to a monitor interrupt input.
- Encoder 5 V supply and level translation to 3.3 V FPGA inputs; a validated
  3.3 V sensor configuration may later delete the translator.
- Small local 12 V→5 V regulator (replaces the shared U509).

Explicitly absent: comparators, voltage reference, supervisor, logic ICs,
arm latch, optocouplers, peer-link connector.

## 4. Central monitor — resource budget and selection

Requirements: `motor-interface-integrated/monitor/README.md`. The single
controller needs **17 digital GPIOs** (4 nFAULT, 4 DRVOFF permits, shared
ALERT, ARM, STOP sense, HB, HALT_OUT, UART×2, I2C×2) plus **6 ADC channels**
(4 IPROPI, 2 battery taps).

| Candidate | Verdict |
|---|---|
| ESP32 (WROOM-32) | Marginal: exactly 6 clean ADC1 inputs, ~14 strap-safe digital GPIOs for 17 needed — only workable by using boot-strapping pins. Not recommended. |
| **ESP32-S3** | **Recommended.** ~33 usable GPIOs, ten ADC1 channels (no Wi-Fi/ADC2 conflict), same toolchain; radios stay disabled. |
| Teensy 4.1 | Accepted fallback: more I/O, substantially better ADC linearity, no radio. Different toolchain; 3.3 V, not 5 V tolerant (same as ESP32). |

Firmware duties (full list in the monitor README): firmware arm latch, fault
aggregation with first-cause latching, threshold policy, DRVOFF permit
control, HB generation, UART telemetry. Task watchdog and brownout detector
mandatory — any reset, hang or brownout stops HB and releases all permits.

## 5. Battery and current monitoring

Battery monitoring carries over from the first revision unchanged in
substance: two divider taps (`TAP_FULL` 12.0–16.8 V, `TAP_MID` 6.0–8.4 V,
top pack by difference) scaled to ≈2.4 V max, 1% resistors, ≈100 nF at each
ADC pin, one-point installation calibration (≈±1% thereafter). Initial
per-pack thresholds: warn < 7.0 V, fault < 6.6 V sustained 500 ms with
hysteresis (stall sag must not trip it), overvoltage sanity > 8.6 V,
imbalance warning at > 0.3 V divergence. A battery-feed INA226 + shunt adds
pack current, an independent stack-voltage reading and charge estimation.

Per-motor current now has three layers:

| Layer | Mechanism | Latency class |
|---|---|---|
| Regulation/protection | DRV8263 on-chip ITRIP current regulation, OCP, TSD, UVLO — set by carrier components per D03b | µs, on-chip |
| Hardware threshold | INA226 limit register → ALERT (per board) | one conversion, ~0.14–1.1 ms by configuration |
| Policy/telemetry | Monitor firmware on IPROPI ADC + INA226 readings | ms–tens of ms |

The carrier ITRIP component selection is now the **primary** fast current
protection and is safety-relevant; D03b (exact carrier configuration,
current-limit components, truth table, thermal tests) remains open and gains
importance from this restart.

## 6. Protection re-evaluation — what changed and what holds

### 6.1 Explicit supersessions (user decision 2026-10-30)

The board-level microsecond-scale discrete protection path (LM393 IPROPI
trips into `RAW_SAFE_N`, hardware arm latch, peer interlock, 5 V supervisor)
is **removed**, not relocated. Board-level protection latency moves from
µs-scale discrete hardware to: on-chip driver protection (µs) → INA226 ALERT
(~ms) → monitor firmware (ms) → HB loss to Zynq (≤50 ms window, initial).
The DMOTOR_ARCH requirement that bounded-latency protection be independent of
software is now satisfied **only** by the DRV8263's internal protections and
the passive fail-safe topology below; every other protective action involves
firmware. This is a deliberate, recorded trade of response time and
independence for a drastic parts-count and board-complexity reduction.

### 6.2 Passive fail-safe invariants (no firmware involved)

1. DRVOFF pulled up on every board: harness disconnected, monitor unpowered,
   held in reset, or GPIO floating → **coast**.
2. NC STOP loop in series with every DRVOFF pull-low return: opening STOP
   coasts all four motors regardless of monitor state, including firmware
   stuck asserting permits.
3. Monitor reset/brownout/hang stops HB (hardware-timer output ceases) →
   Zynq PL qualifier latches the external fault and inhibits PWM.

### 6.3 ARM/STOP recommendation (adopted as initial)

**ARM** becomes a firmware state machine (GPIO button; grant permits only on
release, with no latched fault and all permits inactive). **STOP stays
physically hardwired** into the DRVOFF permit return as in 6.2, and is also
sensed by a GPIO for reporting. Rationale: the operator's last-resort stop
should remain effective against the one failure class firmware cannot cover —
the monitor itself misbehaving while powered. Cost is one series loop and no
logic ICs, which honors the collapse goal; full-firmware STOP was rejected
for that single reason. This is a recommendation adopted as the initial
design; reverting to full-firmware STOP is a one-line schematic change.

### 6.4 Monitor–Zynq contract (unchanged in spirit from revision 1)

- HB: hardware-timer square wave (initial 100 Hz), generated only while
  measurements are fresh and no fault is latched. A small independent RTL
  qualifier in the PL (not the HLS engine) latches a fault and inhibits PWM
  when edges are absent beyond the window (initial 50 ms) or the line is
  static. Assert-only; HB can never enable motion or clear anything.
- FPGA HALT_OUT → monitor interrupt → all DRVOFF permits dropped. Target
  end-to-end FPGA-fault-to-coast latency < 1 ms firmware path (measure);
  the FPGA's own PWM zeroing remains its existing ≤10 µs digital path.
- UART telemetry (sequence + CRC) is advisory; loss of UART is never a
  motion inhibit — HB is the only protective signal toward the Zynq.
- Recovery keeps the DMOTOR_ARCH order: correct cause → acknowledge at the
  monitor (clears its latch, restores HB) → FPGA-side acknowledge/rearm →
  deliberate physical ARM cycle. No path auto-restarts motion.

### 6.5 Honest limitations

A single monitor is now a single point of protection policy. Its failure
modes are fail-safe (6.2), but there is no redundancy and no peer
cross-check anymore; welded STOP contacts, a shorted permit MOSFET, or a
stuck-on DRV8263 output stage are not diagnosed by this architecture. The
ms-scale firmware layers cannot protect the motor from fast electrical
events — that burden is entirely on the carrier's ITRIP/OCP configuration.
Do not represent firmware or I2C thresholds as bounded-latency protection in
any DMOTOR_ARCH evidence.

## 7. Validation gates before reliance

1. D03b closure on one board: carrier ITRIP components selected and verified
   against bounded injected stall; coast truth table measured.
2. Passive invariants: pull harness, power, reset and each permit line on a
   live board; verify coast each time with no firmware assistance. Open STOP
   mid-drive; verify all-board coast.
3. INA226 ALERT threshold accuracy and latency per configuration; wire-OR
   identification correctness with simultaneous alerts.
4. Measured latencies: nFAULT→permits-dropped, HALT_OUT→coast, HB-loss→PL
   inhibit; record against the table in section 6.1.
5. Battery calibration and sag-immunity tests as in revision 1 (thresholds,
   persistence filter, discharged-pack trip).
6. Monitor failure-mode sweep: power pull, reset loop, firmware hang, stuck
   GPIO, severed HB wire — each must end in coast + PL fault latch.
7. ESP32-S3 pin map validated against strapping constraints before PCB
   release; Teensy 4.1 fallback decision point is here if ADC quality or
   pins disappoint.

None of these gates is claimed complete by this document.
