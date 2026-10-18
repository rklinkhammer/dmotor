# Powered commissioning preparation

Software 0.9.0 exposes explicit powered commissioning using the existing DMI2
hardware/driver. No FPGA or kernel ABI change is required. The current temporary
adapter is the **Pmod HB3**, as selected by the user; this does not change the
architecture's longer-term DRV8263H selection or qualify HB3 electrical limits.

## Selected board test settings

User-specified setup: GB37-520-30F motor on channel 0, temporary Pmod HB3,
12 V supply with a 2.0 A supply current limit, and a 50% maximum absolute duty.
Motor and wheels are unloaded/free-turning; no deliberate stall tests are planned.
The approved pulse limit is one second, with no automatic restart and a zero
minimum duty floor until breakaway duty is measured. These are test settings,
not a qualification of adapter or motor ratings. The supply current limit is
set on the bench supply; software does not configure or verify it.

Check the actual HB3 PWM/enable/direction wiring against the adapter documentation;
do not assume the DRV8263H wiring or fault/current-limit facilities apply to it.
The encoder assumption remains 13 pulses/channel/motor revolution, x4 decoding,
30:1 reduction: 1560 counts/output revolution, subject to physical verification.

## Launch and manual commands

First exercise this interface with drive power disconnected. Use a configuration
with exactly one motor row and its measured encoder ratio. The startup ceiling
and floor are whole percentages. Use zero floor until breakaway duty is measured.
Use the installed channel-0 direct-duty configuration below (not a PI tuning profile):

```sh
sudo dmotord --powered-commissioning /dev/dmotor0 /usr/share/dmotor/powered-direct-duty.conf \
  --direct-duty 50 0 > /tmp/dmotor-powered.log 2>&1
# Separate terminal:
sudo dmotor-maint
```

Enter `m 1`, `r`, `e`, `s` for channel 0; then enter
`p PERCENT` within the configured ceiling. Direct duty always controls the single
configured motor. Each pulse is limited to one second. `p 0`, `t` and expiry
stop/disable drive and require `r`, `e` and a fresh `p` before further output.
`!` HALTs all motors; `q` HALTs and exits. `s` displays encoder positions. The
startup banner/log identifies powered mode and its pulse duration. `x` and `w`
are rejected in powered mode and request HALT; use inhibited mode for those tests.
No service starts automatically, and rearm/enable alone produces no drive.

## Runtime behavior

`RuntimePolicy::direct_pulse_us` bounds one direct-duty pulse per rearm. Zero
preserves existing inhibited continuous-duty behavior. The deadline begins on
acceptance of the first nonzero duty request; subsequent requests do not extend
it. Deadline expiry or a zero-duty request performs immediate direct-mode STOP,
disables drive and requires global rearm. HALT, disable and recovery discard the
pulse. Timeout injection is unavailable with bounded pulses.

The deadline is host-enforced, checked before periodic publication/refresh and
before accepting another duty command. It is not a new FPGA timer. Normal service
scheduling adds tick/jitter latency; if the host stalls, the existing hardware
command-age and authorization timers provide fallback inhibition. Neither this
software test nor a commanded STOP proves an electrical shutdown duration.

## Board test sequence

1. Record software source identity, loaded hardware identity, motor/channel,
   encoder ratio, supply/current-limit settings and adapter wiring. Record the
   actual fault polarity and drive-enable behavior. Keep drive power disconnected.
2. Finish output validation: measure PWM/direction/enable and shared inhibition,
   explicit HALT, quit, client disconnect/stall, service termination, external
   fault and both timeout paths. Capture shutdown latency and confirm no automatic
   recovery. The user reported PWM/direction, x/w timeout response, mc0_fault
   shutdown, latched inhibition and explicit recovery; electrical timing and
   all-channel coverage have not been reported.
3. Test the powered interface with drive disconnected first.
   Start a pulse; repeatedly submit duty updates and verify the original deadline
   still stops output. Check zero, STOP, HALT, lease loss and service death. Verify
   expiry rejects further duty/enable until rearm, and rearm/enable alone produces
   no PWM. Record traces and logs. Retain inhibited mode for x/w injection tests.
4. After reviewing those results and adapter current limiting, connect one motor
   with wheels unloaded and a suitable means of interrupting drive power. Start
   below the 50% ceiling and with a short pulse. The ceiling is not a required starting duty. Observe current, direction and
   encoder position before increasing duty. End each pulse at zero; wait for the
   shaft to stop before reversing. Do not deliberately stall the motor.
5. Record breakaway and sustaining duty separately in each direction. Direct mode
   has no velocity regulation or encoder no-motion stall detection; a stationary
   shaft under PWM is not proof of a safe stopped condition. Current limiting and
   external fault protection must be established independently.
6. Use encoder position deltas to verify polarity and counts per revolution.
   Direct-mode measured velocity/PI fields are zero, so do not interpret them as
   measured stationary speed. Global rearm resets position epochs: compare samples
   from the same pulse/session only.
7. Proceed to a separate conservative closed-loop velocity tuning plan after
   direct-drive polarity, current behavior and feedback have been checked.

Keep a record per case: configuration, source/bitstream identity, requested duty,
pulse duration, initial/final encoder counts, peak current, scope trace, observed
fault reason, and recovery behavior. The maintenance menu remains an interactive
maintenance application; semantic tests run separately.

## Implementation validation

Native CMake build with `DMOTOR_SW_SANITIZE=ON`: all eight CTest suites passed on
2026-10-18. Runtime cases cover fixed deadline despite updates, zero-duty STOP,
rearm requirements and late command rejection. The real service/client process
fixture covers powered pulse expiry with continuing lease/commands, early zero,
client loss and powered timeout-injection rejection. The fixture substitutes a
software device; it does not exercise the Linux driver or FPGA. No new ARM/QEMU,
EDF image build, board deployment or powered test was performed for this increment.

Rebuild/redeploy the EDF software/image using the existing source-selection and
staging workflow in EDF's `docs/build.md`; verify the board startup banner reports
0.9.0 and the intended source identity. No bitstream change is required for this
software increment when the currently tested DMI2 direct-duty image is loaded.
