# Workspace evidence audit — 2026-11-12

Read-only inspection of existing outputs; no build, flash or hardware test performed.

- Hardware: `edf-dev/artifacts/zybo-hardware/20261111T014852Z-92f5be46`.
- Image: `edf-dev/artifacts/zybo-z7-20/20261111T021256Z-ee70b3d9`.
- Export contains WIC.xz/bmap, boot.bin, DTB, kernel, rootfs, SHA256SUMS and manifests.
- Fully routed report: setup +2.489 ns, hold +0.015 ns; all specified timing met.
- DRC report: ten DSP-pipeline warnings, no listed errors or critical warnings.
- Software manifest: 0.10.1, PSF1-1000us, a7ead5e941fd3dbcf1480c1c6efcc4065f2cdab0,
  dirty=true; source SHA256 2dc889fbadf52ff05fe87a759edc597130cc6faebf154ae5cd1a61b8158b5fcd.
- Current source HEAD during audit: dmotor 80ef036. Do not equate that commit with
  the earlier dirty build. Match fingerprints and target versions before deployment.

Copied reports/manifests are provenance records. Export checksums were not rehashed
in this documentation pass; target boot/DDR/serial/encoder results were not supplied.
Hardware on hand (operator report): one monitor board, one motor board, one Zybo.

Operator update during reconciliation: Zybo is the default Zynq board, and the
current Zybo successfully detects ESP32 `hb10`. This is accepted as synthetic
HB detection evidence only; exact deployed image/GPIO/latency were not supplied.
See the dated G2 entry.
