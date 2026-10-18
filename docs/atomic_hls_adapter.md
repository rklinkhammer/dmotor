# Atomic mailbox HLS transport

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


`rtl/atomic_hls_adapter.sv` connects the actual generated `dmotor_atomic_mailbox`
ports to a single-request valid/ready interface. Protocol validation, staging,
reservation, cancellation and retirement remain in `src/atomic_mailbox*.cpp`.
This adapter contains no replacement mailbox algorithm.

## Transaction contract

On `command_valid && command_ready`, capture the five request words, eleven
admission words and 64-bit effective tick. Word zero occupies the least significant
32 bits. Packing follows `include/dmotor/atomic_mailbox_top.hpp`. Input buses may
change after acceptance. Only one transaction can be outstanding.

The adapter supplies synchronous one-cycle reads on both HLS request and admission
memory ports, captures both reply write ports, and holds start until the core
accepts it. A finalization cycle lets the last reply writes settle before asserting
`response_valid`. All sixteen response words remain stable until `response_ready`.
A protocol rejection is represented by the HLS reply; `response_error` instead
indicates a transport timeout.

`MAX_CYCLES` defaults to the named `MMIO_RESPONSE_MAX_CYCLES` constant (2048).
It bounds execution after launch; it does not include response backpressure or
external arbitration. Deadline equality wins over completion. A timeout clears the
reply, returns an error, latches `fault`, resets the HLS core and blocks further
commands until reset. `last_latency` and `maximum_latency` measure successful core
execution through completion, excluding finalization and response consumption.

After reset, the trusted owner must explicitly issue the internal reset operation
with the new session before normal publication. Reset is a drained/inhibited
lifecycle operation, not a public host command that may interrupt motor control.

## Integration boundary

This is a transport component, not an AXI endpoint or scheduler arbiter. Admission
is an immutable launch snapshot. The surrounding integration must revalidate live
session, generations and safety before accepting results or releasing outputs.
It must serialize host operations, scheduler begin/retire and fault invalidations,
convert held scheduler requests into exactly one invocation, and invalidate/drain
canceled work. The adapter's fault must feed shared protection. None of those
connections is established by this standalone test. The subsequent
[mailbox-control wrapper](mailbox_control.md) now provides scheduler/host arbitration,
invalidation and live-response qualification with a separate integration test.

## Verification

`tests/atomic_adapter_tb.sv` instantiates actual generated HLS RTL. It checks 49
transactions covering initialization, identity/session reads, four-channel
publication and replacement, timestamp/negative-target capture, reservation before
retirement, cancellation rejecting late retirement, immutable inputs, response
backpressure, injected noncompletion and inhibited reset recovery. Cancellation
here means the HLS invalidate operation; it does not cancel the transport mid-call.

```sh
python3 tools/vitis_check.py --kernel atomic
python3 tools/atomic_adapter_rtl.py
```

Reconfigure CMake after generating the HLS RTL to include `rtl_atomic_adapter`.
The script fails if generated RTL is absent; it never substitutes a behavioral
mailbox. Resource utilization and routed timing of this adapter are not yet measured.
