# G1: Person A's numerical observation requirements

Prepared against merged main `de06aa1`. This is scientific guidance for B's
sensor/reading/snapshot contract review, not an implemented telemetry interface.
B publishes the shared contract; both workstreams adopt it before G2 mappings.
All examples and controls are synthetic. Current storage and transport APIs,
schemas, numerical methods, dependency pins and approval rules remain unchanged.

## State, commands and measurements

Keep project design/commands, synthetic evaluator truth, received measurements,
accepted observed snapshots and immutable runs distinct. In particular, a flow
measurement is not a command. A pump commanded to deliver 44 m3/h but observed to
deliver zero has not recovered when a candidate changes its setpoint. A forecast
needs a declared availability/delivery assumption for that intervention.

Observed inventories may initialize a *new* forecast. Measured rates may inform
its declared constant-rate assumptions. Neither rewrites an old native capture,
the editable nominal commands, approval or the ground-truth fault schedule.
Pressure/temperature/custom channels remain display/alarm evidence until supported
equations and a reviewed mapping exist; there is no pressure-hydraulics model.

## Observation operators and numerical examples

| Measurement | Required interpretation | Worked review example |
|---|---|---|
| Inventory | Canonical m3, attached to a stable tank ID; original unit/value retained. | 80,000 L is 80 m3. A reading of 132 m3 against capacity 130 remains 132 and is an alarm; it is never clipped to 130. |
| Level | Canonical m plus an explicit versioned, strictly increasing level-to-volume calibration; no inferred cylinder dimensions from rendering. | Knots `(h,V)=(0,0),(1,30),(2,80)` in m/m3 give `V(1.4)=30+0.4*50=50 m3` by piecewise-linear interpolation. |
| Flow | Canonical m3/h, stable route/source attachment, physical sign/direction and measured-versus-assumed status. | Source 52 minus curtailment 10 commands 42; an independent meter reading 40 is retained as 40 and a delivery discrepancy, not reset to 42. |
| Pump status | Typed status with separate commanded-off, running feedback, delivery loss and unavailable meanings. | Commanded 44 with measured zero and fault feedback may support an unavailable snapshot; commanded-off alone does not establish a fault. |
| Concentration | mg/L, physical pipe position and spatial/time measurement support, calibration and observation uncertainty. | With four 0.25 m cells containing `[0,2,4,6] mg/L`, the piecewise-constant interval average on `[0.125,0.625] m` is `(0.125*0+0.25*2+0.125*4)/0.5 = 2 mg/L`. |

Level calibration requires finite ordered coordinates and strictly increasing
volumes over its supported domain. Invalid/flat/reversed calibration is rejected.
Out-of-domain readings are retained and flagged; do not silently extrapolate or
clamp them to create a supported initial condition. Within the second interval in
the example, a height error bound 0.01 m contributes 0.5 m3. Adding a declared
0.2 m3 calibration error gives a conservative 0.7 m3 bound, not a standard deviation
or fault probability. At a knot use the larger adjacent slope for this bound.

Native concentrations are cell averages. A point instrument at a cell boundary
does not uniquely determine a point value from those averages. Specify a supported
spatial averaging/reconstruction operator and its error; do not relabel a cell
average as an exact point measurement. Likewise the native outlet flux uses the
last cell average as a face approximation. F1 reports its error against the physical
outlet separately from the last-cell-average error. During zero flow there is no
discharged-effluent concentration observation, although the static pipe field
still exists. A time-averaging instrument also needs its acquisition window and
delay; compare against the same window, not an instantaneous state.

For area 0.2 m2 the interval in the concentration example contains 100 L, so its
tracer inventory is 200 mg. The factor 1000 converts m3 to L. Marker movement is
layout-only; physical location, attachment, calibration or averaging-support
changes alter the observation operator and retire dependent live evidence.

## Coverage, coherence and uncertainty

An independently measured tank balance needs inventory at both interval endpoints
and integrated measured inflow/outflow over that interval. Share a single internal
route observation between adjacent tank balances so its contributions have opposite
signs; do not fabricate independent measurements of the same face.

For an interval of 0.1 h, inventories 80 and 80.5 m3 and constant measured inflow
and outflow of 42 m3/h give

```text
r = 80.5 - 80 - 0.1*(42 - 42) = 0.5 m3.
```

With each inventory bounded by 0.2 m3 and each flow by 0.5 m3/h, a conservative
measurement-only bound is `0.2+0.2+0.1*(0.5+0.5)=0.5 m3`. Timing, quadrature,
calibration dependence and model error need additional declared terms. This example
does not establish an abnormality or distinguish a leak from a biased instrument.
Correlated/redundant sensors do not automatically reduce uncertainty by averaging.

Event time, acquisition interval and receive time must remain separate. B's contract
must define stream epoch, age/skew, ordering, units, calibration/configuration
identity, primary/redundant policy and quality. A snapshot must state which values
are measured, converted, inferred or assumed and retain the original observations.
Compute an innovation against a prediction captured *before* its target observation
is assimilated; comparing a reading to a state overwritten with itself is circular.

Missing/old/conflicting observations yield unavailable or explicitly degraded
results with named assumptions. Missing flow replaced by a command is an assumed
balance term, not an independent measured balance. A finite negative inventory or
over-capacity reading remains visible alarm evidence; if it cannot form a valid
native state, capture creation fails explicitly without repairing it. Missing pump
feedback never silently means healthy. Conflicting primary/redundant readings
cannot use last-writer-wins state assignment.

## B's G1 review checklist and A's later work

B should translate the semantics above into additive contracts with valid/invalid
examples, compatibility, identities and bounded storage/input behavior. Review the
worked calibration, conversion, concentration-support, failed-delivery, residual,
missing-coverage and conflicting-sensor examples together before adoption. This
note defines no new wire version or precedence rule in existing v1 schemas.

After adoption, A implements pure observation/calibration mappings and the seeded
publisher, then residuals and detectability evidence. B implements editor/ingestion,
coherent snapshots, events, persistence, freshness and simulated approval checks.
No physical adapter, sensor runtime, learned detector, RBM training or real actuation
is delivered by this note. B can incorporate this A-owned handoff into shared STATUS.
