# Modern workspace verification evidence

Runtime source: `99aff9851544d2cba00b463b2c617b37a97a4a43`, based on `4559bf1d058d2d5eaadfe2ce50eaca7925727157`. Later delivery changes only documentation and evidence.

- `build-*` preserve full configured build results and source/executable identities.
- `tests-*` preserve the focused 32-CTest result per configuration, with zero failures/skips. JUnit contains counts; `last-test-*` retain original test stdout and actual UI records.
- `ui-records-*` are extracted JSON records from those original stdout logs. All four actual client sizes passed in both modes. Normal automated screenshots were not enabled; ASan screenshots were retained.
- `gallery.json` identifies three byte-exact PNG copies from the ASan synthetic-fixture checks. The gallery excludes private saved-user-workspace captures.
- `REVIEW.md` is the independent same-family source/artifact review at its stated cutoff. It is not formal human approval.
- `ci90/` retains the completed six-job PR90 status and the newly finished Desktop ASan log/verification. Those records qualify source `3d91452`, not this new UI increment. The original downloaded ZIP remains local; its digest is recorded in the readout. Original runner/local paths in these records are provenance, not files bundled in this directory.
- `ci91-checkpoint.json` preserves the earlier five-passed/ASan-pending checkpoint. `ci91/final-status.json`, the final readout and six decoded job logs supersede it: all six jobs passed on the first attempt. Artifact verification confirms clean merge/head source-tree equality for PR head `4559bf1`.

These are development-machine UI/interaction checks. They do not establish a new full 88-test qualification, hosted model behavior, clean-Windows acceptance, additional real display scales or release approval. See [the implementation report](../../../MODERN_WORKSPACE.md) for commands and scope. No binaries, databases, user project document, credentials or raw model reasoning are included.
