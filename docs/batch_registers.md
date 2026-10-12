# Atomic mailbox draft register map

Distinct DMB1 identity; development schema 1. No legacy ring offsets are reinterpreted.

| Name | Offset | Access | Reset | Meaning |
|---|---:|---|---:|---|
| IDENTITY | 0x000 | RO | 1145913905 | Mailbox profile identity DMB1 |
| SCHEMA | 0x004 | RO | 1 | Development schema; not frozen ABI |
| MOTOR_COUNT | 0x008 | RO | 4 | Implemented channels |
| SESSION | 0x00c | RO | 0 | Trusted engine session |
| STAGE_SESSION | 0x010 | RW | 0 | Staged ownership token |
| STAGE_SEQUENCE | 0x014 | RW | 0 | Strictly increasing publication sequence |
| STAGE_MASK | 0x018 | RW | 0 | Nonzero selected motor mask |
| COMMIT | 0x01c | WO | 0 | Write 1 to validate full staged batch |
| PUBLISHED_SEQUENCE | 0x020 | RO | 0 | Last accepted sequence |
| PENDING_SEQUENCE | 0x024 | RO | 0 | Pending sequence or zero |
| PENDING_MASK | 0x028 | RO | 0 | Pending mask or zero |
| APPLIED_SEQUENCE | 0x02c | RO | 0 | Last successful frame sequence |
| APPLIED_MASK | 0x030 | RO | 0 | Last successful frame mask |
| APPLIED_TICK_LO | 0x034 | RO | 0 | Last successful effective tick low |
| APPLIED_TICK_HI | 0x038 | RO | 0 | Last successful effective tick high |
| REJECT_REASON | 0x03c | RO | 0 | Last rejected publication/access reason |
| INFLIGHT_SEQUENCE | 0x040 | RO | 0 | Reserved frame sequence or zero |

Motor staging: base 0x080, stride 8 bytes, four channels.
GENERATION is +0; signed VELOCITY_MRPM is +4. Unselected fields are ignored.
Only aligned full-word writes are accepted. Action registers read zero.
SESSION is supplied by trusted integration reset, never writable by software.
Per-motor applied telemetry: base 0x100, stride 16; sequence +0, tick low +4, tick high +8. +12 reserved.
Use the global applied-sequence/session validation around a multiword read.
See atomic_mailbox.md for publication, activation and snapshot rules.

| Rejection | Code |
|---|---:|
| none | 0 |
| access | 1 |
| incomplete | 2 |
| mask | 3 |
| ownership | 4 |
| sequence | 5 |
| speed | 6 |
| unavailable | 7 |
| pending_mask | 8 |
| busy | 9 |
| retirement | 10 |
