# Software 0.7.0 output-validation evidence

- Native UBSan build: all six suites passed (semantics, protocol, Linux helpers,
  runtime, real-process commissioning and normal service/client tests).
- Runtime tests verify target and watchdog suppression independently, disabled-mode
  rejection, one injection per rearm, host fallback and hardware fault response.
- Process tests use the shipped output-validation config and verify gated commands,
  bounded fallback/disconnect, no implicit re-enable and timestamped event records.
- EDF ZC702 ARM package build: 1034 tasks successful, including do_semantics
  (ARM user-mode QEMU semantics/protocol/Linux/runtime tests). See bitbake.txt.
- No real kernel/FPGA timeout, scope timing, or powered motor validation performed.

Arty package build was blocked by the existing hardware-fingerprint guard. The
installed bundle matches external/dmotor; the sibling checkout has additional
ignored historical evidence .log files included in tools/build.py's fingerprint.
No RTL/board file differences were found in the fingerprint comparison. The
bundle was not replaced and the guard was not bypassed. Software packaging was
validated in EDF's independent ZC702 build context; no SD image was rebuilt.
For Arty deployment, reconcile the selected checkout/pin with the hardware bundle
using the documented build workflow. Do not silently rewrite the manifest hash.
