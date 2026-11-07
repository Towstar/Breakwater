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
robustness, chemical quality, or optimality. Quality is not evaluated in the base.

## Evidence boundaries

Native tests cover canonical cases, capacity equality, arbitrary renamed assets,
shared destination limits, unclipped negative predictions, malformed controls/topology,
tampered trajectories/costs, requested horizon, random graph equivalence/conservation,
and concurrent candidate isolation. See STATUS for actual executed commands.

The affine endpoint function accepts caller-owned workspace and allocates no memory
inside its evaluation loop. No benchmark speedup has been measured yet.

Lean proofs, transport, PyTorch gradients and PhysicsNeMo execution remain planned.
No formal safety guarantee is claimed for the C++ implementation.
