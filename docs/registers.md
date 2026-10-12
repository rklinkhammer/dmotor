# Provisional registers

Development schema; ring registers implemented, motion/safety configuration pending.

| Register | Offset | Access | Reset | Meaning |
|---|---|---|---|---|
| IDENTITY | 0x0000 | RO | 1145917268 | DMOT identity |
| SCHEMA_VERSION | 0x0004 | RO | 4 | Development version; not release ABI |
| CAPABILITIES | 0x0008 | RO | 3 | bit 0 ring, bit 1 safety/configuration; no motion executor yet |
| CLOCK_HZ | 0x000c | RO | 20000000 | Fabric clock in Hz |
| STATUS | 0x0010 | RO | 0 | bit 0 busy, bit 1 admission open, bit 2 cancellation valid |
| ENABLE | 0x0020 | WO | 0 | Write one; validate configuration, session, and safety prerequisites |
| DISABLE | 0x0024 | WO | 0 | Write one; inhibit and cancel queue |
| STOP | 0x0028 | WO | 0 | Write one; controlled stop and queue cancellation |
| FAULTS | 0x0030 | RO | 0 | Sticky protective cause mask |
| ACK_LOCAL | 0x0034 | WO | 0 | Write-one cause mask; reject clearing live causes |
| ACK_EXTERNAL | 0x0038 | WO | 0 | Write one; only clear when HALT input inactive |
| WATCHDOG_US | 0x0040 | RW | 0 | Staging timeout; 0 invalid; commit only disabled |
| REFRESH_SESSION | 0x0044 | RW | 0 | Staged authorization session |
| REFRESH_SEQUENCE | 0x0048 | WO | 0 | Validated refresh strobe; expiration wins |
| EXPECTED_REFRESH | 0x004c | RO | 1 | Next accepted sequence |
| QUEUE_EPOCH | 0x0100 | RO | 0 | Hardware session token |
| QUEUE_PRODUCER | 0x0104 | RO | 0 | Accepted publication counter |
| QUEUE_CONSUMER | 0x0108 | RO | 0 | Captured descriptor counter |
| PUBLISH_SESSION | 0x010c | RW | 0 | Doorbell session staging |
| PUBLISH | 0x0110 | WO | 0 | Requested producer advance; validate capacity and session atomically |
| COMPLETION_PRODUCER | 0x0114 | RO | 0 | Published completion counter |
| COMPLETION_ACK | 0x0118 | WO | 0 | Validated completion consumer advance |
| QUIESCE | 0x011c | WO | 0 | Close admission; adapter drain acknowledgment required |
| REOPEN | 0x0120 | WO | 0 | New session after proven drain and cancellation acknowledgment |
| CANCEL_ACK | 0x0124 | WO | 0 | Write 1 to acknowledge frozen cancellation |
| COMPLETION_CONSUMER | 0x0128 | RO | 0 | Acknowledged completion counter |
| CONFIG_COMMIT | 0x0050 | WO | 0 | Write 1: validate and atomically commit disabled, closed and empty queue |
| MAX_SPEED_MRPM | 0x0054 | RW | 130000 | Staged maximum output speed; 1000..1000000 milli-RPM |
| CONFIG_GENERATION | 0x0058 | RO | 0 | Committed configuration generation; 0 invalid |
| ACTIVE_WATCHDOG_US | 0x005c | RO | 0 | Active timeout |
| ACTIVE_MAX_SPEED_MRPM | 0x0060 | RO | 0 | Active maximum speed |
| SAFETY_STATUS | 0x0064 | RO | 0 | bit 0 enabled, bit 1 external latch, bit 2 synchronized HALT |

Submission and completion capacity: **4 entries each**.
Depth is defined in `include/dmotor/ring_config.hpp`; regenerate after changing it.
