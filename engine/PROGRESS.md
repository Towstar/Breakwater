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

## Increment 4 / A4: actual PhysicsNeMo reference comparison

Added only a private native transport fixture exporter and its engine target. It
computes the fixed-case transient eigenmode at three meshes plus a refined time step,
including signed mass ledgers. The actual PhysicsNeMo training, checkpoint round-trip,
normal/ASan reference comparisons and measured errors are recorded in
[science/PROGRESS.md](../science/PROGRESS.md) and
[science/PHYSICSNEMO.md](../science/PHYSICSNEMO.md). Native transport/public interfaces
and the volume-feasibility path are unchanged.

## Increment 5 / A5: measured candidate performance (November 7, 2026)

Implemented on `codex/person-a-engine` after A4 (`0e3187a`), with changes only under
`engine/`. Person B's files, root setup/solution and shared contracts are untouched.

Delivered:

- `breakwater_benchmark` and an engine-owned Release runner. The default matrix
  executes 100, 1,000 and 10,000 distinct candidates on the canonical incident and a
  custom 24-tank/60-internal-edge graph with shared destinations and synthetic costs.
- Extracted existing production sampled and affine-screen helpers into private
  headers, retaining operation order and public interfaces. Benchmark code calls the
  actual helpers used by simulation/search; no substitute algorithm is timed.
- Separate dense/sparse sampled kernels, affine screening, retained independent
  validation and complete public-search timings, with explicit setup/allocation scope.
- Reusable numeric/trajectory buffers, observable full-field checksums, two warmups,
  calibrated batches, seven rotated repetitions and raw timing distributions.
- Versioned raw results and hardware/source/binary evidence under benchmarks/results/;
  equations, methodology, commands, table and limitations are in BENCHMARKS.md.

Actual verification:

1. Optimized MSVC Release build and **7/7 CTest programs passed** (existing engine
   suites/examples plus benchmark correctness on both 100-candidate graphs).
2. Separate checked build and **7/7 MSVC ASan tests passed**, without reported
   sanitizer errors. Checked binaries refuse performance mode; these timings are
   not included in the performance result.
3. `engine/benchmarks/run.ps1` completed the entire six-case matrix. Every candidate
   was dense-reference simulated and independently validated outside timers; kernel
   trajectories/endpoints, no feasible false negatives and exact best-five ranking
   passed. Maximum forward difference was approximately 1.14e-12 m3.
4. Canonical feasible counts were 5/41/381; custom counts 23/221/2,191 at the three
   sizes. Screen counts matched; each public search independently checked and retained
   five finalists with zero reference rejections. The correctness pass performs
   22,200 candidate checks across six batches; the nested prefixes reuse earlier rows.

Measured on AMD Ryzen 7 7730U / 16 reported logical processors, Windows x64 build 26300,
MSVC 19.51.36260.0, CMake 4.3.1 / Ninja. Build commands show `/O2 /Ob2 /DNDEBUG`,
`/fp:precise` and C++20. Measurements are serial and exclude sanitizer instrumentation.
Started 2026-11-07 22:46:00 UTC; completed 22:46:24 UTC. This is an observed machine
sample, not a timing guarantee or a benchmark of rendering/Python/PhysicsNeMo.

| Graph / candidates | Dense sampled ms | Sparse sampled ms | Affine screen ms | Validate 5 ms | Full search ms |
|---|---:|---:|---:|---:|---:|
| Canonical / 100 | .14549 | .14072 | .00680 | .01121 | .14130 |
| Canonical / 1,000 | 1.24415 | 1.08080 | .05488 | .00904 | .49090 |
| Canonical / 10,000 | 18.63820 | 15.53550 | .71158 | .01461 | 7.72950 |
| Custom / 100 | 14.91610 | 1.67358 | .03489 | .12435 | 1.81436 |
| Custom / 1,000 | 119.74160 | 16.59250 | .32001 | .10296 | 2.24113 |
| Custom / 10,000 | 1110.62110 | 143.15390 | 2.63185 | .09170 | 8.25825 |

Values are median milliseconds per complete batch. At 10,000 candidates, same-task
dense/sparse ratios are 1.20 (canonical) and 7.76 (custom). Sparse/affine ratios of
21.83 and 54.39 include eliminating sampled trajectories; they are separate task-cost
comparisons. Components overlap and do not sum to the full-search measurement.

The largest packed numeric candidate input is 18,251,712 bytes with a 24,584-byte
reused trajectory workspace; these are not process peak memory or allocator counts.
Full snapshot/string batches and all-candidate trajectory retention are avoided.
Raw metadata records the A4 parent plus a dirty A5 working tree and hashes of the
actual measured source; documentation/evidence was added after measurement.

Limitations / handoff:

- Mobile CPU power, thermals, background activity and clock jitter are uncontrolled;
  raw min/p90/iteration counts are retained. Results do not justify universal speedup,
  global optimization or a rewrite using SIMD/GPU/parallel kernels.
- Native independent validation remains authoritative. Search recommendations are
  best among their evaluated Halton inputs over three hours; no robustness, quality
  or sustainability guarantee is added.
- B can regenerate the full solution for the additive benchmark target during
  integration. No public contract, numerical equation or approval behavior changed.
- Next Person A milestone is A6, a pinned Lean/Mathlib algorithm-proof project.
