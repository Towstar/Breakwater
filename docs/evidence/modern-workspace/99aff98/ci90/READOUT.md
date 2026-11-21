# PR #90 CI audit

Checked 2026-11-21 UTC using the connected GitHub API.

- PR head: `3d91452d81af7052effb35f98afe48c6f3661b20`.
- Windows checks run: `44842134400`, attempt 1, completed successfully.
- All six jobs passed. No rerun or source change was necessary.
- Newly completed Desktop ASan job: `136657602713`.
- Downloaded artifact SHA256: `ede1a8e329525be7095ea46b04f131dfd4d1883596c0bfe5d462bbfdbe2418f8`.
- Artifact verification: 88/88 CTests executed, 0 failed, 0 CTest skips; 1,181 Qt passes, 0 failures, one expected optional PhysicsNeMo calibration skip.
- Desktop ASan CTest duration: 2,431.40 seconds.
- CI merge commit: `695e6f65d1efe39d4f3054c1b653a9447fee8b1d`; clean source tree `dcd70be6d4afabfc30a3fc6ab51e8203daabb89b`, verified equal to the PR head tree.
- Instrumentation covers owned C++ targets. Prebuilt Qt, Python and Torch remain outside established ASan coverage.

Evidence: `final-status.json`, normalized decoded `desktop-asan-job.log`, original downloaded `windows-desktop-asan.zip`, and extracted verification under `windows-desktop-asan/out/ci/desktop-asan/verification.json`.

The other five successful job artifacts were retained by the preceding audit under `out/nemotron-completion/ci90`.

Upstream comparison: main remains `3116b3d8349beab729f8a1413cbbd9ff160e67ac`; all 17 Person A branch heads match the prior `out/nemotron-completion/ci89/person-a-heads.json` record. The complete fresh collection is `../person-a-heads-complete.json`; an earlier failed per-branch lookup is preserved separately and is not used as comparison evidence.
