# Documentation index

[Daemon client library](../software/CLIENT.md): required typed API for all daemon callers.

[Controller programming guide](controller_programming.md): consolidated PS interfaces,
lifecycle, units, ownership and failure rules.
[PS development workflow](../software/DEVELOPMENT.md): formatting, analysis and tests.

[Current hardware/Linux build — 2026-11-04](current_build_2026_11_04.md):
matched revision-10 hardware, UART Lite kernel, ARM driver and verified SD export.

[Arty motor/monitor interface](arty_monitor_interface.md): saved wiring, UART
settings, pin-assignment status and the source interface check.

Current implementation increment: [PL monitor integration](pl_monitor_progress.md)
(revision 10: routed heartbeat/coast integration and registered GPIO diagnostics).

[PS monitor development](ps_monitor_progress.md): shared framing codec, bounded
UART transport, controller-driver checks and Yocto UART Lite integration.

Start with [build.md](build.md) for current builds, [architecture](../DMOTOR_ARCH.md)
and [software plan](../DMOTOR_SW.md) for requirements, and
[commissioning](../software/linux/production.md) for board operation.

For the ordered rev A boards, see the
[Zynq and ESP32 software architecture review](software_architecture_review_2026_11_03.md)
for integration findings and the recommended implementation sequence, and the
[joint recovery transition contract](recovery_transition_contract.md)
(PROPOSED v4) addressing its R1/R3 findings. The review's latest assessment records
the completed documentation cleanup and remaining defects in that proposal.

The [ESP32 firmware plan](../software/esp32/README.md) defines its planned home
in this dmotor repository and the separate firmware build boundary.

The following component documents and measurements retain their individual scope.
Historical build logs live in `history/`; raw verification evidence stays in
`evidence/`. Neither overrides the current build guide.

- [DMOTOR_ARCH_0_17](DMOTOR_ARCH_0_17.md)
- [architecture_status](architecture_status.md)
- [atomic_hls_adapter](atomic_hls_adapter.md)
- [atomic_mailbox](atomic_mailbox.md)
- [axi_peripheral](axi_peripheral.md)
- [batch_registers](batch_registers.md)
- [bounded_velocity](bounded_velocity.md)
- [build](build.md)
- [coding_standards](coding_standards.md)
- [command_engine](command_engine.md)
- [controller](controller.md)
- [controller_axi](controller_axi.md)
- [current_ip_export](current_ip_export.md)
- [dispatcher](dispatcher.md)
- [divider_widths](divider_widths.md)
- [DVRZ platform](dvrz.md)
- [dvrmc_validation](dvrmc_validation.md)
- [implementation](implementation.md)
- [mailbox_control](mailbox_control.md)
- [mmio_logic](mmio_logic.md)
- [motion_executor](motion_executor.md)
- [protection_feedback](protection_feedback.md)
- [registers](registers.md)
- [routed_audit](routed_audit.md)
- [safety_configuration](safety_configuration.md)
- [shared_control](shared_control.md)
- [shared_engine_transport](shared_engine_transport.md)
- [shared_mailbox](shared_mailbox.md)
- [target_age](target_age.md)
- [velocity_capability](velocity_capability.md)
- [velocity_estimator](velocity_estimator.md)
- [velocity_exact_onephase](velocity_exact_onephase.md)
- [velocity_exact_shared](velocity_exact_shared.md)
- [velocity_mailbox](velocity_mailbox.md)
- [velocity_only](velocity_only.md)
- [velocity_pair](velocity_pair.md)
- [velocity_tenths](velocity_tenths.md)
- [verification](verification.md)
- [vivado_development](vivado_development.md)
- [vivado_reference](vivado_reference.md)
