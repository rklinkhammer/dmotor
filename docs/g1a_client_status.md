# G1a client recovery status and remaining gate

Prepared 2026-11-06. A8 now has an implemented read-only status API and an explicit
compatibility boundary. This does not enable live recovery or accept G1a.

All callers use `dmotor::client` / `Client::recovery_status()`. The result contains
`RecoveryState` and `admission_open`; neither PL ready nor successful `prepare()`
means monitor recovery succeeded. Current dmotord returns **unavailable / false**
in every commissioning mode, before and after preparation. This describes monitor
admission capability, not physical output state or a change to existing bench modes.

| State | Meaning to a caller |
|---|---|
| unavailable | This daemon has no live recovery coordinator; do not offer an operational recovery action |
| held | No recovery admission; new deliberate preparation is needed |
| pairing | Local preparation done; peer pairing/exchange has not begun |
| recovering | Exchange or challenged query in progress; no motion admission |
| waiting_arm | Monitor exchange reached physical-ARM wait; show operator instruction, do not synthesize button events |
| verifying | Challenged monitor result received; local proof still pending |
| admitted | Coordinator completed monitor and local checks for its current owner/generation; not a physical-safety certificate |
| failed / unknown | Recovery unsuccessful or outcome uncertain; no automatic retry/rearm |

The isolated Coordinator exposes the same typed view. Observation never calls
`finish()` or grants a ticket. Tickets are still stamped only for admitted ingress,
never when a queued command is later dispatched. Cancellation revokes owner and
view immediately; the existing coordinator currently returns to held after abort,
so persistent diagnostic failure attribution remains future work.

## Local protocol compatibility

Existing commands remain schema 1 (28-byte Request, 192-byte Response). The new
read-only query uses schema 2, operation 12, zero mask/payload and the same request
size. Its 200-byte response is the existing response prefix with schema 2 followed
by uint32 state and uint32 admission. Numeric states are 0 through 8 in the table
order. Only admitted may have admission=1, and admitted must have admission=1.
Unknown states, inconsistent admission, wrong schema/size and transport failures
close the client connection. There is no fallback, retry, or false success when
an older daemon disconnects on schema 2. Existing schema-1 callers remain supported.

This is a local native-endian socket ABI, not the framed monitor UART protocol.
There is one outstanding request; late response cannot be reused after timeout.
A successful status query renews the existing owner lease just like status/ping;
it neither prepares nor clears faults. Normal service tick processing continues.

Future mutation API must be explicit (`begin recovery`, `query progress`, `cancel`),
with owner/generation identity and bounded nonblocking daemon work. `prepare()`
keeps its PL-only meaning. Do not expose a successful recovery mutation until live
coordinator, storage and admission prerequisites below are met. No unsupported
mutation stubs are advertised by this increment.

Tests cover independent Python wire decoding, older-daemon disconnect, malformed
state/size, inconsistent admission, current-daemon unavailable status, and isolated
coordinator verifying/admitted/cancelled views. The full native UBSan suite passes
23 tests; installed client consumer remains covered. See
[evidence](evidence/g1_acquisition_review/). No ARM image rebuild or deployment is
claimed for this source increment.

## Remaining G1a gate

The work package explicitly requires review/proof **before live admission hookup**.
These are concrete remaining items, not completed by the new status API:

| Item | Current evidence / required next step |
|---|---|
| A1/A8 live queue admission | Runtime/coordinator tickets host-tested; socket dispatch is still legacy commissioning. Review v4 and full command table before live hookup; add real queued pre-recovery target rejection and owner replacement tests |
| A2/A4 hardware observation races | Repeated identity/configuration reads and injected changes tested; decide whether non-atomic read/ABA risk is acceptable or needs an additional PL snapshot/token before claiming proof |
| A3 cleanup | Actual HLS/AXI reset-interruption, stale completion and request exhaustion tests exist; Robert reviews boundary coverage against accepted cleanup claim, including fault coincident with completion/enable |
| A5 persistence | PS allocation/failure tests exist; target writable storage provisioning, ESP32 lifetime backend and actual storage power-cut behavior remain unqualified; agree lost-journal policy from G1c |
| A6/A7 exchange | Model, link, simulator and coordinator tested; review freshness/retry/unknown policy in v4; no operational ESP32 endpoint yet |
| A8 deployment | Read-only view implemented; error history, explicit mutations, matched ARM build and live admission remain pending prerequisite review |

Review record: Robert Klinkhammer, time-separated self-review against these source
and test records. Date/disposition: **pending**. No physical/powered acceptance is
inferred. G1b electrical experiments can continue independently within their own
inhibited scope. This is the stop point for live G1a integration, not a claim that
all G1a implementation or G1c qualification is complete.
