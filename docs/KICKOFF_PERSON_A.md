# Person A: PDE, simulation and scientific validation

Updated November 7, 2026. Person A is the project owner/user and PDE/simulation
developer. Start with the scientific audit below, then fixes and transport
qualification. The audit is **planned**, not a claim of completed review.

## Baseline, ownership and setup

Repository: https://github.com/Towstar/Breakwater

Start from current `origin/main`. At planning baseline `794c7cd`, A3–A8 were merged:
native storage/search/transport, PyTorch storage, fixed-case PhysicsNeMo smoke,
benchmarks, Lean proofs and the scientific worker. Previous evidence records 18
native/desktop suites, 69 Python tests, 21 audited Lean theorems, and 290 worker
requests across 12 native cases. Reproduce or investigate these; they are not your
new audit results.

| You write | Person B writes |
|---|---|
| `engine/`, `science/`, `proofs/`, `scenarios/`, including local tests/CMake/locks | `desktop/`, `contracts/`, root configuration/scripts/solution, README and shared `docs/` |

Review any files, but report B-owned defects to B with reproductions. Put A findings
in owned paths; B aggregates shared status. Keep the numerical library independent
of Qt, Python and model APIs.

Use a separate clone/worktree on `codex/person-a-engine`. Inspect status and preserve
work before updating from main. Never change another session's branch, reset shared
work, force-push or commit feature work directly on main. B integrates small PRs.
Generating Visual Studio projects rewrites B's `Breakwater.slnx`; exclude it from
A's commits. SDKs, `.tools/`, `.venv/` and `out/` are machine-local, not supplied by Git.

## Read first

- [AGENTS.md](../AGENTS.md), [handoff Part A](../For%20Dev%20Agents.md),
  [accepted plan](IMPLEMENTATION_PLAN.md) and [status](STATUS.md).
- [Parallel ownership](PARALLEL_WORK.md) and [Person B's handoff](KICKOFF_PERSON_B.md).
- [Search](../engine/SEARCH.md), [transport](../engine/TRANSPORT.md),
  [control influence](../engine/CONTROL_INFLUENCE.md), [benchmarks](../engine/BENCHMARKS.md).
- [Science](../science/README.md), [worker](../science/WORKER.md),
  [PhysicsNeMo](../science/PHYSICSNEMO.md), [proof scope](../proofs/README.md).
- [Scientific contract](SCIENCE_CONTRACTS.md), `contracts/science/`, native public
  headers and corresponding source/tests. Private fixtures are not wire JSON.

Preserve the original handoff and scenario constants. The accepted desktop plan
governs architecture; there is no fixed event deadline.

## 1. Audit before new features

Freeze the audited commit and record compiler/runtime versions. Map requirements to
implementation and evidence. Coordinate coverage with B so the whole application is
reviewed, including the scientific meaning of B's captures, plots and identities.

| Area | Review and reproduce |
|---|---|
| Storage | Units, constant-flow equations, graph compilation, source feeds, shared throughput, control bounds/availability and conservation. Permit capacity equality; retain finite negative/overflow predictions. |
| Scenarios | Failed-main Tank A limit near **0.76923 h**, bad-reroute Tank B limit near **2.66667 h**, mixed-plan final **80/117 m3** and **12,780 synthetic units**. Preserve healthy main/backup **44/16 m3/h**, identifying temporary accumulation. |
| Equivalence | Dense/sparse/affine/PyTorch agreement on canonical, renamed/custom graphs, nondividing grids, zero controls and numerical extremes. Compare the original requested horizon/step with actual timestamps, including one-ULP short endpoints and duplicate-final-time cases. Inspect independence of the reference validator. |
| Search/A7 | Bounded enumeration, unchanged omitted controls, feasible-candidate preservation, floating-point fallback, canonical survivor costs and ties. Separate screen and full-search ratios; best evaluated is not globally optimal. |
| FVM | Cell averages, face fluxes, half-cell inlet, outlet/closed boundaries, schedule events, boundary-aware CFL, source/decay, litre conversion and signed inlet exchange. Match every ledger term to the update. |
| A8 | Project/native units, whole IDs, selected-interior derivative domains, exact horizon/indices, 1 MiB framing/output, runtime failures and process verification. Echoed identity does not authenticate a native run. |
| Lean | All 21 statements, assumptions/dependencies, finite indices and C++ mapping. Exact-real proofs do not certify IEEE guards, memory safety, full feasibility, ranking or physical validity. |
| ML/evidence | Actual model/residual/training/checkpoint/reference smoke, declared thresholds, held-out data, dependency pins, hashes and historical claims. A4 remains fixed-case experimental evidence. |

Retain failures. Missing runtimes or skipped checks remain unavailable, not passed.
Identify documentation drift and send B precise corrections.
Investigate time-grid edge cases jointly with B: distinguish a confirmed defect
from an explicit unsupported-grid rejection. Never silently shorten the horizon,
shift requested samples or clip values to make native/worker identities agree.

### Reproduction commands

Install/configure MSVC x64, pinned Python and Lean through the linked instructions.
From the repository root, capture actual outputs:

```powershell
.\scripts\build.ps1 -Preset headless
.\scripts\build.ps1 -Preset asan
uv pip install --python science/.venv/Scripts/python.exe --require-hashes -r science/requirements-worker-tests-win-py312.txt
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/verify_rollout.py --native out/headless/engine/breakwater_rollout_reference.exe --report out/audit-rollout.json
science/.venv/Scripts/python.exe science/verify_worker.py --native out/headless/engine/breakwater_rollout_reference.exe --report out/audit-worker.json
science/.venv/Scripts/python.exe science/physicsnemo_smoke.py --native out/headless/engine/breakwater_physicsnemo_reference.exe --output out/audit-physicsnemo
.\proofs\verify.ps1 -LeanBin 'C:\path\to\lean-4.35.0-windows\bin'
.\engine\benchmarks\run.ps1 -Influence
.\engine\benchmarks\run.ps1 -Influence -SkipBuild -Count 2048
```

Use the documented A4 environment for PhysicsNeMo. Checked native executions need
an MSVC-initialized session for the ASan runtime. Benchmark separately in optimized
precise-floating-point builds on an otherwise idle machine; Debug/ASan timings are
not performance evidence. B handles the full desktop build and installed-app checks.

### Audit deliverable and gate

Write `science/AUDIT_PERSON_A.md`: coverage matrix, severity, file/evidence reference,
expected/observed behavior, reproduction, owner, proposed fix and acceptance test.
Label inherited evidence. B aggregates both reports into `docs/AUDIT_REPORT.md`.

Fix P0/P1 numerical, integrity, stale-result and approval blockers before dependent
features. Assign owners and dispositions to remaining findings. Passing test counts
alone does not close the audit. Submit fixes in small A-owned PRs; request B-owned
changes separately. B reviews open PR #13's native historical-evidence work; do not
assume it is merged or implement its UI in A's subtree.

## 2. A9: systematic transport qualification

Expand only audit-identified gaps. Existing transport tests and A4 already cover
several conservation/refinement cases; inventory them before adding work.

1. Define a bounded matrix of advection-dominated, diffusion-dominated, mixed,
   reaction/source and schedule/boundary regimes.
2. Use analytic/manufactured solutions with consistent initial cell averages,
   physical-unit error norms, and separate spatial and temporal refinement.
3. Predeclare expected convergence, thresholds and tested ranges. Account for
   nonsmooth data where smooth-solution convergence rates do not apply.
4. Measure signed mass residuals, stability/extrema, event alignment and work limits.
   Preserve field/error evidence; never clip or loosen CFL to pass a budget.
5. Deliver a reproducible command, compact report and supported/unsupported-regime
   notes under A-owned paths, including reference uncertainty.

**Acceptance:** independent reference/analytic comparisons meet the declared gates
and limitations are explicit. This qualifies the numerical reference, not industrial
physical fidelity. A9 can run alongside B's deployment/client/UI work without a v1
contract change. B publishes any necessary shared extension before A adopts it.

## 3. Optional A10: formal FVM guarantees

After the audited discretization is stable, consider internal face-flux cancellation
and full discrete boundary/source/decay mass-balance proofs. Then consider positivity
under explicit nonnegative inputs and the actual boundary-aware timestep condition,
including the half-cell inlet. Map the implemented update/ledger, audit assumptions,
keep dependencies pinned, and run Lean plus the assumption-rejection checks.
The current sufficient Dirichlet condition is
`dt * (v/dx + 3D/dx^2 + k) <= cfl_safety`; closed no-flux mode uses
`dt * (2D/dx^2 + k) <= cfl_safety` with zero flow. Verify these against source.

These are exact-arithmetic algorithm guarantees, not PDE-solution, floating-point
or compiled-memory-safety certification. Do not repeat the delivered storage proofs.

## 4. ML decisions and joint acceptance

Broader PhysicsNeMo work needs audit priority and a qualified reference. First define
supported forcing/parameter families, independently held-out parameter cases,
predeclared errors and out-of-domain behavior. Coordinate a B-owned learned-result
contract: the native-FVM envelope cannot label learned predictions as native results.
Keep quality separate from volume approval.

Help B verify installed worker/native parity, derivative units, canonical/custom
cases and scientific presentation. A supplies numerical oracles; B owns packaging,
UI, Nemotron, persistence and integration. Further SIMD/GPU, parallel search or broad
hydraulics requires an audit/profiling-based justification.

## Delivery and starting instruction

Each increment reports branch/commit, paths, methods/units, exact commands, versions,
results, limitations and an executable example. Exclude generated binaries,
environments, checkpoints and credentials. B regenerates the solution and shared status.

```text
Continue BREAKWATER as Person A using docs/KICKOFF_PERSON_A.md.
Start with the scientific audit on current main, coordinate coverage with Person B,
and deliver science/AUDIT_PERSON_A.md before new features. Preserve A3-A8 and the
ownership/contracts. Fix audited blockers in small A-owned PRs, then implement the
agreed A9 qualification gaps. Do not modify B's files or claim unexecuted checks.
```
