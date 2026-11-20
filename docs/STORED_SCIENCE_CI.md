# Stored-candidate Science UI convergence

PR87's original Windows ASan job passed 87/88 CTest entries. Its minimum-size UI
check stopped at phase 51 with `selected_stored_science_source_did_not_reconstruct`,
after the preceding 24 groups passed. The original failed report and parsed UI
output are preserved byte-for-byte in [the verification record](evidence/stored-science-ci/verification.json).
The separate normal-job worker lifecycle failure remains in its
[own follow-up](WORKER_LIFECYCLE_CI.md).

The UI harness previously allowed only three seconds after investigation stopped
for stored Science to become runnable. Investigation completion precedes asynchronous
checkpoint publication, final History selection and independent reconstruction of
the selected candidate. The original output did not retain enough source/history
state to distinguish pending work from terminal reconstruction rejection; it does
not establish a production numerical or authority defect.

Phase 51 now checks convergence of the exact recommendation, History selection
and immutable Science source: session ID, run ID and snapshot ID must agree.
Only pending convergence receives a bounded 15-second wait. An aligned, idle
source that cannot run fails promptly. Structured diagnostics retain readiness,
source identity, History/worker state and public status before the assertions.
The overall UI check remains bounded to 60 seconds; its CTest timeout is unchanged.
The original `preserved()` numerical/hash/run/approval checks and return to the
forecast source remain mandatory. This changes verification, not application
controls, source reconstruction, numerical methods or approval rules.

Targeted local minimum-size checks passed on base
`e5a76aed06703497b3d4c8aafc4ed500a094fc4a` with the preserved uncommitted
verification patch (`dirty: true`):

- Normal: one CTest entry passed, 16.61 seconds, all 25 UI groups.
- ASan: one CTest entry passed, 35.87 seconds, all 25 UI groups.
- Both report a 960×540 logical window and actual device-pixel ratio 1.75.
- Both retain matching nested recommendation/History/source identities and
  `hosted_model_requests: not_requested`.

The evidence index binds the working source file, identical normal/ASan patches,
executable hashes and unchanged raw CTest/LastTest/build outputs. These are
working-tree diagnostic results, not clean-commit final qualification or a rerun
of the entire Windows matrix. Later commits do not relabel the tested binaries.
ASan covers owned C++ targets; prebuilt Qt/Python/Torch are outside that scope.

Independent same-family review found no consequential issue in the convergence
guards. Normal and ASan native execution confirmed the nested Qt variant/JSON
diagnostic data is retained. No new hosted request or control application was
performed for this check.

Full frozen-candidate CI/local checks, the complete declared hosted round, refreshed
private packaging, actual clean-Windows acceptance, owner/licensing review and
ordered integration remain pending. This report grants no release approval.
