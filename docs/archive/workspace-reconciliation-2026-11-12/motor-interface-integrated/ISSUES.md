# Issue register — DMOTOR control boards (2026-10-30 review)

Status of each issue: **OPEN** (needs a decision or hardware change),
**GATED** (verify at bring-up/receiving, no change yet), **FIRMWARE**
(resolved in monitor firmware, no hardware change), **CLOSED**.

## I1 — nFAULT idles at 5 V into a non-5V-tolerant ESP32 input — CLOSED (fixed 2026-10-30)

The Pololu #5238 carrier has an **onboard 10 kΩ pull-up from nFAULT to its
VDD pin** (Pololu product page). The motor board wires carrier **VDD and
SLEEP to +5 V**, so nFAULT idles near 5 V; with the board's R9 10 kΩ to +3V3
the divider still sits ≈4.2 V. The line routes via J4.6 to an ESP32-S3 GPIO,
whose absolute maximum is ≈3.6 V — **out of spec whenever no fault is
latched.**

**Proposed resolution: move carrier VDD and SLEEP from +5 V to +3V3** (one
net change on the motor board). The carrier logic range is 3–5.5 V, the
DRV8263 logic inputs (EN/PH/DRVOFF, driven at 5 V pull-up/3.3 V logic) are
rated independently of VDD, and the change adds a fail-safe: with the MON
harness unplugged, +3V3 disappears, SLEEP falls, and the driver sleeps
(coast). R9 then simply parallels the carrier pull-up at 3.3 V.
**Implemented 2026-10-30 (user approved overwriting hand edits):** carrier
VDD and SLEEP moved to +3V3 (C3 bypass follows); motor board regenerated,
rerouted, silkscreened (ERC 0, parity OK, DRC 0/0) and the panel, gerbers
and BOM rebuilt (panel DRC 0/0).

**2026-11-01 — verified against the published carrier schematic**
([Pololu 0J13947](https://a.pololu-files.com/picture/0J13947.1200.png)):
nFAULT has a populated onboard 10 kΩ pull-up to VDD (this issue's premise);
the nSLEEP pull-up position is unpopulated (NP), so the harness +3V3 is
nSLEEP's only driver — unplugged harness → internal pulldown → sleep/coast
as assumed; and DRVOFF has **no onboard pull at all**, so the motor board's
R6 (10 k → +5 V) is the sole coast-default pull-up. Also confirmed: the
DIAG/SR/ITRIP/MODE/nFAULT pulldown positions on the carrier are unpopulated
(these are Pololu's factory-customization spots), IPROPI carries a
populated onboard 1.5 kΩ (→ 300 mV/A), and VIN→VM passes through an
AP74502HQ load switch (reverse-polarity/UVLO protection).

## I2 — INA226 addressing — CLOSED (battery module removed 2026-10-31)

The battery INA226 was removed entirely: per-pack voltage comes from the
balance-connector dividers, and total battery current is **estimated** from
the four motor-board INA226 feeds. One I2C bus remains, with A0 straps only:

| Device | Bus | A1 | A0 | Address |
|---|---|---|---|---|
| Motor 0 | I2C0 (harness) | GND | GND | 0x40 |
| Motor 1 | I2C0 (harness) | GND | VS | 0x41 |
| Motor 2 | I2C0 (harness) | GND | SDA | 0x42 |
| Motor 3 | I2C0 (harness) | GND | SCL | 0x43 |

**The estimate is a power conversion, not a current sum (corrected
2026-10-31):** the motor-board INA226s measure on the regulated 12 V side,
downstream of the battery→12 V regulator. Firmware must convert through it:

```
I_battery ≈ (12 V × ΣI_motor-feed) / (V_battery × η_regulator) + I_other
```

where `η_regulator` is the 12 V regulator's efficiency at the operating
point and `I_other` covers the monitor module and any other loads fed
directly from the battery (including the regulator's own idle draw).
`V_battery` is live from the ADC dividers; η and I_other must be
characterized or approximated in firmware.

Trade-offs accepted: no independent digital stack-voltage cross-check (ADC
calibration is validated manually at install), and coulomb counting is an
estimate whose accuracy is bounded by the η/I_other model — pack protection
should therefore lean on the per-pack **voltage** thresholds, not the
integrated current.

## I3 — I2C bus across four 0.5 m harnesses — MITIGATED, still GATED (low)

**2026-10-31: the battery INA226 was removed**, so a single bus (I2C0,
GPIO15/16, pull-ups R7/R8) carries only the four motor-board modules — the
earlier second-bus split became unnecessary. The harness star remains four
runs (~200–400 pF total); at bring-up measure rise time with all boards
connected; if marginal, fit 2.2 kΩ at R7/R8 and stay at ≤100 kHz. Keep
SDA/SCL twisted with GND, away from motor leads. **Simulated 2026-11-01
(sim/i2c_rise.cir): 4.7 k meets the 1 µs standard-mode rise limit up to
~250 pF; 400 pF gives 1.59 µs (violation) and the 2.2 k mitigation
restores 0.75 µs — the bring-up measurement picks between them.**

## I4 — STOP chain semantics and transient margin — FIRMWARE (corrected 2026-10-31)

The STOP chain is one series conductor: monitor GND → NC switch → MON0 →
board 0 (J4.9/J4.10 bridged at Q1's emitter) → MON1 → … → MON3 → STOP_SENSE
(10 kΩ pull-up, GPIO40). Ground enters at the **switch end**, so the passive
behavior is position-dependent:

- **Opening the STOP switch** removes the ground return for the entire
  chain: every Q1 loses its emitter path → **all four boards coast
  passively.** This is the guaranteed last-resort stop.
- **A mid-chain break** (cut harness, unplugged MON connector) passively
  coasts only the boards **beyond the break** (toward STOP_SENSE). Boards
  between the switch and the break keep their ground return and remain
  enable-capable; they are stopped by **firmware**, which sees STOP_SENSE
  float high and drops all permits (ms-scale). An earlier claim here that
  any harness break passively stops all motors was too strong — stopping
  the grounded-side boards depends on the monitor firmware (and, one level
  up, HB loss stopping FPGA drive).

**Partial population (1–3 motors, documented 2026-10-31):** the chain must
remain electrically continuous through every MON position, populated or not.
Fit a standard 2.54 mm jumper shunt across **pins 9–10 of each unused MON
header** (they are adjacent, so a plain shunt is the loopback — no special
plug needed). Populated boards can occupy any positions; the shunted
positions are simply pass-throughs. Firmware must be configured with the
set of populated channels: skip their INA226 addresses on the I2C scan and
ignore/weakly-pull the floating NFAULTn and IPROPIn inputs of absent
channels (same mechanism as I5). Without the shunt, STOP_SENSE floats high
and the monitor treats the system as permanently stopped — fail-safe, but
inoperable.

**Simulated 2026-11-01, revised same day (sim/stop_chain.cir):** second
review showed the worst case is **all permits LOW** (post-stop): the four
R7+R8 paths load the open chain to 2.388 V with the original 10 k pull-up —
below V_IH. **Fixed: R10 pull-up → 3.3 kΩ**; the open chain now reads
2.93 V (all-low) to 3.30 V (all-high), crossing V_IH ~1 µs after contact
opening, across permit mixes and partial population.

Transient margin: during switch bounce with permits still driven, conducting
B–E junctions can hold the node near PERMIT−V_BE ≈ 2.6 V, only ~0.13 V above
the S3's V_IH (≈2.47 V). Firmware rule: treat any STOP_SENSE sample that is
not solid-low as STOP — drop all permits immediately (cutting the
transistors off snaps the node to 3.3 V), then require qualified release
plus a deliberate ARM cycle. The monitor cannot distinguish STOP from a
broken chain; both are treated as STOP.

## I5 — MON harness unplug leaves monitor inputs floating — FIRMWARE (low)

An unplugged motor board takes its +3V3, NFAULTn pull-up and INA226 with it;
the monitor's NFAULTn and IPROPIn inputs float while PERMITn goes nowhere
(board coasts via DRVOFF pull-up — safe). Firmware must treat loss of the
board's INA226 (I2C timeout) as a fault for that channel and ignore floating
NFAULT/IPROPI readings rather than interpreting them.

## I6 — Fabrication gates — PHYSICAL-FIT ITEMS CLEARED (builder, 2026-11-02)

**Pre-order verification complete:** the builder confirmed the four
scrap-risk fits against physical parts — DevKitC socket geometry (also
matched to Espressif's official footprint), Ohmite 13FR005E lead span,
HiLetgo INA226 pin order, and the Pololu carrier hole pattern. **The
gerber packages are cleared to order** (panels or separate promo orders).
Remaining items below are assembly/bring-up gates, not fab blockers.

- ESP32-S3-DevKitC-1 socket-row spacing — **VERIFIED 2026-11-02 against
  Espressif's official KiCad footprint** (espressif/kicad-libraries,
  Espressif.pretty/ESP32-S3-DevKitC.kicad_mod: 2×22 pads, 2.54 mm pitch,
  row spacing exactly 22.86 mm, matching our custom footprint; note the
  official part numbers its right column bottom-up while ours numbers
  top-down — irrelevant since our symbol and footprint share one
  convention). **Builder confirmed fit 2026-11-02**; USB-connector
  overhang checked at assembly.
- Mounting holes converted to M4 on the goBILDA 8 mm grid (spans 88 × 64
  mm, 2026-11-01): verify against the actual goBILDA channel/plate,
  including screw-head clearance under the elevated DevKit module corner.
- HiLetgo INA226 header pin order — **VERIFIED by builder 2026-11-02**
  against the parts on hand; remove onboard shunts and I2C pull-ups on all
  four modules at assembly.
- **Carrier bulk capacitance (added 2026-11-01, from the carrier
  schematic):** the carrier has only ≈8.9 µF of onboard ceramics plus an
  unpopulated THT bulk position, and Pololu recommends ≥ a few hundred µF
  across VM for most applications. **2026-11-01: C1 upsized to 220 µF on a
  D10/P5 footprint (accepts 220–470 µF for characterization), and a DNP
  TVS footprint (D1, P6KE16A class) was added across BUS12.** Remaining
  gates: choose final capacitance/clamp from measured startup, reversal,
  braking and STOP transients, and **verify the vehicle 12 V regulator's
  behavior when PH/EN drive/brake returns energy to the supply** (a
  non-sinking regulator pumps BUS12 up; the TVS is not automatically an
  adequate sink for sustained returned energy). **Simulated 2026-11-01
  (sim/bus12_transient.cir): a 3 A / 20 ms regen event clamps at 17.9 V but
  dumps ~1.05 J into the TVS and C1
  size barely matters (220 µF 1.049 J vs 470 µF 1.022 J). Qualify the
  selected TVS against the measured waveform, temperature and repetition
  rate; absorbed energy alone does not establish survival or failure.
  Provide another energy-handling path, such as a sinking regulator or
  dump resistor, or limit braking energy if the TVS limits are exceeded.**
- **R5 (DIAG) must stay 9.1 kΩ 1 % — 10 k substitution analyzed and
  REJECTED (2026-10-31):** DIAG is a multi-level config pin latched at
  power-up; the resistance selects a discrete detection *window* with
  undefined dead gaps between windows — there is no calibration escape
  hatch. The DRV8263H-Q1's own level table is in the gated full datasheet;
  the public sibling DRV8243-Q1 table shows the window structure (e.g. its
  Level 2 is guaranteed 7.4–9.0 kΩ only) but its nominals provably do NOT
  transfer (Pololu documents the 8263 MODE mid-level as 16 k where the 8243
  family uses 8.2 k). Assuming a ±10 % window around the 9.1 k nominal
  (≈8.2–10.0 kΩ), a 10 k ±1 % part (9.9–10.1 kΩ) straddles the window edge:
  possible unit-to-unit or power-cycle-to-power-cycle variation in fault
  reaction / off-state diagnostics — the worst kind of intermittent on a
  safety-relevant setting. **Gate (extends the ITRIP gate): obtain the
  DRV8263H-Q1 DIAG and ITRIP level tables (full TI datasheet or Pololu) and
  confirm 9.1 k sits mid-window before fab.** Useful transfer: the 8243's
  V_ITRIP ladder 1.18/1.98/2.31/2.64/2.97 V (≈3.9/6.6/7.7/8.8/9.9 A at
  300 mV/A) matches Pololu's quoted 3 V default at Hi-Z, so the voltage
  ladder likely carries over for D03b planning even though the resistor
  nominals do not.
- **5 V regulator substitution (2026-10-31, cost):** the Pololu D24V5F5
  modules were replaced — motor boards use an **L7805CV linear** (TO-220,
  12 V→5 V; encoder-only load) with C8 100 n input bypass, the monitor a
  **RECOM R-78E5.0-0.5** SIP-3 switcher (7–28 V in covers the 16.8 V 4S
  peak; Traco TSR 1-2450 / CUI VXO7805 are footprint-compatible alternates).
  **Gate: verify the motor encoder's 5 V draw is ≤100 mA per board** — the
  free-standing TO-220 is good for ≤0.7 W (+≈35 °C rise); above that, fit a
  clip-on heatsink or revert to a SIP-3 switcher (footprint change).
- **R12 shunt (revised 2026-10-31, current res-10.pdf reviewed):** R12 is an
  **Ohmite 13FR005E** (2-terminal, 3 W, 5 mΩ 1 %, active, well stocked). The
  part history matters: the originally specified Vishay LVR03 is EOL (2021);
  the Bourns PWR4412-2S behind KiCad's stock bare-metal footprint is
  obsolete; and Ohmite's 4-terminal 13FPR line was **discontinued June
  2025** (per the datasheet banner), so the briefly chosen 13FPR005E was
  reverted. The custom footprint mounts the 13FR at the datasheet's **fixed
  resistance measuring span M = 33.3 mm**, so the pad-to-pad resistance IS
  the calibrated value, and provides co-located net-tie Kelvin sense pads
  feeding SNS_HI/SNS_LO to the INA226 (no copper-stub bias, no extra TCR).
  Body/lead fit **VERIFIED by builder 2026-11-02** against parts on hand.
- Fab V-cut copper setback: boards use 0.5 mm copper-to-edge; confirm it
  meets the fab's V-cut requirement (commonly 0.4–0.5 mm at 1 oz).
- **Fabrication package (completed 2026-11-01):** the panel gerber ZIP now
  includes the **Cmts.User scoring gerber and FAB-NOTES.txt** (V-cut
  coordinates, copper weight, web/finish instructions) — regenerate with
  `tools/export_panel.sh`. Earlier ZIPs omitted the V-cut layer entirely.
- **ADC fault protection (positive case RESOLVED by re-ratio 2026-11-01;
  residuals below):** worst-case analysis of the old 750k/300k midpoint
  divider condemned it — at the bounding single fault (16.8 V full stack
  miswired onto TAP_MID) the clamp conducted ~4 µA and the pin computed to
  **3.75–3.82 V worst case** (1N4148 Vf 0.38–0.45 V at µA/cold + rail at
  +2 %) against a VDD+0.3 ≈ 3.67 V limit; the simulated 3.596 V was
  nominal luck, not margin. **Fix: mid divider → 1 M/220 k (ratio 0.180)**
  so the bounding fault reads ≈3.03 V (≈3.08 V at worst-direction 1 %
  tolerances) — below the pin-protection limit with the clamp
  NON-conducting (sim: 4 nA leakage), eliminating all reliance on diode Vf
  worst-casing. Note 3.03 V exceeds the S3's ~2.9 V effective measurement
  range: **firmware treats any mid-channel reading above the plausible
  window as an over-range fault code** (FIRMWARE.md §5), which is the
  desired reaction to a miswired plug — not an accurate measurement. The full channel is inherently fault-tolerant (16.8 V is
  its normal input). D1/D2 demote to backup clamps for transients and
  double-faults. Remaining OPEN qualification items (calculated, NOT verified —
  the simulations do not cover these conditions): (a) **reversed balance
  plug** drives the ADC node negative (≈−1.5 V open-circuit at the new
  ratio; calculated injection ≈6 µA through ≥190 kΩ into the pin's lower
  clamp — outside the −0.3 V spec; the ESP32-S3 documents no injection
  limit, so µA-scale is an engineering judgment to be bench-verified;
  optional mitigation is a 1N4148 to GND per channel, nA leakage ≈ mV
  error — decide before rev B); (b) **supply-off back-feed**: monitor
  unpowered with packs plugged back-feeds the unloaded 3V3 net through the
  clamps (calculated ≈6 µA mid + 9 µA full). The final rail voltage
  depends on real load and leakage paths and an unloaded rail can
  accumulate charge — **measure the floating rail voltage and verify
  clean power-up from this state at bring-up** before closing.
- **Copper weight — DECIDED 2026-11-01: 1 oz (standard), 2 oz requirement
  withdrawn.** Rationale: the real load profile is 0.5–1 A running with
  brief 2.3–2.6 A stalls, and after D03b the ITRIP ceiling is ≈3.9 A; a
  2 mm external 1 oz trace carries ~5–6 A at 10 °C rise (IPC-2221), and
  the GND return parallels both pours. **Neither copper weight qualifies
  either board for sustained 10 A** — bounding winding current is the
  carrier's job (D03b), not the copper's, and supply-side INA226 readings
  are never proof that winding current is bounded. **Margin applied 2026-11-01: the
  power paths were widened to the geometric maximum.** The carrier's power
  connections are adjacent same-net PAD PAIRS (VIN×2/OUT1×2/OUT2×2,
  2.54 mm pitch), so each trace arrives on the pair centerline as a
  **4.3 mm** stadium lapping both pads at once (the old 2.0 mm in-column
  necks are gone, and both pins of every power connection share current
  symmetrically — closing the 'use both power pins' review item). Runs:
  OUT1/OUT2 3.5 mm, BUS12/VIN_RAW 3.0–3.5 mm; the shunt terminals are
  **solid copper zones** (priority above the GND pours) that subsume the
  current pads and merge the trace end caps — the zone filler keeps
  Power-class clearance around the co-located Kelvin sense pads
  automatically, so the former 2.0 mm sense-approach neck is gone too.
  Remaining necks only at hard constraints (2.4 beside U2's TO-220 pins,
  2.5 on the edge-pinched bottom trunk, 2.0 at the C2 tie).
  A 3.5 mm 1 oz trace ≈ 1.75 mm at 2 oz; with the GND pours this
  comfortably covers the ≈3.9 A ITRIP ceiling. Bring-up validation
  gates: startup, reversal, and stalled-motor shutdown thermal/voltage
  checks on the 12 V path; verify both twin power pads per carrier
  connection share current (the 2 mm netclass covers their link; check the
  pour spokes at J1/J2 for ground bottlenecks).
- **D03b (dmotor) is a release gate — hardware provision added
  2026-10-31, upgraded to a calibratable trimmer network:** the motor board
  routes the carrier's ITRIP pin through **RV1 (100 kΩ 25-turn 3296W
  rheostat, wiper tied to the low end) in series with floor resistor R13
  (shipped DNP) to GND**, with **TP1 (ITRIP) / TP2 (GND)** wire-loop test
  points across the network. As shipped (R13 DNP) the chain is open and the
  carrier default applies: a 3 V trip at the 300 mV/A IPROPI sense, ≈10 A —
  far above the documented 2.3–2.6 A motor stall figures. Closing D03b =
  per-board calibration: fit **R13 = 6.8 kΩ 1 % (specified 2026-11-01)** —
  the floor guarantees never-disable under any plausible level table (the
  disable band is GND-tied, ≤10–20 Ω) and 6.8 k + RV1's 0–100 k reaches the
  lowest useful ITRIP level under both candidate ladders (8.2 k family
  nominal or 16 k Pololu-scaled) — then trim RV1 into the lowest level band
  (≈3.9 A if the family V_ITRIP ladder transfers) while measuring at
  TP1/TP2 or observing chop onset with bounded injected current, and
  measure the coast/chop truth table before powered motor use — this is the primary
  fast current protection in the architecture. Cautions: the DRV8263H-Q1
  ITRIP pin is a **5-level** input that senses resistance to GND — the
  trimmer selects among discrete bands, it is NOT a continuous current
  adjustment; and **ITRIP at/near 0 Ω to GND *disables* current regulation**
  (Pololu docs — only the ≥28 A OCP remains), which is why the R13 floor
  exists and why the wiper ties to the low end (wiper failure → maximum
  resistance → carrier default, never disable). The exact level-resistor
  table is in the full DRV8263H-Q1 datasheet (TI currently publishes only a
  10-page abridged PDF; get the full document via TI or ask Pololu — they
  also offer factory-configured ITRIP as a carrier customization).
- **Promotional pricing restored (2026-10-31):** removing the battery
  INA226 let both boards shrink to 86.36 mm; **2026-11-01 both returned to
  100 × 75 mm (3.9 × 3 in)** to host the watchdog/HALT permit gate and
  the larger bulk/TVS provision (still under the common
  100 × 100 mm promo limit), so each design can also be ordered separately
  at promo pricing; the uniform V-cut panels are 210 × 235 mm (2×3 set) and 210 × 85 mm (2×1 duo: 1 motor + 1 monitor, for small orders). The stackup
  encodes the decided 1 oz copper (0.035 mm). Cheapest order route
  (evaluated 2026-11-01): two separate promo orders (5× motor, 5× monitor,
  HASL, 1 oz) instead of the multi-design panel — roughly $15–30 shipped
  vs $60–100+; the panel remains available when single-piece handling is
  worth the premium.

## I7 — Companion work not in this repo — OPEN (dmotor side)

- PL heartbeat-qualifier RTL (latches the external fault when HB edges stop)
  and the HALT_OUT wiring to the monitor are not yet implemented in dmotor.
  Concretely: the current deployment's `halt_async` comes from AXI GPIO
  only, and `platforms/arty-z7-20/constraints/arty-z7-20.xdc` explicitly
  routes no external HALT input. Required: allocate PL pins (HB in,
  HALT_OUT out, UART RX/TX) on a Pmod, add the bounded-latency HB qualifier
  RTL, and extend the constraints.
- Monitor ESP32-S3 firmware (arm/fault/HB state machine per
  DMOTOR_CONTROL.md §6, I2C scan/calibration, telemetry protocol) is not yet
  written.

## I8 — ESP32-S3-DevKitC-1 revision — RESOLVED to revision-agnostic (2026-11-02)

The DevKitC-1's addressable RGB LED data input is **GPIO48 on v1.0** and
**GPIO38 on v1.1** (the only documented difference between revisions), and
vendors ship either under the same listing. The design is now
**revision-agnostic: HB moved GPIO38 → GPIO21** (clean on both revisions),
so both LED-DIN candidates are plain spares on the J2 expansion header.
No functional signal shares the LED on either revision; the LED merely
loads a J2 spare (fine for logic use; account for it before analog/
timing-critical use of J2 pins 12/13). Remaining receipt checks: confirm
the module's PSRAM variant leaves GPIO35–37 free (octal-PSRAM R8 modules
consume them), and note which revision arrived for documentation.

## I9 — Balance-lead parallel current paths — CLOSED (fixed 2026-11-01)

External review found that landing both midpoint pins (J10.3 pack-A B+ and
J11.1 pack-B B−) on TAP_MID, and landing J10.1 (pack-A B−) on GND,
duplicated the high-current pack-series and main-negative connections
through the balance wires and monitor copper: if a main connection opened
under load, motor current would route through 24 AWG balance leads — a
fire risk. **Fixed: exactly one conductor lands per balance plug**
(J10.3 = TAP_MID, J11.3 = VBAT_SNS; all other balance pins NC). No
protection parts needed — there is no longer any alternate path. The
divider ground reference is now the main harness (J1), so pack readings
under-estimate slightly under load (fail-safe direction) and are exact at
rest, where the install calibration is done.

## I10 — Watchdog + HALT permit gate — ADDED (2026-11-01)

Hardware added per review so fault shutdown no longer depends on a healthy
monitor application: the four PERMIT lines pass through U5 (74HC08) AND
gates. **Corrected 2026-11-01 after second review, which caught two real
bugs:** (1) the first symbol swapped the supervisor's pins 7/8 (7 is
~RESET, 8 is ~WDO), leaving the watchdog output unconnected; (2) the
MAX705's 4.65 V reset threshold would have held the watchdog cleared
forever on a 3.3 V rail. **U3 is now a MAX706R (2.63 V threshold).
LIFECYCLE (2026-11-01): the DIP MAX706RCPA+ is OBSOLETE — the builder has
stock for rev A; the ACTIVE pin-compatible drop-in for production/future
builds is the ADM706RANZ (PDIP-8, 2.63 V, 1.6 s WDT, DigiKey-stocked).
Gate: verify pinout and WDI V_IH against whichever part is actually
received — obsolete-stock sourcing raises the counterfeit/remarked risk,
so bench-check the watchdog timeout and reset threshold on at least one
unit before soldering.** Timeout is 1.6 s typical, **2.25 s worst case** — the
guaranteed bound. Topology: ENABLE is **default-low** (R19 100 k
pulldown), driven high only by a healthy ~WDO through series R18 10 k; Q2
(FPGA HALT, R16/R17) sinks it unconditionally; RN1 (4×100 k bussed SIP)
holds the PERMITn_CPU gate inputs low while the ESP32 is in reset/hi-Z.
WDI is fed by HB; **firmware must toggle HB only when all sensor data is
within its freshness limit** (FIRMWARE.md §4). Guarantees, stated
precisely: hung firmware, absent/unpowered supervisor, or unpowered gate
⇒ coast; **an ASSERTED HALT inhibits motion; an absent/disconnected FPGA
does NOT inhibit (narrower guarantee — but it cannot drive PWM either)**.
The STOP chain is untouched. The same HB feeds the Zynq liveness
qualifier (I7).

**Watchdog recovery is NOT latched (third review, 2026-11-01):** ~WDO
releases as soon as WDI transitions resume, so a theoretical
recovery-without-reset with stale-high permit registers could re-open the
gate. Layered mitigation (all mandatory, FIRMWARE.md §4): IDF task WDT at
500 ms (chip reset → hi-Z → RN1 holds gate inputs low) sits below the
MAX706R's 1.00 s minimum window; boot writes permits low before the first
HB toggle; and the **Zynq PL qualifier latches HB loss until an explicit
PS handshake** (DMOTOR_CONTROL §6.4) — so even a re-opened gate cannot
restart motion. The recover-without-reset scenario is a mandatory P1
bench test.

## Non-issues reviewed and closed

- **CD4050 inputs with FPGA unpowered:** CD4050B inputs tolerate voltages
  above VDD by design; 5 V encoder signals with VDD absent are safe.
- **STOP chain current path:** Q1 emitter currents (~0.5 mA/board) through
  the chain and switch are negligible; sense levels unaffected when closed.
- **IPROPI_F levels:** divided by 2, ≤≈2.5 V at the ESP32 ADC — in range.
- **Monitor ADC clamps back-feeding +3V3:** microamp-scale through the
  1N4148s with the ESP unpowered — harmless.
- **PERMIT at boot:** S3 GPIOs are high-impedance at reset; the motor
  boards' 100 kΩ base pull-downs hold Q1 off → coast.
- **IPROPI divider loading (verified 2026-11-01):** the motor board's
  R10+R11 (200 kΩ) parallels the carrier's onboard 1.5 kΩ IPROPI resistor →
  −0.74 % sensitivity shift (1.489 k effective). Negligible and absorbed by
  the install calibration.
- **Carrier output LEDs:** red/green indicators across OUT1/OUT2 via 33 kΩ
  (per the carrier schematic) — a benign ~0.4 mA load; explains LED
  activity during coast/drive tests.
