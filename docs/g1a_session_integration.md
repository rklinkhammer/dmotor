# G1a durable PS sessions in coordinator discovery

Status: host integration candidate. The coordinator now reserves lifetimes through
an injected session source; Linux integration tests connect the real
`allocate_endpoint_session` journal implementation. No daemon, installed image,
ESP32 firmware or gate acceptance is changed.

## Ordering and API

The coordinator constructor requires a reservation function. The Linux binding is
`[directory] { return allocate_endpoint_session(directory, true); }`, using an
explicitly provisioned private persistent directory. The callback is trusted
infrastructure, not a client-supplied function. It must durably reserve a nonzero
monotonically increasing ID or throw; it must not provision, repair or reset a store.

`prepare` performs these steps in order:

1. Revoke owner/queue/model authority and confirm HALT through Runtime.
2. Check that reported motor outputs are disabled and the reservation source exists.
3. Reserve the lifetime, including journal and directory synchronization. Storage
   waits occur with HALT asserted. Remember the ID even if later preparation fails.
4. Perform disabled PL preparation, then retain the prepared owner/generation.
5. `discover()` constructs HELLO with the reserved ID. Repeated calls return the
   exact same HELLO without allocating or advancing its wire sequence.

Discovery no longer accepts a caller-supplied numeric ID. A fresh prepare reserves
a new lifetime. Cancellation clears the pending HELLO and active reservation, but
retains the in-object lifetime high-water mark. Zero/reused IDs from a broken
reservation function are rejected. Confirmed-HALT failure prevents allocator calls.
Allocation or subsequent prepare failure keeps admission held, attempts HALT and
exposes no HELLO; the durable reservation is burned, never rolled back.

Reservation may complete and the process may stop before HELLO. Restarting with
the same journal obtains a higher ID. The journal is the authority across process
lifetimes; the coordinator's high-water mark only protects its own object lifetime.
A valid snapshot rollback, journal cloning or privileged tampering is not detected
by the existing allocator. Target persistent storage/provisioning/backup policy,
power-cut qualification and ESP32/NVS persistence remain open A5 work.

## Evidence and next work

The Linux integration test uses temporary private journals with the real allocator,
Runtime and coordinator. A link-time fsync observer verifies HALT/disabled state at
both synchronization barriers. Tests cover exact HELLO retries, allocation across
coordinator restart, failed file and directory sync, missing store with no implicit
provisioning, ID burn after PL cleanup failure, HALT failure before allocation and
an allocator returning an already used ID. Existing joint recovery tests use an
explicit deterministic source and continue to verify protocol/admission behavior.

All 21 native tests pass with UBSan. A negative control that runs PL prepare before
reservation fails the inhibited-allocation assertion. [Evidence](evidence/g1a_session_integration/inputs.json)
records the host run. This is not physical power-cut or hardware inhibition evidence.

Next: implement A8 recovery/status semantics in the shared client/service protocol,
with explicit pending/held/complete/failed/unknown outcomes and compatibility tests.
Keep the daemon's recovery mode disabled until transport, owner/queue routing and
storage provisioning policy are integrated and reviewed. Existing commissioning
profiles remain PL-only.
