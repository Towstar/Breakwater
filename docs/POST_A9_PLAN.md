# BREAKWATER: integration record and next changes after A9

Updated November 7, 2026. This is the active continuation plan after merging
[PR #17](https://github.com/Towstar/Breakwater/pull/17) and
[PR #20](https://github.com/Towstar/Breakwater/pull/20). The full
[implementation plan](IMPLEMENTATION_PLAN.md) and original handoff remain intact.
The work below is planned unless explicitly identified as delivered.

## Integrated baseline

- PR #17 merged at `0d78ffa1b35a6f4dc14aa770a10e9bee645da4b7`:
  A-001 exact forecast horizon/grid, A-002 independent finite-cost validation,
  A-003 constant/linear PDE residual derivatives, and A-004 worker status corrections.
  The [Person A audit](../science/AUDIT_PERSON_A.md) records all four dispositions;
  its original frozen-baseline findings remain historical evidence.
- PR #20 was retargeted from `codex/person-a-engine` to `main` after #17, then
  merged at `696670769ae702dac1fa6da1698d768c11bb7fac`. A9 supplies independent
  transport references, bounded regime/refinement studies and reproducible gates.
  Neither merge required conflict resolutions.
- Local `main` was fast-forwarded to the combined baseline. Credential-support
  commit `dc348d824b688479b61c7a7ba8ee868f9c9bdba8` remains an ancestor.
  Its application, configuration scripts, README and `.env.example` are unchanged
  by these merges. Real `.env` files remain ignored and untracked.
- The root Visual Studio 2026 x64 solution is regenerated to include the new private
  transport qualification exporter. The numerical library remains Qt-independent.

## Fresh integrated verification

Checks use integrated root source at `6966707`, MSVC 19.51 x64/C++20, Qt 6.12.0,
CPython 3.12.13, Torch 2.15.0+cpu and PhysicsNeMo 2.2.2. The existing pinned Python
environment at `.worktrees/person-a-engine/science/.venv/Scripts/python.exe` supplies
packages; all tested scripts/modules are from the integrated root checkout.

| Fresh check | Result | Local evidence under `out/` |
|---|---|---|
| Visual Studio 2026 RelWithDebInfo build and combined native/desktop CTest | Build passed; 18/18 suites, 26.14 s | `post-a9/vs-generate.log`, `vs-build.log`, `vs-tests.log` |
| Direct root `Breakwater.slnx` MSBuild | Passed, zero errors/warnings, 14.46 s | `post-a9/root-solution-build.log` |
| Separate full-desktop MSVC AddressSanitizer build/CTest | Build passed; 18/18 suites, 88.20 s; no sanitizer report | `post-a9/asan-build.log`, `asan-tests.log` |
| Full Python science suite | 127/127 passed, no skips, 81.358 s | `post-a9/python-tests.log` |
| Native/PyTorch rollout | 18 cases across three native solvers; 44 selected controls, three FD steps each | `post-a9/rollout.json` |
| Actual worker/native comparison | 302 NDJSON requests, 132 central differences; offline schemas passed | `post-a9/worker.json` |
| Actual PhysicsNeMo model/training/reference/checkpoint smoke | 12/12 gates; 2,241 parameters, 1,800 updates; checkpoint difference zero | `post-a9/physicsnemo/report.json` |
| Refined native transport qualification | 45 cases / 580 gates passed | `post-a9/transport/report.json` |
| Refined MSVC AddressSanitizer transport qualification | 45 cases / 580 gates passed; no sanitizer report | `post-a9/transport-asan/report.json` |
| Retained original coarse transport study | Expected exit 1; exactly one of 580 gates fails, scheduled-inlet refinement order | `post-a9/transport-coarse/report.json` |
| Lean build and assumption verification | 1,061 build jobs; 21 theorem audits passed; unapproved-assumption probe rejected | `a6-proofs/verification.json` and audit/build logs |

Maximum native/PyTorch/worker volume difference is `7.67386154621e-13 m3`.
PhysicsNeMo held-out field RMSE is `0.00336886618 mg/L`, residual RMS is
`0.0438580470 mg/L/h`; this remains a fixed-case experimental smoke. The normal
desktop worker and Nemotron-client suites execute 24 and 37 cases respectively,
with zero failures/skips. No new hosted model request or performance benchmark is
claimed. Lean remains pinned to 4.35.0 and Mathlib revision
`56a005855ea17e0b0958f73ec801ae140b0a1679`.

The regenerated root solution has 67 projects and 183 build dependencies, zero
missing/dangling paths, and the desktop application as its default startup project.
Generated reports retain actual source/configuration/executable hashes. Local
outputs and runtimes are ignored and are not distributed by Git.

Reproduction commands, in an x64 Visual Studio Developer PowerShell with the pinned
dependencies available (`$sciencePython` is your scientific interpreter and
`$qtPrefix` is the Qt 6.12.0 MSVC SDK):

```powershell
$env:BREAKWATER_PYTHON = $sciencePython
./scripts/Generate-VisualStudio.ps1 -QtPrefix $qtPrefix -EmbeddedDebugInfo
MSBuild.exe Breakwater.slnx /p:Configuration=RelWithDebInfo /p:Platform=x64 /m:1
ctest --test-dir out/visual-studio -C RelWithDebInfo --output-on-failure
& $sciencePython -m unittest discover -s science/tests -v
& $sciencePython science/verify_rollout.py --native out/visual-studio/engine/RelWithDebInfo/breakwater_rollout_reference.exe --report out/recheck/rollout.json
& $sciencePython science/verify_worker.py --native out/visual-studio/engine/RelWithDebInfo/breakwater_rollout_reference.exe --report out/recheck/worker.json
& $sciencePython science/physicsnemo_smoke.py --native out/visual-studio/engine/RelWithDebInfo/breakwater_physicsnemo_reference.exe --output out/recheck/physicsnemo
& $sciencePython science/qualify_transport.py --native out/visual-studio/engine/RelWithDebInfo/breakwater_transport_qualification_reference.exe --config science/a9_transport_refined_config.json --output out/recheck/transport
./proofs/verify.ps1 -LeanBin C:/path/to/lean-4.35.0-windows/bin
```

The checked run uses a separate build directory and initialized MSVC runtime path:

```powershell
cmake -S . -B out/stack-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBREAKWATER_BUILD_DESKTOP=ON -DBREAKWATER_ENABLE_ASAN=ON -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded "-DCMAKE_PREFIX_PATH=$qtPrefix" "-DPython3_EXECUTABLE=$sciencePython"
cmake --build out/stack-asan --parallel 2
ctest --test-dir out/stack-asan --output-on-failure
& $sciencePython science/qualify_transport.py --native out/stack-asan/engine/breakwater_transport_qualification_reference.exe --config science/a9_transport_refined_config.json --output out/recheck/transport-asan
```

Set `$qtPrefix` to the Qt 6.12.0 MSVC SDK. This run used
`.tools/Qt/6.12.0/msvc2022_64`; sanitizer elapsed times are not benchmarks.

Check each exit status. The original coarse config intentionally returns 1; the
refined command above must return 0. Do not substitute the retained coarse failure
for a passing release check or change its gates to make it pass.

## 1. Review the remaining integration boundary before new features

**Owner: Person B, with Person A reviewing numerical meaning.** Start from this
merged baseline, inspect existing audit reports and complete the B-owned audit and
shared finding dispositions. A's four reported defects are resolved; this does not
close an audit of the entire desktop application. Preserve original failed evidence.

Review the existing B stack instead of rebuilding its features:

| Future order | Existing PR | Delivered on its review branch | Integration acceptance |
|---|---|---|---|
| 1 | [#13](https://github.com/Towstar/Breakwater/pull/13), `ac22a1c` | Historical native-pipe evidence reopening | Resolve conflicts against current main; reopened evidence remains historical and cannot restore approval. |
| 2 | [#18](https://github.com/Towstar/Breakwater/pull/18), `9525c25` | Installed worker/package and pinned CPU runtime setup | Preserve `storage_grid.py` and the complete mapper/package; test a relocated installation with checkout sources unavailable. |
| 3 | [#19](https://github.com/Towstar/Breakwater/pull/19), `9f8f550` | Typed asynchronous native/PyTorch forward comparison | Compare actual canonical and renamed/custom networks with original horizon, step, full native grid and stable IDs. |

These PRs were open when this plan was prepared. Their integration is a next task;
this change merges only #17 and #20. Recheck heads, bases and diffs before merging.
Retarget each stacked PR only after its predecessor lands. Preserve the dotenv and
in-app credential setup while reconciling shared build, solution and documentation.

Required exit checks:

- Reproduce A-001's one-ULP terminal tail and actual update-count bound through the
  desktop boundary. Valid requests produce strictly increasing timestamps ending
  at the exact horizon once; independently reject malformed reference arrays with
  duplicate timestamps. Never derive a step from a decimated preview or silently
  shorten the original horizon.
- Exercise A-002 product/sum/horizon cost overflow and finite repair candidates.
  Python agreement cannot substitute for independent native feasibility validation.
- In one actual worker process, negotiate capabilities, execute storage, inspect
  updated verification state, and require a clean process exit before publication.
  Restart resets session verification. Missing runtimes remain explicit failures.
- Test cancellation, crashes, wrong IDs/shapes, fabricated and stale runs, numerical
  edits/deletion, historical imports and layout-only movement. Preserve the 1 MiB
  framed-message limit and native-only approval transitions.
- Run normal/full-desktop MSVC AddressSanitizer tests, installed-runtime checks and
  manual save/export/reopen workflows. Record fresh counts, versions and skips;
  existing branch reports are inherited evidence until rerun after integration.

## 2. A10a: prove discrete transport mass balance

**Owner: Person A. Paths: `proofs/`, with mapping notes/tests in A-owned paths.**
This can proceed in parallel with the B stack without changing the public contract.
Use a separate worktree on `codex/person-a-engine` updated from current main after
checking its local status. Keep B's files untouched.

1. Formalize finite cells and signed face fluxes matching the implemented FVM update,
   half-cell inlet, outlet/closed boundary modes, source, decay and litre conversion.
2. Prove cancellation of interior face fluxes and the one-step total tracer inventory
   balance against signed inlet/outlet/source/decay ledger terms.
3. Prove the telescoping balance over variable, event-aligned time steps. Permit
   negative inlet diffusion where the native boundary permits it.
4. Map every mathematical term to `engine/src/transport.cpp` and its ledger tests.
   Keep Lean/Mathlib pins and the transitive assumption policy; add new statements to
   the audit and run `lake build`, the audit and the rejection probe.

**Acceptance:** checked general finite-cell statements with explicit assumptions,
no admitted results, and native regression examples consistent with the mapping.
This proves an exact-arithmetic discrete algorithm identity. It does not establish
PDE accuracy, IEEE floating-point behavior, memory safety, valid optimization plans,
global optimality or physical fidelity. Existing 21 storage/control-influence
theorems remain checked; do not repeat that delivered work.

A separate A10b may prove positivity under nonnegative data and the actual
boundary-aware timestep condition after reviewing the discretization assumptions.
Do not bundle that stronger claim into a mass-balance result.

## 3. Investigate transport fronts before broader quality claims

**Owner: Person A. Paths: `engine/`, `science/` and their tests/reports.**
The [A9 report](../science/TRANSPORT_QUALIFICATION.md) qualifies only its declared
synthetic cases and ranges. The original coarse scheduled-inlet study failed and
remains preserved. The refined study passes its mean-error gates but has maximum
cell-average error about **0.40191 mg/L**; its space/time refinement is coupled at
fixed CFL. Local peaks and threshold-crossing decisions are not qualified.

- Predeclare a separate nonsmooth-front study: mean and maximum field errors,
  outlet errors and arrival/threshold timing, with physical units and tested ranges.
- Separate mesh and time effects where feasible and report reference uncertainty.
  Include flow pauses and schedule events; retain signed mass/work-limit evidence.
- Measure existing numerical diffusion before proposing a different discretization.
  Keep failed configurations and existing gates unchanged. A new method, if justified,
  needs its own comparison and compatibility decision.

**Acceptance:** reproducible evidence states exactly which front metrics/ranges pass
or fail. Do not infer peak accuracy from mass conservation or mean-L1 convergence.
No generic quality approval or hydraulic model is introduced by this study.

## 4. B5c: selected-control derivatives and durable science evidence

**Owner: Person B, with Person A supplying numerical acceptance oracles.** Depends
on the forward path from #19 being integrated and verified.
Deliver derivative presentation and durable evidence in separate reviewed PRs.

- Expose only explicitly selected interior controls, in the requested column order;
  keep omitted controls fixed. Reject fixed/boundary derivative requests explicitly.
- Show terminal-volume Jacobian units (hours) and cost gradients (synthetic units
  per m3/h), method versions, warnings and the advisory domain.
- Verify finite differences on canonical and renamed/custom graphs; independently
  enforce identity, dimensions, horizon and stale-job invalidation.
- Persist bounded versioned scientific evidence with captured controls, identities,
  units and provenance. Reopening remains historical and non-actionable; it cannot
  bypass a fresh native validation and explicit simulated operator approval.

**Acceptance:** real worker requests and fault tests cover ordering, domain errors,
restart/cancellation and persistence. Numerical changes invalidate results; layout
movement preserves eligible numerical evidence. No new A-owned worker method is
needed for the existing selected-control storage derivatives.

## 5. Conditioned PhysicsNeMo work and foundation release

**Person A:** after reference/error gates are agreed, define supported parameter and
forcing families, held-out parameter cases, predeclared error thresholds and explicit
out-of-domain behavior. The existing actual model/training/checkpoint smoke remains
fixed-case experimental evidence, not arbitrary-route inference.

**Person B:** publish a distinct learned-result contract before integrating broader
inference. Complete the outstanding clean operator-review rehearsals on canonical
and custom networks and relocated installed Windows acceptance. Retain actual model
tool calls, validated run IDs and no-plan attempts; do not require a prescribed cost.

**Joint release gate:** reproducible MSVC/Qt/Python setup, current normal/checked,
science and Lean evidence, declared feature status and a working review workflow.
Native volume feasibility and explicit simulated approval remain authoritative.
Parallel/GPU search, broad hydraulics and automatic topology redesign remain outside
these increments unless new profiling or requirements justify a separate plan.

## Parallel ownership and delivery

Person A owns `engine/`, `science/`, `proofs/`, `scenarios/` and local tests/locks.
Person B owns `desktop/`, `contracts/`, root build/scripts/solution and shared docs.
Review across boundaries, but coordinate any shared interface change before editing.
Use separate worktrees and small reviewed PRs; do not reset another session's work
or force-push. Report source commit, exact commands, actual results, limitations and
a runnable example for each increment. Keep credentials, generated fields,
checkpoints, binaries and environments out of commits.
