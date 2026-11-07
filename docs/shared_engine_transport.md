# Shared motor-engine transport

`rtl/shared_engine_transport.sv` is now the PSF1 ticketed PS transaction bridge.
It no longer instantiates the shared motor HLS core. See the authoritative
[PSF1 register/transaction contract](ps_executor.md).

The atomic-mailbox HLS adapter is unchanged in role and remains a separate block.
The 35-word motor request, 16-word result, configuration and fault meanings are
preserved. Transport expiry still feeds the original independent scheduler fault
path. The previous HLS-specific timing/memory-port measurements are historical
in the Git history and `docs/evidence/shared_engine_transport/`.
