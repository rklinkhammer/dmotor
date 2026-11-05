# Documentation consolidation and archival proposal

**Status: DRAFT FOR REVIEW — 2026-11-04.** Companion to the
[integrated system plan](system_bringup_plan.md). This is a migration proposal and
inventory, not an executed archive. Existing links and documents remain in place
while reviewers decide the replacement content and requirement ownership.

G1d accepts ownership rules and the initial [requirements-to-test matrix](system_requirements_matrix.md).
Archive extraction/migration proceeds in parallel with B2–B4 work and completes
before G7 acceptance; any remaining migration is a B7 work item, not a B7 entry
prerequisite. It is not a prerequisite for starting independently approved software
or inhibited electrical work. Preserve the B1–B7/BV identifiers and separate GV
vehicle gate when extracting the older PS-only phase plan.

## Desired result

The normal documentation entry points show only **current state, current decisions,
current operating/development procedures and unresolved issues**. Prior designs,
review conversations, experiments, superseded instructions and chronological progress
reports remain accessible through an explicitly historical archive. Evidence remains
traceable and immutable; old test success cannot appear to qualify new software.

“Old” means superseded content, not simply an old file date. A stable register
contract or a still-applicable scope measurement must not disappear because its
filename predates this plan. Mixed documents require extraction and review first.

## Proposed current set and ownership

| Current destination | Owns | Must not duplicate |
|---|---|---|
| `README.md`, `docs/index.md` | Short orientation/current-document directory; one archive link | Historical chronology or independent requirements |
| `docs/system_bringup_plan.md` | Accepted development sequence, prerequisites and acceptance gates after review | Detailed build recipes or repeated protocol definitions |
| `docs/system_status.md` (to create) | One current component/gate matrix and exact latest verified artifact identities | Append-only progress journal; unsupported “all current” claims |
| `docs/system_requirements_matrix.md` | Stable requirement-to-contract/test/evidence mapping; TBD acceptance limits and affected gate | Test count used as proof of a passing run; duplicate requirements |
| `docs/open_issues.md` (to create) | One unresolved issue register with owner, blocking gate, disposition and evidence | Separate copies of R/F/V finding status in multiple reviews |
| `docs/decisions.md` (to create) | Concise active decisions, rationale, scope and source; proposals explicitly marked | Superseded alternatives presented as selectable current behavior |
| `DMOTOR_ARCH.md`, `DMOTOR_CONTROL.md` | Current system/PL boundary and ordered-board protection/electrical behavior | Historical revision sections or duplicate firmware state machines |
| `docs/recovery_transition_contract.md` | Single cross-device state/authority/recovery contract; retain PROPOSED until adopted | Multiple v1/v2/v3 files in the active tree |
| `docs/arty_monitor_interface.md` | Current physical pins, harness/BD signals and interface checks | Historical CK_IO4/5 assignment or programming recipes |
| `docs/controller_programming.md`, `software/CLIENT.md` | Consolidated programming rules and public daemon client API | Competing raw-socket application API or implementation history |
| `docs/controller_axi.md`, `docs/atomic_mailbox.md`, generated maps | Detailed current PL ABI and atomic semantics | Legacy ring interpreted as current product ABI |
| `docs/pl_design.md` (to create) | Current internal PL algorithms/invariants and links to source/tests | Chronological experiments and abandoned architecture alternatives |
| `docs/build.md`, `software/DEVELOPMENT.md`, `docs/coding_standards.md` | Component build/edit/test workflow and conventions | Copies of EDF image or ESP-IDF environment recipes |
| `software/esp32/README.md`, `software/protocol/monitor/README.md` | Current firmware layout/build boundary and shared wire implementation limits | Independent firmware repo or duplicate wire schema |
| `software/linux/production.md`, output/direct-duty/powered procedures, Linux README | Current binding-specific operations and measured acceptance procedures | Old implementation milestone narratives; unsupported qualification claims |
| Companion repository build/hardware guides | Commands/artifacts owned by edf-dev, esp32-dev and motor-interface-integrated | A second DMOTOR control/recovery specification |

The status/issue/decision documents are separate views with distinct ownership.
The plan links to those records after migration rather than maintaining another
live copy. The draft plan's baseline/issue tables seed them; keep a fixed reviewed
plan snapshot in the archive when replacing those tables with links.

## Archive layout and retention

Proposed first migration root: `docs/archive/pre-system-plan-2026-11-04/`, preserving
original relative paths underneath it. For example, the old `DMOTOR_SW.md` becomes
`docs/archive/pre-system-plan-2026-11-04/DMOTOR_SW.md`; a review under `docs/` keeps
that relative location within the snapshot. Do not overwrite an existing archive;
use a distinct reviewed baseline if a migration with this name already exists.

Archive scope includes superseded root documents, component design alternatives,
chronological reviews/progress notes, and historical portions of mixed documents.
For rewritten current files, archive the complete original first. Generated ABI
documents are not moved independently of their generator; their current paths and
build checks remain intact. Additional platform documents are not obsolete just
because the initial integration target is Arty.

Keep `docs/evidence/` and `software/evidence/` at their existing stable paths unless
a separately reviewed manifest moves them. Keep build artifacts in the owning
artifact store; do not add large bitstreams/images to Markdown archives. Evidence
is historical by nature and belongs behind the evidence/archive navigation, not
in the current-state table unless it matches that table's exact source/configuration.

No source, test, schema, calibration, generated-header input, board fabrication
artifact or executable build script is archived merely because it is mentioned
by an old document. Git history alone is not the browseable archive requested here.

## Migration procedure and acceptance

1. **Snapshot:** record the actual source commit/dirty files and companion repository
   revisions immediately before migration; preserve local changes. Inventory tracked
   and relevant untracked documents, diagrams and linked attachments. The table below
   is the tracked Markdown baseline, not proof that other formats are absent.
2. **Extract:** label each paragraph/table as current requirement, current fact,
   unresolved proposal, historical rationale or evidence. Map every current requirement
   and unresolved finding to one owning destination. Do not mark historical R2 open
   implementation work when rev-10 code now exists; preserve its qualification gap.
3. **Reconcile:** use current source/schema/saved BD and explicit user decisions to
   resolve stale implementation claims. Do not infer physical success from code.
   Where evidence conflicts, create an open issue instead of silently picking a claim.
4. **Prepare replacements:** write the current documents, state matrix, active decisions
   and issue register; replace obsolete “next step” prose and dated claims. Every
   proposed timing/safety decision remains visibly proposed until dispositioned.
5. **Manifest and move:** record original path, source commit/blob or content SHA-256,
   action, archive path, current successor, extracted issue IDs and reviewer. Snapshot
   mixed documents before rewriting; move wholly historical documents. Use `git mv`
   where appropriate. Update tracked source manifests if any moved document is a
   packaging input (monitor protocol README is one); preserve reproducible staging.
6. **Repair navigation:** make README/index link only current documents plus one archive
   index. Update active inbound links to successors. Preserve necessary external-link
   compatibility with a short “superseded” pointer only, not duplicate instructions.
   Archive-to-archive links follow the mirrored layout; historical code references use
   commit-pinned links. Fix paths to retained evidence. Label every archive page as
   historical/non-authoritative; do not edit old technical claims to look current.
7. **Verify before completion:** check Markdown/HTML/local attachment links, generator
   outputs and staged build inputs; search active docs for superseded pin choices,
   XDC reset-safety claims, “backend pending,” competing firmware locations and stale
   gate closure claims. Run affected packaging tests if hashed/staged docs change.
   Review the requirement/issue mapping with subsystem owners.

**Acceptance:** no lost current requirement or unresolved issue; one owner for each
contract; no broken active links; archive originals recoverable by manifest; current
procedures internally consistent; historical evidence clearly scoped; current index
contains no accumulated review rounds. Do not delete originals before these checks.

## Companion repositories

Keep EDF's canonical build instructions in `edf-dev/docs/build.md` and ESP-IDF
instructions in `esp32-dev/docs/build.md`. Add links to the accepted system plan
and state where their build responsibility ends. Hardware-repository firmware
plans remain historical inputs; extract any unique still-current electrical or
sensor requirement into its owning current document before archiving them.
Inventory and archive companion documentation within its owning repository, with
cross-repository commit/path mapping. No companion files are moved by this draft.

## Initial dmotor Markdown inventory

This table is a **proposed disposition**, not a finding that every listed component
reference is obsolete. “Extract/review” entries require section-level review before
archival; if still uniquely normative, retain/rewrite until the successor is ready.
Evidence collections are retained separately as described above.

| Existing path | Proposed action / current successor |
|---|---|
| [DMOTOR_ARCH.md](../DMOTOR_ARCH.md) | Snapshot then rewrite current architecture; extract active decisions/issues. |
| [DMOTOR_CONTROL.md](../DMOTOR_CONTROL.md) | Snapshot then rewrite ordered-board protection; retain unresolved R4 explicitly. |
| [DMOTOR_SW.md](../DMOTOR_SW.md) | Extract requirements into integrated plan/programming guide/issues; archive full PS-only plan and milestone history. |
| [README.md](../README.md) | Rewrite navigation to current set and one archive link. |
| [docs/DMOTOR_ARCH_0_17.md](../docs/DMOTOR_ARCH_0_17.md) | Archive superseded architecture; preserve rationale links, no current implementation authority. |
| [docs/architecture_status.md](../docs/architecture_status.md) | Merge verified current facts/issues into status/issues; archive previous assessment. |
| [docs/arty_monitor_interface.md](../docs/arty_monitor_interface.md) | Retain current wiring; remove stale UART-transport-pending statement; link implementation status. |
| [docs/atomic_hls_adapter.md](../docs/atomic_hls_adapter.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/atomic_mailbox.md](../docs/atomic_mailbox.md) | Retain current atomic publication/activation semantics. |
| [docs/axi_peripheral.md](../docs/axi_peripheral.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/batch_registers.md](../docs/batch_registers.md) | Retain generated mailbox map at generator-owned path. |
| [docs/bounded_velocity.md](../docs/bounded_velocity.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/build.md](../docs/build.md) | Retain canonical component/Vivado workflow; archive superseded alternatives. |
| [docs/coding_standards.md](../docs/coding_standards.md) | Retain current conventions. |
| [docs/command_engine.md](../docs/command_engine.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/controller.md](../docs/controller.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/controller_axi.md](../docs/controller_axi.md) | Retain current detailed AXI contract; distinguish current profile from historical layouts. |
| [docs/controller_programming.md](../docs/controller_programming.md) | Retain current programming reference; link shared status/issues rather than duplicate chronology. |
| [docs/current_build_2026_11_04.md](../docs/current_build_2026_11_04.md) | Archive as dated build record; current status points to exact matching evidence, not presumed latest source. |
| [docs/current_ip_export.md](../docs/current_ip_export.md) | Extract exact active export identity into status; archive dated export history. |
| [docs/dispatcher.md](../docs/dispatcher.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/divider_widths.md](../docs/divider_widths.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/dvrmc_validation.md](../docs/dvrmc_validation.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/dvrz.md](../docs/dvrz.md) | Retain separate platform scope and pending qualification; not superseded by Arty. |
| [docs/history/development-log.md](../docs/history/development-log.md) | Archive existing historical log; preserve evidence links. |
| [docs/history/ip-export-log.md](../docs/history/ip-export-log.md) | Archive existing historical log; preserve evidence links. |
| [docs/implementation.md](../docs/implementation.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/index.md](../docs/index.md) | Rewrite as current-only directory after migration. |
| [docs/mailbox_control.md](../docs/mailbox_control.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/mmio_logic.md](../docs/mmio_logic.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/motion_executor.md](../docs/motion_executor.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/pl_monitor_progress.md](../docs/pl_monitor_progress.md) | Extract current PL state/evidence/issues; archive chronological increment record. |
| [docs/protection_feedback.md](../docs/protection_feedback.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/ps_monitor_progress.md](../docs/ps_monitor_progress.md) | Extract current PS state/evidence/issues; archive chronological increment record. |
| [docs/recovery_transition_contract.md](../docs/recovery_transition_contract.md) | Retain single current PROPOSED contract; replace stale implementation-status prose with status link. |
| [docs/registers.md](../docs/registers.md) | Retain generated map; make profile/internal scope explicit, not a legacy product API. |
| [docs/routed_audit.md](../docs/routed_audit.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/safety_configuration.md](../docs/safety_configuration.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/shared_control.md](../docs/shared_control.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/shared_engine_transport.md](../docs/shared_engine_transport.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/shared_mailbox.md](../docs/shared_mailbox.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/software_architecture_review_2026_11_03.md](../docs/software_architecture_review_2026_11_03.md) | Extract open issues/active rationale; archive all chronological review rounds. |
| [docs/target_age.md](../docs/target_age.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/velocity_capability.md](../docs/velocity_capability.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/velocity_estimator.md](../docs/velocity_estimator.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/velocity_exact_onephase.md](../docs/velocity_exact_onephase.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/velocity_exact_shared.md](../docs/velocity_exact_shared.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/velocity_mailbox.md](../docs/velocity_mailbox.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/velocity_only.md](../docs/velocity_only.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/velocity_pair.md](../docs/velocity_pair.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/velocity_tenths.md](../docs/velocity_tenths.md) | Extract/review current PL algorithms, invariants and tests into docs/pl_design.md or ABI references; archive obsolete alternatives only after mapping. |
| [docs/verification.md](../docs/verification.md) | Extract applicable acceptance requirements into gate/test matrix; archive superseded claims. |
| [docs/vivado_development.md](../docs/vivado_development.md) | Merge any unique current procedure into docs/build.md; archive superseded instructions. |
| [docs/vivado_reference.md](../docs/vivado_reference.md) | Review/extract current technical reference into build/PL design; archive alternatives. |
| [software/CLIENT.md](../software/CLIENT.md) | Retain public client API and rules. |
| [software/DEVELOPMENT.md](../software/DEVELOPMENT.md) | Retain PS developer workflow. |
| [software/README.md](../software/README.md) | Snapshot then retain orientation/current links; archive milestone chronology. |
| [software/esp32/README.md](../software/esp32/README.md) | Retain current firmware ownership/layout; link actual esp32-dev environment and phase status. |
| [software/linux/README.md](../software/linux/README.md) | Retain current diagnostic binding rules; extract historical validation narrative. |
| [software/linux/direct-duty.md](../software/linux/direct-duty.md) | Retain reviewed direct-duty procedure; reconcile current HB/monitor gates. |
| [software/linux/output-validation.md](../software/linux/output-validation.md) | Retain reviewed inhibited validation procedure; evidence separated from requirements. |
| [software/linux/powered-commissioning.md](../software/linux/powered-commissioning.md) | Retain reviewed powered procedure; review/update for integrated G5/G6 scope, physical STOP, battery/operator/bystander controls and measured limits; approval remains pending. |
| [software/linux/production.md](../software/linux/production.md) | Snapshot then retain current commissioning procedure; remove superseded milestone claims. |
| [software/protocol.md](../software/protocol.md) | Merge unique details into programming guide; replace with a short pointer after review. |
| [software/protocol/monitor/README.md](../software/protocol/monitor/README.md) | Retain shared wire/transport reference; preserve packaging/provenance input. |

Inventory: 65 tracked Markdown documents listed; 6 tracked Markdown evidence documents remain in their evidence collections. The system plan, consolidation plan and initial requirements matrix are additional current review inputs at this inventory baseline. Re-inventory at execution time.
