# Inhibited physical INA226 health diagnostic

Build, menuconfig, flashing, output format and acceptance limits are documented in
[the monitor implementation procedure](../../../../docs/monitor_next_steps.md#inhibited-one-ina-firmware).

This is a separate profile from the acquisition UART and timing diagnostics. It
checks one actual INA at 0x44 with six reads per cycle, explicit identity and
configuration, CVRF clear history and conservative conversion age. It neither
writes INA configuration nor treats alias slots as separate physical sensors.

Permits GPIO8–11 and HB GPIO21 remain LOW. The high-priority supervisor adapter
cannot enable outputs. No ADC calibration, protection threshold, ARM or live
recovery qualification is provided by this diagnostic. Motor drive power must
remain disconnected. Firmware build success is not a physical test result.
