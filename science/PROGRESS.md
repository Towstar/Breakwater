# Person A science evidence

## A3 — differentiable fixed-flow storage (November 7, 2026)

Implemented on `codex/person-a-engine`, based on current shared baseline `bf8beb1`.
All incremental edits are in `science/` and `engine/`. Desktop, shared contracts, root
configuration/scripts/solution and shared documentation are untouched.

Delivered:

- Standalone generic CPU double-precision PyTorch storage rollout with captured input
  state, stable catalog IDs, selective overrides, source-derived flows and synthetic
  cost gradients. Initial/final-time semantics and validation follow the native model.
- Explicit control/draft errors, no clipping and no Torch feasibility verdict. Includes
  the distinction between tolerated omitted stored defaults and strictly bounded
  explicit overrides, plus independence from ambient PyTorch device settings.
- Actual native-reference exporter with 12 current canonical/custom/renamed cases,
  separately executed dense/sparse/affine forecasts and native validation flags.
- Verification CLI, per-case/input and native-executable hashes, all-sample comparisons,
  multi-epsilon terminal/cost/smooth-objective gradient checks and Torch gradcheck.
- Python 3.12.13 pin, CPU Torch 2.15.0+cpu / NumPy 2.5.4 and hash-locked transitive
  dependencies. Setup, equations, commands and limitations are in README.md.

Actual executed checks:

1. `scripts/build.ps1 -Preset headless`: native build and all six CTest programs
   passed (47 existing baseline/search/transport case groups and three examples).
2. `scripts/build.ps1 -Preset asan`: native build and the same six programs passed
   MSVC AddressSanitizer. No sanitizer errors were reported. After adding the stored
   tolerance fixture, rebuilt both native exporter variants and executed each below.
3. `science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v`:
   **26 tests passed**, including 24 rollout cases and two unchanged worker tests.
4. `science/verify_rollout.py --native out/headless/engine/breakwater_rollout_reference.exe
   --report out/a3-verification.json`: **12 cases × three native solvers passed**.
5. Repeated the comparison with `out/asan/engine/breakwater_rollout_reference.exe` in
   the MSVC developer environment: all comparisons/gradients passed, without reported
   sanitizer errors. A fresh plain shell first failed to load the checked executable
   (exit 3221225781); initializing the developer runtime PATH resolved it. README
   explains running checked commands in the same initialized PowerShell session.
6. Both final reports checked **44 positive-width controls**, **132 central-difference
   perturbations**, and Torch gradcheck for every case with variable controls.

Measured errors in both final reports:

| Quantity | Maximum absolute error | Acceptance |
|---|---:|---|
| All-sample storage vs native | 7.67386154621e-13 m3 | 1e-9 + 1e-10 relative |
| Terminal-volume derivative | 4.44836967262e-9 h | 1e-6 + 1e-4 relative |
| Synthetic cost derivative | 1.94024778466e-8 units per m3/h | 1e-6 + 1e-4 relative |
| Dimensionless objective derivative | 1.05575923004e-10 per m3/h | 1e-6 + 1e-4 relative |

The mixed case ends at approximately 80/117 m3 and costs 12,780 synthetic units.
Negative, overflowing and shared-destination-overload cases remain unclipped and are
reported infeasible by the native validator. Time zero, exact nondividing final time,
generic/escaped IDs, omitted controls, availability, input mutation and concurrent
candidate isolation received executable checks.

Runtime: Windows x64, CPython 3.12.13, Torch 2.15.0+cpu (`torch.version.cuda is None`),
NumPy 2.5.4, uv 0.11.9; native MSVC 19.51.36260.0, CMake 4.3.1 / Ninja / C++20.
Generated environments/reports are ignored, not committed. Performance timings are
not presented as benchmarks.

Scope/next handoff:

- This is continuous fixed-flow storage differentiation, not transport differentiation,
  optimization, learned quality, PhysicsNeMo execution or a Lean proof.
- Native C++ continues to decide nominal volume feasibility and approval eligibility.
  Gradients apply at tested interiors, not through invalid controls, active-bound
  projection, nonsmooth constraints or discrete candidate selection.
- The worker still implements health only. B3 must publish the agreed method/provenance/
  output contract before scientific worker exposure. A3 does not invent that contract.
- B can consume the additive module after adopting that boundary and regenerate the
  root solution for `breakwater_rollout_reference`. No existing engine API changed.
- Next A increment is genuine PhysicsNeMo execution/reference evaluation (A4), followed
  by separately delivered measured benchmarks (A5) and checked algorithm proofs (A6).
