# A7: precomputed selected-control influence

The existing `search_candidates()` and `evaluate_candidates()` APIs now use a
request-local control influence map for screening sufficiently large workloads.
Public types, bounds, partial
override semantics, diagnostics and independent finalist validation are unchanged.
No desktop, shared contract, root configuration or solution changes are included.

## Preparation and evaluation

For one validated captured network, let `u0` be its current selected controls and
`delta = u - u0`. Route setpoints have coefficient `+1` on their own route; source
curtailment has coefficient `-1` on its one feed route. Other coefficients are zero.

```text
route_flow(u) = route_flow(u0) + B * delta
tank_rate(u) = tank_rate(u0) + J * delta
destination_total(u) = destination_total(u0) + D * delta
hourly_cost(u) = hourly_cost(u0) + c * delta
```

`ControlInfluence` in [control_influence.hpp](src/control_influence.hpp) prepares
baseline rates, destination totals, hourly cost and up to four sparse columns.
Each selected control affects at most two tank rates and one destination. Source
curtailment includes both its direct cost and the reduced feed-route cost. Unselected
controls stay in the baseline, including when different rows omit different controls.

Ordinary candidates copy the baseline numeric vectors, apply their few column
changes, and inspect affine storage endpoints and shared destination totals. The
allocation-free loop reuses `InfluenceWorkspace`; there are no string lookups or
route traversals for the rate/destination calculation. Preparation is linear in the
source/route count plus selected controls. That numeric calculation per candidate is
linear in tanks/destinations plus selected controls, rather than all routes.

The map refers only to this call's validated `Prepared` snapshot and selected control
set. The existing APIs rebuild both for every request. No map is cached across network,
cost, availability or control changes; concurrent requests have separate workspaces.

The measured dispatch selects the map for at least **32 routes and 2,048 candidates**.
Smaller workloads retain the original route-walk evaluator: the initial benchmark
showed no total-search benefit there. This is a documented performance heuristic,
not a feasibility condition or a universal break-even threshold. Both paths share
input processing, ranking and independent finalist validation; the paired kernel
benchmark always exercises both numeric algorithms regardless of dispatch.

## Floating-point boundaries and cost ordering

The exact-real affine identity does not guarantee identical double rounding.
Preparation computes conservative upper envelopes for incident flows, destination
totals and cost over the selected control bounds. Nonnegative envelope operations
round outward with `nextafter`. A deliberately broad route-count-dependent roundoff
allowance covers the baseline sum, rounded deltas, column updates and endpoints.
Its absolute floor remains the engine's `1e-9` constraint tolerance.

Candidates near a storage/destination boundary use the existing route-walk screen.
Unusable or nonfinite preparation, endpoint arithmetic or scale/cost envelopes also
fall back to that screen. This includes a nonfinite baseline whose overridden
candidate becomes finite, and reciprocal huge flows with overflowing auxiliary sums.
No predicted volume or requested control is clipped. Fallbacks are observable in the
private test/benchmark workspace, without changing shared result contracts.

Cost reassociation can change the order of close candidates. The affine cost is
therefore an estimate only for rejected rows. **Every potential survivor recomputes
cost in the original source-then-route operation order before sorting.** This retains
the reference's bitwise computed costs, input-index tie break and top-five cutoff.
It requires a route pass for survivors; performance results include that work.

Finalists are still independently compiled, dense-reference simulated and validated.
The existing screen/reference cost disagreement check remains. No global optimality,
continuous optimization, robustness or quality guarantee is added.

## Checked equivalence

[ControlInfluence.lean](../proofs/Breakwater/ControlInfluence.lean) defines direct
route evaluation and prepared-column evaluation as different sequential folds.
Loop induction, distributivity and finite-sum interchange establish their equality
for arbitrary finite route/control/output dimensions, including direct control terms.
Output rows cover tank rates, shared-destination totals and hourly synthetic cost.
Graph-rate and affine-trajectory corollaries connect this to the existing proofs.

The checked statement uses exact real arithmetic with fixed coefficients. It does
not prove the compiled C++, coefficient construction, floating-point envelopes,
fallback classification or ranking. Native differential/oracle tests and MSVC ASan
provide separate implementation checks. See [proof scope and mapping](../proofs/README.md).

## Verification and reproduction

```powershell
.\scripts\build.ps1 -Preset headless
.\scripts\build.ps1 -Preset asan
.\engine\benchmarks\run.ps1 -Influence
.\proofs\verify.ps1 -LeanBin 'C:\path\to\lean-4.35.0-windows\bin'
```

The new `engine.control_influence` suite has 14 groups: numeric comparisons on
canonical/custom networks; an exhaustive independent dense/validator oracle;
partial overrides; source/feed pricing; shared and storage limits; bitwise cost ties
at the recommendation cutoff; overflow repair; cancellation; snapshot changes;
zero controls; malformed/unavailable control requests; and 2,048-row public batch
and Cartesian searches through the optimized dispatch. Both normal and MSVC
ASan builds passed all eight engine CTest suites. Pinned `lake build`, all 21 theorem
audits and the intentional unapproved-assumption rejection passed.

The benchmark preserves the original route-walk screen and uses the same production
batch preparation, sorting and validation for its baseline full search. It measures
precomputation separately, compares screens with matching numeric-output consumption,
and reports old/new end-to-end search. See [BENCHMARKS.md](BENCHMARKS.md) for timings,
scopes and recorded evidence. This is a measured optimization of the existing
constant-flow model, with no SIMD, parallel search, GPU or hydraulic changes.
