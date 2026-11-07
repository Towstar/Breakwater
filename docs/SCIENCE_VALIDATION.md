# Numerical base

All equipment, data, costs and controls are synthetic. This is a fixed-flow inventory
model, not a pressure/head hydraulic model or validated industrial controller.

## Equations and units

Only tanks store fluid. Production enters at the connected tank; treatment and trucking
leave the system. Internal routes subtract and add the same volumetric flow:

`dV/dt = external_in - external_out + incidence * internal_flow`.

Time is hours, volume m3, flow m3/h. Source flow is nominal production minus curtailment.
Combined flow into a destination is checked against its m3/h throughput capacity.
Cost over horizon T is `T * (source reduction costs + route costs + destination costs)`.

Initial state is sample zero. The final sample is exactly the requested horizon even
when the step does not divide it. Both Euler implementations use constant rates and
therefore match the affine analytical trajectory up to floating-point rounding.

Capacity equality is permitted. `limits` reports when a capacity/empty limit is reached;
`violations` reports a subsequent excursion during the forecast. Trajectories are not
clipped at zero or capacity. Reference agreement uses absolute 1e-9 plus relative 1e-10
tolerance; storage comparisons use 1e-9 m3, conservation rejects residual above 1e-7 m3.

## Canonical expectations

| Case | A at 3 h | B at 3 h | Limit / cost |
|---|---:|---:|---|
| Healthy: main 44, backup 16 | 104 | 114 | Feasible preview, A accumulates 8 m3/h |
| Failure baseline: backup 16 | 236 | 114 | A reaches 120 at 40/52 h |
| Bad: bypass 25, truck 22, reduce 5, backup 35 | 80 | 132 | B reaches 130 at 16/6 h |
| Mixed: bypass 20, truck 22, reduce 10, backup 35 | 80 | 117 | Nominal pass, cost 12,780 |

Nominal three-hour feasibility does not establish indefinite sustainability, stress
robustness, chemical quality, or optimality. Quality is separate from that decision.

## Evidence boundaries

Native tests cover canonical cases, capacity equality, arbitrary renamed assets,
shared destination limits, unclipped negative predictions, malformed controls/topology,
tampered trajectories/costs, requested horizon, random graph equivalence/conservation,
and concurrent candidate isolation. See STATUS for actual executed commands.

The affine endpoint function accepts caller-owned workspace and allocates no memory
inside its evaluation loop. [Recorded benchmarks](../engine/BENCHMARKS.md) separate
same-task dense/sparse timing from the different work performed by affine screening.

## Native single-pipe transport

PR #4 delivers a cell-centered finite-volume scalar reference with upwind advection,
centered dispersion, decay/source terms, stable boundary-aware stepping and signed
mass accounting. Equations, units, boundary semantics and resource limits are in
[engine/TRANSPORT.md](../engine/TRANSPORT.md); executed checks are in
[engine/PROGRESS.md](../engine/PROGRESS.md).

The 64-cell example used 136 substeps with max CFL 0.9 and maximum mass residual
1.36887055523e-12 mg. Eighteen transport case groups and the example passed normal and
MSVC ASan builds. This is a standalone synthetic scalar-pipe calculation; it does not
add tank chemistry, quality approval or physical pipe inventories to the storage model.

## Differentiable and learned calculations

[A3](../science/README.md) implements a CPU double-precision PyTorch storage rollout.
It agrees with actual dense, sparse and affine native runs within documented
tolerances, with continuous-control derivatives checked using central differences
and Torch gradcheck. Gradients do not establish feasibility and are not derivatives
through discrete candidate selection or active-bound projections.

[A4](../science/PHYSICSNEMO.md) executes an actual PhysicsNeMo model, autograd PDE
residual, bounded training, checkpoint reload and held-out native transport comparison.
It is an experimental fixed-case smoke; its error gates do not qualify arbitrary
edited networks, operating conditions or chemical quality. Native C++ remains the
volume-feasibility authority. The scientific worker remains health-only until the
agreed contracts and process adapter expose these standalone modules.

## Checked Lean algorithms

These are **algorithm correctness proofs over exact real arithmetic**, for arbitrary
valid finite graphs with fixed topology, external rates and route flows during the
forecast. Lean/Mathlib versions and dependencies are pinned. The default build audits
the transitive assumptions of 15 advertised theorems; the verifier also checks that
an intentionally unapproved assumption is rejected. Details, theorem names, C++
mapping and executed evidence are in [proofs/README.md](../proofs/README.md).

| Proven statement | What it justifies |
|---|---|
| Sequential source-subtract/target-add edge updates equal the independently defined dense incidence update | Replacing the dense mathematical update with sparse edge traversal |
| Internal edge contributions sum to zero | Internal transfers preserve total stored volume; external terms account for net change |
| Repeated constant-rate updates equal the affine trajectory, including a shortened last sample interval | Computing exact-real states directly instead of accumulating Euler steps |
| Storage bounds at both endpoints imply storage bounds throughout the constant-rate interval | Screening storage over the entire forecast using endpoint bounds |

The last result supplies a sufficient test for **storage bounds under these
assumptions**. It is not a proof that a candidate satisfies every application
constraint: native validation separately checks controls, pump availability, route
and shared-destination capacities, conservation, costs and run provenance. The proofs
do not establish global optimality or robustness of a recommended intervention.

The C++ correspondence is documented rather than formally derived. Parser/index
construction, compiled memory safety, IEEE floating-point rounding, nonfinite inputs,
tracer PDE discretization, ML predictions and the optimizer are outside the formal
scope. Native reference comparisons and sanitizer checks cover implementation evidence
separately. Exact-real equality does not authorize arbitrary floating-point reordering.

## Proof-backed performance

Lean checking does not make a running simulation faster. It supports the reasoning
for using cheaper algorithms under explicit conditions. The current sparse traversal
and affine endpoint screen already use the mathematical transformations above.
For the recorded custom 24-tank/60-internal-edge graph at 10,000 candidates, median
sampled dense/sparse times were 1,110.62/143.15 ms. Affine screening took 2.63 ms,
but omits sampled trajectories and has a different timing scope. These measurements
are machine/workload-specific; see the benchmark report before comparing ratios.

A possible future optimization is to precompute how selected controls change tank
rates, destination totals and synthetic cost for a captured network, then evaluate
candidates through those smaller affine maps. It would need its own equivalence
proof, checked applicability conditions, native-reference validation and profiling.
It is not implemented by A6.
