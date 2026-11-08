# Person A science evidence


## F1b experimental advection comparison — November 8, 2026

[Comparison report](FRONT_ACCURACY_COMPARISON.md) completes the private MC/SSPRK2
experiment against frozen PR #28/F1. Each build executes 22 baseline and 28 candidate
cases; normal/ASan raw fields are byte-identical. **58/63 synthetic targets pass**:
all six field-improvement and 44 non-regression gates pass, but the extended inlet
peaks at .834466 mg/L and still misses .9 mg/L; box .1/.9 brackets exceed .025 h.
The box maximum field error increases to .512500 mg/L. Accuracy remains unqualified.

Fresh native checks pass 9/9 normally and under MSVC ASan (21 new native checks),
with zero sanitizer reports. Final pinned Python discovery passes 225/225 without
skips (39 new checks); PhysicsNeMo is available. Refined A9 remains 45 cases/580 gates passed
and the original coarse scheduled-inlet failure remains. All 80 original F1
checksum entries are preserved, including the six original time-contamination failures.

B handoff: private experiment only; production solver, interfaces, pins and original
54-proposition proof mapping are unchanged. Retain missed events and maximum-error
limits in presentation. A separately declared boundary/interior-smearing experiment
is justified before any promotion; application budgets and B integration remain gates.
The report records the failed reference-check regression, correction and all attempts.
Earlier milestone entries below preserve their original scope.

## G1 numerical guidance and F1 characterization — November 7, 2026

[G1 observation semantics](OBSERVATION_SEMANTICS.md) supplies worked calibration,
measurement-support, uncertainty and missing-coverage examples for B's contract
review. It does not implement sensors or adopt a shared telemetry schema.

[F1](FRONT_ACCURACY.md) completes 22 actual front/outlet/timing cases normally and
under MSVC ASan. Native data are byte-identical across builds; both legacy A9 modes
are byte-identical to the retained pre-F1 executable. Normal/ASan native suites
passed 8/8; final Python passed 186/186 with no skips, including 59 new checks.
Refined A9 passed 45 cases/580 gates in both; the original coarse failure remains.
No numerical solver, API, pin, old threshold or proof changed.

Accuracy remains unqualified: at N512 the extended inlet's saved outlet peak is
0.610814 mg/L versus 1 mg/L continuous reference and it misses the 0.9 crossing.
Six initial-pulse spatial runs exceed the predeclared 5% time-contamination target;
separate spatial/temporal errors and all failed attempts are retained. A subsequent
bounded advection-accuracy study is justified once required error budgets are agreed.

B handoff: review the G1 physical semantics, publish/adopt the shared contract before
G2 mappings, and incorporate these scoped results/limitations into shared STATUS.
G2/G3/RBM implementation, new proofs and application/release acceptance are outside
this increment. Earlier entries below preserve their original revisions and scopes.

## Post-A10a audit and A10b — November 7, 2026

The [new audit](AUDIT_POST_A10A.md) froze `5545417`, checked prior A-001–A-004
resolutions and found no new A-owned blocker for A10b. Audit commit `631f783`
precedes dependent implementation. Fresh checks passed 8/8 native suites normally
and with MSVC ASan, 127/127 Python tests with no skips, 18 native/Torch cases across
three solvers and 302 actual worker requests. All 37 preceding Lean statements and
the expected assumption-rejection probe passed. B-owned capture configuration and
shared proof-status findings are reported with reproductions, without B edits.

[A10b](../proofs/FVM_NONNEGATIVITY.md) now adds conditional exact-real FVM
nonnegativity on the native >=2-cell domain, all six derived stencil expansions,
physically admissible variable-step preservation, A10a mass-balance composition
and the deliberately unsafe half-cell inlet counterexample. Final pinned Lean
build and fresh transitive audit passed **54 statements** with unchanged pins and
allowlist; the unexpected-assumption probe was rejected. Final normal/ASan native
checks each passed **8/8 suites and 25/25 transport cases**, without sanitizer
reports. [Proof evidence](../proofs/PROGRESS.md) and
[native evidence](../engine/A10B_REGRESSIONS.md) record the actual commands,
versions, source/executable hashes and limitations.

The exact-real result does not prove compiled nonnegativity under epsilon margins,
scheduler correctness, IEEE error bounds or PDE/front accuracy. Native diagnostics,
no-clipping behavior, solver, contracts and B-owned files are unchanged. No new
training, hosted-model, installed-app, benchmark or accuracy study is claimed.
A9's original failure and about 0.40191 mg/L refined front maximum error remain
explicit. The front/threshold accuracy study is the separate next task and has
not been started here. B can incorporate these owned records into shared status.

The dated entries below preserve status and evidence at each milestone. In
particular, the A3/A4 health-only worker statements describe those deliveries;
A8 later added capabilities, storage rollout and selected-control derivatives.
Use [WORKER.md](WORKER.md) for current repository-worker behavior and the
[audit follow-ups](AUDIT_PERSON_A.md) for subsequent corrections. Historical counts
and limitations below are retained rather than presented as new executions.

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
   checks** (each evaluates both signs), and Torch gradcheck for every variable-control case.

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

## A4 — executed PhysicsNeMo transport smoke (November 7, 2026)

Implemented after A3 on the same requested `codex/person-a-engine` branch. Existing
A3 commits remain intact. Incremental files are confined to `science/` and `engine/`;
Person B's desktop, contracts, root solution/setup and shared docs remain untouched.

Delivered and actually executed:

- NVIDIA PhysicsNeMo 2.2.2 `FullyConnected` MLP, CPU float64, tanh, three 32-unit
  hidden layers and 2,241 parameters. Explicit physical autograd residual for
  advection/dispersion/decay; no optional Sym/PhysicsInformer execution is claimed.
- A positive transient eigenmode with exact zero inlet and zero-gradient outlet,
  analytically integrated cell-average initialization and signed tracer-mass ledger.
- Four newly computed native references: 32/64/128 cells at CFL 0.9, then 128 at 0.45.
  Native fields are evaluation-only; training uses IC samples and the PDE residual.
- Boundary-enforcing ansatz, 256 training collocation points, 64 IC points, 512
  disjoint held-out residual points, and eight-point quadrature for neural cell averages.
- Versioned predeclared thresholds/config, bounded 1,800-step training, actual weight
  updates, `.mdlus` save/load and strict double-precision inference equivalence.
- Separate hash-locked complete PhysicsNeMo CPU environment, reproducible CLI, reports,
  private native exporter and independent equation/averaging/checkpoint tests.

Actual checks:

1. Normal native build and **6/6 CTest programs passed**. MSVC AddressSanitizer build
   and the same **6/6 passed**, with no reported sanitizer errors.
2. Final full Python suite: **39 tests passed**, including 13 new A4 mathematical/
   actual-PhysicsNeMo tests, 24 A3 rollout tests and two unchanged worker tests. No
   optional PhysicsNeMo tests were skipped in this environment.
3. Actual final normal smoke at `out/a4-smoke-verified/report.json`: all gates passed.
   Repeated complete training/checkpoint/evaluation against the checked native exporter
   at `out/a4-smoke-asan/report.json`: all gates passed, no reported sanitizer errors.
4. A3 cross-language CLI after the larger environment sync still passes 12 cases ×
   three native solvers and 132 finite-difference checks (44 varying controls).

The preflight/config hash in both final A4 runs is
`c49601f355d9bc326243f837ae7be341eb371ec1c67447e1e59e9d07c200395a`.
It is unchanged from before the first training/evaluation attempt; thresholds were
not relaxed. Seed 20261107, one CPU thread and 1,800 steps reduced loss from
2.86528132106 to 0.00226199636 (last pre-update loss). Parameter update L2 was
2.80302703103. Training took 11.34 s and 14.43 s in the two final runs, both below
the declared 120-second training budget; these are run records, not benchmarks.

| Native run | Field RMSE vs analytic averages (mg/L) |
|---|---:|
| 32 cells, CFL .9 | .00307767821 |
| 64 cells, CFL .9 | .00157389028 |
| 128 cells, CFL .9 | .000796376683 |
| 128 cells, CFL .45 | .000792740456 |

Observed spatial orders were .9675 and .9828. Time-refinement field RMSE was
9.22442487e-6 mg/L. Native maximum mass residual across these runs was approximately
3.61e-13 mg. The negative inlet diffusion exchange was retained; outlet and decay
terms were positive and independent analytic ledger differences are recorded.

| Learned-model gate | Measured in both final runs | Predeclared limit |
|---|---:|---:|
| Held-out neural/native cell-average RMSE | .00336886618 mg/L | .03 mg/L |
| Held-out maximum field error | .0135691065 mg/L | .10 mg/L |
| Held-out PDE residual RMS | .0438580470 mg/L/h | .05 mg/L/h |
| Maximum outlet trace error | .0180117144 mg/L | .10 mg/L |
| Maximum inventory relative error | .0100422003 | .05 |
| Minimum sampled prediction | .00017309089 mg/L | >= -.005 mg/L |
| Checkpoint inference maximum difference | 0 mg/L | 1e-10 mg/L |

Environment: native Windows x64 / CPython 3.12.13, PhysicsNeMo 2.2.2, Torch 2.15.0+cpu,
NumPy 2.5.4, uv 0.11.9; MSVC 19.51.36260.0 / CMake 4.3.1 / Ninja. Dependencies were
actually resolved/installed and hash-checked. No WSL, container or CUDA execution was
needed. Warp's imported CUDA probe reports the unavailable driver; CPU calculations run.

Two setup problems were resolved without changing vendor code: ordinary extra-index
resolution selected incompatible old dependencies, so the CPU backend-specific routing
was used; the first training run reached checkpoint reload but the default user cache
was denied, so the documented `LOCAL_CACHE` setting now uses an owned output directory.
The test suite uses a temporary cache. Both final complete smoke runs succeeded.

Limitations and B handoff:

- This is a fixed-case experimental PINN smoke, not a qualified quality predictor or
  cross-setting surrogate. Training does not condition on arbitrary flow/inlet/geometry.
  No pressure hydraulics, network chemistry or transport differentiation is delivered.
- Native volume feasibility and explicit simulated approval are unchanged. The worker
  stays health-only pending B's published method/provenance/output contract and adapter.
- No generated checkpoints, reference fields, cache or environments are tracked. The
  reported checkpoint hashes identify artifacts, not byte-deterministic zip archives.
- B may regenerate the root solution for `breakwater_physicsnemo_reference` during
  integration. Public native interfaces and shared application schemas are unchanged.
- Full setup, equations, thresholds, actual API and commands are in PHYSICSNEMO.md.
  Next A increments are measured performance (A5) and checked Lean algorithms (A6).

## A8: agreed scientific worker integration — November 7, 2026

Continued on `codex/person-a-engine` after the reviewed B history/contracts/viewer
stack and A7 were merged. Public schema files were read/adopted; the feature changes
only `science/`, preserving the B-owned adapter, packaging and native approval path.

Delivered:

- Self-contained stdlib health with its exact existing response shape, plus versioned
  capability negotiation and lazy CPU float64 `storage_rollout` execution.
- Explicit project-to-private-model unit/default/ID mapping, identity echo and strict
  captured-network/control validation before Torch import. No feasibility verdict.
- Tank-major indexed previews and optional selected-interior-control derivatives.
  Full forward samples use no-grad; one-interval AD bounds backward work and is
  checked against full-rollout AD and three finite-difference sizes.
- Strict UTF-8 NDJSON framing, valid error IDs, finite data, bounded library output,
  1 MiB publication including newline, preview reduction and explicit unsupported/
  missing-runtime/work/output failures. No artifact or generic pipe/ML method.
- Process-local execution verification switches only after successful checked and
  size-bounded publication. A worker copied alone keeps installed health compatible
  and reports scientific mapping unavailable.
- Separate hash-locked schema test dependencies, constrained to the existing A4
  runtime pins; no scientific runtime packages were upgraded.

Actual checks in the pinned CPython 3.12.13 / Torch 2.15.0+cpu environment:

1. `python -m unittest discover -s science/tests -v`: **69/69 passed**, 38.557 s,
   including prior rollout/PhysicsNeMo regressions, 14 mapper groups and 16 actual
   subprocess protocol tests. Stdlib `-S` and copied-single-file health checks pass.
2. `science/verify_worker.py --native out/headless/engine/breakwater_rollout_reference.exe
   --report out/a8-worker-verification.json`: **12 actual native cases**, all three
   solvers, **290 real NDJSON requests**, **44 interior controls** and **132 central
   finite-difference checks** passed. Offline request/response schemas passed using
   jsonschema 4.25.1 / referencing 0.37.0.
3. Maximum native/worker volume difference: `7.67386154621e-13 m3`. Maximum FD terminal
   Jacobian/cost-gradient errors: `4.44836967262e-9 h` / `1.94024778466e-8` synthetic
   units per m3/h, within documented tolerances. Forecast values remain unrounded.
4. Maximum fixture response: 1,684 bytes; separate tests cover output-limit failure,
   preview shrinking without derivative loss, bad frames, semantic-ID errors,
   capability verification state, restart/isolation and explicit boundary failures.

Source/native/schema/verification-lock hashes, versions, tolerances and actual final
values are in [evidence/2026-11-07-worker-verification.json](evidence/2026-11-07-worker-verification.json).
The IDs/fingerprints in that report are explicitly test provenance, not authenticated
desktop run-store entries. Full commands, API scope and delivery boundary are in
[WORKER.md](WORKER.md).

Remaining B handoff: install the mapper/package with a compatible selected Python,
extend its async adapter beyond health, and bind typed scientific responses to the
captured run/generation. These steps are not claimed by this A-only increment.
PhysicsNeMo remains A4's fixed-case experimental smoke; native FVM/volume validation
and simulated approval remain authoritative. No new C++/Lean/PDE/quality guarantee
is inferred from these Python checks.

## A9 — bounded transport qualification (November 7, 2026)

Added a private native exporter, independent analytic/semidiscrete references,
declared gate configurations and a report command. The refined matrix passes
45 cases / 580 gates in both normal MSVC and MSVC AddressSanitizer executions.
Fresh existing native suites pass 8/8 in each build; the full Python suite passes
127/127 tests without skips.

The original coarse scheduled-inlet convergence failure is retained. Only that
study's meshes were refined, with every gate, forcing and CFL unchanged. The
refined mean-L1 pass does not qualify local peaks: its maximum cell-average error
is still .40191 mg/L, and its mesh/time refinement remains coupled.

See [TRANSPORT_QUALIFICATION.md](TRANSPORT_QUALIFICATION.md) for inventory, equations,
measured convergence, uncertainty, exact commands, hashes and unsupported regimes.
No native algorithm, public contract, B-owned file, Lean proof or learned-model
training method changed in this increment.

## A10a — checked discrete FVM mass balance (November 7, 2026)

Added the exact-real finite-cell transport proofs in `proofs/Breakwater/Transport.lean`:
interior-face cancellation, one-step signed inventory/source/old-state decay balance,
native open/closed boundary laws and recursive variable-step cumulative balance.
The native algorithm, scientific worker and public contracts remain unchanged.

Pinned `lake build` passed (1,062 jobs); all **37 theorem assumption audits** passed,
including 16 new transport results and the existing 21 storage/control-influence
results. The standard-foundation allowlist is unchanged and the deliberate
unapproved assumption was rejected. The targeted native transport regressions passed
**21/21** normally and under MSVC ASan, including three independent hand-computed
mapping examples. Person B's files are untouched.

See [proof evidence](../proofs/PROGRESS.md) and
[FVM mass-balance scope/mapping](../proofs/FVM_MASS_BALANCE.md). This proves a
discrete algebraic ledger identity, not PDE accuracy, positivity, floating-point
implementation correctness or valid optimization plans. A9's retained coarse-front
failure and peak-accuracy limitations are unchanged. No new Python/ML execution or
benchmark is claimed by this proof increment.
