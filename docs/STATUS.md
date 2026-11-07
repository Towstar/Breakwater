# Implementation status

Scope: shared base, native search, Person B's agent/run store and the single-pipe
transport reference merged through PRs #1–#4. The accepted plan and original handoff
are preserved. Next work is ordered in [CONTINUATION_PLAN.md](CONTINUATION_PLAN.md);
the kickoff prompts now target unfinished increments.

## B4 native selected-pipe viewer — November 7, 2026

- Select a route and captured manual/investigation run, edit physical pipe inputs,
  and execute Person A's native scalar transport outside the UI thread. Flow resolves
  captured controls, source curtailment and pump availability; layout supplies no geometry.
- Added actual outlet traces, sampled time-distance fields, signed mass ledger, CFL,
  diagnostic extrema/residuals and run/snapshot/route/parameter provenance. Bounded
  previews retain native sample indices and endpoints; diagnostics cover the full run.
- Quality edits, numerical changes, reset, route deletion and changed source identity
  retire stale work. Layout moves preserve valid evidence. One native job may finish
  after cancellation, but cannot publish late or overlap a new job. Quality operations
  never apply controls or confer volume approval. Atomic JSON export is capped at 1 MiB.
- Final C++ checks: **17/17 combined suites passed** in direct VS2022 normal and full
  desktop MSVC ASan builds. The new pipe suite reports **10 passed, 0 failed/skipped**,
  covering direct native field/ledger parity, captured flows, candidate overrides,
  invalidation, lifetime/cancellation, bounds and atomic export. Independent scoped
  review found no actionable issue. Final QML render checks passed in both builds.
- Interactive native-only QA passed at 1440×900 and 1100×700: actual plots/ledger,
  physical length 100→120 m invalidating/rerunning quality while preserving volume
  approval, resizing and layout dragging preserving both forecast and native result.
  Only pre-existing Shortcut warnings appeared. The user's open app was preserved.
- UI export pickers opened, but computer-use tooling rejected filename input in owned
  pickers; no UI file write or UI saved-project round trip was verified. The native
  export, parameter persistence and cancellation paths passed automated checks. A GUI
  in-flight cancel was too fast to exercise manually. No additional hosted calls ran.
- The root solution now has 65 projects, 177 dependency references and no missing paths.
  Usage and scientific limits are in `docs/PIPE_TRANSPORT.md`. Worker methods beyond
  health remain reserved pending A's mapping; generic PhysicsNeMo route inference,
  checked Lean proofs and clean live-model rehearsals are not claimed.

## Person A merge and B3 science boundary — November 7, 2026

- Merged `codex/person-a-engine` at `eca890d` into this workstream with both parents
  preserved and no conflicts. Public numerical interfaces and worker health-v1 remain
  unchanged. A's source adds standalone CPU-double rollout/gradient checks, the fixed-case
  PhysicsNeMo smoke, native reference exporters and measured benchmark evidence.
- Independent scoped reviews found no integration blocker. The merged baseline passed
  **7/7 native suites** and **15/15 combined desktop ASan suites**, including the new
  benchmark correctness check. No performance measurements were repeated here.
- A's scientific training/parity results remain A-reported in `science/PROGRESS.md`.
  This laptop's bundled interpreter has no Torch/PhysicsNeMo; only the two dependency-
  free worker tests were rerun locally. The worker still implements health only.
- B3 publishes explicit units, identity, array ordering, signed mass ledger, bounded
  previews and safe reserved artifact rules. All three Draft 2020-12 schemas and all
  seven illustrative examples passed `jsonschema` 4.25.1 validation. These are proposed
  scientific methods for A's review/adoption, not advertised worker capabilities.
- Added strict native pipe-parameter parsing/hashing and optional project-v1 route
  profiles. Geometry is independent of layout; quality edits preserve otherwise eligible
  volume runs/approval. Route deletion/undo, malformed profiles and old-project round trips
  are covered. Projects without profiles keep their previous serialized shape.
- B3 combined checks: **16/16 suites passed** in both the direct VS2022 normal build
  and full desktop MSVC ASan build. Regenerated the root solution: 63 projects,
  170 dependency references and no missing project paths. Native pipe UI integration
  is the next B4 increment; A4's fixed-case model is not generic route inference.

## Live Nemotron verification follow-up — November 7, 2026

- A locally configured Windows user-environment key authenticated successfully. The
  existing protocol smoke verified the exact configured model identity, a genuine
  `get_network_state` call, native execution, matching result continuation and completion.
- A production CLI investigation selected a native-validated 4,800-unit run and staged
  it for operator review. No simulated controls were applied by verification tools.
- The already-running desktop investigation exposed repeated calls after its candidate
  quota was exhausted, followed by an invalid final response. Its evidence was retained
  and approval stayed disabled. The orchestrator now supplies remaining budgets on each
  request, removes exhausted candidate tools and offers no tools after the final call.
  Backend limits and strict validation/final-output checks remain authoritative.
- Three subsequent canonical production-controller sessions archived full trajectories
  and request metadata: one returned `no_validated_plan`; one selected a validated
  5,280-unit plan and staged it; one selected a validated 16,530-unit advisory plan
  without staging. A fourth session on a seven-asset, three-tank network with renamed
  IDs and a bypass redirected to a new buffer selected and staged a validated
  5,280-unit plan. These are selected evaluated plans, not global optima.
- All archived requests in those four sessions returned HTTP 200 with matching model
  identity and zero retries. Archives exclude credential/header/reasoning fields.
  A verification-only harness under ignored `.tools/live-check/` reused the actual
  AgentController, history recorder, native tools and repository; it did not mock the
  model or apply controls. It reloaded stored terminal evidence before JSON export.
- New budget regressions and existing orchestration checks: **17 passed**; normal
  history/controller checks also passed. Full desktop ASan: **14/14 suites passed**.
  Direct Visual Studio `/m:1` build and **14/14 suites passed**. The attempted parallel
  rebuild stalled and was cancelled before the serial verification; no parallel-build
  success is claimed for this follow-up.
  An updated normal desktop was built separately, preserving the user's open app.
- Hosted access is now verified. Three clean canonical operator-review rehearsals
  are **not complete**: bounded no-plan and unstaged advisory results remain explicit.
  Planning quality is variable; the demonstration still needs rehearsal. Scientific
  PyTorch/PhysicsNeMo execution remains unavailable.

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

## Person B desktop foundation — November 7, 2026

PRs #1 and #2 were reviewed and merged with merge commits. The new foundation work
starts from `main` commit `630e33c` and merges the later changes through `bf8beb1`,
preserving transport, client safeguards, long control IDs and Qt build ordering.
This increment adds no edits to Person A's native/scientific files.

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

- Current combined desktop/Ninja build: **14/14 CTest suites passed**.
- Regenerated the root Visual Studio solution with 58 projects and all 155 existing
  dependency edges preserved. Direct VS2022 solution build and **14/14 suites passed**.
- Full combined desktop MSVC AddressSanitizer build: **14/14 suites passed**, no sanitizer
  failures reported. This includes persistence, controller/history, worker, agent,
  numerical search and QML startup/render checks.
- The configured real Python 3.12 worker suite reports **24 passed, 0 failed, 0 skipped**.
  It executes the existing worker's actual health operation as well as bounded failure
  fixtures; no PyTorch/PhysicsNeMo model execution is claimed.
- SQLite checks report **14 passed, 0 failed, 0 skipped**, including worker-thread
  capture, immediate shutdown draining, restart recovery, rollback and long control IDs.
- Combined transport-client checks report **30 passed, 0 failed** with fake HTTP responses.
- Interactive checks at 1440×900 and 1100×700 exercised all four tabs, actual tank A/B
  trajectories, control comparisons, historical provenance and disabled approval.
  QML lint passed for the investigator and chart. Qt's pre-existing Shortcut warnings
  remain; no QML runtime errors were observed.
- The CMake-installed application passed render/import/health smoke with Windows-only
  PATH and explicit Python. It loaded the deployed Qt SQL/SQLite plugins and the
  installed worker, returning real Python 3.12.14 health with scientific execution
  `not_implemented`. This checks the local install, not a fresh-machine VC runtime.
- Restarted that installed application with its existing database and no import
  argument, reopened the saved session through History, and recovered both actual
  runs, trajectories, cost and headroom. Historical operator approval stayed disabled.
- Comparison tests can export `offline-verified-session.json` when
  `BREAKWATER_TEST_ARTIFACT_DIR` is configured. Its numbers are actual native baseline
  and mixed-candidate results; the archive explicitly states no live model ran.

At original foundation delivery, the live milestone was **pending credentials**.
`NVIDIA_API_KEY` was absent from the process,
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
- Selected-pipe transport UI, Torch differentiation,
  PhysicsNeMo model/training/inference, and ML readiness evidence.
- Lean/Mathlib project and checked algorithm proofs.
- Performance benchmarks, custom SIMD/GPU kernels, and sophisticated routing handles.

The UI labels unavailable science capabilities as planned. No live model execution, completed proof,
validated chemical quality, measured speedup, or production control is claimed.

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
