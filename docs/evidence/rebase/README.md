# Reverted local-rebase experiment

This experiment was undone at the user's request because dispatcher LUT usage
increased from 24,578 to 25,577 at 20 MHz. The active source and top-level reports
have been restored to the preceding wide-position implementation. The added
2^28-count configuration limit and local rebase helper/test are no longer active.

`implementation.patch` captures the experiment against `17116b7`, including its
new header and test. The reports and original regression/equivalence logs in
this directory describe the reverted implementation, not the current source.
See `../../verification.md` for measurements and rollback verification.
