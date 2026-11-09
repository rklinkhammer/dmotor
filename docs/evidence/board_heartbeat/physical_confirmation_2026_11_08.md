# Operator confirmation — synthetic heartbeat / indicators

Date: 2026-11-08 (America/New_York). Operator: Robert Klinkhammer.
Evidence source: conversation report; no hardware run performed by Codex.

Initial report: “my breadboard is too narrow for the esp32 to connect to the HB
pin. But, a contact shows that it is working, and the zynq turned blue”.
The operator subsequently confirmed the following checklist with “This is
confirmed. Updaste the documentation and gates”.

| Check | Recorded result |
| --- | --- |
| U1 `hb10`, synthetic heartbeat | PASS, operator-confirmed ESP32 and Zynq HB LEDs flash blue |
| Stop/loss indication (`stop` or disconnect checklist) | PASS, operator-confirmed Zynq LD5 returns to flashing red |
| SW0 asserted during this test | PASS, operator-confirmed LD4 stays flashing red |

These are functional visual checks of the synthetic heartbeat path and status
indicators. The report does not distinguish separately executed `stop`, timeout
and disconnect trials, supply measured latency, or establish static-HIGH rejection.
SW0/LD4 observation does not establish measured motor-output inhibition, release
latching, or acknowledgement behavior. Those remain separate validation cases.

ESP32 setup context: UART/U1 diagnostic, LED GPIO48, HB GPIO21; Arty HB input
CK_IO2/U13, LD5 heartbeat status and LD4 inhibit status. The preceding GPIO48 build
hashes are in `led_gpio48.json`; they identify the local build, not independently
verified flashed image identity. The loaded Zynq bundle/bitstream identity was not
supplied. Functional deployment is demonstrated, but no timing/DRC report or
matched-release provenance is inferred. The breadboard contact limitation has not
been explicitly reported resolved; no sustained connection/duration is claimed.

Disposition: U1 synthetic-HB/LED visual subcases PASS (operator report); U1 overall
PARTIAL. G1b/G2 remain OPEN. Sensor-qualified operational HB, electrical waveforms,
permit-low measurements, SW0 output/rearm behavior, four-sensor acquisition,
calibration, protection and recovery gates remain open. No powered motor approval
or automatic/live recovery acceptance follows.
