# Credential-free Windows checks

The Windows checks workflow runs on pull requests, pushes to main and manual
dispatch. Native normal/AddressSanitizer jobs run first. Separate desktop
normal/ASan, CPU Python science and Lean assumption-audit jobs follow. A failed
dependency/setup/test step remains a failed job with retained evidence.

The workflow uses `windows-2022`, full commit-SHA action references and
`contents: read`. It does not receive an NVIDIA key or run hosted model rehearsals.
Those production sessions remain separately recorded in DEMO_REHEARSAL.md.

## Setup and execution

Run from the repository root in PowerShell 7 on a Windows x64 machine with Visual
Studio 2022 C++ tools, CMake, Ninja and a bootstrap CPython 3.12 interpreter:

```powershell
.\scripts\ci\Run-Checks.ps1 -Suite native -Mode normal
.\scripts\ci\Run-Checks.ps1 -Suite native -Mode asan
.\scripts\ci\Run-Checks.ps1 -Suite desktop -Mode normal
.\scripts\ci\Run-Checks.ps1 -Suite desktop -Mode asan
.\scripts\ci\Run-Checks.ps1 -Suite science
.\scripts\ci\Run-Checks.ps1 -Suite lean
```

Downloads and environments remain in ignored `.tools/`; builds use
`out/ci-build/<suite>-<mode>/`. These commands do not regenerate the tracked root
Visual Studio solution. The CI bootstrap dependencies have a B-owned hash lock.
The existing SDK installer fixes Qt 6.12.0 and its SDK revision. The existing
scientific setup enforces CPython 3.12.13, CPU Torch/NumPy and their committed hash
locks. Lean uses the audited 4.35.0 release and existing Mathlib revision.

The OS image and installed compiler patch versions can change on hosted runners.
Each job records the actual image, compiler, SDK and relevant dependency versions;
an image label alone is not a claim of an immutable toolchain.

Desktop jobs use `QSG_RHI_BACKEND=d3d11` and
`QSG_RHI_PREFER_SOFTWARE_RENDERER=1`, selecting Direct3D WARP while exercising the
actual Qt Quick/Quick3D render/reopen checks. This is not a GPU benchmark.

## Evidence and failure handling

Evidence lives in `out/ci/<suite>-<mode>/`: step logs, verification metadata,
registered/executed test counts and test reports. Desktop evidence includes Qt
case totals and retained charts/history fixtures. Python evidence names every
skip; only the declared optional PhysicsNeMo test class may skip in the CPU lane.
Missing Torch/schema dependencies, zero tests, missing expected desktop science
checks or unexpected skipped cases fail the job. Lean runs both the positive audit
and the deliberately rejected-assumption probe using the unchanged proof script.

Workflow artifacts retain evidence on failure as well as success, for 14 days.
Prebuilt Qt/Python/Torch remain outside established ASan instrumentation coverage.
Hosted-runner success does not close clean-machine/manual-dialog release acceptance.

STATUS.md records actual executed results. A newly added workflow is not itself
evidence that its hosted jobs passed.
