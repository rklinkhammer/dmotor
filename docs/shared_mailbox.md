# Four-motor shared velocity mailbox measurement

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


This isolated experiment adds four independent motor contexts to the velocity
mailbox, with one calculation path selected by motor index. It preserves the
bounded exact velocity arithmetic and one-phase divider. Main sources and the
public architecture remain unchanged.

## Results

Vitis 2026.1, `xc7z020clg400-1`, 20 MHz (50 ns clock, 36.50 ns effective
HLS timing budget). Arithmetic, controller, and mailbox sources match the prior
single-motor measurement byte-for-byte; hashes are preserved with the evidence.

| Implementation | LUT | FF | DSP | BRAM18K | Maximum latency | Maximum interval | Path ns |
|---|---:|---:|---:|---:|---:|---:|---:|
| One mailbox | 17,589 | 7,191 | 20 | 0 | 324 cycles | 325 cycles | 35.251 |
| Four independent mailboxes (arithmetic sum) | 70,356 | 28,764 | 80 | 0 | 324 each, parallel | 325 each | 35.251 each |
| Four-context shared mailbox, load/store | **25,894** | **32,416** | **20** | **0** | **333 cycles** | **334 cycles** | **35.578** |

Sharing saves **44,462 LUT (63.2%)** and 60 DSP compared with four independent
mailboxes, while adding 3,652 FF (12.7%). The selected-context storage and working
registers explain the FF tradeoff. The RTL/report hierarchy has one service
instance and still uses 20 DSP, confirming that the calculation path is shared.
The engine uses 48.7% of available LUT, leaving 27,306 LUT by HLS estimate for
integration and other logic. This is a promising fit estimate, not a completed
board utilization or place-and-route result.

A conservative four-service reservation is `4 * 334 = 1336` clocks, **66.8 us**
at 20 MHz, before wrapper handshakes and arbitration. This leaves 733.2 us of the
800 us computation budget. The per-service latency is 16.65 us. The scheduler
still needs bounded tick priority and must account for any in-flight bus service.
This result does not measure the independent 10 us protective-inhibit path.

The in-place variant (both ordinary and two-bit index) passed native and HLS C
simulation but remained in the compiler performance-optimization phase for
several minutes. Those two runs were stopped after the load/store variant
synthesized successfully; they have no resource or RTL co-simulation result.
The load/store variant completed synthesis in under a minute. The helper defaults
to `--variant copy`; `--variant direct` preserves the unmeasured alternative.
No claim is made that load/store is the smallest possible implementation.

The next useful step is the four-channel scheduler/safety/PWM integration and
Vivado implementation measurement. BRAM-backed context storage and removal of
unused completion metadata are possible later reductions, but are not measured
here.

## Architecture and interface

`dmotor_mailbox_shared` has the single-mailbox transaction inputs plus a motor
selector. `mailbox_motor_count` defines the four-context capacity. The measured version
loads the selected context into a working object, calls `working.service(...)`
once, then stores the updated context. Its selector is explicitly two bits after
validating the external index. All per-motor
staging, pending targets, sequences, configuration cache, estimator history,
ramp, PI integral, and fault state remain separate.

Only one transaction may be in flight. The scheduler must hold the motor index,
configuration, encoder snapshot and other inputs according to the HLS handshake.
The reset argument resets only the selected context; physical HLS reset initializes
the whole engine. Invalid selectors return zero except rejection bit 16 in reply
word 9 and cannot modify any motor. Normal request and reply meanings are those
of the [single-motor mailbox](velocity_mailbox.md).

This is an internal HLS service, not a complete four-channel AXI peripheral. A
hardware scheduler, independent encoder/PWM channels and safety wrapper are still
required. Any motor's protective fault must inhibit all motor outputs without
waiting for HLS service. Watchdog/deadline enforcement, fault latching, and rejection
of late HLS results remain outside the shared calculation path. The test models
shared HALT authorization removal; it does not implement or measure that wiring.

The scheduler must reserve a bounded window for all four control updates ahead
of ordinary bus traffic. Coherent snapshots and either a common PWM update point
or explicitly bounded channel skew are integration decisions. The old
single-channel scheduler tests do not establish those new guarantees. Reset and
configuration/session changes must obey the inhibited transition contract.

## Verification

The shared-engine test compares all 16 reply words over 1,673 interleaved services
against four independent C++ mailbox objects. It exercises staging-word interleaving,
different sessions/configuration/gains, different encoder rates, supersession,
replay rejection, direction reversal, reset of one motor while peers retain state,
controlled stop, authorization loss, captured fault, modeled shared HALT fanout,
invalid selectors, and reset/reinitialization. The existing single-mailbox test
also compares control behavior with the Controller oracle. This verifies context
isolation and retained behavior; it does not qualify a physical multi-motor system.

HLS C simulation and Verilog co-simulation pass, including all 1,673 interleaved
transactions. All 16 applicable native/RTL tests pass with undefined-behavior sanitization for
native code. Existing protection RTL tests remain unchanged. Matched core source
hashes and native test output are retained in `docs/evidence/shared_mailbox/`.

## Reproduce

From the repository root, using a fresh output directory:

```sh
python3 tools/prepare_shared_mailbox.py --variant copy --output build/shared_mailbox_copy
cmake -S build/shared_mailbox_copy -B build/shared_mailbox_copy/build
cmake --build build/shared_mailbox_copy/build -j
ctest --test-dir build/shared_mailbox_copy/build --output-on-failure -R '^(arithmetic|generated_abi|control|motion|velocity|controller|dispatcher|mailbox|shared_mailbox|rtl_safety|rtl_scheduler|rtl_qcencoder|rtl_pwm|rtl_latency|rtl_axi_bridge|rtl_encoder_snapshot)$'
python3 build/shared_mailbox_copy/tools/vitis_check.py --kernel dispatcher
```

Install Icarus or supply CMake `IVERILOG`, `VVP`, and (for relocated installs)
`IVERILOG_BASE` to include the seven RTL tests. The helper reuses the dispatcher
HLS configuration slot with top `dmotor_mailbox_shared`. It composes the existing
bounded velocity and mailbox helpers, copying versioned shared-engine source and
test files into the isolated tree.
