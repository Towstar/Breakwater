# Credential-free Windows checks

The workflow runs for pull requests, main pushes and manual dispatch. Native normal
and ASan jobs precede separate desktop normal/ASan, Python science and Lean jobs.
No hosted model call or NVIDIA credential is used. Setup failures fail the job;
they are not recorded as successful tests or silently skipped integrations.

The workflow uses `windows-2022` for VS 2022 compatibility. The hosted image is a
moving service image: `verification.json` records its image version and the actual
MSVC/SDK/CMake/Ninja versions rather than claiming exact compiler-image pinning.
Qt 6.12.0 and its SDK metadata revision use the existing checked installer. Scientific
CPython 3.12.13, Torch 2.15.0+cpu and NumPy 2.5.4 use the committed CPU hash locks.
Lean 4.35.0 uses the official archive/hash already documented in `proofs/README.md`;
the existing proof verifier checks the committed Mathlib/Lean pins and positive and
negative assumption audits. No A-owned dependency manifest is changed.

Actions are exact commit pins, resolved from official repositories on 2026-11-08:
checkout v6, setup-python v6, cache v5 and upload-artifact v6. Permissions are
`contents: read`, checkout credentials do not persist, and cache paths are limited
to declared public tool downloads/dependencies. Credentials and broad environment
dumps are excluded from metadata, caches and artifacts.

Run a job locally from PowerShell 7:

```powershell
./scripts/ci/Run-Checks.ps1 -Suite native -Mode normal
./scripts/ci/Run-Checks.ps1 -Suite native -Mode asan
./scripts/ci/Run-Checks.ps1 -Suite desktop -Mode normal
./scripts/ci/Run-Checks.ps1 -Suite desktop -Mode asan
./scripts/ci/Run-Checks.ps1 -Suite science
./scripts/ci/Run-Checks.ps1 -Suite lean
```

`-SkipDependencySetup` reuses the existing project-local CI bootstrap, CPU Python
and Qt SDK without downloads or scientific-environment synchronization. It still
checks their expected versions, runs actual tests and reports missing dependencies
as failures. It is intended for local verification; the workflow always performs
the checked setup. Lean's archive checksum and pinned verifier remain mandatory.

Compiled builds use ignored `out/ci-build/<suite>-<mode>` directories and do not
rewrite the tracked root solution. Evidence under `out/ci/<suite>-<mode>` includes
step logs, toolchain/source/helper identities, CTest registration and JUnit reports,
Qt case counts and copied render/history evidence. GitHub artifacts retain these
even after a failing step. Full build trees and tool binaries are not uploaded.

Desktop rendering uses D3D11 WARP via `QSG_RHI_BACKEND=d3d11` and
`QSG_RHI_PREFER_SOFTWARE_RENDERER=1`; this retains the RHI needed by Quick 3D.
The full desktop suite requires actual Torch worker and historical-view checks to
be registered and rejects missing Qt suites, skipped CTests or skipped Qt cases.
The Python report permits only the explicitly named optional PhysicsNeMo class;
all other dependency skips fail the job. Skips remain listed by test/reason.
Prebuilt Qt/Python/Torch remain outside established ASan instrumentation coverage.

Sources for setup behavior:
[runner image](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md),
[workflow permissions/pins](https://docs.github.com/en/actions/reference/security/secure-use),
[Qt WARP](https://doc.qt.io/qt-6/windows-graphics.html),
[Lean release](https://github.com/leanprover/lean4/releases/tag/v4.35.0).
