# Shared motor-engine HLS transport

`rtl/shared_engine_transport.sv` drives the actual `dmotor_mailbox_shared` kernel
from the measured four-context copy/load/store experiment. The engine's velocity,
ramp, PI and per-motor state remain C++ HLS. This module only captures an invocation,
provides synchronous request/configuration memories and collects the reply.

One command is accepted on valid/ready. Every scalar, request word and configuration
word is captured on that edge. Both HLS memory read ports have one-clock latency;
both reply write ports are captured. The complete sixteen-word reply is held
under backpressure, with no new invocation accepted until consumption.

## Internal record

This is a provisional internal transport record, not the software MMIO ABI.
Word zero occupies the least significant 32 bits; all 64-bit fields use low word
first. The record is 35 words (1120 bits).

| Word | Contents |
| --- | --- |
| 0 | Motor index, validated by the C++ kernel |
| 1 | Flags: bit 0 selected-context reset, 1 tick, 2 authorized, 3 STOP, 4 capture fault, 5 negative encoder direction |
| 2 | Session |
| 3–4 | Effective tick timestamp |
| 5–6 | Encoder event count |
| 7–8 | Last encoder edge timestamp |
| 9 | Direction epoch |
| 10 | Invalid transition count |
| 11–14 | Existing four-word per-motor mailbox request |
| 15–34 | Existing twenty-word controller configuration |

Reply packing is unchanged from `experiments/mailbox/mailbox.cpp`. In particular,
word 0 is request acceptance, 2 is the motor's applied sequence, 4 is duty, 7 holds
velocity/acquisition/warm-up flags, 8 contains motor faults and 9 contains protocol
rejections. A transport success does not mean that the motor has accepted a target.

`MAX_CYCLES` defaults to the named full-frame `CONTROL_DEADLINE_CYCLES` upper bound.
This transport watchdog protects an individual invocation; it does not extend the
scheduler's independent deadline across all invocations. Deadline equality wins
against done. A timeout zeroes the reply, returns an error, latches a fault and
resets the complete shared HLS core. Recovery requires a global inhibited reset and
per-context reinitialization. Logical context reset in the record only resets the
selected motor; it must not be confused with this global protection reset.

## Verification

`tools/shared_engine_rtl.py` exports reference transactions from the prepared
shared-mailbox C++ test without modifying that experiment. Each checked transaction
is then replayed through this transport and the actual generated RTL. All sixteen
response words are compared to the independent per-motor C++ contexts. The bench
also checks immutable inputs, response backpressure and injected timeout/reset.

The reference exercise includes interleaved staging, warm-up, repeated velocity
updates, differing encoder rates, direction reversal, sequence replay, selected
context reset, STOP, authorization loss and capture faults. The exporter includes
1,673 ordinary transactions; the original test's separate invalid-selector calls
are checked natively but are not part of the exported RTL trace.

```sh
# First prepare and synthesize the experiment as described in shared_mailbox.md.
python3 tools/shared_engine_rtl.py
```

All 30 regression tests pass (148.11 seconds total; this test took 110.92 seconds).
The trace's maximum observed invocation latency was 315 cycles (15.75 us at 20 MHz),
excluding response finalization/backpressure. This is an observed test maximum,
not a replacement for the synthesized worst-case bound. Evidence is retained in
`docs/evidence/shared_engine_transport/regression.txt`.

Reconfigure CMake after generating the shared kernel to include this test. Missing
RTL causes an explicit error in the runner. No new synthesis/area or routed timing
claim is made for this transport.

## Next connection

The mailbox/scheduler integration still uses behavioral motor results. Its next
adapter must translate each selected batch into the measured engine's four staging
writes, commit and tick operation, while servicing ordinary ticks without a new
target. It must initialize and prepare contexts, expose qualified readiness, check
actual motor applied sequence before allowing batch retirement, echo immutable
frame tags and integrate STOP/disable/cancellation. Warm-up may leave a target
pending; accepting its staging writes is insufficient evidence for atomic batch
retirement. Removing this redundant per-motor mailbox in a later HLS top is another
option, but would require fresh synthesis and equivalence measurements.

The old four-service estimate (66.8 us) covers ordinary ticks only. Retaining the
measured per-motor mailbox requires six calls per newly targeted motor: four staging
writes, commit and tick. Using the reported 334-cycle maximum interval conservatively
for every call gives `4 * 6 * 334 = 8016` cycles, or 400.8 us at 20 MHz, before
transport overhead and atomic-protocol operations. This is planning arithmetic,
not an integrated latency measurement; warm-up and initialization require a
separate lifecycle schedule and cannot be charged as ordinary target acceptance.
