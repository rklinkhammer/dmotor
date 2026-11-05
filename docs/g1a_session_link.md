# G1a endpoint pairing and replay validation

Status: isolated A6 implementation candidate for review. Not installed, linked
into the daemon, or connected to motion admission. No gate is accepted by these
host tests. The wire schema and deployed software behavior are unchanged.

## Boundary and behavior

[`SessionLink`](../software/include/dmotor_sw/session_link.hpp) runs above the
existing monitor framing/CRC decoder. Both endpoints must allocate a fresh durable
lifetime before calling `begin(id, inhibited)`. The inhibition argument is a caller
assertion, not a hardware measurement. IDs must increase; failed begin and explicit
invalidation close the link. Re-pairing requires another fresh local lifetime.

Each side sends HELLO, observes its peer's HELLO, and receives HELLO_ACK echoing
its own lifetime before accepting control frames. An ACK arriving before HELLO
is ignored. Discovery may replace an unconfirmed candidate with a newer peer
lifetime. Once paired, a newer peer HELLO closes the pair and reports
`re_pair_required`; it cannot silently replace the peer. Delayed acknowledgments
with old local or peer IDs cannot confirm a new pair. Pairing alone grants no
recovery, ARM or motion permission. Loss of one ACK can leave only one side paired;
the unpaired endpoint still rejects control traffic.

Wire sequence numbers increase independently in each direction, including
HELLO and ACK. Zero and UINT32_MAX are not usable control sequence numbers; the
link closes on exhaustion. A configurable smaller transmit budget supports
bounded testing. Exact retries reuse their original frame and sequence.

The link retains four received control/ACK frames. An exact duplicate returns
`duplicate`, never `accepted`. Its optional cached response is immutable.
Changed-content duplicates, older frames outside the cache, and wrong lifetime
echoes are rejected. Cache eviction never allows re-execution. The original peer
HELLO and its ACK are pinned separately so discovery retries remain answerable.
Comparison includes all transmitted header fields and payload bytes, excluding
unused payload padding. Malformed frames do not advance the replay window.

## Executor and integration obligations

Only `Delivery::accepted` may reach a control executor, once. The executor must
validate message role, epoch, transaction, challenge and payload semantics before
changing recovery state. `remember_reply` stores a trusted executor's already
numbered response; it does not prove the response is the correct semantic outcome.
A duplicate with no cached response must never execute the operation again.

The caller must inhibit/abort recovery on restart or exhaustion, enforce bounded
discovery retry/timeout policy, and invalidate the link when its transport or local
lifetime ends. This class has no timer, UART thread, hardware inhibit, persistent
peer store or cryptographic authentication. Peer high-water marks survive only
within the object. The [PS allocator](g1a_endpoint_sessions.md) supplies a candidate
local persistence primitive; ESP32 persistence and physical storage fault
qualification remain open. CRC and session echoes assume a trusted physical link;
they do not authenticate a malicious peer.

The [wire adapter](g1a_recovery_link.md) now joins this layer to the
[post-pairing recovery model](g1a_recovery_model.md) in scripted two-endpoint
exchange tests. The [behavioral monitor simulator](g1a_monitor_simulator.md) now extends that coverage. The adapter must treat the model's `Transmission.sequence`
as a logical request identity: `SessionLink` owns the wire sequence, which already
includes discovery. Create and number a wire frame once per logical request,
retain it, and retransmit that exact frame. Calling `send` anew on every retry
would defeat duplicate detection. Confirmed link IDs must bind each model
instance; invalidation must close admission and discard that instance.

Live A1 admission/queue integration, A8 public recovery/status API, physical ARM
behavior and ESP32 firmware are still separate work. Existing installed/exported
images predate these isolated prototypes.

## Evidence

[`session_link_test.cpp`](../software/tests/session_link_test.cpp) exercises both
handshake orders through the actual encode/decode path, lost discovery replies,
stale peer/local acknowledgments, peer restart, candidate replacement, immutable
cached replies, changed duplicate payloads, cache eviction, malformed frames,
pre-pair control rejection and counter exhaustion. All 17 native PS tests pass
with UBSan. A negative control that delivers an exact duplicate as a new accepted
command fails the duplicate assertion.

[Input hashes and commands](evidence/g1a_session_link/inputs.json),
[native test log](evidence/g1a_session_link/native-tests.log), and
[negative control](evidence/g1a_session_link/negative-control.log) record this host
increment. They do not qualify target timing, storage power loss or motor behavior.
