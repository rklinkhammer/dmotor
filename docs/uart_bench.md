# U0 UART bench test

Implemented: `dmotor-uart-test` for Linux and the
[ESP32-S3 bench application](../software/esp32/diagnostics/bench/README.md).
Host tests and an ESP-IDF build are recorded in
[evidence](evidence/uart_bench/inputs.json). No board has been flashed or tested by
this implementation. Current SD export `arty-z7-20/20261106T005035Z-4c9ce75c`
contains the ARM tool; see [image evidence](evidence/arty_uart_image/inputs.json).

## Build and deploy

Native utility and tests, from dmotor:

```sh
cmake -S software -B build/ps-dev -DDMOTOR_SW_SANITIZE=ON
cmake --build build/ps-dev -j4
ctest --test-dir build/ps-dev -R software_uart_diagnostic --output-on-failure
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 build
```

For the ARM package, from edf-dev:

```sh
EDF_DMOTOR_SOURCE=../dmotor ./edf --target arty-z7-20 bitbake dmotor-semantic-tests
```

The utility is installed in `dmotor-runtime`; both dmotor's recipe template and
the active edf-dev recipe include it. Package build alone does not update the SD
card. Rebuild the target image/export with the normal EDF workflow or install the
matched built package using your existing target deployment method. Do not copy
the native x86 executable to the Arty. The ESP32 README gives the explicit flash
command; no flashing is performed by a build.

## First run

1. Use the [G1b pre-run checklist](gates/G1b.md). Disconnect motor drive power;
   confirm the flashed image is this diagnostic with permits/HB low.
2. Connect 3.3 V UART: Arty CK_IO0/T14 TX → J7-5 monitor RX (GPIO18);
   J7-4 monitor TX (GPIO17) → Arty CK_IO1/U12 RX; ground J7-1/6. J7 has no supply.
3. Identify the UART Lite tty associated with device-tree address `0x42C00000`.
   Keep the PS console separate. Stop any getty or competing monitor transport
   owner on that tty. The utility requests exclusive access; it does not stop
   other services for you.
4. On the Arty, substitute the actual tty below:

   ```sh
   dmotor-uart-test /dev/ttyUL0 100
   ```

   `/dev/ttyUL0` is an example, not an established assignment. Count is 1–1000,
   default 100. Configuration is fixed at 115200 8N1 without flow control.
5. Save the Linux summary and ESP32 console summary (printed after idle). Linux
   must report `PASS`, sent=verified=count, rejected=0 and mismatched=0. Compare
   ESP32 cumulative counter changes for this run; investigate any error increment.
6. Start new runs after resetting either endpoint, and after disconnect/reconnect.
   A disconnect during a run must fail, not report success. Reconnection starts a
   new command/run ID. Reset counters only by an explicit ESP32 reset, retaining
   the preceding logs. Record results in the G1b run log.

One request and independently generated response are outstanding at a time. This
checks both wires, framing and payloads; **simultaneous-stream/load testing is not
implemented**. A response/TX deadline is 1 second; total run limit is 120 seconds.
The tool exits nonzero for timeout, I/O failure, rejected frames or unexpected
run/sequence/payload. It does not retry dropped packets and conceal errors.

No controller device or motor client is opened. GPIO/HB test modes, full recovery,
ARM and protection tests remain later tasks. A UART PASS says nothing about sensor
calibration or protection timing.

## Diagnostic wire v1

Both targets compile `software/include/dmotor_diag/uart.hpp`; the operational
monitor codec is unchanged and rejects this envelope. Unescaped bytes:

| Field | Encoding |
|---|---|
| Magic, version, kind | `d7 01`, then 1=request or 2=response |
| Run ID | Nonzero u64 little-endian, Linux getrandom |
| Sequence | u32 little-endian, 1..1000 |
| Payload length | u8; sequence cycles 0, 1, 2, 16, 64 bytes |
| Payload | Deterministic direction/sequence pattern; includes 00/ff/7d/7e |
| CRC | CRC-16/CCITT-FALSE over header+payload, big-endian |

Boundary 7e; escape 7d followed by byte XOR 20. Decoded frame length is 18+payload,
maximum 82 bytes; worst-case encoded length is 166 bytes. Fixed buffers are bounded.
The independent Python PTY peer uses `binascii.crc_hqx` and checks payloads; the C++
test checks the standard CRC vector `123456789 → 29b1`, maximum lengths, escaping,
overflow resynchronization and rejection by operational decoding. PTY tests cover
fragmented responses, corruption, stale run IDs and no peer replies.
