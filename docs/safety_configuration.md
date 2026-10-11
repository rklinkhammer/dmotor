# Safety/configuration integration — development schema 4

The register peripheral now owns a C++ HLS `Configuration` object, with staged
and active settings. `WATCHDOG_US` accepts staging writes while disabled; commit
validates 1,000..10,000,000 us. `MAX_SPEED_MRPM` validates 1,000..1,000,000 milli-RPM.
Staging may contain invalid values, but they cannot become active.

`CONFIG_COMMIT=1` requires disabled state, closed admission, no active command,
and an empty submission ring. It atomically installs both settings and increments
CONFIG_GENERATION without wrapping. Reset generation zero is invalid. The active
timeout is converted with ceiling division using the generated 100 MHz constant.
Descriptors must use the active configuration generation. Legacy trusted maximum
speed/generation inputs on the peripheral top are retained temporarily for wiring
compatibility but ignored; C++ configuration now owns those values.

## Bring-up sequence

1. Reset with outputs inhibited and upstream transactions drained/isolated.
2. Stage WATCHDOG_US and MAX_SPEED_MRPM, then write CONFIG_COMMIT=1.
3. Supply the platform's trusted drain confirmation and write REOPEN=1.
4. Read QUEUE_EPOCH and CONFIG_GENERATION; initialize and publish descriptors.
5. Write ENABLE=1. It requires valid configuration, admission open, no active
   command, no cancellation record, and no live or latched protective condition.
6. Stage REFRESH_SESSION to the queue epoch. Write the expected monotonic value
   to REFRESH_SEQUENCE periodically. Every enabled state requires authorization;
   queue occupancy and executor readiness do not suspend the watchdog.

ENABLE arms the independent RTL watchdog and initializes its expected refresh
sequence to one. A refresh is accepted only for the current epoch and next
sequence. Expiration wins a same-clock refresh. A refresh while disabled does not
pre-authorize enable. Exhausted sequence space requires a new drained session.
The HLS reply reports errors through AXI SLVERR for invalid requests.

## Live revalidation and fault recovery

The HLS block samples safety state at transaction entry. The wrapper revalidates
ENABLE, REFRESH_SEQUENCE, ACK_LOCAL, and ACK_EXTERNAL against current RTL state
when the HLS transaction retires, before returning its AXI response. A fault,
expiry, cancellation, changed expected sequence, or newly active physical cause
can therefore invalidate a request after its initial HLS check. Protection runs
every fabric clock and never waits for HLS or the bus.

ACK_LOCAL is a write-one cause mask. Unknown or still-live causes are rejected;
the RTL clears only the selected inactive causes. A newly observed event wins
same-cycle acknowledgment. ACK_EXTERNAL=1 requires synchronized HALT inactive.
Local faults assert HALT_OUT; receipt of HALT alone does not echo HALT_OUT.
Recovery still requires local acknowledgment, shared-aggregator release,
external-latch acknowledgment, queue recovery, and explicit enable. No automatic
restart occurs when a latch clears.

The wrapper captures one sticky queue-cancellation request per protective episode
and keeps it pending until HLS accounts for it. Further fault attribution remains
in the RTL fault mask. The independent output `inhibit` must be connected to the
final physical output gate during motion integration. ENABLE indicates authorized
state, not an instruction to drive the motor. STOP remains only a request to the
future controlled-stop executor; it leaves the authorization watchdog armed.

The AXI engine timeout maps to local fault bit 1; watchdog expiry uses bit 0.
Board-local fault inputs must reserve those internal bits. The complete stable
fault encoding remains a future ABI freeze deliverable.

## Verification and boundaries

Tests cover invalid timeout/speed bounds, commit preconditions, staging protection,
refresh session/sequence errors, acknowledgment masks, and current-state guards.
The actual AXI/generated-HLS/RTL-safety integration test runs a 1 ms watchdog at
100 MHz, checks valid and stale refreshes, waits for expiry, checks shared HALT and
inhibition, rejects late refresh, and verifies acknowledgment does not restart.
The independent safety RTL suite retains same-cycle expiration precedence tests.

This configuration subset is not motor tuning. Encoder/scaling parameters,
PI gains, profile/settling parameters, coherent telemetry, interrupts, and the
motion executor remain pending. No physical output, clock-loss behavior, or board
current limit is validated here. SmartHLS validation is also still pending.
