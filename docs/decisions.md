# Active system decisions

Current accepted user/project choices, not a list of unresolved alternatives.
[Open issues](open_issues.md) retain proposals requiring disposition.

## Release and review ownership

- **D01 — First release:** commissioning-only, explicitly selected by Robert
  Klinkhammer when implementation began on 2026-11-04. Includes the integrated
  monitor, deliberate ARM/recovery and qualified motor operation. Vehicle planning,
  odometry/localization integration and GV are deferred to a later release.
- **D02 — Gate ownership:** Robert Klinkhammer is accountable owner and reviewer
  for the gate roles, using time-separated self-review of recorded evidence.
  This is not independent assurance. Gate records need separate preparation/review
  timestamps and explicit dispositions. Operational spotter requirements still apply.
- **D03 — Execution:** begin B1/G1d extraction and G1 work-package preparation.
  Starting the plan does not adopt v4, select an R4 circuit or approve unknown limits.

- **D04 — Hardware-test pause and next focus (2026-11-06):** Robert pauses
  further physical testing until the monitor and motor boards are available.
  Preserve U0 PASS; U1 is implemented/build-tested but not physically qualified.
  Begin autonomous-motion architecture discussion. D01 commissioning-first scope
  remains unchanged; this does not select middleware, sensors or an external host.

## Technical baseline retained from the reviewed plan

The following decisions were consolidated in the plan at `20f5972`; implementation
and qualification status is maintained separately in [system status](system_status.md).

1. **Repository ownership:** dmotor owns PL, PS, `software/esp32/`, and shared monitor
   schema/vectors in `software/protocol/monitor/`. `motor-interface-integrated`
   owns ordered PCB/assembly/harness sources. `edf-dev` owns Zynq build/deployment
   tooling; `esp32-dev` owns the native ESP-IDF Docker environment. No firmware fork.
2. **Pins:** `mon_hb` = CK_IO2/U13; `coast_req` = CK_IO3/V13. Monitor UART uses
   `mon_tx` CK_IO0/T14: **PL TX → monitor RX**;
   `mon_rx` CK_IO1/U12: **monitor TX → PL RX**, 115200/8N1. Earlier CK_IO4/5 choices are superseded.
   U13's PUDC_B relationship is not a configuration-time safety guarantee.
3. **Preserved interfaces:** physical_faults[3:0] remains in the controller, currently
   tied inactive in the saved BD; future routing is separate work. drive_enable
   remains internal GPIO diagnostic readback, not an external permission signal.
4. **Software ownership:** the daemon exclusively owns the controller. Every
   application calling it uses `dmotor_client`. Independent protocol tests and
   explicitly inhibited UIO board diagnostics retain their distinct scopes.
5. **Control (2026-11-07):** retain the 1 ms control loop; PS executes fixed-point
   velocity estimation, ramps and PI. PL retains scheduling, encoder capture, PWM
   and independent protection. PS also owns position/trajectory/vehicle behavior. No restoration of the legacy FPGA command
   ring. Current runtime requires global rearm after STOP/disable.
6. **Output meanings:** controlled velocity STOP decelerates selected channels;
   DISABLE immediately inhibits a channel; neither asserts global coast. Disabled
   EN with monitor permits high can brake, not coast. Direct-duty STOP disables.
   Global HALT requests all-channel coast through the monitor gate.
7. **Current commissioning timing:** 100 ms authorization, 20 ms refresh, 100 Hz
   target publication, 100 ms target timeout and 500 ms client lease are the initial
   policy. They require target measurements, not merely matching constants; see
  SYS-18 in the requirements matrix.
8. **Development baseline:** PS C++14, clang-format/clang-tidy 18, native UBSan/tests;
   ESP-IDF tools stay separate. No build, connection or service start automatically
   arms, enables motion or flashes hardware.

Recovery v4 remains a **proposal**, including its exact transition semantics and
acceptance targets. Its corrections replace the withdrawn XDC pull-up mitigation
and earlier zero-target interpretation of DISABLE. Do not treat a healthy HB,
valid UART frame, successful PL prepare or low PWM as completed recovery/coast proof.

## D05 — Adopt the 1 ms PS executor on main (2026-11-07)

User authorized promotion to main, documentation reconciliation and rebuild/retest
to the existing software/FPGA/ARM-image baseline. The 5 ms profile remains historical
experimentation. Preserve D04's physical-test pause, U0 evidence, pending U1 physical
acceptance and commissioning-first scope. No operational live recovery is implied.
