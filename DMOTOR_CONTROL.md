# DMOTOR external motor-control circuitry — per-motor PCB restart

Revision: 2026-11-03 (3) — **current-system contract for the ordered rev A
boards** (motor-interface-integrated `5ebcebe`+, fabrication cleared and
ordered 2026-11-02). This revision reconciles the text against the
as-ordered hardware per review R9 (docs/software_architecture_review_2026_11_03.md);
sections describing superseded intermediate states are corrected in place,
and the joint recovery semantics of §6.4 are being redesigned — see
[docs/recovery_transition_contract.md](docs/recovery_transition_contract.md)
(PROPOSED) for the R1/R3 transition table that will replace them on adoption.

Revision 2026-10-30 (2) superseded the same-day first revision and
re-evaluated the complete external circuitry after the user's restart decision.
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
│ nFAULT pull-up   ──── GPIO ──►│ battery dividers (balance plugs)              │
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

- DRV8263H #5238 carrier (PH/EN mode). **Carrier VDD and SLEEP run from the
  harness +3V3** so the carrier's onboard nFAULT pull-up sits at 3.3 V
  (ESP32-safe) and a disconnected monitor harness puts the driver to sleep
  (coast). The motor board routes the carrier's ITRIP pin through **RV1
  (100 kΩ 25-turn rheostat) in series with floor resistor R13 (shipped
  DNP) to GND, with TP1/TP2 test loops**: as shipped the chain is open and
  the carrier's default ≈10 A current limit applies, far above the
  2.3–2.6 A motor stall figures, so D03b stays a release gate — closing it
  means fitting R13 (6.8 kΩ 1 % specified) and trimming RV1 into the lowest
  useful level band (≈3.9 A if the family V_ITRIP ladder transfers —
  reconciled to the issue register 2026-11-03) at TP1/TP2
  (the DRV8263H ITRIP pin is 5-level/discrete; near 0 Ω disables the
  regulation, hence the floor).
- INA226 + shunt in the board's 12 V feed. A0/A1 address straps make the four
  boards distinct on one I2C bus. The INA226's open-drain ALERT output is the
  **hardware threshold element that replaces the LM393 comparator**: its limit
  register asserts ALERT after one conversion without firmware in the loop.
  All four ALERTs wire-OR onto one monitor interrupt line (the
  battery-feed INA226 was removed 2026-10-31).
- IPROPI, RC-filtered, to a monitor ADC channel (coarse per-motor telemetry).
- DRVOFF pulled up locally (**coast is the default state**); a small-signal
  NPN (Q1, 2N3904) pulls DRVOFF low to permit drive, its base driven by the
  monitor permit (through the hardware watchdog/HALT gate) and its emitter
  returned through the series STOP loop.
- nFAULT pull-up, routed to a monitor interrupt input.
- Encoder 5 V supply and level translation to 3.3 V FPGA inputs; a validated
  3.3 V sensor configuration may later delete the translator.
- Small local 12 V→5 V regulator (replaces the shared U509).

Explicitly absent: comparators, voltage reference, supervisor, logic ICs,
arm latch, optocouplers, peer-link connector.

## 4. Central monitor — resource budget and selection

Requirements: `motor-interface-integrated/monitor/README.md`. The single
controller needs **17 digital GPIOs** (4 nFAULT, 4 DRVOFF permits, shared
ALERT, ARM, STOP sense, HB, HALT_OUT, UART×2, and one I2C bus to the four
motor-board INA226s over the harnesses) plus **6 ADC channels** (4 IPROPI,
2 battery taps). All remaining spare GPIOs are brought out on a 2×7
expansion header. The battery-feed INA226 was removed (2026-10-31): battery
current is **estimated** from the four motor-board feeds — those measure on
the regulated 12 V side, so firmware converts power, not current:
I_battery ≈ 12 V × ΣI_feed / (V_battery × η_regulator) + other battery
loads. Accuracy is bounded by the regulator-efficiency model; pack
protection therefore rests on the per-pack voltage dividers, not on this
estimate or on coulomb counting.

| Candidate | Verdict |
|---|---|
| ESP32 (WROOM-32) | Marginal: exactly 6 clean ADC1 inputs, ~14 strap-safe digital GPIOs for 17 needed — only workable by using boot-strapping pins. Not recommended. |
| **ESP32-S3** | **Recommended.** ~33 usable GPIOs, ten ADC1 channels (no Wi-Fi/ADC2 conflict), same toolchain; radios stay disabled. |
| Teensy 4.1 | Accepted fallback: more I/O, substantially better ADC linearity, no radio. Different toolchain; 3.3 V, not 5 V tolerant (same as ESP32). |

**On-hand board analysis (2026-10-31, design unchanged).** The monitor needs
23 module pins: 13 plain digital + UART×2 + I2C×2 + 6 ADC. Against the three
boards on hand:

| On-hand board | Fit |
|---|---|
| **Teensy 3.2** (MK20DX256, 72 MHz M4) | **Clean drop-in replacement — validated fallback.** All 23 functions fit the outer DIP rows with one spare: UART 0/1, I2C 18/19, ADC 14–17 (IPROPI) + 20/21 (battery), 13 digital from 2–13/22/23. Better ADC than any ESP32 (dual 13-bit effective), Kinetis pins disabled (hi-Z) at reset, 5 V-tolerant inputs, VIN takes the D24V5F5 5 V, 250 mA 3.3 V regulator covers the harness +3V3 load. Caveats: product line discontinued (fine for a one-off; Teensy 4.0 shares the outer-row form factor as an upgrade path, 12-bit ADC, not 5 V tolerant), spare-GPIO expansion shrinks to ~1 pin (bottom pads inaccessible when socketed), Teensyduino toolchain, and switching costs a monitor re-layout (new socket footprint and pin map; the module is far smaller than the 70 mm DevKitC, so the board gets easier). MK20 V_IH = 0.7·VDD ≈ 2.31 V; the "any non-solid-low STOP_SENSE = STOP" firmware rule is unaffected. |
| HUZZAH32 Feather (ESP32 classic) | **No — 2 pins short** (21 broken-out GPIOs incl. three input-only vs 23 needed). Closing the gap means dropping a safety-relevant signal or putting PERMIT/NFAULT behind an I2C GPIO expander — rejected. |
| Feather M0 (ATSAMD21 + ATWINC1500) | **No.** Fits only by holding the WINC1500 in reset and scavenging its SPI pins as GPIO; zero margin, weakest CPU (48 MHz M0+). The WINC1500 is itself EOL. |

Since the monitor firmware (I7) is not yet written, a toolchain switch has
no porting cost; the Teensy 3.2 is recorded as the validated hardware
fallback, and the ESP32-S3 design stands.

Firmware duties (full list in the monitor README): firmware arm latch, fault
aggregation with first-cause latching, threshold policy, DRVOFF permit
control, HB generation, UART telemetry. Task watchdog and brownout detector
mandatory — any reset, hang or brownout stops HB and releases all permits.

**Hardware backstop (added 2026-11-01):** the monitor now routes all four
PERMIT lines through AND gates (74HC08) enabled by a MAX706R watchdog and an
FPGA-HALT inhibit transistor. **HB (which also feeds the watchdog) must be
toggled in firmware only after a completed monitoring cycle with fresh ADC
and I2C data** — never from a free-running hardware timer, which would keep
toggling while application tasks are dead. A hung application therefore
drops all permits in ≤2.25 s without firmware help, and the FPGA can force
coast directly via HALT even when the ESP32 is stuck. The path is
inhibit-only.

**Telemetry role separation (2026-11-01):** the INA226 measures the 12 V
*input* current, not circulating winding current — in PH/EN drive/brake
PWM, motor current recirculates while input current is far lower. Do not
treat a low averaged INA226 reading as proof of low winding current. Roles:
INA226 = input power/energy/supply-overload trends (its "one conversion"
time is not a system response spec — conversion, averaging and alert
configuration set the real latency); IPROPI = driver current, sampled with
appropriate timing; ITRIP = fast limiting; encoder + elapsed time/current =
stall policy. ITRIP itself is two-stage: characterize with RV1/R13 + test
points, then fit a fixed, documented band resistor for production builds.
Harness hardening (keyed locking connectors, strain relief, protection
footprints) is deferred to the vehicle-integration revision; bench phase
uses pin headers.

## 5. Battery and current monitoring

Battery monitoring carries over from the first revision in substance, with
the sense interface now the packs' **balance connectors** (two JST-XH 3-pos
headers mate the 2S balance plugs directly; a separate 2-pos terminal feeds
the monitor's regulator so supply current never crosses balance leads): two
divider taps (stack top 12.0–16.8 V via pack B's B+, series midpoint
6.0–8.4 V via pack A's B+ — **exactly one conductor lands per balance plug**
(ISSUES.md I9: landing duplicates would parallel the high-current pack
interconnects through the balance wires); the divider ground references the
main harness, so readings under-estimate slightly under load (fail-safe)
and are exact at rest where calibration is done; top pack by calibrated
full-minus-mid difference; no battery-feed current monitor) scaled to
≈2.4 V (full) / ≈1.5 V (mid, fault-tolerant 1 M/220 k ratio) max, 1%
resistors, ≈100 nF at each ADC pin, one-point installation calibration
(≈±1% thereafter). Initial per-pack thresholds: warn < 7.0 V, fault
< 6.6 V sustained 500 ms with hysteresis (stall sag must not trip it),
overvoltage sanity > 8.6 V, imbalance warning at > 0.3 V divergence.
Battery current is estimated by power conversion from the four motor-feed
INA226s (§4); there is no independent pack-current measurement.

Per-motor current now has three layers:

| Layer | Mechanism | Latency class |
|---|---|---|
| Regulation/protection | DRV8263 on-chip ITRIP current regulation, OCP, TSD, UVLO — set by carrier components per D03b | µs, on-chip |
| Hardware threshold | INA226 limit register → ALERT (per motor board, on the shared harness I2C bus) | one conversion, ~0.14–1.1 ms by configuration |
| Policy/telemetry | Monitor firmware on IPROPI ADC + INA226 readings | ms–tens of ms |

The ITRIP level selection is now the **primary** fast current protection
and is safety-relevant; the motor board provides the RV1 trimmer + R13
floor + TP1/TP2 network, and D03b (per-board calibration against the full
DRV8263H-Q1 datasheet level table, truth table, thermal tests) remains open
and gains importance from this restart.

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
3. Monitor reset/brownout/hang stops HB (software-gated toggling ceases) →
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

- HB: **software-toggled square wave (~50 Hz at the 10 ms safety cycle;
  never a hardware timer)**, toggled only after a completed monitoring
  cycle with all sensor data inside its freshness limits and no fault
  latched (monitor FIRMWARE.md §4). A small independent RTL
  qualifier in the PL (not the HLS engine) **latches** a fault and inhibits PWM
  when edges are absent beyond the window (initial 50 ms) or the line is
  static. Assert-only; HB can never enable motion. **The latch is cleared
  only by an explicit PS-side handshake (AXI register write after software
  review), never by HB merely resuming** — this is the system-level
  backstop for the monitor watchdog's non-latching ~WDO (monitor
  FIRMWARE.md §4) and must be verified end-to-end in the P1/P3 bench tests.
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
cross-check anymore; welded STOP contacts, a shorted permit transistor, or a
stuck-on DRV8263 output stage are not diagnosed by this architecture. The
ms-scale firmware layers cannot protect the motor from fast electrical
events — that burden is entirely on the carrier's ITRIP/OCP configuration.
Do not represent firmware or I2C thresholds as bounded-latency protection in
any DMOTOR_ARCH evidence.

## 7. Validation gates before reliance

1. D03b closure on one board: R13 fitted and RV1 trimmed into the target
   level band (measured at TP1/TP2), verified against bounded injected
   stall; coast truth table measured.
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
