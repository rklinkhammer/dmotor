# G1a recovery coordinator and command admission integration

Status: host integration candidate. The coordinator is built only into an
uninstalled test executable; the daemon and client wire API are unchanged. A1 is
partially implemented, not accepted. No target image was rebuilt or programmed.

## Lifecycle and authority

`recovery::Coordinator` owns the wire/model lifetime and references a Runtime
that must outlive it. Construction selects Runtime's sticky recovery hold.
`prepare(owner, configuration, mask)` cancels old authority, performs disabled PL
preparation, then records the owner and Runtime recovery generation. Prepare is
rejected while an exchange is active; cancellation is required before starting
over. An injected durable source now reserves the PS lifetime while HALT is confirmed,
before PL preparation; [discovery](g1a_session_integration.md) takes no numeric ID. `begin()` acquires [verified MMIO proof](g1a_cleanup_attestation.md) from Runtime
and starts the exchange.

The existing real codec, session link and recovery model process messages.
`finish` first requires the model's challenged ARMED result and matching local
proof, then calls Runtime's generation-bound completion/readback checks. Only
success from both admits new commands. Admission requires the current owner,
unchanged Runtime generation, released Runtime hold and completed model.

Ownership loss, explicit cancellation, detected peer restart, exchange exhaustion,
Runtime lifecycle changes, protection/lease failure and command I/O exceptions
revoke software authority and attempt HALT. Software revocation occurs before
hardware access, so HALT failure cannot preserve a valid command ticket. HALT
failure still propagates to the caller; no physical inhibition is claimed when
hardware access fails. Destruction also revokes and attempts HALT without throwing.

## Queued command rule

Stamp each incoming command with `ticket()` **at ingress**, retaining that stamp
alongside its immutable command contents. A ticket contains owner and Runtime
recovery generation. Ingress while held produces an invalid ticket. Dispatch
checks the stamp before calling enable, velocity or controlled STOP. An old ticket
cannot execute after recovery, including when the same numeric owner is reused.
Completion does not execute a queue or publish buffered Runtime targets.

Tickets are internal provenance, not authentication credentials. The caller must
bind authenticated socket ownership to the owner argument and must never mint a
new ticket when dispatching an old buffered command. Runtime and coordinator are
serialized, not thread-safe. This increment tests delayed dispatch with retained
tickets; the real socket buffer/parser is **not yet wired to this mechanism**.
Duty and timeout injection have no coordinator dispatch API and remain unavailable
through it. Client status/heartbeat/disable and ownership event routing still need
the daemon integration policy; telemetry must not renew the client lease.

## Evidence and remaining boundary

The joint test uses the behavioral monitor, real framing/session/recovery layers,
Runtime and a fake MMIO platform shared with Runtime tests. It covers successful
physical-gesture simulation and admission, commands received while held, old queue
tickets after re-recovery, owner mismatch, no buffered-target replay, cancellation
at each exchange phase and local-check, changed final configuration, peer restart,
lease expiry, request timeout, direct Runtime HALT, failed HALT confirmation and
command access failure. All 20 native tests pass under UBSan. Removing the ticket
generation comparison makes the negative-control test fail.

[Input hashes, commands and evidence](evidence/g1a_recovery_coordinator/inputs.json)
record this increment. The simulator provides synthetic physical inputs; no UART,
GPIO timing or motor qualification is claimed.

The [MMIO attestation path](g1a_cleanup_attestation.md) now supplies Cleanup at
begin, outgoing-frame checks and completion. `begin()` and `finish()` take no
caller proof or timestamp; acquisition uses the Runtime clock. The [session integration](g1a_session_integration.md) now binds durable reservation
to preparation/discovery. Next complete A8 public recovery/status semantics and
target storage policy before daemon integration.
