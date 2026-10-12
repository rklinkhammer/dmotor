# Velocity-only profile evaluation

The active project remains the full 20 MHz profile. This experiment lives in
`build/velocity_only`; its reproducible patch and evidence are under
`docs/evidence/velocity_only/`, based on source revision `17116b7`.

## Contract evaluated

Retain opcode 1 (velocity setpoint) and opcode 6 (queued controlled stop), the
four-entry command ring, completion/cancellation protocol, stale-session and
configuration checks, rational velocity estimation, low-speed feedback acquisition,
PI regulation/anti-windup, fractional acceleration/deceleration ramps, immediate
controlled stop, stop timeout, and zero drive after stop completion. Velocity
setpoint completion still means acceptance; regulation continues afterward.

Reject position opcodes 2/3, raw duty opcode 4 and dwell opcode 5 as unsupported
opcodes, without activation. Supervisory software now owns positional trajectory
planning, position completion and dwell sequencing. Encoder capture and position
reporting remain 64-bit. No shared-motor engine or simplified queue was added.
The watchdog and HALT wrappers remain independent; HLS is not the inhibition path.

Remove position target/error, braking/lookahead preparation and divisions, dwell
state, and open-loop state from the motion executor. Without 64-bit dwell durations,
stop timeouts can compare elapsed cycles directly with `uint64_t(timeout_us) *
clocks_per_us`; this retains expiration priority and unsigned timestamp wrap behavior.
No speed-estimation or PI approximation is introduced.

The existing wire/configuration layouts are retained for comparison. Some fields
are unused in the candidate. This is not a deployable new ABI: adoption requires
profile/capability identification, host behavior and documentation changes, plus
updated AXI-to-PWM integration verification. A host must not assume that the old
position/dwell/raw-duty commands remain supported.

## Measured results

Vitis 2026.1, xc7z020clg400-1, 20 MHz, unchanged constraints:

| Kernel | LUT full → velocity-only | FF full → velocity-only | DSP full → velocity-only | Worst time us full → velocity-only |
|---|---:|---:|---:|---:|
| Motion | 16616 → 11499 | 7135 → 5621 | 32 → 15 | 37.60 → 24.45 |
| Controller | 23793 → 18871 | 10526 → 9188 | 39 → 22 | 47.55 → 34.40 |
| Dispatcher | 24578 → 21532 | 12107 → 10695 | 39 → 22 | 53.20 → 40.05 |

Dispatcher saves 3,046 LUT (12.39%), 1,412 FF (11.66%) and 17 DSP (43.59%).
BRAM remains four BRAM_18K for dispatcher and zero for motion/controller.
Estimated paths are 35.860 ns motion, 36.205 ns controller and 34.617 ns dispatcher,
within the effective 36.50 ns HLS budget. These are pre-route estimates, not
physical timing closure. Resource totals from separately synthesized kernels
must not be added or subtracted as an exact integrated cost breakdown.

Four independent velocity-only dispatchers would total 86,128 LUT, exceeding
the device's 53,200 LUT before surrounding integration. Two copies total 43,064,
leaving 10,136 LUT before additional motor contexts, scheduling, AXI, PWM and
protection. This motivates a two-motor shared-engine experiment; it does not
establish its fit. Two sequential current services would nominally take 80.1 us
before additional sharing overhead, within the 800 us computation budget.

## Verification and scope

All three affected HLS flows passed C simulation, synthesis and Verilog
co-simulation. Fourteen selected native/RTL tests passed: arithmetic, generated
ABI, control, motion, velocity, controller, dispatcher, and the seven independent
RTL tests (safety, scheduler, encoder, PWM, latency, AXI bridge, encoder snapshot).
The profile motion tests exercise signed ramping, fractional increments, PI
saturation/anti-windup, controlled stops, timeout over timestamp wrap, authorization
loss and invalid feedback. Dispatcher tests retain ordering, completion pressure,
stop/cancellation priority, generation/session errors and rejected-opcode checks.

UBSan trace comparisons against the full source match 4,451 motion replies,
990 controller replies and 1,251 dispatcher replies for retained behavior. Removed
opcode cases are excluded from equivalence and separately tested for rejection.
The old full-profile test suite cannot be claimed to pass unchanged: position,
dwell and raw-duty tests assert intentionally removed behavior. The old generated
motion/peripheral integration tests were not used to qualify this profile;
complete profile-specific AXI/safety/PWM integration and physical motor testing
remain outstanding. No four-motor implementation has been measured.

## Reproduce

From the main repository (output must not already exist):

```sh
python3 tools/prepare_velocity_profile.py --output build/velocity_only_new
python3 tools/check_velocity_profile.py --profile build/velocity_only_new
python3 build/velocity_only_new/tools/vitis_check.py --kernel motion
python3 build/velocity_only_new/tools/vitis_check.py --kernel controller
python3 build/velocity_only_new/tools/vitis_check.py --kernel dispatcher
cmake -S build/velocity_only_new -B build/velocity_only_new/build
cmake --build build/velocity_only_new/build -j4
ctest --test-dir build/velocity_only_new/build --output-on-failure -R '^(arithmetic|generated_abi|control|motion|velocity|controller|dispatcher|rtl_safety|rtl_scheduler|rtl_qcencoder|rtl_pwm|rtl_latency|rtl_axi_bridge|rtl_encoder_snapshot)$'
```

CMake needs Icarus Verilog available to register all seven RTL tests; use the
explicit tool/backend paths documented by the main project if it is relocated.
The preparation helper requires `patch`, preserves the source tree, and refuses
to overwrite an existing output. Preparation was checked to reproduce all six
modified source/test files exactly in a second isolated directory.
