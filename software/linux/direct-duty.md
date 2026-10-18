# Direct-duty commissioning (software 0.8.0 / DMI2)

This mode checks PWM without encoder stimulus. It is open-loop: duty is not RPM,
and there is no speed regulation or controlled deceleration. The existing
maintenance application remains manual, not an automatic test harness.

## Build and compatibility

Rebuild/package the controller IP, board bitstream, kernel module and software
using the centralized EDF build workflow. The endpoint identity is now DMI2
(0x444d4932): the new software/driver rejects the older DMI1 image rather than
accessing undefined registers. The mailbox ABI remains DMB1. Do not install only
the new daemon onto the previous bitstream/driver. No powered mode is introduced;
`--externally-inhibited` still requires motor drive power disconnected.

## Start and adjust

Load the production FPGA overlay and driver. Use a configuration containing
exactly one motor row (the supplied output-validation.conf selects motor 0):

```sh
sudo dmotord --externally-inhibited /dev/dmotor0 \
  /usr/share/dmotor/output-validation.conf --direct-duty 50 0 \
  > /tmp/dmotor-direct-duty.log 2>&1
# Separate terminal:
sudo dmotor-maint
```

`50` is the maximum permitted absolute duty percentage; `0` disables a minimum
nonzero duty. Both are explicit whole percentages, 0..100, with minimum <= maximum.
The maximum is enforced in hardware and software; the minimum is a host command
policy. Change either by quitting, restarting the service and explicitly rearming.
They do not reuse the PI profile's 5% duty cap: the PI engine is held reset.
A 100% limit is supported, but does not establish that the adapter/motor can safely
operate at 100%. Adapter turn-on/turn-off and power-stage limits remain separate.

Enter `m 1`, `r`, `e`, then `s`. For other configured motors select its bitmask.
Use `p 25`, `p -25`, `p 12.5` to command signed percentage (up to three decimals).
The daemon prints the active mask and Q16 floor/limit; conversion is rounded to
Q16. It controls the single configured motor, independent of the menu selection.
`p 0` always means zero drive, even with a nonzero minimum. Commands below a
configured nonzero minimum or above the maximum are rejected and HALT is requested;
there is no silent boost to a larger duty.

`t` immediately disables direct drive and requires global rearm. `!`, `q`, client
loss and service close retain all-motor HALT behavior. Reversal uses the existing
PWM boundary/blanking logic. There is no output until an enabled motor receives a
fresh sequenced duty command. Mode and limit cannot change while enabled.

## Drive-disconnected checks

No encoder input is needed. Probe actual PWM, direction, drive-enable and HALT:

- Confirm zero drive after reset, rearm and enable, before the first `p` command.
- Measure 20 kHz PWM at 25%, 50%, and an appropriate higher value within the limit.
- Check signed direction, reversal blanking, `p 0`, STOP and explicit HALT.
- Test `x`: duty publication stops, watchdog refresh continues; command age must
  expire to shared HALT. Test `w`: watchdog refresh stops, duty publication continues.
- Both timeouts default to 100 ms. The host fallback after 120 ms is a failed FPGA
  timeout test, not evidence of hardware expiry. Rearm between cases.
- Assert each external fault and external HALT, and test client/service loss.
  Encoder acquisition/no-motion faults do not apply in direct mode; external
  faults, encoder decoder-range faults, authorization and command expiry remain.

Velocity commands are rejected in direct mode. All other motors are disabled;
this first implementation deliberately does not mix direct and closed-loop axes.
Telemetry's duty field reflects the direct PWM applied duty. Velocity/PI telemetry
is zero in this mode; encoder position remains observable.

## Establishing a minimum later

Breakaway duty (starting rotation) and sustaining duty (continuing rotation) can
be different, and change with load, voltage, direction and temperature. Do not
infer a single universal minimum from an unloaded motor. Record both directions
and representative loads during a separately qualified powered commissioning step.
Keep the floor at zero until measured; then select a policy deliberately. The
current drive-disconnected procedure cannot measure mechanical breakaway duty,
and its inhibition assertion must not be used with a powered motor.

[Implementation validation](../../docs/evidence/direct_duty/README.md) records
simulation, software and implementation checks separately from board tests.
