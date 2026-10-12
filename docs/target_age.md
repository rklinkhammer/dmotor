# Independent target-age protection

`rtl/target_age.sv` implements the next revision 0.18 component: four independent
tracking-intent timers with a common inhibit request and per-motor latched causes.
It runs every fabric clock, outside the shared HLS computation. The existing
`dmotor_safety` authorization watchdog is unchanged. Integration with the atomic
protocol, scheduler, public MMIO and output gate remains pending.

## Interface and lifecycle

`timestamp` is the trusted free-running 64-bit fabric timestamp. `configure` selects
channels whose timeout is updated from `timeout_cycles`; updates are accepted only
while that channel is disabled and disarmed. `configure_accepted` reports successful
writes. Invalid updates preserve the prior configuration. Reset invalidates all
configuration and leaves every channel disarmed.

Generated named constants in `abi/timing.json` define the initial 100 Hz processor
publication period and 100 ms target timeout. The configurable engineering range
is initially 1 ms through 10 s, expressed as generated cycle limits. These bounds
are design choices subject to integration/host timing measurements. The module
requires explicit valid configuration; reset does not silently install the default.

`applied_valid`, `applied_mask`, and `applied_tick` describe a trusted, successfully
computed candidate batch at the retirement boundary, not a raw software commit.
The module accepts the entire selected mask or none. All selected channels must
be enabled, configured and outside STOPPING/READY transitions. A valid timestamp
must not be in the future/ambiguous half of modulo time, and must not already be
at or beyond any selected channel's timeout. No channel arms while any timer fault
or expiration inhibits the engine. `applied_accepted` is the registered acceptance
pulse. Only selected channels acquire the common effective tick.

READY starts disarmed and requests zero drive. A successful velocity batch arms
tracking, including a zero-velocity target. `stop_or_ready` ends tracking on a
controlled-stop or READY transition; the controlled-stop deadline and authorization
watchdog remain separate protections. Dropping `enabled` also disarms. The scheduler
must not incorrectly assert `stop_or_ready` during velocity tracking.

Expiration is `timestamp - last_tick >= limit`, using 64-bit modulo intervals
shorter than half the timestamp range. It is evaluated from the previous armed
state before stop, disable, acknowledgment or new activation. A fresh target one
clock before expiry renews intent; a target on the expiry boundary loses. An
already-old first activation or invalid future timestamp fails closed with a
selected-channel fault. A rejected/unavailable mask does not partially arm channels.

There is no watchdog-refresh input: refreshing authorization cannot renew intent.
There is also no connection from this timer to the watchdog's last-refresh state.
Each remains independently effective.

## Fault and retirement wiring contract

`inhibit_all` asserts on reset, a current expiration/invalid-age event or any latched
fault; it must feed the independent final output gate and late-result rejection.
`faults` identifies every initiating timer. `halt_out` is the OR of those latched
causes and must enter the shared latched HALT aggregator. All timers disarm after
global inhibition, preserving last-applied and first-fault timestamps for diagnosis.

Acknowledgment clears a local cause only after that channel is disabled and
disarmed, with no new expiration. New causes win simultaneous acknowledgment.
Clearing local causes must not clear the external aggregator or enable outputs.
The existing explicit recovery/session-drain/enable sequence still applies.

The future wrapper must validate the candidate batch/session/generation, test
live safety and target-age eligibility, and withhold public successful retirement
and PWM release if this module refuses the batch. Its acceptance pulse alone is
not permission to drive: recheck live inhibit at output release. Do not first
publish a successful applied sequence and then discover that freshness rejected
it. Common PWM commit and stop/deadline monitors are separate integration work.

Outputs include per-motor 64-bit age (zero while disarmed), last-applied tick and
first-fault timestamp. A bus adapter must snapshot these coherently and expose
configuration/acknowledgment through the draft ABI; this component adds no new
software offsets yet.

## Tests and synthesis

The RTL test instantiates an actual independent authorization watchdog and a
latched shared HALT aggregator alongside the target timers. It covers:

- Unconfigured/disabled/unavailable members and all-or-none arming.
- Configuration rejection while enabled; zero/out-of-range limits.
- READY, tracking, controlled-stop disarming and watchdog expiry during stopping.
- Exact-boundary expiry versus renewal/stop/acknowledgment, and early renewal.
- Every initiating motor, simultaneous faults and per-motor renewal isolation.
- Default 100 ms boundary, stale initial timestamps, future timestamps and modulo wrap.
- Watchdog refresh continuing while target age expires; no automatic restart;
  local acknowledgment preserving the external HALT latch.

The test supplies deterministic timestamps, including jumps to boundaries; it does
not prove the integration timestamp source or elapsed wall-clock behavior. Existing
scheduler/encoder/PWM/protection tests remain applicable within their prior scope.
All 26 native/RTL regression tests pass; the extended timer cases also pass separately.

Vivado 2026.1 out-of-context synthesis for `xc7z020clg400-1` reports **887 LUT,
657 registers, zero DSP and zero BRAM**. At the generated 20 MHz constraint,
standalone setup slack is +40.925 ns; hold slack is -0.327 ns before placement and
routing. The report therefore does **not** establish timing closure. Boundary
delays are idealized to zero, and the OOC clock source has no integrated skew
model. Complete clock/I/O constraints, placement, routing and hold fixes remain
required; no physical inhibit/network timing claim is made.

```sh
python3 tools/generate_abi.py --check
cmake -S . -B build/atomic_protocol
cmake --build build/atomic_protocol -j
ctest --test-dir build/atomic_protocol --output-on-failure
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch -source tools/synthesize_target_age.tcl
```

Icarus is required for RTL tests. Reports and test output are retained under
`docs/evidence/target_age/`. The synthesis script derives the clock period from
the generated timing package. The [scheduler/PWM boundary](shared_control.md) now connects this protection to
four PWM channels. Generated-HLS protocol/engine adapters and complete end-to-end
verification remain the next milestone.
