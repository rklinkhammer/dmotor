# Current system issues and relevant resolved findings

Reconciled **2026-11-12** for the default **Zybo + one monitor + one motor board**
fixture. This is the consolidated current register across dmotor, EDF and the
hardware repository. Robert Klinkhammer is accountable owner/reviewer for all
rows. Implemented fixes are distinguished from physical qualification.

[Current status](system_status.md) · [Next tests](system_bringup_plan.md#current-one-board-sequence--2026-11-12)
· [Verbatim previous registers and archive manifest](archive/workspace-reconciliation-2026-11-12/README.md).
Historical closed design/fabrication matters and Arty build chronology live in the
archive. Original issue IDs remain traceable. U08 vehicle work is deferred, not
closed; it is outside this fixture and commissioning release.

## Open work affecting the current fixture

| ID / origin | Current disposition and action | Closure evidence / gate |
|---|---|---|
| U01 / R4 | Independent PL-only clock/reset/config/power failure response remains unresolved; monitor watchdog alone does not cover it | Measured failure experiments or explicit restricted failure model; G1b/G5/G6 |
| U02 / recovery | CLN1/PSF1 and host models implemented; operational monitor pairing, persistent sessions, ARM/recovery coordinator and race qualification incomplete | Adopted transition contract, adversarial tests and physical recovery traces; G1a/G4/G5. No live recovery claim |
| U03 / timing | 16 ms period, <12 ms work / >=4 ms reserve retained; previous one-INA measurements support diagnostics only | Actual one-board workload and age/deadline measurements; later four-device timing separately; G1c/G3 |
| U04 / IDF isolation | Task-level injected expiry works; ISR/cache/flash/WDT independence and blocked-worker behavior need qualification | Measured stalls, late results and output inhibition; G3/G5 |
| U05 / PS timing | Explicit receiver failure reporting and loss timeouts demonstrated on prior fixture; loaded 1 ms worker and bounded logging/storage remain open | Current Zybo load/deadline evidence, including 800 us hardware limit; G4/G5 |
| U06 / population and calibration | One actual motor sensor, battery ADCs, IPROPI mapping and policy thresholds must be configured; missing/corrupt data must inhibit | Verified address, known-reference calibration/persistence, invalid/missing sensor cases and current-policy tests; G1c/G3/G6 |
| U07 / hardware interface; I7 | HB/coast/PL protection implemented; current Zybo `hb10` detection confirmed. Full electrical acceptance is open | UART mapping, actual GPIO21 connection, HB loss/latch/rearm, STOP/HALT/DRVOFF and manual encoder evidence; G2/G5 |
| U09 / release; EDF console | Latest Zybo hardware/WIC artifacts exist; dirty build fingerprint differs from current HEAD. Clock/console source corrections implemented; exact installed image not recorded | Match target to manifests, readable 115200 console, DDR/Ethernet/controller checks; G2/G7 |
| U10 / documentation | Current status/register/plan reconciled; original issue logs snapshotted. Historical assertions are not current authority | Requirement/link review and dated gate acceptance; G1d/G7 |
| U11 / powered entry | Owner/self-review method selected; current one-channel powered procedure and gate disposition not yet accepted | Verified ITRIP, operating limits and G5/G6 entry record; D10 waives motor-survival deadline only |
| H01 / I6 ITRIP/DIAG | Verify fitted R13/RV1 and selected ITRIP band; R13 DNP leaves high default. Do not treat the trimmer as continuous current control. Retain R5 9.1k specification | Actual fitted values, driver-specific level data and bounded trip/coast/chop measurement before powered use; G1b/G6 |
| H02 / I3,I5 | I2C pull-up/module-shunt preparation and absent-board handling need assembled checks; old bench INA was 0x44, design motor0 is 0x40 | Identify actual address/straps; no assumption either address is correct. Missing INA must fault expected channel; ignore floating nFAULT/IPROPI from absent boards; G2/G3 |
| H03 / I4,I10 | STOP sense pull-up correction and MAX706R/74HC08 HALT gate exist; verify actual partial population and supervisor | STOP closed/open/bounce, permits low at boot/reset, HALT, watchdog timeout and no unintended restart. WDO recovery is not latched; G1b/G2/G5 |
| H04 / I6 power/ADC | Re-ratio protects positive midpoint miswire in analysis; reversed sense and supply-off backfeed remain unqualified | Measure rail-off/back-power/clean startup; check battery ADC scaling and chosen fault envelope; do not repeat historic claim that backfeed is proven harmless |
| H05 / I6 power-distro | Converter/fuse/cable/heatsink selection, regeneration and motor-board bulk/TVS behavior still open | One populated branch, measured rail droop/overshoot and thermal behavior; encoder L7805 load check (<=100mA assumption); G1b/G6 |
| H06 / I8 | HB GPIO21 avoids RGB pin conflict; PCB firmware must not use breadboard GPIO3 override. Verify received ESP32 variant permits GPIO35–37 use | Actual board/module identity and pin continuity recorded; G2 |

## Resolved or implemented findings still relevant to this assembly

| Finding | Recorded resolution / evidence | Retained check or limitation |
|---|---|---|
| I1 carrier nFAULT overvoltage | Motor carrier VDD/SLEEP moved to monitor 3V3 in board design | Verify assembled rail/nFAULT levels; FPGA and MON 3V3 must not be tied |
| I2 address allocation | Four design addresses 0x40–0x43; battery current INA removed in favor of pack ADCs | Single fixture only: record actual INA identity. A repeated read of one device is not four-board evidence |
| I4 STOP semantics | R10 pull-up changed to 3.3k; closed chain LOW is healthy, open/non-solid-low means STOP | Partial-population and bounce measurements remain H03 |
| I8 HB/RGB conflict | HB assigned GPIO21; GPIO3 was only a breadboard diagnostic override | Restore PCB profile, record actual LED variant independently |
| I9 balance current paths | Only monitor J10.3 (midpoint) and J11.3 (full pack) connect; pins 1/2 NC | Verify assembled continuity; no duplicate battery-series or ground path via balance leads |
| I10 watchdog gate | MAX706R/74HC08, permit pulldowns and HALT gate added; prior wrong threshold/pinout corrected | Actual device identity, timeout and reactivation behavior remain H03 |
| Zybo clock/console | PS clock corrected 50 → 33.333333 MHz; EDF boot/Linux console directed to USB UART1, 115200 | Routed/WIC output exists; attach target boot identity/result under U09 |
| Zybo HB | Operator reports current Zybo detects ESP32 `hb10` on 2026-11-12 | PASS synthetic detection only; no sensor qualification, loss-time bound or motor permit claim |
| Diagnostic failures | Prior UART ownership problem resolved; INA bench found 0x44; explicit failure reporting and 16 ms timing diagnostic passed | Preserve [G2 results](gates/G2.md), do not reopen solved bench failures or transfer them to new assembly |
| Schematic overwrite | Two-sheet power-distro restored from 19:10 backup; ERC zero; operator reopened | Prototype design only; converter selection and hardware tests remain H05 |

## Scope and archive rules

The assembly wait is resolved for this one-board fixture. Four-motor population,
full-vehicle planning and autonomous behavior remain later work. Earlier fabrication
fit/order, cost, package lifecycle and superseded design-review narratives are
archived; material residuals (ITRIP, STOP, backfeed, power and actual module variant)
remain above. Historical encoder/build reports retain their original scope.

For new results, update the existing row and G2/evidence record with source/image
identity, actual population, stimulus, expected/observed behavior and limitations.
Close only the demonstrated subcase; do not turn build or visual evidence into
powered acceptance. No firmware or electrical test was performed by this audit.
