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
