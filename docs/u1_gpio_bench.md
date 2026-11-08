# U1 — inhibited GPIO and synthetic heartbeat diagnostic

**2026-11-08 update:** Standalone ESP32/INA characterization has resumed;
see [measured results and revised monitoring test procedure](ina_bench_timing.md).
This supersedes the blanket pause below only for that standalone work. Integrated
board tests and U1 physical acceptance remain pending; synthetic HB is unchanged.

**Current scheduling (2026-11-06):** Further physical testing is paused until
assembled monitor and motor boards are available, at Robert's request ([D04](decisions.md)).
Completed U0 results remain valid; U1 is implemented/build-tested, with no physical
U1 acceptance recorded. Procedures below are retained for resumption.

Implemented in the existing `software/esp32/diagnostics/bench` application.
Native tests and ESP32 build are development evidence; **U1 physical tests remain
pending**. This firmware is not the operational monitor and never authorizes ARM.
The existing U0 UART v1 request/reply remains available without a Zynq image change.

## Meaning of permits low

GPIO8/9/10/11 are always commanded LOW, including while synthetic heartbeat runs.
There is no command to raise them. GPIO21 heartbeat starts LOW. Output latches are
written low before output-enable at startup; external bias covers reset/boot.
A console message reports commanded state, not measured permit or DRVOFF voltage.
Motor drive power stays physically disconnected. Synthetic HB can satisfy a
watchdog or PL health input despite missing sensors, so never use it for operation.

## Build and flash

From dmotor, with esp32-dev beside it:

```sh
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 build
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 --port /dev/ttyACM0 flash monitor
```

Select the actual native USB device. Flashing is explicit; build does not flash.
Fresh configs use native USB Serial/JTAG, no secondary console and
`CONFIG_FREERTOS_HZ=1000`. Existing sdkconfig overrides defaults: if the build
reports the U1 tick assertion, run the same launcher with `menuconfig`, set
Component config → FreeRTOS → Kernel → tick rate to 1000 Hz, save and rebuild.
The local development build has been updated; deployed firmware has not.

## USB commands

Type the complete command followed by Enter in the **native USB** monitor. Input
is not echoed; CR, LF and CRLF are accepted. Commands are case-sensitive. These
commands never travel on UART1/GPIO17/18.

| Command | Effect |
|---|---|
| `help` | Print commands and synthetic-HB warning |
| `gpio` | Report raw input levels and HB status once per second |
| `quiet` | Stop periodic reports; does **not** stop HB |
| `hb10` | Explicitly start a nominal 50 Hz synthetic heartbeat for 10 seconds and enable reports |
| `stop` | Cancel heartbeat, drive GPIO21 LOW and enable reports |

Repeated `hb10` while running is rejected, not an extension. A stale queued start
is rejected; `rejected_starts` reports this. Stop replaces a pending start. Invalid
or overlong lines cannot start HB. No automatic start/restart on boot, reconnection
or UART traffic. USB physical disconnect stops HB when detected; closing the PC
monitor alone is **not** USB disconnect. The independent duration still expires.
USB reports are best-effort and can be dropped under congestion.

Expect a line like:

```text
RAW ARM41=1 STOP40=1 HALT39=0 nFAULT12/13/14/42=1/1/1/1 ALERT47=1; permits commanded LOW; HB=idle level=0 remaining_ms=0 rejected_starts=0
```

These particular input levels are typical for an unconnected ESP32 with this
firmware's biasing, not a healthy assembled system. During HB: `HB=SYNTHETIC`, then
`HB=expired level=0 remaining_ms=0`. `HB=missed-deadline` means the task was late;
it stops rather than generating catch-up pulses. Restart deliberately only after
investigating unexpected missed deadlines. `HB=USB-disconnected` records USB loss.
Terminal UART TX failure also revokes HB until reboot.

## Tests without assembled motor boards

1. Keep motor drive power absent. Use ESP32 native USB logging and common ground
   to the Arty. UART wiring can stay as proven by U0.
2. After flashing, run `gpio`. Verify input reports, then use a meter/scope to
   check GPIO8/9/10/11 and GPIO21 low. Record actual readings; no measurement is
   inferred from the firmware banner.
3. With each tested input disconnected from any other driver, briefly connect
   ARM41, STOP40 or nFAULT12/13/14/42 or ALERT47 to GND. Its reported level should
   change 1→0, returning to 1 after release. Hold each level long enough to appear
   in a one-second report. These inputs have internal pull-ups in this diagnostic.
   HALT39 has an internal pulldown: if unconnected to Arty, a 3.3 V test signal
   should report 1, removal 0. Never inject voltage against a connected output;
   never use 5 V. Skip pin stimulus if its connection is uncertain.
4. Connect ESP32 GPIO21 → Arty CK_IO2/U13, plus common ground. If using the monitor
   board this passes through J7-2. Run `hb10` and scope the signal at both ends:
   nominal 10 ms high / 10 ms low, then LOW after the bounded run. A one-second
   USB level sample cannot demonstrate a 50 Hz waveform.
5. Run `hb10` again and then `stop`; confirm LOW at the pin. Try a second `hb10`
   during an active run: the original run must not be extended. Confirm permits
   remain LOW throughout. Check reset returns to HB LOW without automatic restart.
6. Optionally disconnect native USB during HB while ESP32 is independently powered
   using a verified supply arrangement; observe HB stops. If USB was its only
   supply, the experiment tests power loss, not the software disconnect path.
7. Rerun `dmotor-uart-test /dev/ttyUL0 100` on the Arty after the firmware change;
   then repeat during `gpio` reports / synthetic HB to check coexistence. Keep
   motors inhibited; this is diagnostic traffic, not recovery.

Arty coast output is CK_IO3/V13 → ESP32 HALT39 (J7-3 with monitor). U1 only observes
this input; it does not issue Zynq HALT/reset commands. Use a separately documented
controller test if exercising PS HALT. Do not use PL prepare merely to generate a
single HALT edge: prepare is a multi-step recovery operation. No new raw MMIO tool
is introduced here. Scope arrival at CK_IO2 is a wiring observation, not proof of
PL heartbeat qualification/ACK; that status inspection remains a separate check
through supported controller tooling.

## Tests that wait for assembled boards

Verify physical ARM/STOP-chain behavior, actual nFAULT/ALERT paths and all downstream
permit/DRVOFF levels. Unused MON STOP-chain positions need the documented 9–10
jumpers. Pull-ups on absent inputs cannot prove a sensor/board is present.
This diagnostic reports raw levels only: no debounce, interrupts, calibrated
protection, fault response or sensor-qualified HB. Changing a raw STOP/HALT input
**does not stop synthetic HB**; permits remain low regardless. Test overriding an
actively asserted CPU permit only in a separately designed inhibited experiment.

## Implementation and evidence

One priority-5 task owns HB and samples GPIO every 1 ms, with no console/I²C/ADC
work on its normal path. Edges use monotonic absolute deadlines. Missing a full
10 ms edge interval cancels HB; no timer/peripheral keeps HB running when the task
stops. USB command/reporting runs separately at priority 1; snapshots overwrite a
one-element queue, and command storage is bounded. The duration is a software
deadline: clearing occurs on the next scheduled task pass. Scope actual timing;
this is not a hardware guarantee during a CPU hang or blocked interrupts.

[Build/test evidence](evidence/u1_gpio/inputs.json) includes native parser/timer
boundary tests and pinned IDF compilation. Record physical results in
[G2](gates/G2.md): command, connections, levels, waveform/timing, run termination
reason and UART coexistence result. No U1 gate is accepted by this implementation.
