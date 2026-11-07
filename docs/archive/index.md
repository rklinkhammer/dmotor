# Historical documentation and extraction backlog

Historical documents do not override [current status](../system_status.md),
[active decisions](../decisions.md), [open issues](../open_issues.md) or current
contracts. Evidence is valid only for its recorded revision/configuration and scope.

## First extraction snapshot

Source baseline: `20f59728dce1076b44769efd3e597d66d38a3286`.
[Machine-readable manifest](pre-system-plan-2026-11-04/manifest.json) records original
Git blobs, original hashes, exact `.original.txt` copies, browsable-copy hashes,
successors and issue mappings. Browsable copies add a historical banner and adjust
links; original copies are byte-exact. Extraction acceptance is pending Robert
Klinkhammer's time-separated review.

- [Reviewed system plan](pre-system-plan-2026-11-04/docs/system_bringup_plan.md)
- [Architecture review rounds](pre-system-plan-2026-11-04/docs/software_architecture_review_2026_11_03.md)
- [PL monitor development record](pre-system-plan-2026-11-04/docs/pl_monitor_progress.md)
- [PS monitor development record](pre-system-plan-2026-11-04/docs/ps_monitor_progress.md)
- [2026-11-04 full EDF build record](pre-system-plan-2026-11-04/docs/current_build_2026_11_04.md)

## Retained evidence and remaining extraction

[Hardware evidence](../evidence/) and [software evidence](../../software/evidence/)
remain at their existing locations. The [migration inventory](../documentation_consolidation_plan.md)
identifies remaining mixed documents: they are not all obsolete, nor have all their
requirements been migrated. Use current contracts first; treat conflicts as open
issues. No additional platform or generated ABI is deprecated by this archive.

- [Legacy architecture 0.17](../DMOTOR_ARCH_0_17.md)
- [Previous PS software plan](pre-ps-executor-2026-11-07/DMOTOR_SW.md)
- [Previous HLS motor architecture](pre-ps-executor-2026-11-07/DMOTOR_ARCH.md)
- [Development history](../history/development-log.md)
- [IP export history](../history/ip-export-log.md)

The inventory links every remaining component reference while it awaits extraction
into the current PL design/ABI documents. Removing old references from the main
index does not by itself complete their review or transfer their requirements.
