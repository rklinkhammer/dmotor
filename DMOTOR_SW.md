# DMOTOR processor software architecture

Main now uses the **1 ms PS motor executor** described in [DMOTOR_ARCH.md](DMOTOR_ARCH.md).
The daemon owns Runtime, the exclusive KernelDevice and a PS calculation worker.
Runtime alone owns intent, client/authorization leases, target publication and
explicit lifecycle operations. The worker owns four fixed-point control contexts
and services the hardware scheduler through the private PSF1 bridge.

The worker computes in C++14 using the production Controller/VelocityMailbox
implementation. It validates PSF1 identity and the 1,000 us cadence, starts with
HALT asserted, and commits speculative state only after hardware accepts the
matching ticket. Failure asserts HALT and blocks later release; there is no
background command replay or automatic recovery. Hardware timers/PWM inhibition
operate independently of the worker and the AXI response consumer.

All applications use [dmotor_client](software/CLIENT.md). The client protocol,
commissioning policy and monitor-recovery limitations remain unchanged. Source
provenance and Yocto staging include the engine, worker and 1 ms timing headers.
Use the matched PSF1 driver, daemon and FPGA image; an old driver cannot access
the executor aperture.

See [programming guide](docs/controller_programming.md), [PSF1 registers](docs/ps_executor.md),
[build guide](docs/build.md), [status](docs/system_status.md) and
[previous software plan](docs/archive/pre-ps-executor-2026-11-07/DMOTOR_SW.md).
Hardware testing stays paused; no service startup grants ARM or enable authority.
