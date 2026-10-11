# HLS command engine

`dmotor_command_engine` synthesizes `CommandRing` directly from C++14. Static
ring state persists across internal hardware transactions. The same code runs in
native tests, Vitis C simulation, synthesis, and RTL co-simulation. The public
MMIO ABI remains provisional and is not the generated HLS port layout.

## Internal request interface

A request is four uint32 words: operation and up to three operands. A reply is
16 words as documented in `include/dmotor/command_engine.hpp`. The generated
array ports and ap_ctrl_hs handshake belong to the future hardware adapter.
Inputs remain stable throughout each transaction; results are consumed only on
completion. No request in this interface is a CPU-issued motor-control tick.

| Operation | Operands | Effect |
|---|---|---|
| 0 poll | none | Hardware may capture/validate one descriptor when executor_ready |
| 1 stage | slot, word, value | Write a free descriptor word |
| 2 publish | requested producer, epoch | Validate capacity and session; lock slots |
| 3 reopen | none | New epoch when executor not ready, upstream drained, no active/pending work or unacknowledged results/cancellation |
| 4 quiesce | none | Close admission; retain work and status |
| 5 acknowledge completions | requested consumer | Release only published completions |
| 6 acknowledge cancellation | none | Release cancellation record |
| 7 read completion | counter, word | Read a stable unacknowledged record word |
| 8 read active | word | Read the captured immutable descriptor |

Separate trusted hardware inputs carry configuration generation, validated maximum
speed, measured position, timestamp, executor readiness, completion epoch/sequence,
and sticky cancellation. Software must not be able to forge readiness, upstream
write draining, or motor-command completion through raw register writes.

Cancellation wins over requests and executor completion at transaction entry.
It closes admission, records the original pending interval and active sequence,
and deposits an aborted active completion using previously reserved capacity.
Further cancellation events do not overwrite the frozen record; fault accumulation
remains in the independent protection block. A stale executor completion cannot
finish a new command with a different epoch or sequence.

A poll with executor_ready false cannot activate queued work. The adapter must
poll fairly under CPU load and must forward readiness only while authorized.
Readiness cannot be a substitute for the independent final output inhibit.
Command-ring completion timestamps currently record transaction-entry timestamps;
actual motor activation is a later executor handshake. The executor integration
must establish the required control-boundary activation timestamp before this is
a release completion interface.

## Integration constraints

This is a sequential HLS transaction engine, not an AXI peripheral. The new [AXI/HLS peripheral](axi_peripheral.md) captures requests atomically,
manages independent AW/W channels, validates full-word writes, returns errors,
and maintains stable responses. Full telemetry snapshots remain integration work. A quiesce result does not prove the upstream interconnect is drained.
Reset still requires platform bus isolation/draining before token reuse.

HLS processing can take multiple clocks. Safety cancellation must be latched by
independent hardware until serviced; newly returned activation/results must be
masked if authorization was revoked during the transaction. The processor cannot
hold an HLS request indefinitely or prevent protection by withholding AXI responses.
The revision 0.11 HLS-first MMIO budget is 2,048 clocks, with measured integrated
results in axi_peripheral.md. The original eight-clock proposal is superseded.

No control algorithm, controlled-stop deceleration, watchdog refresh handler, or
physical output is implemented by this command engine. Those remain separate
components, with integration still pending.
