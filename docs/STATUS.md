# Implementation status

Scope: the shared base plus Person B's native agent/run-store increment, following
KICKOFF_PERSON_B.md at `bf90e55`. The accepted plan and original handoff are preserved.

## Person B desktop foundation — November 7, 2026

PRs #1 and #2 were reviewed and merged with merge commits. The new foundation work
starts from `main` commit `630e33c`; Person A's native/scientific files are unchanged.

Implemented:

- Automatic SQLite investigation history with a dedicated worker-owned connection,
  transactional complete checkpoints, out-of-order checkpoint rejection, interrupted
  session recovery, atomic JSON export and validated historical-only import.
- Saved immutable snapshots, complete native trajectories/reports, objectives, events,
  sanitized model request metadata, recommendations and operator approval audit.
- History browsing, baseline/candidate selection, cost/control/margin/violation
  comparison, tank selection and actual trajectory charts including capacity boundaries
  and unclipped violations. Selection does not change simulated operating controls.
- Explicit staged-plan review showing IDs, native evidence and current→proposed
  controls. Historical evidence or a different selected/staged/recommended run cannot
  be applied; existing revision/reset/validation/double-application gates remain.
- An asynchronous Python-worker health adapter for the existing NDJSON-v1 protocol,
  with identity/schema checks, bounded output, timeout/cancel/crash handling, local
  interpreter configuration and accurate unavailable/not-implemented states.
- Sanitized request completion metadata before terminal events, including cancellation
  and deadlines. Final checkpoint capture/serialization is queued off the UI thread.
- The project-v1 and numerical engine interfaces remain unchanged. Imported evidence
  is never inserted into an authoritative live model gateway.

Executed evidence:

- Current desktop/Ninja build: **12/12 CTest suites passed**.
- Full desktop MSVC AddressSanitizer build: **12/12 suites passed**, no sanitizer
  failures reported. This includes persistence, controller/history, worker, agent,
  numerical search and QML startup/render checks.
- The configured real Python 3.12 worker suite reports **24 passed, 0 failed, 0 skipped**.
  It executes the existing worker's actual health operation as well as bounded failure
  fixtures; no PyTorch/PhysicsNeMo model execution is claimed.
- Isolated SQLite checks report **13 passed, 0 failed, 0 skipped**, including worker-
  thread capture, immediate shutdown draining, restart recovery and rollback.
- Isolated transport checks report **28 passed, 0 failed** with fake HTTP responses.
- Comparison tests can export `offline-verified-session.json` when
  `BREAKWATER_TEST_ARTIFACT_DIR` is configured. Its numbers are actual native baseline
  and mixed-candidate results; the archive explicitly states no live model ran.

Live milestone: **pending credentials**. `NVIDIA_API_KEY` was absent from the process,
user and machine environments. No authenticated hosted request or live model rehearsal
was performed. Protocol smoke, three canonical rehearsals and edited/custom-network
live investigations remain required before claiming the live demonstration complete.
See [DESKTOP_FOUNDATION.md](DESKTOP_FOUNDATION.md) for workflow, storage, worker setup,
archive semantics and verification commands.

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
- Streaming, HTTP 202 polling, robust optimization and animated replay remain later
  work. The foundation above now provides durable evidence history and trajectory charts.
- Quality, scientific ML and proofs remain separate and unavailable as described below.

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

- Animated replay, robust optimization and additional chart types.
- Transport reference, Torch differentiation,
  PhysicsNeMo model/training/inference, and ML readiness evidence.
- Lean/Mathlib project and checked algorithm proofs.
- Performance benchmarks, custom SIMD/GPU kernels, and sophisticated routing handles.

The UI labels unavailable science capabilities as planned. No live model execution, completed proof,
validated chemical quality, measured speedup, or production control is claimed.
