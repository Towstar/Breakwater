# Person A audit: simulation, science and algorithm proofs

Audit date: **November 7, 2026 (America/Chicago)**. Raw UTC evidence crosses
November 8. Audited source: **`6fb8c29cf08399859ab41ea60771c31f4ec04e41`**,
the clean main baseline, reviewed on **`codex/person-a-engine`** in a separate
worktree. This report follows [the Person A kickoff](../docs/KICKOFF_PERSON_A.md).

All four findings from this Person A audit are resolved on `codex/person-a-engine`:

| Finding | Current disposition | Resolution/evidence |
|---|---|---|
| A-001 | Fixed in `d602160` | [Exact requested horizons](A001_HORIZON_FIX.md) |
| A-002 | Fixed in `b24004b` | [Finite costs and independent validation](A002_COST_FIX.md) |
| A-003 | Fixed in `0c2efe9` | [Constant/linear residuals and parameter gradients](A003_RESIDUAL_FIX.md) |
| A-004 | Documentation corrected | [Worker status and documentation verification](A004_STATUS_FIX.md) |

This closes the reported A-owned findings, not Person B's audit or the broader
transport/learned-model qualification work. The original audit below is retained
as evidence of frozen commit `6fb8c29`, including its then-open dispositions.

## Result and gate

**Original result at `6fb8c29`; current resolutions are listed above.**

**The audit is complete for the Person A scope below; the gate for dependent
features remains blocked by A-001 (P1).** Fresh existing regression suites pass,
but adversarial probes expose one false-feasibility case, a cost-validation
failure, and a PDE residual edge case. There are no confirmed P0 findings in
this review. Passing counts do not close these findings.

This increment records findings only. No engine, worker, proof, contract, desktop,
solution, root build, or shared documentation files were changed. No fixes or A9
features were implemented. All four findings remain open, owned by Person A.
Person B should aggregate this report into the shared audit and handle the
integration actions below.

| ID | Severity | Finding | Disposition |
|---|---|---|---|
| A-001 | P1 | Storage can stop short of the requested horizon and validate/recommend an infeasible endpoint; related native grids can duplicate or omit the final sample | Fix before dependent features |
| A-002 | P2 | Nonfinite independently computed cost can make a finite altered result cost pass validation | Fix numerical validation before expanding planning |
| A-003 | P2 | PDE residual helper throws for valid constant and linear fields | Fix before broader manufactured-solution/ML qualification |
| A-004 | P3 | PhysicsNeMo documentation still calls the repository worker health-only | Correct current-status wording |

P1 denotes a demonstrated wrong feasibility/recommendation. P2 denotes a bounded
correctness defect without a demonstrated current UI approval failure. P3 denotes
documentation drift. Extreme inputs used below are accepted by the current public
interfaces; their magnitude is explicitly retained in the impact assessment.

## Scope, method and environment

Reviewed the original scientific handoff, accepted desktop plan, ownership rules,
current contracts and scenario fixtures, then traced public APIs through source,
tests, benchmarks, workers and proofs. Independent review covered storage/search,
worker boundaries, and FVM/ML. Fresh builds and executions used unchanged source;
counterexamples were compiled/run against the freshly built native library.

The engine remains independent of Qt, Python and model APIs. Layout coordinates
are not hydraulic inputs. Storage remains directed, capacity-limited constant
flow, with unclipped finite negative/overflow predictions and equality permitted.
Selected-pipe quality remains separate from volume feasibility.

| Tool/runtime | Observed version/configuration |
|---|---|
| Host | Windows x64, build 10.0.26300; AMD Ryzen 7 7730U; 16 reported logical processors |
| Native | MSVC 19.51.36260, x64, C++20; CMake 4.3.1-msvc1; Ninja |
| Native regression builds | RelWithDebInfo; desktop OFF; testing ON; precise floating point; separate ASan OFF/ON directories |
| Performance build | Release /O2, /fp:precise, ASan OFF, serial execution |
| Shell | PowerShell 7.4.22 |
| Python/science | Python 3.12.13; PyTorch 2.15.0+cpu; NumPy 2.5.4; NVIDIA PhysicsNeMo 2.2.2; jsonschema 4.25.1 |
| Dependency consistency | `uv pip check`: all 84 installed packages compatible; existing Windows/Python 3.12 lock files retained |
| Proofs | Lean/Lake 4.35.0; Mathlib `56a005855ea17e0b0958f73ec801ae140b0a1679`; all 9 manifest dependencies resolved to 40-hex commits |

No dependencies were upgraded. Local SDKs, environments, caches and generated
outputs are not Git deliverables. Full desktop/Visual Studio solution, installed
application, credentials/live Nemotron, historical-run UI and approval transitions
were not executed as part of this A-only audit; Person B owns those checks.
The previous 18-suite native/desktop result is inherited evidence, not a fresh
18-suite result from this audit.

## Coverage and fresh evidence

| Area | Reviewed behavior and fresh evidence | Assessment |
|---|---|---|
| Scenario/science | Units, failed main, isolated reroute, mixed plan, healthy 44/16 discharges | Canonical values reproduced below |
| Graph/storage | ID compilation, supported source feeds, internal edges, external rates, bounds, pump availability, aggregate destination throughput, conservation, unclipped trajectories | Existing tests pass; A-001 and A-002 remain |
| Dense/sparse/affine | Independent dense incidence, sparse updates, affine endpoints, renamed/custom fixtures, nondividing grids and numerical extremes | Ordinary cases agree; grid counterexamples expose a shared contract defect |
| Search/control influence | Four-control/10,000 bound, omitted controls fixed, no-plan result, deterministic retained-cost ranking, canonical cost recomputation, fallback and workload dispatch | Existing suites and benchmark row/reference comparisons pass; A-001 reaches a recommendation |
| PyTorch/A8 | Differentiable rollout, three-step finite differences, selected interior controls, whole IDs, versions/units, framing limits, malformed/runtime errors, verification state and offline schemas | Fresh parity checks pass on their 12 cases; shortened horizon accepted by worker |
| Native FVM | Cell averages, upwind/diffusive fluxes, half-cell inlet, outlet/closed boundaries, source/decay, event alignment, litre conversion, signed inlet ledger, boundary-aware stable stepping | Existing tests and native refinement smoke pass; tested storage-grid failures do not reproduce here |
| Lean | All 21 audited statements, finite graph indices, induction/finite-sum arguments, assumptions, pins, C++ mapping | Checked build and assumption audit pass; exact-real scope only |
| PhysicsNeMo | Real model/residual, bounded training, parameter changes, held-out comparison, checkpoint reload, hashes/thresholds | Actual fixed-case smoke passes; helper edge case A-003; no generalization claim |
| Performance | 100/1,000/10,000 candidates on two graph shapes, plus 2,048 dispatch boundary | Fresh timings below; sampled kernel and endpoint-work reduction separated |
| Documentation | Current versus historical claims and unavailable capabilities | A-004; disclosed B integration remains pending |

Fresh executions at the frozen commit:

| Check | Actual result | Local evidence, relative to the audit worktree |
|---|---|---|
| Fresh MSVC native build/CTest | 8/8 suites, 3.85 s | `out/audit-person-a/native-build.log`, `native-tests.log` |
| Fresh MSVC ASan build/CTest | 8/8 suites, 40.37 s; no sanitizer report | `out/audit-person-a/asan-build.log`, `asan-tests.log` |
| Python unittest discovery | 69/69 tests, 69.017 s | `out/audit-person-a/python-tests.log` |
| Native/PyTorch rollout verification | 12 actual native cases, 3 solvers, 44 interior controls, 132 central-difference comparisons | `out/audit-person-a/rollout-verification.json` |
| Actual worker verification | 290 NDJSON requests, 12 native cases, 3 solvers, 44 controls, 132 central-difference checks; offline schemas pass | `out/audit-person-a/worker-verification.json` |
| Lean wrapper including Lake build | 21 audited algorithm theorems; unapproved-assumption negative probe rejected as expected | `out/audit-person-a/lean-verify.log`, `out/a6-proofs/` |
| Actual PhysicsNeMo/native smoke | All 12 gates pass; 1,800 Adam updates; 34.9539 s training | `out/audit-fvm-ml/physicsnemo/report.json` |
| Optimized benchmark and threshold rerun | All rows/reference/ranking checks pass; script also reruns 8 native suites | `out/audit-person-a/benchmark-matrix.json`, `benchmark-threshold.json`, corresponding environment JSON and `benchmarks.log` |
| Additional audit probes | Findings below; intentionally retain failing behavior | `out/audit-storage-search/`, `out/audit-worker-boundary/`, `out/audit-fvm-ml/` |

For rollout and worker verification, maximum observed volume disagreement was
**7.67386154621e-13 m³**; configured forward tolerance is absolute 1e-9 plus relative
1e-10. Gradient tolerance is absolute 1e-6 plus relative 1e-4. These passing fixtures
do not include the new counterexamples and do not establish universal equivalence.

The above `out/` evidence is machine-local and ignored, not bundled with this
report. Commands and self-contained counterexamples below permit reproduction.
Existing committed A3–A8 progress evidence is historical; every result explicitly
called fresh here was executed again for this audit.

### Canonical acceptance

Fresh `breakwater_cli --case <name>` with default 3 h / 0.025 h samples:

| Case | Final Tank A/B (m³) | Result | Limit time (h) | Synthetic cost |
|---|---|---|---|---|
| healthy | 104 / 114 | feasible preview | none | 0 |
| baseline, main failed | 236 / 114 | infeasible, A capacity | 0.769230769231 | 0 |
| bad isolated reroute | 80 / 132 | infeasible, B capacity | 2.66666666667 | 9,030 |
| mixed | 80 / 117 | feasible | none | 12,780 |

Healthy main/backup discharge remains **44/16 m³/h**. Source A supplies 52 m³/h,
so A temporarily accumulates 8 m³/h; the healthy preview is not steady-state.
Maximum canonical conservation residual observed here was 1.24344978758e-14 m³.
Overflow predictions remain visible.

## Findings

The finding statuses and line references below describe the original audited
commit. They are retained as historical reproductions; see the current disposition
table above for completed fixes.

### A-001 — P1: requested-horizon coverage is not guaranteed

**Owner:** Person A. **Status:** open, blocks dependent numerical features.

**Locations at audited commit:** `engine/src/engine.cpp:23–26,170–172,220–223,247–253`;
`science/breakwater_science/rollout.py:228–233`;
`science/worker_storage.py:181–188`; `science/worker.py:175–188`.

**Expected:** an accepted forecast covers the original requested horizon with
strictly increasing representable samples, and feasibility is independently
checked at that requested endpoint. Unsupported grids must fail explicitly before
being presented as a completed forecast or exhausted search.

**Observed:** storage computes `ceil(horizon/step)` and
`min(k*step,horizon)`. Floating-point division/multiplication can leave the final
sample short. Native validation tolerates the horizon difference, then computes
bounds using the returned final time instead of the requested horizon.

| Inputs, hours | Native dense/sparse/affine result | Search/worker consequence |
|---|---|---|
| horizon .9000000000000001, step .1 | Last sample .90000000000000002, one ULP below requested .90000000000000013; stationary tank validates | Search recommends; actual worker returns completed with shortened endpoint |
| horizon .30000000000000004, step .1 | Five samples with duplicate final timestamp; native validator reports `result_time` | Stationary valid network returns no recommendation after reference rejection; worker explicitly rejects this duplicate grid |
| horizon smallest positive double, step 24 | Ratio underflows to zero; only time 0 returned; validator reports `result_shape` | Native search returns no recommendation; worker explicitly rejects the unrepresentable interval |

The strongest counterexample uses source flow **2^50 m³/h**, initial inventory 0,
tank capacity **.9 × 2^50 m³**, horizon .9000000000000001 h and step .1 h. These are
extremely large but currently supported finite values. All three native solvers
return the shortened endpoint at capacity and validate it as feasible; search
returns one recommendation with zero reference rejections. Evaluation at the
requested horizon exceeds capacity by **0.125 m³**. This demonstrates a real wrong
feasibility decision, without suggesting that ordinary equipment uses this scale.

In the actual NDJSON probe using the contract example, forward output ends at .9 h
(index 9) while the final control Jacobian uses -.9000000000000001 h. The difference
is tiny in that ordinary fixture, but it shows that forward and derivative endpoints
are not identical. Worker self-verification repeats the same grid expression.

**Not affected in the tested cases:** native FVM constructs the final time explicitly
and removes duplicate/nonincreasing samples (`engine/src/transport.cpp:118–122`).
Its three grid probes all ended exactly at the requested horizon. This finding is
not a counterexample to the Lean exact-real algorithm theorems.

**Proposed fix:** define common storage grid semantics with an exact requested final
timestamp and strictly increasing samples, or explicit early unsupported-grid
failure. Consider the existing transport implementation's approach. Independently
validate bounds at the original requested horizon, not a nearby result timestamp.
Align native, Torch and worker calculations without shifting requested samples,
clipping predictions or rewriting request identities.

**Acceptance tests:** reproduce all cases above across dense/sparse/affine,
PyTorch, worker and search; verify that the high-flow case cannot receive a feasible
recommendation. Include nondividing steps, step greater than horizon, subnormal and
10,000-update boundaries. Assert exact final timestamp and strict increase for every
accepted grid; assert derivative/forward endpoint agreement. Any unsupported case
must have a specific input/work-limit error, not a misleading no-plan result.

### A-002 — P2: independently recomputed cost overflow can pass validation

**Owner:** Person A. **Status:** open.

**Locations:** `engine/src/engine.cpp:14,104–105,227–239,276`.

**Expected:** nonfinite derived costs are explicitly invalid, including when all
individual inputs are finite. A finite supplied result cost cannot agree with an
infinite independently recomputed cost.

**Observed:** a tank-to-treatment route with flow 1 m³/h and both route and destination
prices equal to `DBL_MAX` passes network compilation. For 1 h, the normal result
cost is infinity and correctly fails validation. Changing only
`SimulationResult.cost` to 0 makes validation return feasible with no violations:
the comparison becomes `abs(0-inf) <= atol + rtol*inf`, i.e. `inf <= inf`.
The derived control catalog price is also infinity.

This is an independent-validator integrity defect. Unmodified infinite simulation
costs and search screening are already rejected; no current desktop approval
bypass was demonstrated.

**Proposed fix:** reject nonfinite independently derived hourly/total costs and
catalog sums, and make tolerant comparisons reject nonfinite arguments. Return an
explicit overflow/invalid-cost failure; do not clamp prices or substitute zero.

**Acceptance tests:** finite input prices whose sum/product overflows must fail
independent validation for supplied finite, infinite and NaN result costs.
Check catalog derivation and search behavior, while preserving valid zero-price
cases and canonical mixed-plan cost.

### A-003 — P2: PDE residual cannot handle valid zero derivatives

**Owner:** Person A. **Status:** open.

**Location:** `science/breakwater_science/transport_pinn.py:119–130`, specifically
the two unconditional `torch.autograd.grad` calls at 126 and 128.

**Expected:** a callable field of shape [point,1] may be constant or linear in space
and time. Its mathematically zero derivatives must produce a valid residual.

**Observed:** at points [[.2,.3],[.7,.9]], both a constant field of 1 mg/L and a field
`x+t` throw `RuntimeError: element 0 of tensors does not require grad and does not
have a grad_fn`. Constant outputs have no coordinate gradient; linear first
derivatives have no derivative graph for the second call. With the existing CASE,
expected residuals are [.1,.1] and [1.521238898038469,1.631238898038469] mg/L/h,
respectively. A quadratic field works.

The current nonlinear tanh PhysicsNeMo smoke passes; this is a reusable helper and
manufactured-solution gap, not a failed execution of that trained model.

**Proposed fix:** handle coordinate-independent/constant derivatives as zero while
preserving genuine model-parameter gradients. Do not detach the residual or swallow
all autograd errors.

**Acceptance tests:** constant, linear, quadratic and actual neural fields;
verify analytic residual values and backpropagation to trainable parameters.

### A-004 — P3: worker status documentation is stale

**Owner:** Person A. **Status:** open.

**Location:** `science/PHYSICSNEMO.md:154`.

**Expected/observed:** the current-tense statement that the scientific worker
"remains health-only" is no longer true for the repository worker after A8.
Actual `storage_rollout`, selected-control gradients and capabilities were exercised
above. The desktop adapter remains health-only; these are different components.

**Proposed fix/acceptance:** preserve A4's historical scope, update current status to
link the A8 worker documentation, and explicitly distinguish repository worker
methods from desktop deployment/client support. No claim of implemented Python
`pipe_transport` or general learned inference should be added.

## Portable counterexamples

Run from the repository root at the audited commit, after the fresh native build
below. Save this block as `out/audit-person-a/findings.cpp`. This standalone version
of the native audit probe was also compiled and executed against the fresh library:

```cpp
#include "breakwater/engine.hpp"
#include "breakwater/search.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
using namespace breakwater;

Network flat() { return {{{"tank", "Tank", AssetKind::tank, 1.0, .5}}, {}}; }
Network fed(double q, double cap) {
    return {{{"tank", "Tank", AssetKind::tank, cap, 0.0},
             {"source", "Source", AssetKind::source, 0.0, 0.0, q}},
            {{"feed", "source", "tank", q, 0.0}}};
}
void show(const char* label, const Network& n, SimulationConfig cfg) {
    std::cout << "case=" << label << " requested=" << cfg.horizon_h
              << " step=" << cfg.step_h << '\n';
    const auto c = compile(n);
    for (const auto solver : {Solver::dense_reference, Solver::sparse, Solver::affine}) {
        const auto r = simulate(c, cfg, solver);
        const auto v = validate(c, r, cfg);
        double exact = 0.0;
        evaluate_endpoints(c, cfg.horizon_h, std::span(&exact, 1));
        bool increasing = true;
        for (std::size_t i=1;i<r.time_h.size();++i)
            increasing &= r.time_h[i] > r.time_h[i-1];
        std::cout << "solver=" << static_cast<int>(solver)
                  << " samples=" << r.time_h.size() << " last=" << r.time_h.back()
                  << " exact_last=" << (r.time_h.back()==cfg.horizon_h)
                  << " increasing=" << increasing
                  << " final=" << r.volumes_m3[0].back()
                  << " requested_endpoint=" << exact << " capacity=" << c.capacity_m3[0]
                  << " feasible=" << v.feasible << " issues=";
        for (const auto& issue : v.violations) std::cout << issue.code << ',';
        std::cout << '\n';
    }
    SearchOptions opt; opt.config=cfg;
    const auto result=search_candidates(n, {}, opt);
    std::cout << "search recommendations=" << result.recommendations.size()
              << " reference_rejections=" << result.diagnostics.reference_rejections << '\n';
}
int main() {
    std::cout << std::setprecision(17);
    show("short_flat", flat(), {.9000000000000001, .1});
    show("duplicate_flat", flat(), {.30000000000000004, .1});
    show("underflow_ratio", flat(), {std::numeric_limits<double>::denorm_min(), 24.0});
    const double q=std::ldexp(1.0, 50);
    show("short_capacity", fed(q, .9*q), {.9000000000000001, .1});
    Network priced;
    priced.assets = {{"tank", "Tank", AssetKind::tank, 100.0, 50.0},
        {"dest", "Destination", AssetKind::treatment,
         10.0, 0.0, 0.0, 0.0, 0.0, std::numeric_limits<double>::max()}};
    priced.routes = {{"out", "tank", "dest", 10.0, 1.0, true, true,
                      std::numeric_limits<double>::max()}};
    const auto pc=compile(priced);
    auto pr=simulate(pc, {1.0, .25});
    std::cout << "overflow_cost=" << pr.cost
              << " original_feasible=" << validate(pc,pr,{1.0,.25}).feasible;
    pr.cost=0.0;
    const auto pv=validate(pc,pr,{1.0,.25});
    std::cout << " altered_cost=" << pr.cost << " altered_feasible=" << pv.feasible;
    std::cout << " catalog_cost=" << control_catalog(priced)[0].cost_per_m3 << '\n';
}
```

In an MSVC x64 Developer PowerShell, compile and run (all outputs stay under out):

```powershell
cl.exe /nologo /std:c++20 /O2 /MD /EHsc /fp:precise /Iengine/include /Icontracts/include /Foout/audit-person-a/findings.obj out/audit-person-a/findings.cpp out/audit-person-a/native/engine/breakwater_engine.lib /Fe:out/audit-person-a/findings.exe
& out/audit-person-a/findings.exe
```

Expected current defect signatures: short_capacity is feasible for all three
solvers and has one search recommendation even though requested endpoint minus
capacity is .125 m³; duplicate_flat has `result_time`; underflow_ratio has
`result_shape`; overflowing original cost fails but altered cost 0 passes.

For A-003, save this as `out/audit-person-a/residual.py` and run it with
`science/.venv/Scripts/python.exe out/audit-person-a/residual.py`:

```python
import sys
sys.path.insert(0, "science")
import torch
from breakwater_science.transport_pinn import pde_residual
points = torch.tensor([[.2,.3],[.7,.9]], dtype=torch.float64)
for name, field in [
    ("linear", lambda z: z[:,:1] + z[:,1:2]),
    ("constant", lambda z: torch.ones_like(z[:,:1])),
    ("quadratic", lambda z: z[:,:1]**2 + z[:,1:2]),
]:
    try:
        print(name, pde_residual(field, points).detach().tolist())
    except RuntimeError as error:
        print(name, type(error).__name__, str(error))
```

For the actual worker short-grid case, save/run this Python block in the same way:

```python
import json, subprocess, sys
with open("contracts/science/examples/storage-rollout-request.json") as stream:
    request = json.load(stream)
request["request_id"] = "audit-short-grid"
request["payload"].update(horizon_h=.9000000000000001, step_h=.1, derivatives=True)
reply = subprocess.run(
    [sys.executable, "science/worker.py"],
    input=(json.dumps(request)+"\n").encode(), capture_output=True, check=True)
response = json.loads(reply.stdout)
print("requested", request["payload"]["horizon_h"])
print("status", response["status"])
print("final_time", response["result"]["time_h"][-1])
print("final_index", response["result"]["time_indices"][-1])
print("derivatives", response["result"]["derivatives"])
```

## What the Lean proofs actually establish

Fresh `proofs/verify.ps1` ran Lake build and the assumption audit for all 21
algorithm theorems. Each reported only the allowed standard axioms
`propext`, `Classical.choice`, `Quot.sound`; the unapproved-assumption probe was
rejected. The audit did not introduce axioms or permit `sorry`.

Reviewed [Graph.lean](../proofs/Breakwater/Graph.lean),
[Affine.lean](../proofs/Breakwater/Affine.lean) and
[ControlInfluence.lean](../proofs/Breakwater/ControlInfluence.lean):

- On finite indexed graphs, the sparse edge fold equals the independently defined
  incidence update; internal edge transfers cancel in the total.
- Repeated constant-rate updates equal the affine trajectory (including accumulated
  variable step times), with external rates accounted for in total volume.
- Endpoint bounds imply bounds at all times in the specified constant-rate interval.
- Direct weighted route/control evaluation equals the precomputed influence
  decomposition; graph rate and trajectory corollaries justify the algebra behind A7.

These are exact-real algorithm correctness/equivalence proofs, including
optimization-preserving equivalence. They do **not** prove a candidate is feasible
for every compiled execution, validate a global optimum, or certify C++ memory
safety, IEEE-754 arithmetic/fallback guards, time-grid construction, cost ranking,
FVM convergence/positivity, PDE physical fidelity or learned predictions.
A-001 demonstrates an implementation precondition gap outside that proof scope.
The mapping to C++ was reviewed manually; there is no machine-checked refinement
from Lean to the compiled executable.

## Fresh performance results

All values below are **median milliseconds per candidate batch**, seven timed
repetitions after two warmups, rotated phase order and calibrated inner iterations.
The run was separated from training/tests and other audit compute. It was not a
controlled CPU-frequency/thermal experiment; raw samples and spread remain in JSON.
No timing threshold is an acceptance gate.

Dense and sparse perform the same 120 updates, 121 sampled outputs and full-buffer
consumption. Affine screening avoids trajectories and final validation, so a ratio
between sparse and affine includes less work and is not a sparse-kernel speedup.

| Graph | Candidates | Dense sampled | Sparse sampled | Affine screen |
|---|---:|---:|---:|---:|
| canonical, 2 tanks / 6 routes | 100 | .116829 | .114634 | .005039 |
| canonical | 1,000 | 1.094244 | .991200 | .047058 |
| canonical | 10,000 | 10.031700 | 9.562100 | .453028 |
| custom, 24 tanks / 66 routes | 100 | 11.443300 | 1.453713 | .029100 |
| custom | 1,000 | 114.463600 | 14.879700 | .289130 |
| custom | 10,000 | 1244.734800 | 147.249100 | 2.867600 |

At 10,000 custom candidates, the same-work dense/sparse ratio is **8.453×**.
The endpoint reduction has a different workload and must be reported separately.

Paired route/influence screens consume the same outputs; surviving influence rows
recompute canonical costs in original order before ranking. Full search includes
preparation, screening, ranking, retained dense simulation/reference validation and
materialization; at most five retained runs are independently validated inside
each timed search. All candidate rows are reference-checked outside timing.

| Graph | Count | Influence preparation | Route screen | Influence screen | Route full search | Public full search | Public dispatch |
|---|---:|---:|---:|---:|---:|---:|---|
| canonical | 100 | .001803 | .006226 | .006118 | .116561 | .116669 | route walk |
| canonical | 1,000 | .001555 | .056559 | .056342 | .387675 | .376484 | route walk |
| canonical | 10,000 | .001501 | .519769 | .504453 | 3.079575 | 3.725125 | route walk |
| custom | 100 | .004329 | .032738 | .016563 | 1.426650 | 1.445963 | route walk |
| custom | 1,000 | .004178 | .302350 | .156713 | 1.847625 | 1.914575 | route walk |
| custom | 10,000 | .004463 | 3.256550 | 1.658438 | 8.614400 | 7.648400 | influence |
| canonical | 2,048 | .001613 | .118684 | .122939 | .750938 | .762144 | route walk |
| custom | 2,048 | .004829 | .702825 | .371919 | 2.845525 | 2.598150 | influence |

For 10,000 custom rows, paired screening gains **1.964×** and full search gains
**1.126×** in this run. Influence preparation includes allocation, destruction and
checksum consumption; common topology/candidate preparation is excluded from that
individual phase. Phases overlap and must not be summed to reconstruct total.
Canonical route/public totals differ despite using the same route-walk path;
this spread reinforces that these measurements do not establish a universal gain.

Dispatch remains route walk for small networks/batches and selects influence at
the tested 2,048 boundary on the larger network. Each workload recorded one
fallback. Maximum sampled forward error across benchmark cases was
1.1368683772161603e-12 m³; maximum influence rate error was
7.105427357601002e-15 m³/h. All feasibility counts/rankings agreed in these workloads.
No new SIMD, parallel search or GPU work is justified by this audit alone.

## Actual PhysicsNeMo smoke and FVM limits

The real `physicsnemo.models.mlp.fully_connected.FullyConnected` model used 2,241
parameters, CPU float64 and 1,800 Adam updates. Parameter update L2 was
2.80302703103; loss fell from 2.86528132106 to .002261996361.
Checkpoint round-trip inference difference was zero.

| Measured quantity | Fresh result | Predeclared gate |
|---|---:|---:|
| Held-out cell-average RMSE | .00336886618 mg/L | <= .03 |
| Held-out maximum field error | .0135691065 mg/L | <= .10 |
| Held-out PDE residual RMS | .0438580470 mg/L/h | <= .05 |
| Outlet maximum error | .0180117144 mg/L | <= .10 |
| Maximum inventory relative error | .0100422003 | <= .05 |
| Minimum sampled prediction | .00017309089 mg/L | >= -.005 |
| Checkpoint inference difference | 0 mg/L | <= 1e-10 |

Native finest-mesh RMSE was .000796376683 mg/L; observed mesh orders were
.967507 and .982812. The separate CFL .45 time-refinement difference was
9.2244e-6 mg/L. Native mass/refinement, training and checkpoint checks passed all
12 declared smoke gates.

This is one manufactured fixed-case experimental smoke. It is not held-out
parameter-family validation or an industrial quality model, and cannot approve
volume controls. A9 still needs a bounded regime matrix with independent analytic
or manufactured solutions, consistent cell averages, separate mesh/time refinement,
signed mass residuals and declared supported ranges. Existing tests already cover
several boundary, source/decay, schedule, refinement and negative-input cases;
inventory those before adding more.

## Reproduction commands and provenance

Use the existing setup/locks in science and proofs documentation. In x64 Developer
PowerShell, these reproduce the isolated native configuration used here without
regenerating Person B's solution (adjust SDK paths to the installed toolchain):

```powershell
cmake -S . -B out/audit-person-a/native -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBREAKWATER_BUILD_DESKTOP=OFF -DBUILD_TESTING=ON -DBREAKWATER_ENABLE_ASAN=OFF -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded
cmake --build out/audit-person-a/native --parallel 2
ctest --test-dir out/audit-person-a/native --output-on-failure
cmake -S . -B out/audit-person-a/asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBREAKWATER_BUILD_DESKTOP=OFF -DBUILD_TESTING=ON -DBREAKWATER_ENABLE_ASAN=ON -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build out/audit-person-a/asan --parallel 2
ctest --test-dir out/audit-person-a/asan --output-on-failure
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/verify_rollout.py --native out/audit-person-a/native/engine/breakwater_rollout_reference.exe --report out/audit-person-a/rollout-verification.json
science/.venv/Scripts/python.exe science/verify_worker.py --native out/audit-person-a/native/engine/breakwater_rollout_reference.exe --report out/audit-person-a/worker-verification.json
science/.venv/Scripts/python.exe science/physicsnemo_smoke.py --native out/audit-person-a/native/engine/breakwater_physicsnemo_reference.exe --output out/audit-fvm-ml/physicsnemo
.\proofs\verify.ps1 -LeanBin 'C:\Projects\Breakwater\.tools\lean-4.35.0-windows\bin'
.\engine\benchmarks\run.ps1 -Influence
.\engine\benchmarks\run.ps1 -Influence -SkipBuild -Count 2048
uv --cache-dir out/audit-person-a/uv-cache pip check --python science/.venv/Scripts/python.exe
```

The first dependency-consistency attempt could not write the sandbox's default uv
cache; rerunning with the workspace cache above passed. No test or runtime was
substituted/skipped to turn a failure into a pass.

Benchmarks write `out/a7-benchmarks/results.json`, `environment.json` and
`results-2048.json`, `environment-2048.json`. Audit copies use the names in the
evidence tables. Environment files record source commit/clean state, compiler
commands, workload hashes and executable/source SHA-256 values.

| Fresh local artifact | SHA-256 |
|---|---|
| rollout-verification.json | `4c1111fa7fcfba4e21de5f2c3fdf691b6b46fab49a909a895055e1c4972aa886` |
| worker-verification.json | `28f3cacb84d4ee6ca896f4e9b7a2eda736002bc66a573070bb363b28dd82a222` |
| benchmark-matrix.json | `7a87afd23af223b478fb5aeb9c7d6efc87d577b2ec7263fc53f3eb351c58799f` |
| benchmark-threshold.json | `b6def98e2fd6193321c0b1d38b1c6137b825469d4932138ccc9a61ba90289d3c` |
| PhysicsNeMo report.json | `b8f9e52a560007cae0029ad343b341a50b34e707f2cf3e957b45ab6711f3fe42` |

These hashes identify this run's local artifacts; timing/environment/checkpoint
bytes are not expected to be identical across machines.

## Ordered follow-up and Person B handoff

1. **A fixes A-001 first**, in a small owned-path PR with native/Torch/worker/search
   regressions and independently requested-horizon validation. Re-run parity,
   worker tests, checked native tests and relevant benchmarks after the change.
2. **A fixes A-002 and A-003** with focused overflow and analytic-residual tests;
   correct A-004 documentation. Preserve canonical constants, strict bounds,
   invalid-input behavior and genuine parameter gradients.
3. **B aggregates this audit** and checks that stored/requested configs and full
   native timestamp grids are retained exactly. Do not normalize/shorten a request
   in a UI/adapter merely to agree with a faulty result. Send any contract extension
   through B before A adopts it.
4. **B completes and verifies deployment/client integration:** the desktop adapter
   currently invokes health only, and packaging copies worker.py without the A8
   module tree/runtime. This was already disclosed pending work, not a newly
   demonstrated installed-app regression. A's repository worker does implement
   storage and selected derivatives; Python pipe_transport remains unavailable,
   and selected-pipe authority remains native_fvm. Echoed identity is not native
   validation or simulated approval evidence.
5. **B reviews the native historical-evidence stack separately** (PR #13 at audit
   planning); no review/merge/desktop acceptance of that work is claimed here.
   B owns live Nemotron, stale jobs/approvals, installed-app checks and the full
   Visual Studio solution verification.
6. **After the blocking fix, A starts A9 qualification** from the kickoff's supported
   transport regime matrix. A10 discrete FVM proofs and broader learned model
   families remain optional subsequent work with explicit scope and thresholds.

This report supplies A's findings and evidence for that integration gate.
