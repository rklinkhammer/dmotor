# G1a Runtime admission hold

Status: internal Runtime boundary implemented and host-tested. A1 is partially
implemented; coordinator, daemon queue and client API integration remain open.
This increment changes compiled Runtime code, but no installed/ARM image has been
rebuilt. Existing daemon/commissioning profiles remain PL-only: none selects the
new `RuntimePolicy::recovery_required` option yet.

## Internal coordinator API

Set `recovery_required=true` at construction to start held, or call
`hold_for_recovery()` to enter the mode permanently for this Runtime lifetime.
Entering the hold asserts HALT and clears all buffered motion intent. Every HALT
and prepare invalidates completion generations. After prepare succeeds, a trusted
serialized coordinator captures `recovery_generation()` for that exchange.

Only that coordinator may call `complete_recovery(generation)`, after it has
validated the challenged ARMED response and fresh local proof for the exact
endpoint/epoch/transaction/PL tuple. The generation protects against completion
from an earlier local prepare/HALT; it is **not** proof of remote ARM and is not an
authentication token. The completion method is not a daemon command or client API.
The [host coordinator](g1a_recovery_coordinator.md) now wires the recovery model
to this method in deterministic tests; the daemon is not connected.

Completion requires prepared/held state, a matching nonzero unexhausted generation,
a live maintenance lease, the current session and CLN1 completion/request/session,
qualified monitor status, unchanged selected-motor configuration, and disabled
healthy controller status. Read/proof failure attempts HALT and leaves admission
held. A stale generation is rejected without replacing the current exchange.
Generation exhaustion stays closed. Direct-duty completion is not qualified and
fails closed; existing direct-duty commissioning behavior remains unchanged when
recovery mode is not selected.

Completion only releases software admission. It emits no enable, refresh, duty
or target write. Subsequent motion requires a new explicit enable and new command.
Readback is a point-in-time check, not a perpetual or atomic hardware guarantee;
PL protection and the coordinator's fault/ownership ordering remain necessary.

## Command behavior while held

| Operation | Behavior |
|---|---|
| enable, velocity, duty, controlled STOP, timeout injection | Reject before motion/register writes |
| tick | While prepared, checks lease/protection and unexpected enables; publishes no target or watchdog refresh |
| heartbeat | Renews the normal client lease only; cannot release hold |
| status, capture | Remain available with existing coherence/access checks |
| disable | Remains available; cannot release hold |
| HALT / destruction | Clears buffered targets, enabled/tracking state, pulse and injection activity; invalidates completion and attempts hardware HALT |
| prepare | Performs disabled PL cleanup and advances generation; never releases recovery admission |
| complete_recovery | Trusted coordinator only; checks above, never starts motion |

Runtime is serialized, not thread-safe. The coordinator must reject/defer prepare
during an in-flight exchange according to its state table, invalidate on ownership
loss/fault/peer restart, and discard queued daemon commands from older admission
generations. The Runtime buffer is tested here; external socket queues are not yet
generation-tagged. No daemon recovery mode should be enabled before this integration.

## Evidence and next work

Expanded `software_runtime` tests cover blocked commands with zero register writes,
stale/repeated completion, sticky hold across prepare, lease expiry during hold,
unexpected hardware enables, changed session/cleanup/configuration/monitor status,
access loss and discarded buffered velocity. Fresh enable alone never republishes
the old target. Existing commissioning/client tests continue to pass.

All 19 native tests pass with UBSan. Removing the enable guard causes the
negative-control test to fail. [Evidence](evidence/g1a_runtime_admission/inputs.json)
records inputs and results; no physical timing or gate acceptance is claimed.

The [coordinator integration tests](g1a_recovery_coordinator.md) now cover ownership
loss and retained command tickets. The [MMIO attestation path](g1a_cleanup_attestation.md) now provides proof to the
coordinator. Next complete durable lifetime hookup and A8 recovery/status API
before daemon hookup.
