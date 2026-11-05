# G1d preparation record — initial extraction

**Status: prepared for time-separated self-review; NOT ACCEPTED.**
Accountable owner/reviewer: Robert Klinkhammer. Preparation date: 2026-11-04.
Preparation performed by Codex at the user's instruction to begin implementation.
Review timestamp, disposition and reviewer observations: **pending**.
Review the committed revision of this record (`git log -1 -- docs/gates/G1d.md`).

## Scope and evidence

- First release is commissioning-only; vehicle work is deferred. Owner and
  time-separated self-review method were explicitly selected by the user.
- [Current status](../system_status.md), [decisions](../decisions.md) and
  [issues](../open_issues.md) have separate ownership; the execution plan links to them.
- [G1 work package](../g1_work_package.md) identifies A1–A8 recovery proof tasks,
  inhibited R4 experiment preparation, and firmware budget/configuration deliverables.
- [Requirements matrix](../system_requirements_matrix.md) has 19 seed requirements;
  no row is declared accepted and numeric TBD blockers remain visible.
- [Archive manifest](../archive/pre-system-plan-2026-11-04/manifest.json) identifies
  five original source blobs, exact retained bytes, browsable copies, successors,
  issue mappings and companion baseline commits/dirty state. Original bytes were
  compared with Git blobs and SHA-256; browsable-copy hashes and successor paths pass.
- Local file links in changed/new Markdown and `git diff --check` pass. The archived
  PL report anchor in the Arty interface now points to the relocated report section.
- Run `python3 tools/check_documentation_archive.py` to repeat byte/hash/source checks
  from a checkout containing the recorded source blobs (fetch history if shallow).

## Review checklist still requiring disposition

1. Verify that the extracted choices agree with the user decisions and technical baseline.
2. Check U01–U11 and source reviews for lost or overstated requirements; U11 is only
   partially resolved, and R4/recovery/firmware contracts remain open.
3. Confirm that all 19 seed rows have owners and appropriate blockers; select the
   first atomic test cases without inventing operational thresholds.
4. Confirm that old evidence is labelled historical and does not imply the latest
   client library is in the old target image or that a board has been qualified.
5. Accept/reject the current document ownership and the initial archive extraction.
   Record any corrections as new changes, not edits to immutable archived bytes.

G1d acceptance covers initial mapping/rules. Remaining mixed-document extraction,
companion inventories and full migration acceptance remain U10/B7 work even if this
record is later accepted. No firmware, PL, kernel, hardware or powered tests were
executed by this documentation increment; no technical gate was closed.
