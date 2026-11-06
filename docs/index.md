# Current documentation

## System execution and decisions

- [System status](system_status.md): implemented scope, evidence boundaries and gate state.
- [Active decisions](decisions.md): commissioning-only release and accepted technical choices.
- [Open issues](open_issues.md): U01–U11 owners, remaining work and blocking gates.
- [Integrated execution plan](system_bringup_plan.md): B1–B7 and deferred BV/GV.
- [G1b editable test-plan and review record](gates/G1b.md): operator worksheets and pending conditional acceptance.
- [G1d preparation/review record](gates/G1d.md): evidence and pending acceptance checklist.
- [G1a reset/cleanup audit and admission proposal](g1a_cleanup_admission.md).
- [G1 work package](g1_work_package.md): concrete contract, proof and experiment tasks.
- [Requirements-to-test matrix](system_requirements_matrix.md): acceptance traceability.

## Current contracts and interfaces

- [Architecture](../DMOTOR_ARCH.md) and [protection circuitry](../DMOTOR_CONTROL.md).
- [Joint recovery contract](recovery_transition_contract.md): PROPOSED v4; not yet accepted or fully implemented.
- [Arty wiring/monitor interface](arty_monitor_interface.md).
- [Controller programming guide](controller_programming.md) and [required daemon client API](../software/CLIENT.md).
- [AXI controller](controller_axi.md), [atomic mailbox](atomic_mailbox.md) and [generated mailbox map](batch_registers.md).
- [Monitor wire/transport](../software/protocol/monitor/README.md) and [ESP32 firmware location](../software/esp32/README.md).

## Development and board procedures

- [Component build guide](build.md), [PS development](../software/DEVELOPMENT.md) and [coding conventions](coding_standards.md).
- [Production-access commissioning](../software/linux/production.md), [inhibited output validation](../software/linux/output-validation.md), [direct duty](../software/linux/direct-duty.md), and [powered procedure](../software/linux/powered-commissioning.md).
- [Separate inhibited UIO diagnostics](../software/linux/README.md).
- [Additional DVRZ platform](dvrz.md): separate platform scope, not Arty qualification.

Current operational procedures still require integrated gate qualification; their
presence does not constitute permission to power motors. Architecture/component
extraction is in progress; unresolved conflicts belong in the issue register.

## Documentation migration and history

[Consolidation/migration tracker](documentation_consolidation_plan.md) records the
remaining extraction work. [Historical archive, evidence and remaining inventory](archive/index.md)
is the entry point for previous review rounds, milestones and build records.

- [G1a post-pairing recovery model and remaining integration obligations](g1a_recovery_model.md)

- [G1a PS endpoint-session allocator and storage qualification limits](g1a_endpoint_sessions.md)
- [G1a endpoint pairing, replay validation and integration obligations](g1a_session_link.md)
- [G1a recovery wire adapter and scripted exchange evidence](g1a_recovery_link.md)
- [G1a behavioral monitor simulator, timing assumptions and evidence](g1a_monitor_simulator.md)
- [G1a Runtime admission hold](g1a_runtime_admission.md)
- [G1a recovery coordinator and queued command admission](g1a_recovery_coordinator.md)
- [G1a MMIO cleanup attestation acquisition and limits](g1a_cleanup_attestation.md)
- [G1a durable PS sessions in coordinator discovery](g1a_session_integration.md)
