# Science and mathematics for Devpost

Use the first paragraph directly. The remaining equations are optional talking
points, checked against the implemented source rather than the original proposal.

## Copy-pastable project paragraph

Breakwater combines conservation laws, event-driven simulation and model-led
investigation. A native C++ engine forecasts storage using mass-balance ODEs,
models pump hysteresis and failures, and evaluates discrete truck pickups and
well shut-ins against shared fleet calendars and synthetic economic costs.
Candidate plans are checked by a separately implemented numerical validator.
The experimental pipe workbench solves a one-dimensional advection–dispersion–
reaction PDE with a conservative finite-volume scheme and an explicit tracer
mass ledger. Our new PhysicsNeMo tool learns a first-order decay coefficient
from sparse observations of a closed tracer test, checks the estimate against an
independent scalar fit, and offers it for explicit review before native forecasts
use it. PyTorch sensitivities and checked Lean algorithm proofs provide additional
scientific support. The numerical solver and independent checks remain
authoritative; Nemotron investigates and proposes experiments rather than
dispatching equipment. All data, costs and controls are synthetic.

## How each model is enforced

The software enforces its declared equations, units, operational rules and
acceptance checks. It cannot enforce that a physical facility actually obeys an
assumed constant-flow, first-order or one-dimensional model. Synthetic tests and
small residuals do not establish empirical physical accuracy.

| Topic | Enforcement in the program | Independent check and practical limit |
| --- | --- | --- |
| Storage conservation ODE | Validate topology, finite inputs, units and control bounds; subtract each internal flow from its source and add it to its destination; evolve constant-rate intervals in double precision. Invalid predicted volumes are retained, not clipped. | ID-based reference recomputes rates, trajectory, capacity limits and conservation separately from compiled sparse arrays. Dense/sparse/affine comparisons and analytical fixtures check implementations. Fixed flow is a modeling assumption, not pressure hydraulics. |
| EOG hysteresis and discrete events | Explicit pump on/off/failed state, configured start/stop/shutdown thresholds and repair times; advance between event roots; apply full truck-load jumps only with sufficient inventory. Threshold crossings precede pickups at exact ties. | Known-value, simultaneous-event, one-ULP-neighbor and stateful replay cases exercise the event rules. Numerical inability to represent a supported event returns unavailable; no hidden clipping or time rounding creates feasibility. |
| Shutdown countdown | Bind the forecast to a causal captured state, active well rates, configured threshold and remaining repair horizon. Solve the continuous crossing within each supported constant-rate segment; already tripped is immediate. | Independently recompute crossing time and retained trajectory. Stale, missing or contradictory observations make unsupported results unavailable. This enforces consistency with the available data and stated rate assumption, not knowledge of the true future. |
| Trucking, shut-ins and economics | Validate well/facility IDs, full loads, pickup inventory, strict pickup-before-repair deadlines and nonoverlapping shared-truck occupancy through return. The private committed-state evaluator also carries existing reservations/pending pickups forward and excludes sunk costs. Charge lost production over the union of voluntary/automatic shut-in time. | Independent evaluator checks exact retained plan resources, timelines, loss durations, costs and volume balance; analytical optima and generated/adversarial cases check errors. The public planner remains snapshot-based; authenticated state compilation and evolving dispatch/replanning UI are still outstanding. Search budgets and incomplete coverage are exposed; a search score alone cannot certify feasibility or global optimality. |
| Transport PDE and finite volumes | Validate geometry, concentration/source/decay/dispersion units and bounds, positive-flow schedules and boundary compatibility. Use a shared flux at each cell face and an advection–diffusion–reaction timestep bound. Budget cells, frames and updates; reject materially negative/nonfinite outcomes instead of clipping. | Expose the complete tracer mass ledger and residual; compare against independent analytic/reference cases and refinements. Conservation is necessary but does not prove that the chosen scalar PDE describes actual chemistry or mixing. |
| PhysicsNeMo decay inference | Require known positive initial concentration, 3–64 valid observations with increasing known times and adequate spread; use a bounded coefficient, CPU float64 optimization, a 45-second job limit and cancellation. The estimate binds to route/parameters/network/revision and requires an explicit apply action. | A separate scalar least-squares coefficient and raw concentration RMSE gate the fitted result. Changed inputs prevent stale application; explicit application clears prior forecast/approval eligibility before a new native calculation. The software cannot verify that an entered experiment was truly closed, well mixed, source-free or measured correctly. Passing the fit gate is not physical certification or a calibrated uncertainty bound. |
| Automatic differentiation | Differentiate the implemented smooth storage rollout for explicitly supplied continuous controls; validate finite Jacobian/gradient dimensions and bind outputs to the captured numerical context. | Compare forward values with native results and derivatives with finite differences. This checks derivatives of that model; it does not differentiate discrete trucks, event switches or approval logic. |
| Lean proofs and separate PINN studies | Lean checks algorithm theorems and their explicit hypotheses with a pinned toolchain and assumption audit. The standalone PINN minimizes a transport PDE/initial-data loss with declared boundary handling. | Proofs cover mathematical update identities and conditional invariants, not every C++ floating-point operation, memory safety or the application. PINN residual/reference studies are numerical evidence, not a proof of generalization or a deployed replacement for native validation. |

The application uses precise double-precision semantics; checked mathematical
proofs over real-valued algorithms do not automatically prove the compiled binary.
Native regression tests and AddressSanitizer runs are additional, separate
evidence. No model or optimizer result can dispatch equipment or bypass explicit
human review of simulated controls.

Combined submission evidence reported by the integrator: the actual desktop pipe
suite passed **16/16 with no skips**, including real C++ → PhysicsNeMo worker →
explicit apply → forecast invalidation, and project tests passed **13/13 with no
skips**. Full science discovery passed **459 tests** at the pre-split test source;
the final discovery layout has 456 default tests plus three separately invoked
real PhysicsNeMo qualification tests. Keep those source/test-layout distinctions
when quoting totals; they are not a new physical validation study.

## 1. Network storage: conservation-law ODEs

For tank `i`, with volume `V_i` in m³, time in hours and flows in m³/h:

```text
dV_i/dt = sum(incoming flows) − sum(outgoing flows) = r_i
V_i(t) = V_i(0) + t r_i                 [constant-flow interval]
```

In graph notation, `dV/dt = Bq + b`, where the incidence matrix `B` subtracts
each internal edge flow from its source and adds it to its destination.
Internal transfers cancel in the total-volume sum. The native implementation
has sparse edge updates, an independent dense reference and an affine solver.
Its validator checks storage bounds, route/control limits, shared destination
throughput and the volume balance. With constant rates, checking both endpoints
establishes storage bounds throughout an interval. Invalid volumes are retained
as evidence rather than silently clipped.

**Say:** “We use conservation laws, not an arbitrary animation, to determine tank
levels.” This is a capacity-limited fixed-flow model, not pressure hydraulics.

## 2. EOG operation: a hybrid continuous/discrete model

The separate EOG engine uses barrels and seconds. Between events:

```text
dV_f/dt = sum(active-well water_bpd)/86400 − pump_bpd/86400
V_f(t_pickup+) = V_f(t_pickup−) − number_of_full_loads × truck_capacity_bbl
```

Pump flow is zero when failed or off. Healthy pumping switches on at the upper
start threshold and off at the lower stop threshold: this hysteresis naturally
produces tank sawtooth cycles. A failed facility crossing its configured shutdown
threshold automatically shuts in its productive wells. Repair, well shut-in,
truck arrival and return-to-yard are explicit events. Full pickups require actual
inventory; trucks remain occupied until return, including between repeat loads.
Exact event ties follow a declared order; a threshold crossing is processed
before a pickup at the same instant.

For a supported constant positive net inflow `r`, the threshold countdown is:

```text
T_shutdown = (V_shutdown − V_now)/r
```

An already reached threshold gives an immediate trip. A nonpositive rate does not
produce this positive countdown. Changing controls/rates requires new piecewise
event calculations, and predictions stop at the supported repair/economic horizon.

**Say:** “The EOG simulator is a hybrid dynamical system: continuous inventory
evolution plus discrete operational events.” The extended stateful EOG core and
commitment extensions are implemented scientific work; their full evolving
operator workflow remains separate from the recorded EOG console.

## 3. Trucking and well decisions: a constrained economic objective

For each productive well, define synthetic lost revenue per second:

```text
R_w = (oil_bpd × oil_price_per_bbl + gas_mcfd × gas_price_per_mcf)/86400
J = trip_count × cost_per_trip + sum_w R_w × lost_production_duration_w
```

Lost production is the union of voluntary and automatic shut-in time through
repair, so it is not charged twice. A shared system truck cannot occupy
overlapping dispatch/return intervals. Plans also respect travel/response times,
full-load inventory and strict pickup-before-repair deadlines. The commitment
extension carries already reserved trucks, pending pickups and previously stopped
wells into a future evaluation, without charging sunk costs again.

We compare no action, truck-only, well-only and mixed responses on the same causal
snapshot. Search is bounded and reports its actual coverage. A separately coded
reference recomputes retained-plan timing, resources, crossings, conservation and
itemized costs.

**Say:** “We search for lower modeled cost while preserving operational
constraints, then independently validate the exact retained plan.” Use
**best found within the explored space**, unless a particular result explicitly
proves complete enumeration of its stated finite space. Do not claim universal
or continuous global optimality.

## 4. Pipe transport: an advection–dispersion–reaction PDE

The actual experimental native pipe panel solves:

```text
∂C/∂t + u ∂C/∂x = D ∂²C/∂x² − k C + s
u = Q/A
```

`C`: mg/L; `x`: m; `t`: h; `u`: m/h; `Q`: m³/h; `A`: m²;
`D`: m²/h; `k`: 1/h; `s`: mg/L/h. Open pipes use specified inlet concentration
and zero diffusive gradient at the advective outlet. The separate closed option
has zero boundary flux and requires zero through-flow.

The finite-volume update uses shared face fluxes and explicit Euler stepping:

```text
F_(j+1/2) = u C_j − D (C_(j+1) − C_j)/Δx        [positive flow]
C_j^(n+1) = C_j^n − Δt/Δx (F_right − F_left) − Δt k C_j^n + Δt s_j
```

The timestep is bounded by advection, diffusion and reaction rates. The native
open-boundary bound includes the half-cell inlet term:
`Δt [u/Δx + 3D/Δx² + k] ≤ CFL_safety`; the closed zero-flow case uses `2D/Δx²`.
Concentrations are not silently clipped. The mass ledger checks:

```text
M = 1000 A sum_j C_j Δx
residual = M_final − M_initial − inlet_mass + outlet_mass − source_mass + decay_mass
```

**Say:** “We use conservative finite volumes and expose the tracer mass balance.”
This scalar demonstration is not a general chemistry, treatment or pressure model.

## 5. PhysicsNeMo: sparse parameter inference in the actual workbench

The new **Learn decay · PhysicsNeMo** tab fits a first-order coefficient from a
closed, well-mixed tracer test with known initial concentration and elapsed times:

```text
dC/dt = −k C,       C(0) = C0,       C(t) = C0 exp(−k t)
k = 10 sigmoid(PhysicsNeMo_FullyConnected(0))
training loss = mean[(−k t_i − log(C_i/C0))²]   [a fixed time scaling is applied]
independent k_reference = −sum_i t_i log(C_i/C0) / sum_i t_i²
```

A real PhysicsNeMo network is optimized with CPU float64 PyTorch LBFGS. The
independent stdlib fit must agree within `1e-6 + 1e-5 |k_reference|` per hour and
concentration RMSE must be at most 5% of `C0`. There must be 3–64 observations
with adequate known time spread. Applying an accepted estimate is explicit and
requires new native forecasts; fitting alone changes no project input.

**Say:** “PhysicsNeMo estimates an unknown decay parameter from sparse tracer
observations; native transport uses the reviewed physical parameter.” This is a
bounded inverse problem with an analytical forward solution, not a learned
general PDE surrogate. It does not infer flow, dispersion, unknown sources or
calibrated uncertainty. Synthetic recovery tests demonstrate implementation
behavior, not validation against physical laboratory data.

## 6. Optional deeper science: sensitivities, PINNs and Lean

- **Implemented PyTorch storage sensitivities:** differentiable fixed-flow rollout
  computes terminal-volume Jacobians and synthetic cost gradients. For constant
  flow, `∂V(T)/∂a = T ∂r/∂a`. Native forward comparisons and finite differences
  check these derivatives. Smooth interior controls are supported; discrete truck
  choices, automatic event switches and approvals are not differentiated.
- **Separate implemented PhysicsNeMo PINN experiments:** the neural field learns
  against initial data and the transport PDE residual
  `C_t + u C_x − D C_xx + k C`, with explicit boundary treatment and independent
  references. These standalone studies are separate from the desktop calibration
  tool; do not describe them as a qualified production surrogate in the UI.
- **Checked Lean algorithm proofs:** sparse/dense graph update equivalence,
  internal-transfer conservation, constant-rate affine trajectories, endpoint
  bounds, discrete finite-volume mass balance and conditional nonnegativity.
  These concern mathematical algorithms under stated assumptions, not a formal
  proof of every floating-point operation, all application code or ML predictions.
- **Causal observation analysis:** detection uses only the captured telemetry
  prefix, observed operating state and configured freshness/coherence policies.
  Ambiguous or stale evidence is explicitly unavailable. Do not claim a calibrated
  confidence interval or an oracle that reads future failure labels.

No separate explicit discrete-adjoint solver was verified in the current source;
describe the actual PyTorch automatic differentiation rather than adding that claim.

Source anchors: `engine/src/engine.cpp`, `engine/src/eog_response.cpp`,
`engine/src/eog_simulation.cpp`, `engine/src/transport.cpp`,
`science/eog_response.py`, `science/eog_plan_validator.py`,
`science/transport_calibration.py`, `science/breakwater_science/rollout.py`,
`science/breakwater_science/transport_pinn.py`, `science/worker_storage.py`,
and `proofs/README.md`.
