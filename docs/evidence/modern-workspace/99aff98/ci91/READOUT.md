# PR #91 final CI audit

Checked 2026-11-21 01:59 UTC using the connected GitHub API. Windows checks run `44848185755`, attempt 1, completed successfully. All six jobs passed. No source fix or workflow rerun was necessary.

## Source identity

- PR head: `4559bf1d058d2d5eaadfe2ce50eaca7925727157`.
- CI merge commit: `9c8998a998357fedcde1fe05292ebc10c670d753`.
- Exact shared source tree: `7e21f641a4801d44d8b25dffd7c032a2241c818d`.
- All six job verification records report a clean tree equal to the PR head tree.

## Results

| Job | Result |
| --- | --- |
| Native normal | 17/17 CTests; no failures/skips |
| Native ASan | 17/17 CTests; no failures/skips |
| Desktop normal | 88/88 CTests; 1,181 Qt passes; zero failures; one expected optional PhysicsNeMo skip |
| Desktop ASan | 88/88 CTests; 1,181 Qt passes; zero failures; one expected optional PhysicsNeMo skip |
| Python science CPU | 468 run, 464 passed, four expected optional PhysicsNeMo skips; zero failures/errors/unexpected skips |
| Lean assumption audit | Passed |

Normal desktop CTest duration was 718.02 seconds. ASan desktop CTest duration was 1,999.99 seconds; its verification finished at 01:58:31 UTC.

The runner recorded Windows Server 2022 image `20261116.370.1`, Visual Studio `17.14.37906.3`, compiler `14.44.35230.0`, Qt `6.12.0`, scientific Python `3.12.13` and Torch `2.15.0+cpu`. ASan coverage is limited to owned C++ targets; prebuilt Qt/Python/Torch remain outside established instrumentation coverage. Hosted Nemotron requests were not run by this credential-free workflow.

## Retained evidence

- `final-status.json`: final API workflow/job/artifact metadata and upstream comparison.
- `artifact-verification.json`: exact tree comparison, all six archive hashes, extracted result counts/toolchain records, and decoded log hashes.
- Six original downloaded artifact ZIPs were SHA256-verified against GitHub metadata before extraction.
- Six decoded job logs are saved as `job-<id>.log`; line endings are normalized to LF. The original artifact ZIPs are retained unchanged.
- Earlier pending snapshots remain separate records and are superseded for completion status by this final result.

Main remained `3116b3d8349beab729f8a1413cbbd9ff160e67ac`, and all 17 Person A branch heads remained unchanged at the 01:48:44 UTC pre-push refresh (`../pre-push-heads-0148.json`).

PR #90 also completed all six jobs successfully; its final ASan evidence and audit are retained in `../ci90/`.
