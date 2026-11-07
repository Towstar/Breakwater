# Implementation status

Scope: shared base, native search, Person B's agent/run store, single-pipe transport,
and Person A's A3–A6 science, benchmark and proof increments, merged through PR #6.
The accepted plan and original handoff are preserved. The execution sequence remains
in [CONTINUATION_PLAN.md](CONTINUATION_PLAN.md); its A3–A6 items are now delivered.

## PR #6 integration — November 7, 2026

- Merged A3–A6 at `fe40c20` without conflicts. Native reference exporters and benchmark
  targets are included in the regenerated root Visual Studio solution.
- Direct Visual Studio 2026/MSVC x64 RelWithDebInfo build of `Breakwater.slnx` passed,
  followed by **12/12 combined CTest suites passed**, including benchmark correctness
  and desktop/QML checks. This normal-build result does not imply a new combined
  sanitizer run.
- The regenerated solution includes 55 projects with valid paths. The merged-main
  Python suite also passed **39/39 tests** in the pinned science environment.
- A3 delivers CPU double-precision PyTorch storage rollout, actual dense/sparse/affine
  native comparisons and finite-difference/autograd checks. Recorded comparison:
  12 cases, 44 varying controls and 132 central-difference checks; maximum all-sample
  volume difference `7.67386154621e-13 m3`. See [science evidence](../science/PROGRESS.md).
- A4 executed an actual PhysicsNeMo 2.2.2 model, PDE residual, bounded 1,800-step
  training, checkpoint round-trip and held-out native transport comparison. The full
  component Python suite passed 39 tests. This is an experimental fixed-case smoke,
  not a qualified chemical-quality predictor. See [PHYSICSNEMO.md](../science/PHYSICSNEMO.md).
- A5 records 100/1,000/10,000-candidate optimized MSVC benchmarks for two network
  shapes, separating dense/sparse sampled kernels, affine screening, retained-run
  validation and full search. Normal and MSVC ASan component checks passed. Timing
  scopes, raw evidence and limitations are in [BENCHMARKS.md](../engine/BENCHMARKS.md).
- A6 delivers Lean 4.35.0/Mathlib-pinned exact-real algorithm proofs for valid finite
  graphs. Repeated in the merged-main checkout: `lake build` completed all 1,060 jobs,
  the 15-theorem assumption audit passed, and the deliberate unapproved-assumption
  probe was rejected as required. Local verification logs are under ignored
  `out/a6-proofs/`. See [component proof evidence](../proofs/PROGRESS.md) and
  [proof scope](SCIENCE_VALIDATION.md#checked-lean-algorithms).
- The health-only worker is unchanged. Desktop science contracts, process adapter
  and selected-pipe transport UI remain separate integration work.

## Person A transport integration — November 7, 2026

- PR #4 merged the pure C++20 finite-volume scalar transport API, example/CSV export,
  numerical documentation and 18 transport case groups. See engine/TRANSPORT.md and
  engine/PROGRESS.md for equations, limitations and original executed evidence.
- Headless normal and MSVC ASan builds passed six CTest programs: 47 baseline/search/
  transport case groups and three executable examples. The post-PR #2 integration
  headless run also passed. Transport remains separate from volume feasibility.
- Example: 64 cells, 41 frames, 136 substeps, maximum CFL 0.9 and maximum signed mass
  residual 1.36887055523e-12 mg. No model training or quality qualification is claimed.
- Desktop selected-pipe UI, physical-parameter persistence and worker integration are
  planned next. The Python worker still supports health only.
- Regenerated the full Visual Studio 2026 solution after transport integration:
  52 projects and 134 dependency references, with no missing project paths. A fresh
  parallel build exposed Qt 6.12's QML statistics aggregation preceding its input
  generation; a deferred executable dependency now orders that target correctly.
- After MSBuild Clean (with generated QML statistics confirmed absent), direct parallel
  root `.slnx` MSVC 19.51 / Qt 6.12.0 build passed, followed by **11/11 combined CTest
  suites passed**. This is a new normal-build result; prior component ASan evidence
  above does not imply a new combined 11-suite sanitizer run.

## Person B agent increment — November 7, 2026

Implemented:

- Asynchronous Qt Network Nemotron client with process-environment configuration,
  non-streaming tool calls, matching tool-result continuation, retries, deadlines,
  cancellation, bounds, redirect rejection and redacted errors.
- Eight allowlisted tools backed by the existing engine, immutable snapshot/run IDs,
  a 128-run session store, full retained trajectories, explicit independent validation,
  candidate comparison and approval staging. Existing native contracts are unchanged.
- Bounded adaptive orchestration: 12 calls, three candidate-refinement/search rounds,
  two minutes. Model-generated numbers cannot establish feasibility or create runs.
- A Qt investigator panel with activity, historical run summaries, grounded result
  fields and an explicit operator action to apply a staged simulated plan.
- Numerical edits/reset invalidate sessions and approvals; layout-only moves preserve
  them. Completed evidence survives cancellation/failure within the current session.
- Protocol/full-investigation live smoke executable and integration guide with schemas:
  [AGENT_INTEGRATION.md](AGENT_INTEGRATION.md).

Actual checks on Person B's laptop:

- Qt 6.12.0 pinned SDK, MSVC 19.44.35219.0 / VS2022 17.14.19, CMake 3.31.6 / Ninja.
- Native baseline: engine acceptance passed; baseline/bad/mixed/healthy CLI cases
  reproduce 0.769230769231 h, 2.66666666667 h, mixed 80/117 m3 and cost 12,780.
- Desktop RelWithDebInfo build and all six CTest suites passed: engine, project,
  fake-HTTP transport, real-engine gateway, fake-model orchestration and QML render.
- Full desktop AddressSanitizer build and the same six CTest suites passed with no
  reported sanitizer failures. The render smoke now waits for an actual rendered
  frame within 20 seconds instead of assuming readiness after a fixed delay.
- Agent-panel screenshot captured and visually inspected. Interactive model execution
  was not exercised in the screenshot; automated controller tests use a fake model.
- `breakwater_nemotron_smoke.exe`: exit 2, credentials unavailable, no live request.
- Independent same-family subagent code review identified cancellation/restart signal
  reentrancy; guards and regression cases now cover that issue and custom graph IDs.

Reproduce with `scripts/build.ps1 -Preset desktop -EmbeddedDebugInfo` after setting up
the Qt SDK. This machine also needed a short process-local PATH before importing the
VS developer shell. `/Z7` avoids this installation's C1902 PDB-helper mismatch. Checked
desktop configuration used `BREAKWATER_ENABLE_ASAN=ON`, `BREAKWATER_BUILD_DESKTOP=ON`
and `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` in a separate build directory.

Unavailable/limited:

- Live NVIDIA model access/tool execution is **unverified**; no API key is configured.
  Fake responses test software behavior, not live adaptive model performance.
- Person A's native bounded search (`4eea5a7`) is merged and exposed through
  `search_interventions`; combined post-merge verification is recorded below.
- History is in memory for the current agent session; persistence/replay, streaming,
  HTTP 202 polling, robust optimization and detailed charts remain later work.
- Scientific calculations and proofs are available as standalone components; desktop
  science integration and qualified quality predictions remain unavailable.

## Implemented

### Person A search integrated into Person B

- Merged `codex/person-a-engine` commit `4eea5a7` without conflicts.
- Added the real `search_interventions` tool without changing the existing engine
  interfaces. Search axes, candidate counts, fixed horizon, run storage, native
  reference validation and operator approval gates are enforced in C++.
- Post-merge desktop/Ninja build: **9/9 CTest suites passed**. Full desktop MSVC
  AddressSanitizer build: **9/9 passed**, including the 16-case engine search suite,
  both search examples, gateway search and model/tool orchestration tests.
- The canonical tool test actually evaluates 432 combinations, finds 39 screening
  survivors and stores five independently validated finalists. Lowest evaluated cost
  is 4,800 synthetic units; final A/B volumes 116/117 m3, minimum headroom 4 m3.
  This uses storage headroom and establishes neither global optimality nor indefinite
  sustainability. The restricted search reports no feasible plan among its inputs.
- The fake-model integration test chooses search, receives real stored run IDs and
  native validation, stages the selected result and produces a grounded recommendation.
  This remains distinct from an unexecuted live NVIDIA investigation.
- Regenerated `Breakwater.slnx` with the optional VS2022 generator and existing .NET
  10 migration. The complete generated graph (50 projects, 127 dependency edges)
  is preserved, including all new agent/search targets. Direct single-process MSBuild
  of the root solution passed, followed by **9/9 generated-tree CTest suites passed**.
  VS2026 remains the generation-script default. No generated `.vcxproj` or SDK files
  are committed.

### Shared base

- C++20 / MSVC numerical library, canonical scenario, native CLI and 13 acceptance cases.
- Generic supported topology, control catalog, immutable candidate compilation, dense
  reference, sparse rollout, allocation-free affine endpoints, independent validator.
- Versioned C++/JSON contracts and scientific-worker protocol/health boundary.
- Native Visual Studio project generation and root `Breakwater.slnx`.
- Qt Quick 3D editor: primitives, placement, connections, pump attachment,
  property editing, save/load, undo/redo, background forecasts and approval invalidation.

## Original baseline evidence recorded before this increment

- `scripts/build.ps1 -Preset headless`: native compilation and all 13 engine cases passed.
- `scripts/Generate-VisualStudio.ps1 -Headless -Build`: Visual Studio/MSBuild x64 build
  and engine acceptance suite passed using MSVC 19.51.36260.0 / CMake 4.3.1.
- `scripts/build.ps1 -Preset asan`: MSVC AddressSanitizer build and engine suite passed.
- Python 3.12.13: `python -m unittest discover -s science/tests -v`: 2 tests passed.
- `scripts/Generate-VisualStudio.ps1 -Build`: full native Visual Studio/MSBuild x64
  RelWithDebInfo solution passed all three CTest suites: engine (13 cases), desktop
  project/controller (7 cases), and QML startup/render smoke (including asset labels).
- Direct `MSBuild.exe Breakwater.slnx /p:Configuration=RelWithDebInfo /p:Platform=x64`:
  root solution built successfully. Generated projects explicitly use v145 / stdcpp20.
- The Qt 6.12.0 scene was captured to `out/desktop-smoke.png` and visually inspected.
  Desktop controller behavior is tested; full mouse-gesture end-to-end automation is later work.

Tested runtime: Qt SDK revision `6.12.0-0-202609280346`, MSVC 19.51.36260.0,
CMake 4.3.1, Visual Studio 2026. Dependency downloads/build products are ignored in `.tools/` and `out/`.

## Planned, not implemented in this base

- Durable history/replay, robust optimization and detailed charts.
- Desktop-to-Python process adapter and selected-pipe transport UI.
- Qualified/generalized ML quality predictions and transport differentiation.
- Custom SIMD/GPU kernels and sophisticated routing handles.

The UI's unavailable science integrations remain planned. Actual standalone PhysicsNeMo
execution, checked algorithm proofs and measured native timings are recorded above;
live hosted Nemotron execution, validated chemical quality and production control
remain unverified or outside scope.

## PR #2 integration review (2026-11-07)

- Fixed a final-response reentrancy edge in `NemotronClient::finish`: consuming an
  oversized body may synchronously trigger client deletion or a replacement request.
  Lifetime, generation and reply-identity guards now protect the continuation. Both
  paths have regression tests, including a reply that emits only `finished`.
- Control IDs now have a separate 512-character parser/schema limit, so control names
  derived from valid 256-character imported asset/route IDs work through simulation
  and search. Snapshot/run/candidate ID limits remain 256. A regression round-trips
  the imported project and executes the discovered controls through real tools.
- Independently rebuilt the reviewed PR using MSVC 19.51.36260.0 / Qt 6.12.0:
  desktop RelWithDebInfo **9/9 CTest suites passed**, and the full desktop ASan
  configuration **9/9 passed**, with no reported sanitizer errors.
- Captured and visually inspected the investigator panel. Missing-key smoke returned
  the expected exit code 2 and explicit unavailable message; no live request was made.
  Genuine hosted NVIDIA access/tool calling remains unverified.
