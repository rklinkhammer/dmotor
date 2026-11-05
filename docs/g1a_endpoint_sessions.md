# G1a PS endpoint-session allocator

Status: Linux implementation and host tests, 2026-11-05; deployment/storage review
pending. Implements the PS portion of A5 as an isolated candidate. It is not linked
into the daemon, installed or used by the [post-pairing model](g1a_recovery_model.md).
There is no provisioning CLI or automatic startup initialization. ESP32 storage
and physical power-loss qualification remain open.

## Allocation and durability contract

`allocate_endpoint_session(directory, inhibited)` requires an existing, provisioned
store. The caller must establish actual physical/PL inhibition before invoking it;
the boolean is a checked caller precondition, not a hardware observation. Allocation
must finish before HELLO/discovery or any recovery request. A thrown exception keeps
inhibition and prevents discovery; it must never select an old or random replacement ID.

The directory must have mode 0700 and belong to the effective user. The journal
`endpoint-sessions.log` must be a single-link regular file with mode 0600 and the
same owner. Final-component symlinks and unexpected file types are rejected. Its
ancestors and filesystem must be trusted and stable. This implementation assumes
local persistent storage that honors file/directory `fsync`; it makes no guarantee
for tmpfs, network filesystems, lying storage caches or media that loses synced data.

Each allocation opens the existing inode, takes an exclusive `flock`, validates
all records, appends the next reservation, syncs the file, then syncs the directory.
Only after both sync operations succeed does it return the nonzero 64-bit ID.
The lock is held through completion. Concurrent cooperating processes receive
distinct IDs. Short writes and EINTR are handled; other errors throw. No counter
wraps, file is replaced, record is removed, or automatic journal repair occurs.
A missing, empty, malformed, corrupt or partially written store refuses allocation.

A record is 32 bytes with fixed little-endian integers:

| Bytes | Meaning |
|---|---|
| 0–7 | ASCII `DMSID001` format identifier |
| 8–15 | Reserved session ID |
| 16–23 | Bitwise inverse of ID |
| 24–27 | Reserved, must be zero |
| 28–31 | CRC-32/ISO-HDLC of bytes 0–27: reflected polynomial 0xedb88320, init/final XOR 0xffffffff |

The first record is ID zero, the non-allocatable genesis. Every following ID must
be exactly the preceding ID plus one. Unknown formats, gaps, duplicates and damaged
records are errors. CRC provides accidental-corruption detection, not authentication.
The scan is capped at 1,000,000 records (32 MB). There are at most 999,999 issued
reservations per provisioned store; capacity exhaustion refuses allocation and
requires reviewed maintenance. This conservative bound avoids unbounded scanning;
there is no assumed wall-clock bound on filesystem I/O or lock waiting.

## Failure behavior and provisioning limits

| Interruption/failure | Next behavior |
|---|---|
| Before a record is appended | No ID was returned; normal next allocation is possible if the store is intact |
| During an append, leaving a partial record | Refuse the entire journal; never fall back to the previous valid record |
| Complete record appended but sync fails | No ID returned; a subsequent complete, valid record is treated as reserved and skipped |
| Sync succeeds, then process dies before returning ID | Reservation remains burned; the next process allocates a higher ID |
| Old record changes or integrity fails | Refuse allocation, retain inhibition |
| Journal missing after prior use | Refuse; do not recreate or restore an old counter |

`provision_endpoint_sessions()` is for a **never-used endpoint**, while inhibited,
in an existing private directory. It exclusively creates the genesis file and syncs
file and directory. Existing paths are never overwritten. Failure does not unlink
partial state to permit a blind retry. No daemon path calls this function.

**Snapshot rollback and reprovisioning are not solved by CRC or fsync.** A wholly
valid old journal, whole-record truncation, a cloned filesystem, or reinitialization
on a previously used device can reuse issued IDs without detectable corruption.
Do not restore this store from backups, delete/recreate it, clone it to another
active endpoint, or replace its inode during service. Lost-state recovery and
capacity migration need a separately verified, non-reused identity strategy;
this API does not provide one. Protection against privileged/malicious rollback
would require an external trusted monotonic authority. These are A5 deployment
requirements, not guarantees established by the host tests.

The store must live outside volatile image/overlay replacement paths on the target.
Its final location, service ownership, update/backup exclusions and provisioning
workflow are not selected by this increment. Physical power-cut tests on the chosen
media/filesystem remain required. ESP32 NVS/session allocation needs a separate
backend and validation; Linux filesystem tests do not qualify flash/NVS behavior.

## Verification and integration boundary

[Host tests](../software/tests/endpoint_session_test.cpp) exercise an independent
record-format vector, restart, 12 concurrent child processes, missing stores,
permissions/types/hard links, corruption in each journal record, torn/empty tails,
capacity exhaustion, short writes, EINTR, failed file and directory sync, and process
termination immediately after a successful file sync. Syscall fault injection uses
linker wrappers in the test executable only. It does not simulate a real power cut
or demonstrate resistance to valid filesystem rollback.

The native PS suite passes with UBSan enabled. A negative control that removes the
sync barrier fails the durability-error test. [Evidence](evidence/g1a_endpoint_sessions/inputs.json)
records the inputs, build and results. The previously exported PL/ARM image remains
unchanged; the new allocator/model sources are included in staging provenance but
are compiled only into uninstalled test executables.

The isolated [A6 pairing/replay link](g1a_session_link.md) is now host-tested.
The [wire adapter](g1a_recovery_link.md) now connects confirmed IDs to the
post-pairing model in scripted exchanges; actual allocator hookup remains open. The
[behavioral monitor simulator](g1a_monitor_simulator.md) now extends joint coverage. A5 remains open for target provisioning,
ESP32 implementation and storage fault qualification. Live admission/daemon wiring
and public recovery API remain separate A1/A8 work.
