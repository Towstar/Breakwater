# Person A audit after A10a

Audited source: `55454173987c5a75d1bb502bb5bde4910051e2d7`, fetched from
`origin/main` and fast-forwarded into the clean `codex/person-a-engine` worktree
at `C:/Projects/Breakwater/.worktrees/person-a-engine`. Audit date: November 7,
2026 (America/Chicago; execution timestamps can fall on November 8 UTC).
The local continuation plan is deliberately excluded from this worktree's commits.

## Gate and finding dispositions

Source review and the completed runtime checks found **no new A-owned correctness,
identity or approval blocker for A10b. The audit gate is closed for that increment.**
No native algorithm, timestep rule,
contract, application or shared documentation is changed by this audit.

The original [Person A audit](AUDIT_PERSON_A.md) is frozen historical evidence.
Its A-001 through A-004 findings remain resolved; the original failure prose is
not a current open finding. Rechecking the fixes means checking their source and
specific regression behavior, rather than relying on total test counts.

| Finding | Severity / owner / class | Disposition |
|---|---|---|
| A-001, exact storage horizon/grid | Prior P1 / A / implementation | Fix retained: native and Python preserve rounded interior multiples and append the original endpoint once. Actual update count includes a one-ULP tail. Independent validation uses the requested horizon. |
| A-002, finite independent costs | Prior P2 / A / implementation | Fix retained: independent reconstruction rejects nonfinite product/sum/horizon costs and altered finite reported costs; finite lower-control repairs remain available. |
| A-003, degenerate residual derivatives | Prior P2 / A / implementation | Fix retained: constant/linear coordinate derivatives become zero without detaching trainable amplitudes or suppressing genuine backward errors. |
| A-004, worker status | Prior P3 / A / documentation | Corrected chronology retained. Mapper presence, successful current-process execution and desktop deployment are explicitly distinct. |
| PA10-B01, original manual-run configuration absent from capture | P2 / B / deferred integration requirement | Open for the future storage-comparison path; does not block A10b. Source reproduction and acceptance below. No current false comparison or approval demonstrated. |
| PA10-B02, shared proof status omits A10a | P3 / B / documentation drift | Open handoff to B; does not block A10b. |
| PA10-S01, local front accuracy unqualified | Scientific limitation / A / deferred scope | Retain A9's failed coarse configuration and maximum-error limitation. Separate subsequent study; no accuracy claim added here. |

### PA10-B01: preserve original configuration before comparing scientific results

**Source/evidence:** `desktop/ProjectController.hpp`'s `StoredRun` stores result,
validation, ID and revision, but no `SimulationConfig`.
`desktop/ProjectController.cpp:32` returns a manual `capturedRun()` containing
the terminal result timestamp as `horizon_h`, network, controls and identities;
it contains neither original `step_h` nor the native full timestamp grid.
The delivered desktop worker is still health-only on this audited baseline.
`desktop/agent/SimulationGateway.cpp` separately retains `config_`, includes
`sample_step_h` in summaries and offers a full native grid through its run access.
Do not conflate those two capture paths.

**Minimal reproduction:** capture a normal manual forecast and inspect
`ProjectController::capturedRun()` (the existing
`desktop/tests/project_tests.cpp` checks its horizon and identity). Its object has
no sampling step or full grid. Read `StoredRun` to confirm those original inputs
are not retained there. This is source-level reproduction, not a newly executed
Qt acceptance test. The one-ULP fixture in A-001 shows why deriving a request
from sampled output is insufficient once comparisons are exposed.

**Impact/correction:** a future adapter cannot establish that it used the
original manual sampling request from this object alone. B should retain the
original configuration and expose the full native reference grid for the common
comparison path. Do not infer a step from decimated previews. No public contract
change is made by A.

**Acceptance gate:** original horizon/step, stable tank/control identities and full
grid agree through manual and investigation captures, including
`.9000000000000001/.1`, `.30000000000000004/.1`, subnormal horizon, exact work-limit
and one-extra-update rejection. Wrong IDs, duplicate/shifted timestamps, numerical
edits, restarts and historical imports fail publication; layout-only movement
preserves eligible results. Native independent validation and explicit simulated
approval remain authoritative. This gate belongs to B's comparison integration,
not to the exact-real FVM theorem.

### PA10-B02: shared status needs the A10a integration record

**Source/minimal reproduction:** `docs/STATUS.md`'s top audit/A9 section reports
21 checked statements and its scope line ends at A9; `proofs/Breakwater/Audit.lean`
advertises 37 and `proofs/PROGRESS.md` records A10a. The old dated A9 evidence is
valid history, but the current summary lacks A10a.

**Impact/proposed correction:** B should add the A10a/A10b integration status and
link the owned evidence, preserving dated historical counts. Acceptance is a
current-status entry distinguishing implemented exact-real guarantees from
unproved scheduler/IEEE/accuracy properties. Shared files are untouched here.

### PA10-S01: retain the front-error limitation

**Evidence/reproduction:** the unchanged `science/a9_transport_config.json` and
`a9_transport_refined_config.json`, `test_transport_qualification.py` and
[TRANSPORT_QUALIFICATION.md](TRANSPORT_QUALIFICATION.md). The coarse N32/64/128
scheduled-inlet study has one failed order gate (0.20370 < 0.25); refined
N128/256/512 passes mean-L1 gates while maximum cell-average error remains about
0.40191 mg/L. Its space/time refinement is coupled at fixed CFL, unlike the
separated smooth-mode studies. Semidiscrete reference conditioning estimates are
numerical evidence, not interval bounds.

**Impact/next gate:** conservation and conditional nonnegativity do not qualify
peaks or arrival/threshold timing. A later study must predeclare mean/maximum field,
outlet and timing errors with units/ranges and reference uncertainty, separate
space/time effects where feasible, and retain the failures. That study is not run
or implemented in this increment. Prior 45-case/580-gate qualification results are
inherited evidence, not fresh executions of this audit.

## Review matrix and specific acceptance evidence

| Area | Implementation and assumptions reviewed | Regression/evidence |
|---|---|---|
| Storage/grid | `engine/src/storage_grid.hpp`, `engine.cpp`, Python `storage_grid.py`, rollout and worker publication. Positive subnormal endpoints survive quotient underflow; work bounds use actual rounded multiples. | Existing native engine/search and Python rollout/worker regressions check exact endpoints, duplicates, terminal tail, capacity excess of 0.125 m3 and update 10,001 rejection. |
| Search/cost | `search.cpp`, `control_influence.hpp`, `search_kernel.hpp`, independent ID-based native validation. Omitted controls start from captured settings; survivors recompute canonical cost and receive dense validation. No global-optimum claim. | Existing route/influence regressions exercise finite repairs, product/sum/horizon overflow, altered supplied/cached costs, fixed controls, bounds and independently validated survivor ranking. |
| Transport | `transport.cpp`, `transport_tests.cpp`, `TRANSPORT.md`: shared signed faces, half-cell inlet, advective outlet, closed zero flow, old-state source/decay, per-cell litre scaling. Events shorten stable steps; resource limits reject instead of relaxing CFL. | Existing 21-case transport executable includes open/closed independent dyadic examples, old-inventory unequal steps, signed inlet loss, unsafe-inlet request safely split, invalid/extreme inputs and work budgets. |
| Floating point | Finite geometry/flux/update/ledger guards; scheduler permits epsilon margins and records tiny negatives without clipping. | A10b must explicitly stop at exact-real budget <= 1. No proof of compiled nonnegativity, epsilon safety or roundoff bound is inferred. |
| A10a model | `Transport.lean`: finite shared-face telescoping includes `Fin 0`; boundary laws use nonempty fields. Inventory conversion requires `dx != 0`. Generic signed flux/source/decay need no nonnegativity. | All 16 transport propositions and their assumptions inspected; native >=2-cell domain is a strict subset. `rollout` evaluates each flux law on the recursively computed old field; its independent four-term ledger is not an assumed conservation conclusion. |
| Proof infrastructure | `lean-toolchain`, `lakefile.toml`, manifest, root library, audit and wrapper. Lean 4.35.0 / Mathlib `56a005855ea17e0b0958f73ec801ae140b0a1679`; proposition check plus transitive dependency traversal. | All 37 advertised results retained. Wrapper scans owned sources for prohibited tokens, re-executes the audit even when cached and requires rejection for the deliberate unexpected assumption. Foundational allowlist remains `propext`, `Classical.choice`, `Quot.sound`. |
| Existing science | Constant/linear/quadratic residual and actual neural residual-only gradient code/tests; worker envelope, lazy imports, publication and session flags. | Full Python suite includes genuine PhysicsNeMo neural forward/backward/finite differences. No new 1,800-update training/checkpoint smoke is claimed; A-003/A9/PR22 training evidence is historical. |
| Worker execution | `worker.py`, `worker_storage.py`, protocol tests and parity drivers. Presence flag does not establish executable runtime; echoed identity does not authenticate a native run. | Actual process tests cover capabilities before/after execution, restart reset, missing packages, output budget, whole IDs, omitted controls and selected-interior derivatives. Python pipe transport and generic learned inference remain unavailable. |
| Integration boundary | Read-only `ProjectController`, `SimulationGateway`, `PipeTransportController`, history and their tests. Physical hash/revision/generation changes invalidate stale results; layout does not determine physics. | Source review only; no fresh full-desktop/installed-app/hosted-model acceptance is claimed. Pipe parameters bind quality evidence separately from storage approval. PA10-B01 records the pending capture requirement. |

## Fresh commands and results

All commands run in the Person A worktree. Dependencies were reused without
upgrading pins. Local logs/reports are under `out/post-a10a-audit/`; they identify
this frozen-source run separately from PR22's root integration logs.

```powershell
./scripts/build.ps1 -Preset headless -EmbeddedDebugInfo
./scripts/build.ps1 -Preset asan -EmbeddedDebugInfo
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/verify_rollout.py --native out/headless/engine/breakwater_rollout_reference.exe --report out/post-a10a-audit/rollout.json
science/.venv/Scripts/python.exe science/verify_worker.py --native out/headless/engine/breakwater_rollout_reference.exe --report out/post-a10a-audit/worker.json
./proofs/verify.ps1 -LeanBin C:/Projects/Breakwater/.tools/lean-4.35.0-windows/bin
```

| Fresh check | Actual result |
|---|---|
| Normal MSVC native CTest | 8/8 suites passed, 5.50 s |
| MSVC AddressSanitizer native CTest | 8/8 suites passed, 59.55 s; no sanitizer report |
| Python science suite | 127/127 passed, 122.977 s; zero failures/skips |
| Native/Torch parity | 18 actual native cases across 3 solvers; 44 interior controls and 132 central differences passed |
| Actual-worker/native parity | 302 NDJSON requests across the same 18 cases; offline schemas passed; maximum volume difference 7.67386154621e-13 m3 |
| Pinned Lean build/audit/probe | 1,062 jobs passed; fresh proposition/transitive audit passed all 37 results; unexpected-assumption probe rejected for `A6UnexpectedProbe`; verifier exit 0 |

Runtime imports freshly reported CPython 3.12.13, Torch 2.15.0+cpu, NumPy 2.5.4
and PhysicsNeMo 2.2.2. Native builds used Visual Studio 2026 x64 MSVC 19.51.36260.0/C++20 with
precise floating-point semantics. All commands above exited 0. No check was
skipped to turn a failure into a pass. Timings are observations, not benchmarks.
The committed [audit evidence record](evidence/post-a10a-audit/verification.json)
identifies tested source/report hashes; adjacent logs retain actual outputs.

Initial sandboxed Git fetch/merge could not write worktree metadata; the authorized
retry with filesystem escalation succeeded normally. No reset, branch switch or
force push occurred. Some exploratory reads used nonexistent filenames before
locating the actual files; those are discovery errors, not passed checks.

## A10b acceptance boundary

Only after closing this audit gate: derive all six native stencil expansions from
the existing `eulerStep`/`openFlux`/`closedFlux`, prove coefficient and one-step
nonnegativity on >=2 cells, then physically admissible variable-step preservation
and A10a ledger composition. Include the exact unsafe two-cell inlet
counterexample. Do not assume arbitrary `StepData.flux` preserves nonnegativity.
Keep zero data/coefficients allowed and preserve all existing results/pins. Native
regressions must test equality, minimum mesh/interiors, source/decay and unequal
steps with independent expectations normally and under ASan. Stop before the
separate front-accuracy study.
