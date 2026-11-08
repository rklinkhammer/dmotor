# G1a behavioral monitor recovery simulator

**Cadence migrated (2026-11-08):** The model and joint coordinator tests now use
16 ms monitoring samples. Host regressions pass; target jitter, physical ARM and
shutdown qualification remain open. See [implementation evidence](monitor_next_steps.md).

Status: host-only implementation candidate under `software/tests/support`.
Not ESP32 firmware, not installed or connected to the daemon. G1a remains open.
The simulator replaces scripted state changes in a new joint PS/monitor test;
the earlier scripted fixture remains as a separate wire-adapter regression.

## Modeled rules

- Boot drops permits, clears staged/completed transactions and requires a fresh
  durable endpoint ID supplied by the harness. The real session/replay layer
  performs discovery. Boot cannot restore ARM eligibility.
- The harness supplies an aggregate active-cause mask representing already
  qualified health, STOP and HALT inputs. Newly asserted bits advance the fault
  epoch and invalidate recovery/ARM state in every state; a continuously active
  cause does not repeatedly advance it. Clearing and reasserting is a new episode.
  Causes remain latched diagnostically until successful commit; wire recovery
  preconditions report current active causes, allowing recovery after clearance.
- RECOVER_REQ requires SAFE/FAULT, fresh healthy input, nonzero PL session, all
  cleanup flags and a newer nonzero transaction. A newer accepted request replaces
  a staged transaction. COMMIT requires its exact transaction/current epoch and
  rechecks healthy inputs. Faults and expiry invalidate the staged transaction.
- COMMIT enters SAFE with permits low. ARM must first observe release after
  commit, then a 20 ms stable press and 20 ms stable release. Ten subsequent
  healthy checks separated by 16 ms are required before permits rise. Held or
  bouncing buttons cannot skip this sequence. Repeated same-time samples do not
  count as elapsed checks. A missed check during ARM_PENDING/ARMED revokes
  eligibility; backwards time locks the simulator until a fresh boot lifetime.
- Fault and restart drop permits and invalidate completion. Cached replies may
  be replayed but never execute COMMIT or clear a newer fault. STATE_QUERY echoes
  the challenge and reports current state, epoch and completed tuple. PS admission
  still requires a challenged ARMED reply and matching local proof.

## Simulation choices requiring review

The staged, uncommitted transaction expires **300 ms after its accepted request**;
retries do not extend it and expiry wins at the exact boundary. This is a proposed
simulator policy derived from the three 100 ms transmission budget, not an adopted
firmware timeout. A lost response near that boundary can therefore require a new
explicit recovery. Completed transactions do not expire on this timer.

Health samples must be no more than 16 ms old for acceptance/commit. The model
requires exact maximum 16 ms scheduling gaps while ARM_PENDING/ARMED; this is a
conservative deterministic policy, not an established target jitter budget.
A new press during qualification/ARMED invalidates eligibility. Timing violations
use synthetic cause bit 31, not a frozen sensor/firmware ABI.

The harness serializes events and **must apply fault samples before messages at
the same timestamp**. Tests verify that ordering; they do not prove ISR/task
atomicity. No autonomously running clock exists: the harness must continue
sampling. A simulator that is not advanced cannot detect an elapsed watchdog.
Analog acquisition, raw STOP debounce, HB generation, GPIO timing, storage and
ESP32 scheduling are outside this model. The harness provides healthy/qualified
input assertions; these must be established by real firmware before use.

## Verification and next work

The new test uses the real framing codec, session layer, PS wire adapter and
recovery model. It covers normal/lost-ACK recovery, held and bouncing ARM,
nine-versus-ten checks, repeated same-time samples, faults at press/release/final
qualification boundaries, fault-at-COMMIT ordering, repeated/new fault causes,
recovery after fault clearance, stale COMMIT after fault, superseded transactions,
expiry just before/at 300 ms, retry without lifetime extension, invalid attestation,
stale health, missed checks, reset and forbidden endpoint-lifetime reuse.

All 19 native tests pass with UBSan. A negative control permitting ARM after nine
checks fails the qualification-boundary assertion. [Evidence and input hashes](evidence/g1a_monitor_simulator/inputs.json)
record the run. Target UART and physical fault/permit timing remain untested.

The [Runtime admission boundary](g1a_runtime_admission.md) is now host-tested.
The [host coordinator](g1a_recovery_coordinator.md) now connects it to the model
and tests ownership and retained command tickets. Trusted attestation acquisition
and the real socket queue integration remain open.
Durable allocator hookup, verified MMIO cleanup acquisition, public recovery/status
API (A8), ESP32 implementation and G1b/G1c decisions remain open. No existing
installed image is updated by this test-only increment.
