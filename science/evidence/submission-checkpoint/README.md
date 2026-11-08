# Retained submission verification

Runtime source: 6c0c2a2152dfe854619524529beb9193bc5da959.
See build-provenance.json for exact binary hashes, source file identities and
source-qualified test totals. The test output files are actual captured output.
The full-science files are deliberately named pre-test-split: 459 tests ran with
three calibration runtime checks still in discovery. Final discovery moves those
same three checks to explicit qualification; its real-runtime log is retained in
../transport-calibration/explicit-runtime-qualification.log.

The baseline image and provenance refer to source4a94649, not the final PhysicsNeMo
binary. They show a genuine installed Detect events result without hosted inference.
The final calibration image is an unedited native UI capture of source6c0c2a2.
All observations/costs/control examples are synthetic. Neither image is empirical
validation of a physical plant or permission to operate equipment.

Reproduction commands and application/remaining-scope notes are in
../../SUBMISSION_CHECKPOINT.md and ../../TRANSPORT_CALIBRATION.md.

Final selector runtime is 325a168b4356157d10e25783e88e7381a4d5e850, adopting B55
7fda267 after the actual6c0c2a2 calibration/trucking GUI workflows. The final
latest-selector/ frames each exited0 with empty stderr and were visually checked.
Both normal/compact installed native UI checks passed all9gates. The initial
hidden-window phase0 timeout is retained separately; visible reruns passed.
Build-provenance records both executable hashes and verifies that the calibration,
provider and numerical source files are unchanged by the selector-only update.
