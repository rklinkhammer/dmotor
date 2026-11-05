# Monitor wire ABI — first PS development increment

`wire.json` records version 1 message IDs, byte layouts and lengths implemented by
`monitor_protocol.hpp` / `monitor_protocol.cpp`. These files belong to dmotor and
are shared with the future ESP32 implementation. All integer payload fields use
little-endian encoding. Serialize explicitly; never transmit a packed C struct.

The codec uses fixed storage: at most 97 decoded or 196 escaped bytes. It rejects
unknown versions/types, invalid lengths, bad CRC, malformed escaping and overlong
frames. A delimiter restores framing after damage. HELLO requires peer echo zero,
epoch/transaction zero, a nonzero source session and nonzero sequence; other
frames require both sessions nonzero. The decoder is a framing validator, not an
authorization decision. It does not accept recovery or grant ARM/motion.

The state record identifies the completed transaction independently of the
queried transaction in the frame header. Zero completed transaction/epoch and
PL session means no completed exchange. Query replies echo the 64-bit challenge.
Recovery-state header epoch captures the monitor epoch for the prospective
transaction. Payload cleanup flags must equal 7 before recovery can be accepted;
the later coordinator must establish each flag from verified PL cleanup, not
simply populate the bits. Fault cause bits remain reserved until the monitor
sensor ABI is completed; any nonzero mask is a fault. NAK values are specified
in the schema, with unknown values treated as refusal.

Golden checks: CRC("123456789") = 0x29b1. A HELLO with source session 1,
sequence 1 and all other fields zero has CRC 0x318f. The complete escaped vector
is in `software/tests/monitor_protocol_test.cpp`.

## PS transport

`monitor::Uart` exclusively opens an explicitly selected tty, configures raw
115200/8N1 without flow control, and flushes old bytes only at initial ownership.
It uses nonblocking I/O, one fixed TX slot, an absolute monotonic TX deadline and
at most 196 RX bytes per poll. Partial frames persist; following frames remain
in the kernel queue until the next poll. Disconnects and write deadline expiry
throw `IoError`. The owner must assert HALT/retain recovery hold on these failures
during recovery. Wire completion and peer response deadlines are separate;
finishing a write only means the kernel accepted the bytes.

No tty index is assumed. Select the device by the matched DT/sysfs mapping for
0x42c00000. Do not run a console, getty, or another serial client on it. The PTY
test verifies software framing/partial I/O, not UART Lite FIFO/IRQ performance.

## Still required before recovery or powered deployment

- Persistent endpoint session allocation, HELLO pairing and explicit re-pair.
- Session/epoch/sequence validation, duplicate-response handling and bounded
  request retries, including lost-COMMIT_ACK reconciliation.
- Recovery coordinator tied to runtime command admission and verified PL
  reset/drain completion; physical ARM status challenge before opening admission.
- ESP32 endpoint and final cause-bit definitions; joint fault-injection tests.

The commissioning daemon does not yet use this transport. Its existing rearm
operation is PL preparation only, not completion of the monitor recovery contract.
