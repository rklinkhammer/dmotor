# G1a MMIO cleanup attestation acquisition

Status: Runtime acquisition and host coordinator integration implemented; daemon
hookup and target qualification remain open. No installed image was rebuilt.
This replaces caller-supplied Cleanup values at the coordinator boundary.

`Runtime::attest_cleanup(generation)` requires a prepared, held, non-direct-duty
runtime with the current nonzero, unexhausted generation. It reads and checks:

- Controller identity and CLN1/HB capabilities; CORE_RESET deasserted.
- Current PL session and exact completed cleanup request/session (status 2).
- HB qualified and no monitor fault; no enabled motor, controller fault, shared
  HALT or blocked status.
- Every configuration word for each selected motor against the configuration
  saved by successful prepare, in two passes. Identity/protection checks bracket
  and separate those passes.
- A live maintenance lease at both ends, nondecreasing platform time, and total
  acquisition duration strictly below the Runtime transition budget (20 ms by
  default). Fences precede each identity/protection pass.

Only successful acquisition returns disabled/drained/causes-clear flags. The
configuration identity is the Runtime recovery generation, binding the selected
configuration and mask for this prepared lifetime. It is not a content hash or
wire field. Session/request come from the verified prepared CLN1 tuple. Failure
attempts HALT, invalidates the Runtime generation and keeps admission closed.

Coordinator `begin()` and `finish()` no longer accept proof arguments. They acquire
proof internally and use the Runtime platform clock after acquisition for model
timing. Every outgoing recovery frame, including retries, gets another acquisition
check before being returned to the transport caller. Completion also retains the
Runtime final recheck. A failed check cancels the exchange and revokes command
tickets; a failed first transmission produces no request frame.

## Limits and integration obligations

Repeated reads detect persistent changes across the scan, not arbitrary change-and-
restore (ABA) events or changes after the last read. Configuration is serialized
and single-owner in software; the PL must enforce its cleanup invalidation and
protection rules. This does not claim an atomic hardware snapshot, independently
readable PI state or physical output measurement. CLN1 supplies the drain proof.

The transport must send returned frames promptly under the same serialized owner,
continue Runtime supervision, and drop pending TX on faults or generation changes.
These tests do not implement a real UART TX queue. Caller-supplied model proofs
remain useful in isolated semantic tests; they are no longer accepted by the
coordinator. [Durable endpoint reservation](g1a_session_integration.md) now runs through an
injected storage source before PL preparation.

## Evidence and next step

Expanded Runtime tests mutate configuration after its earlier word was returned,
session, cleanup request/status, monitor fault, output-enable state and reset
between reads; they also inject access loss and acquisition deadline expiry.
Coordinator tests verify no first TX after cleanup identity changes and no admission
when cleanup session changes during the final readback. The existing joint monitor,
queue, ownership and lease tests continue to pass.

All 20 native tests pass with UBSan. A negative control removing the second
configuration pass fails the mid-read configuration-change assertion. See
[evidence and input hashes](evidence/g1a_cleanup_attestation/inputs.json).

The [session integration](g1a_session_integration.md) now tests inhibited reservation
and allocation failures. Next implement the A8 recovery/status API;
daemon transport/queue integration, ESP32 persistence and physical qualification
remain separate work. No G1 gate is accepted by this host evidence.
