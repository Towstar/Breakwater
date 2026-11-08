# Checked storage, control-influence and finite-volume algorithms

This standalone Lean project checks mathematical storage algorithms and discrete
single-pipe tracer mass balance. The storage results apply to arbitrary valid finite
graphs; the transport identities apply to finite cells with shared face fluxes.
Person B's application, contracts, root setup/solution and shared documentation are
untouched. See [FVM_MASS_BALANCE.md](FVM_MASS_BALANCE.md) for A10a's equations,
assumptions, native mapping and limits.

## Reproduce

Pins:

- Lean `leanprover/lean4:v4.35.0`, selected by `lean-toolchain`.
- Mathlib `56a005855ea17e0b0958f73ec801ae140b0a1679` (`v4.35.0`).
- All nine direct/inherited Git dependencies have exact resolved revisions in
  `lake-manifest.json`; the source manifest is committed, compiled dependencies are not.

With that Lean/Lake toolchain on the process PATH:

```powershell
cd proofs
lake build
lake env lean -DwarningAsError=true -DautoImplicit=false Breakwater/Audit.lean
```

The default target imports `Breakwater.Audit`, so a successful normal build includes
the transitive assumption check. For setup/cache plus reproducible logs and a negative
audit regression, run the owned helper from the repository root:

```powershell
.\proofs\verify.ps1 -FetchCache
```

Alternatively pass `-LeanBin 'C:\path\to\lean-4.35.0-windows\bin'`. The toolchain used
here was downloaded directly into ignored project-local `.tools`, verified and
extracted, without installing elan or changing the system/user PATH. This helper only
changes its process environment and writes logs under ignored `out/a6-proofs`.

If materializing dependencies manually, set `MATHLIB_NO_CACHE_ON_UPDATE=1` before
`lake update`; this suppresses Mathlib's automatic unfiltered cache download. Keep
the pinned manifest for ordinary builds and review any intentional lock update.
The partial cache command used for this project is:

```powershell
$env:MATHLIB_NO_CACHE_ON_UPDATE = '1'
$env:MATHLIB_CACHE_DIR = 'C:\path\to\project\.tools\mathlib-cache'
lake exe cache get Mathlib.Algebra.BigOperators.Fin Mathlib.Algebra.BigOperators.Group.List.Basic Mathlib.Basic.Real.Basic Mathlib.Tactic.Ring Mathlib.Tactic.Linarith
```

The toolchain archive is the official
[Windows release](https://github.com/leanprover/lean4/releases/tag/v4.35.0),
`lean-4.35.0-windows.zip`, 902,262,322 bytes, SHA256
`5fc033e93e1a7bc59c5ebf311103d2c9cd18d4b3eddda4ffa3414e8adb0ef79e`.
Its Lake reports `5.0.0-src+eb6cafb`; Lean reports native x86_64 Windows GNU, Release,
commit `eb6cafb7290e8bd5f2937522d8d804f58f02bafc`. The Lean proof toolchain is independent
of the existing C++20/MSVC build. No SDKs, `.lake`, or cached proof binaries are tracked.

## Proven algorithms

| Required result | Checked definitions/theorems |
|---|---|
| Sparse edge loop equals dense incidence update | `Graph`, `edgeStep`, `sparseFold`, `denseRate`; `sparse_eq_dense`, `sparse_permuted_eq_dense` |
| Internal transfers conserve total volume | `denseRate_total_zero`, `sparse_total_conserved`, `graphEuler_total_balance` |
| Repeated constant-rate updates equal an affine trajectory | `repeatedEuler_closed_form`, `variableEuler_closed_form`, `timeSteps_sum`, `variableEuler_on_grid`, `network_rollout_affine` |
| Endpoint bounds hold throughout a constant-rate interval | `scalar_endpoint_bounds`, `trajectory_bounds`, `network_interval_bounds`; explicit zero-horizon corollary |

`Graph n m` stores edge endpoints as `Fin m → Fin n`, so every endpoint is valid by
construction. Parallel edges have distinct indices. Self-loops cancel correctly;
the native input validator excludes them, giving a supported subset of the theorem.
Empty finite dimensions are permitted when an endpoint function can exist. In
particular, a nonempty edge set cannot map into an empty vertex set.

The sparse algorithm is an actual sequential `List.foldl` in ascending edge-index
order, with source subtraction followed by target addition. The dense algorithm is
an independently defined incidence sum. Their equality is proved by loop induction
and finite-sum algebra, not by defining both algorithms as the same expression.
A permutation theorem also covers any enumeration containing every edge exactly once.

The trajectory is derived from actual recursive Euler steps. The variable-step proof
telescopes differences between consecutive sample times, so a shortened final interval
is covered. Endpoint inequalities are proved by the sign of the fixed rate, including
zero rate, equality at a limit and zero horizon. For a queried point the assumptions
`0 ≤ t ≤ horizon` already imply a nonnegative horizon; a negative interval is empty.

`Network.lean` composes these results: a captured graph/external rate generates the
constant vector, repeated graph steps equal the affine trajectory, total changes are
exactly external net flow, and graph endpoint bounds imply interval storage bounds.

## Precomputed control influence

`ControlInfluence.lean` proves the additional optimization for arbitrary finite
numbers of routes, selected controls and output coordinates. A route's candidate
flow is

```text
q[route] = q_base[route] + sum_control coefficient[route, control] * delta[control]
delta[control] = candidate[control] - captured_base[control]
```

For the native model, a selected route setpoint has coefficient `+1` on that route.
Source curtailment has coefficient `-1` on its feed route. Other route/control
pairs have coefficient zero. All unselected controls remain in the captured baseline.

The direct evaluator performs an actual sequential route fold of weighted candidate
flows. Precomputation obtains baseline outputs and each control column through the
same route-fold definition, while the optimized evaluator performs an actual
sequential **control-column fold**. Loop induction, distributivity and finite-sum
interchange prove equality; the equivalence is not assumed in the hypotheses.

| New checked result | Meaning |
|---|---|
| `weightedFold_eq_sum` | Sequential weighted route updates equal an independent finite sum |
| `weighted_candidate_decomposition` | Weighted candidate route sums equal baseline sums plus precomputed columns |
| `precomputed_eq_direct` | The prepared-column loop equals full route recomputation, including direct control terms |
| `precomputed_zero_delta` | Zero changes reproduce the captured baseline, including zero selected controls |
| `precomputed_network_rate` | Incidence-weight columns yield exactly the existing finite-graph network rate |
| `precomputed_network_trajectory` | Using those columns leaves the exact-real affine trajectory unchanged |

One generic output row can represent a tank rate (signed source/target weights),
destination throughput (receipt weights), gross incident flow (nonnegative endpoint
weights), or hourly synthetic cost (route plus destination prices). Direct control
terms cover source-curtailment cost. Therefore the main equivalence theorem covers
these numeric calculations together, not just a single two-tank example.

The coefficient matrix, baseline and weights are fixed for one captured network and
selected-control set. They must be rebuilt when that input changes. Finite graph
indices are valid by construction; zero-sized finite dimensions are supported.
Real coefficients, changes and weights may be signed, so the proof covers the
native nonnegative physical input subset without assuming away cancellation.

The numerical implementation still needs separate checks for arithmetic rounding,
nonfinite results and cost-ranking ties. The theorem does not itself justify a
floating-point rejection or a recommendation's complete feasibility.

In [control_influence.hpp](../engine/src/control_influence.hpp), `ControlInfluence`
stores request-local `base_rates_`, `base_destinations_`, `base_cost_` and sparse
`columns_`. Its candidate loop applies baseline plus column times control delta.
This is the part covered by the exact-real equivalence. The implementation's
`maximum_flows_`, `maximum_destinations_`, `maximum_cost_` and rounding guards are
conservative envelopes, rather than equal candidate outputs; their construction and
the near-boundary fallback are separate implementation checks. Potential survivors
have cost recomputed in the original source-then-route order before sorting, so the
formal cost identity is not used to assert bitwise floating-point ranking equality.

## Discrete finite-volume mass balance

`Transport.lean` formalizes cell averages, bounded neighboring faces, the explicit
Euler transport/source/old-state decay update, and a recursive variable-step rollout
with a cumulative ledger. Interior face differences telescope to the two boundary
fluxes. Multiplying by `1000 * area * dx` gives inventory in mg and the signed
inlet/outlet conversion used by native C++.

The one-step balance and cumulative zero-residual result are derived from these
updates, including state-dependent fluxes recalculated at each step. Open boundary
definitions include the half-cell Dirichlet inlet and advective outlet; closed
mode uses zero boundary exchange and zero-velocity interior diffusion. Signed inlet
diffusion is permitted. Unequal substeps include the native event/output-shortened
step sequence, without claiming a proof of the scheduler itself.

The required algebraic geometry condition is nonzero cell width. Native valid
geometry is stricter, with positive width/area and at least two cells. No CFL or
nonnegative-flux premise is needed for conservation. Positivity and PDE accuracy
remain separate: a balanced numerical solution can still be inaccurate.

The complete correspondence, boundary/source/decay units, native examples and
scope exclusions are in [FVM_MASS_BALANCE.md](FVM_MASS_BALANCE.md).

## Assumptions and C++ correspondence

All arithmetic is **exact over real numbers**. For the storage results, graph,
flows and external rates are fixed during the forecast. Flows/rates/step sizes may
be signed in the algebraic identities; the native nonnegative-flow and increasing-time rules restrict their
physical caller domain. No theorem assumes the desired interior storage inequality
or conservation conclusion. Interval bounds assume only the bounds at both endpoints.

| Lean object | Native correspondence |
|---|---|
| `Fin n`, `Fin m`, graph endpoints | Compiled tank/edge indices; runtime compiler supplies/validates these |
| `incidence`, `denseRate` | `build_incidence` / `dense_rates` in [forecast_kernels.hpp](../engine/src/forecast_kernels.hpp) |
| `edgeStep`, `sparseFold` | `sparse_rates` source-subtract/target-add sequence, starting with external-in minus external-out |
| `graphEuler`, recursive rollout | `euler_update` and sampled loop in [engine.cpp](../engine/src/engine.cpp) |
| `trajectory`, time-grid closed form | `evaluate_endpoints` and exact-horizon sample grid in the native engine |
| Interval storage bounds | Mathematical basis for constant-rate endpoint screening in [search_kernel.hpp](../engine/src/search_kernel.hpp) |
| `directOutputs`, `baselineOutputs`, `controlColumn`, `evaluateColumns` | Route evaluator in [search_kernel.hpp](../engine/src/search_kernel.hpp) and request-local `ControlInfluence` baseline/columns/candidate loop in [control_influence.hpp](../engine/src/control_influence.hpp) |
| Transport inventory, cell update and cumulative ledger | `mass_sum`, face flux/update loops and signed ledger in [transport.cpp](../engine/src/transport.cpp); details in [FVM_MASS_BALANCE.md](FVM_MASS_BALANCE.md) |

This mapping is documented and reviewed; it is **not a compiler/refinement proof**
of the C++ binary or parser. Floating-point order, roundoff guards, nonfinite values,
index/memory safety and implementation correctness still rely on their separate native
tests and sanitizer checks. Exact-real edge-order independence is not permission to
reorder floating-point operations. There is no formal speedup, hydraulics, tracer PDE accuracy,
complete destination/control/cost constraint validation, ML quality, robustness or optimizer proof.
Endpoint screening still needs actual independent native validation for recommendations.

## Assumption audit

`lakefile.toml` disables auto-implicit binders and promotes Lean warnings to errors.
Owned sources contain no proof placeholders, custom assumption declarations, unsafe
declarations or compiler-trusted decision shortcuts. The wrapper also rejects those
tokens lexically in the owned Lean files.

`Audit.lean` prints the transitive assumptions of every advertised algorithm theorem,
checks that their types are propositions, and rejects every dependency outside the
explicit standard-foundation allowlist: `propext`, `Classical.choice`, `Quot.sound`.
These standard principles of the pinned classical-real library are disclosed; this
is not a claim of foundation-free proofs. Lean 4.35's exported axiom metadata retains
dependencies when module exports hide proof bodies.

The verifier independently re-executes the audit after `lake build`. It also generates
a deliberately unapproved assumption/proof in ignored `out/` and requires the same
audit function to reject it for that reason. This probe is not part of the proof
library and is not among its checked assumptions.

Actual commands/results, logs and limits are recorded in [PROGRESS.md](PROGRESS.md).
The [Lean setup guide](https://leanprover-community.github.io/get_started.html)
and [Mathlib source pin](https://github.com/leanprover-community/mathlib4/tree/56a005855ea17e0b0958f73ec801ae140b0a1679)
provide the upstream installation/dependency references.
