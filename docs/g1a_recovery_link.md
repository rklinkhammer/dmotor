# G1a recovery wire adapter

Status: host-tested implementation candidate, isolated from the daemon and not
installed. G1a remains open. This increment connects the PS recovery model to the
session/replay layer and tests exchanges against a scripted monitor fixture through
the actual framing codec. It is not a complete monitor safety-state simulator or
ESP32 firmware.

## Implemented boundary

[`recovery::Link`](../software/include/dmotor_sw/recovery_link.hpp) owns a
`SessionLink` and creates a recovery `Model` only after mutual discovery confirms
the endpoint IDs. The caller must supply durably allocated lifetimes and actual
inhibition/cleanup proof. This adapter performs no MMIO, storage or UART I/O.

The model's sequence is a logical request identity. The adapter numbers each new
request once through SessionLink, which already consumed wire sequences for
HELLO/ACK. Retries return the exact stored frame, including sequence, challenge,
transaction and payload. RECOVER_REQ flags are derived from the accepted cleanup
fields; configuration and cleanup request identity remain local proof data.

Only new link-accepted RECOVER_STATE, COMMIT_ACK and STATE_REPLY frames are decoded
into model replies. Wrong-direction control messages are ignored. Duplicates do
not re-enter the model. State values outside the schema abort; NAK and FAULT_EVT
conservatively abort even when their payload does not identify a current cause.
Pair restart, link exhaustion, explicit invalidation or a new discovery attempt
discard the model and cached request, closing admission with a HALT obligation.
The caller must execute that obligation in hardware; this prototype cannot do so.

Transport `Delivery::accepted` means only that framing/identity/replay checks
passed. Callers must inspect phase, admission and HALT status for semantic results.
A COMMIT_ACK never opens admission. A matching challenged ARMED response moves to
local_check; matching fresh local cleanup proof is still required. Ordinary UART
silence after completion does not itself revoke admission, consistent with the
advisory operational link. An observed peer restart or explicit invalidation does.

## Evidence and limits

The scripted two-endpoint fixture uses real SessionLink instances and codec bytes.
It counts COMMIT execution, caches replies, and exposes an explicit simulated ARM
event. Tests cover normal and lost-ACK recovery, byte-identical retries, duplicate
responses, one COMMIT execution, stale challenges, changed final proof, request
exhaustion, UNKNOWN after query exhaustion, delayed replies, peer restart, invalid
states, faults, NAKs and wrong session echoes.

All 18 native tests pass with UBSan. A negative control that re-numbers a cached
retry fails the byte-identity assertion. See [evidence](evidence/g1a_recovery_link/inputs.json).
Existing installed/ARM exports predate these isolated candidates.

The [behavioral monitor simulator](g1a_monitor_simulator.md) now tests fault epochs,
staged expiry, ARM gestures and reset ordering through this adapter. Its timing
choices remain for review. Next implement the runtime admission/queue hold, then
integrate durable lifetime allocation, cleanup proof acquisition and public recovery API.
Discovery retry scheduling and physical UART transport integration remain open.
The scripted fixture does not qualify these missing behaviors.
