# Person A progress

## Increment 1: bounded native candidate search

Status: implemented and verified on `codex/person-a-engine`, based on shared commit
`bf90e55`. Work is isolated from the original `main` checkout. All tracked changes are
under `engine/`; Person B's desktop, contracts, root scripts/configuration, solution,
shared docs and canonical fixture files are untouched.

### Delivered

- Additive `breakwater/search.hpp`: explicit candidate batches and Cartesian search,
  limits of four varying catalog controls / 10,000 candidates / five recommendations.
- Captured immutable network settings, deterministic cost/index ordering, generic IDs,
  request validation and explicit `no_feasible_plan_found` results.
- Reusable affine screening workspace, shared destination checks, numerical diagnostics,
  and actual dense-reference simulation/validation for every returned recommendation.
- A runnable canonical search example and documentation in `engine/SEARCH.md`.
- Sixteen new search test cases, including exhaustive reference comparison, malformed
  inputs, custom/colon-containing IDs, cost effects of source curtailment, boundary
  tolerances, maximum candidate counts, omitted controls, concurrency and overflow.

### Executed verification (2026-11-07)

Toolchain: Windows x64, MSVC 19.51.36260.0, CMake 4.3.1, Ninja, C++20,
RelWithDebInfo with precise floating-point semantics.

1. Rebuilt the unchanged baseline in this worktree: all 13 existing cases passed.
2. `scripts/build.ps1 -Preset headless`: all four CTest programs passed, containing
   13 existing cases, 16 search cases, and normal/no-feasible executable examples.
3. `scripts/build.ps1 -Preset asan`: the same four programs passed with MSVC
   AddressSanitizer enabled. No sanitizer errors reported.
4. Ran the canonical example: 432 candidates, 39 screen survivors, five actual reference
   checks and five validated recommendations. Lowest evaluated cost was 4,800 synthetic
   units; final A/B volumes 116/117 m3, minimum headroom 4 m3 over three hours.
5. Ran the restricted example: two candidates, zero survivors, explicit no-plan result.
6. Verified the feature worktree has no tracked diff outside `engine/`, and the original
   `main` checkout remains clean.

### Person B integration notes

Include `breakwater/search.hpp` and call `search_candidates` or `evaluate_candidates`.
The existing shared contracts and current desktop-facing engine functions are unchanged.
Use the returned native trajectories and validation reports to populate the application
run store; assign application run/snapshot IDs there. Keep operator approval separate.
See SEARCH.md for limits, diagnostics, input errors and an example invocation.

Only `engine/CMakeLists.txt` changed for build integration. B should regenerate the
root Visual Studio solution after merging this increment; A did not modify it.

### Limitations / next work

Results are nominal finite-candidate volume/throughput checks, not continuous/global
optimization, robust guarantees, quality checks or indefinite sustainability. Screening
counts do not imply all survivors were reference-validated. No speedup benchmark or
formal proof is claimed. Transport, differentiable PyTorch, PhysicsNeMo execution and
Lean proofs remain separate future increments. Shared docs/STATUS.md is for B to update
when this branch is integrated.

## Increment 2: single-pipe transport reference

Status: implemented and verified on the same isolated Person A branch. Adds only
engine-owned transport API, implementation, example, tests and numerical notes.

- Conservative cell-centered upwind/centered advection-diffusion-reaction solver, with
  half-cell Dirichlet inlet, zero-gradient diffusive outlet and optional closed boundary.
- Forward/zero flow schedules, interval-average piecewise-linear inlet forcing, output
  and schedule event alignment, enforced boundary-aware positivity CFL, bounded work.
- Time-distance fields, exact final-time sample and signed tracer-mass ledger in mg.
- Robust finite-mass accumulation and scaled roundoff/ledger checks; no clipping.
- `engine/TRANSPORT.md` records equations, units, boundaries, resource limits and usage.

Executed on 2026-11-07 with the same MSVC/CMake toolchain:

1. `scripts/build.ps1 -Preset headless`: all six CTest programs passed, containing
   13 baseline, 16 search and 18 transport cases plus three executable examples.
2. `scripts/build.ps1 -Preset asan`: the same six programs passed under AddressSanitizer.
3. Explicit native example/CSV run: 64 cells, 41 frames, 136 substeps, max CFL 0.9,
   final inventory 1994.10774493 mg, max mass residual 1.36887055523e-12 mg.
4. Independent source review checked boundary coefficients, signed unit conversion,
   event alignment and positivity. Extreme-value findings received regression tests.

The transport module is not yet connected to the desktop/agent and does not change
volume-plan feasibility. PyTorch differentiation, PhysicsNeMo training/inference and
Lean proofs remain subsequent independent increments.

## Increment 3 / A3: differentiable storage science

The standalone PyTorch rollout and gradient checks are implemented in `science/`.
This increment adds only a private native fixture-export executable and its target
under `engine/`; the existing public native interfaces and shared contracts are unchanged.
See [science/PROGRESS.md](../science/PROGRESS.md) for actual normal/ASan reference
comparisons, gradient errors, pinned environment, commands and limitations.
