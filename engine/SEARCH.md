# Native candidate search (Person A increment 1)

Include `breakwater/search.hpp` and link `breakwater_engine`. Existing engine functions,
shared C++ types, project-v1 serialization and worker schemas are unchanged.

## Entry points

- `search_candidates(network, axes, options)`: Cartesian product of explicitly supplied
  catalog control values. The last axis varies fastest. Zero axes evaluates the unchanged
  operating settings once. Repeated values are retained and count toward the limit.
- `evaluate_candidates(network, candidates, options)`: an explicit batch of partial
  control overrides. Omitted controls retain the captured network's current settings,
  including when another row supplies those controls.

Both accept at most four distinct varying control IDs and 10,000 candidates/combinations.
`SearchOptions` supplies the captured `SimulationConfig` and `result_limit` (1..5,
default 5). Existing engine horizon/sample bounds apply. Input must be a structurally
valid network, although its current forecast may be infeasible.

Unknown/repeated control IDs, nonfinite or out-of-range values, unavailable pump requests,
invalid source-feed capacity settings, empty value lists/batches, and over-limit requests
throw `std::invalid_argument` before candidate evaluation. Control IDs are matched to
the catalog exactly; asset/route IDs may themselves contain colons.

Catalog unit prices must be finite, including the combined route/destination
price of each tank-origin discharge. An unrepresentable combined price rejects
the network explicitly, even at zero flow. With representable unit prices, an
overflowing current hourly/forecast cost does not prevent search from evaluating
finite lower-flow interventions. Nonfinite candidate costs are numerical screen
rejections; independently validated results require finite nonnegative costs.
See [A-002 resolution](../science/A002_COST_FIX.md) for overflow regressions.

## Results and evidence

`recommendations` contains actual dense-reference `run` values and passing `validation`
reports, plus each candidate's input `candidate_index` and applied `overrides`. The engine
does not invent run UUIDs; the application owner assigns IDs when storing these results.

Recommendations sort by actual computed synthetic cost, then input index for deterministic
ties. A mismatch between screening and reference cost calculation aborts the request
rather than silently returning a potentially incorrect ordering. Cost includes curtailment,
feed-route flow, internal routes and external destination costs, following existing rules.

The diagnostics count candidate evaluation, potential screen survivors, constraint/numerical
screen rejections, reference checks and reference rejections. Screen survivors are not
declared feasible. Reference checks stop when the requested number of valid results is
filled; remaining survivors are not claimed to have been reference-validated.

`no_feasible_plan_found` means no validated recommendation was found among these inputs.
It is not a global infeasibility proof. Check numerical rejection counts when explaining
that outcome. Returned plans have nominal volume/throughput feasibility only; robustness,
quality, indefinite sustainability, reserves and global optimality are not established.

## Evaluation path

The implementation compiles an immutable baseline once, resolves control IDs/topology,
and prepares bounded candidate-value rows. At 32 or more routes and 2,048 or more
candidates it prepares sparse selected-control influence columns; smaller workloads
retain the original route walk. Both allocation-free screening loops reuse numeric
buffers and evaluate constant-rate affine endpoints rather than every trajectory.
Potential survivors retain the reference compiler's source-then-route cost order,
including feed-route savings from curtailment. The prepared map, floating-point
fallback, checked equivalence and performance policy are documented in
[CONTROL_INFLUENCE.md](CONTROL_INFLUENCE.md).

Borderline floating-point bounds proceed to reference validation. Overflow of an auxiliary
roundoff scale also defers the affected bound to the reference engine. Actual nonfinite
forecasts/costs are rejected and counted. Potential survivors are cost-ordered; finalists
are independently compiled, dense-reference simulated and validated at the same horizon.

## Executable example

From this checkout, run:

```powershell
.\scripts\build.ps1 -Preset headless
.\out\headless\engine\breakwater_search_example.exe
.\out\headless\engine\breakwater_search_example.exe --infeasible
```

The normal example enumerates 432 canonical incident settings. An executed run found
39 screen survivors and reference-validated five recommendations. The lowest-cost one
used bypass 20, backup discharge 35, trucking 20 and curtailment 0 m3/h: final A/B volumes
116/117 m3, cost 4,800 synthetic units over three hours, minimum headroom 4 m3.
It deliberately consumes storage headroom; it is not an indefinitely balanced solution.
The restricted `--infeasible` example returns no feasible plan among its two candidates.
These are computed native results, not Nemotron output or a performance benchmark.

## Application integration

Person B can map these functions into tool schemas after integrating this branch. Keep
the selected network/snapshot and requested horizon fixed, store the returned native
runs/reports with application provenance, and retain the separate approval transition.
No desktop, root solution, shared contract, or canonical fixture edits are required by
this increment. B owns regeneration of the Visual Studio solution after integration.
