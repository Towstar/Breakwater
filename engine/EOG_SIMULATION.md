# Stateful EOG synthetic plant

Private native method **eog-synthetic-events-0.1.0**, corrected source **af73a42**,
in `include/breakwater/eog_simulation.hpp` / `src/eog_simulation.cpp`. This is an
additive C++20 core independent of Qt, Python, JSON, files and model APIs. It does
not replace the workbench or the protected-capture analysis provider.

The plant copies an explicit scenario: indexed wells/facilities/system fleets,
initial inventories/pump states, imported operating/economic parameters, a bounded
horizon, and explicit future failure/repair and rate-change schedules. Initial
conditions and future forcing are synthetic assumptions, not an extrapolation of
recorded telemetry. Geometry never sets flow. The current `Snapshot` exposes no
future incident schedule or repair endpoints; the full scenario remains private
inside the plant. A separately tested causal sensor projector is still required
before detector/planner connection.

## Executing a session

Construct `Simulator(scenario)`, then advance monotonically to observation/control
times with `advance(elapsed_s, cancellation_callback)`. `snapshot()` returns copied
current state/ledgers; `trace()` retains bounded event/sample points. `reset()`
constructs the same initial plant and clears all actions/history. A completed or
failed/cancelled session accepts no more controls. Cancellation is also honored
at the current clock. Snapshots/input mutations cannot change the copied plant.

At the current clock, `dispatch(facility, system_truck_index, trips)` reserves that
physical truck through its last pickup plus final return. Busy trucks cannot be
reassigned. A later job begins at the yard and pays the new facility's response
time. Repair never releases a truck. Pickups must advance and arrive strictly
before the current failure's repair. Full stock is checked at execution; a short
pickup stops with `invalid_action`, retaining its raw prefix without a partial
load or certified aggregate cost. Simultaneous pickups at a facility check their
combined demand before stock/cost changes. Calls made individually at the same
clock are serialized commands; a failed later command cannot undo an earlier one.

`shut_in(well)` requires an active failure and known production eligibility. It
lasts through repair. Voluntary and automatic masks form a union per well; loss
is counted once. Repairs immediately resume initially productive wells using the
current explicit rates. Preexisting nonproductive wells remain ineligible. The
normal pump controller retains hysteresis memory within its band, starts at the
start threshold and stops at the stop threshold. During failure flow is zero;
reaching shutdown latches the wells off until repair. A late pickup never clears
that latch. Healthy inflow exceeding available pumping at physical capacity is
explicitly unavailable, without clipping. Pump capacity above pipeline capacity
is an unsupported configuration, not an inferred hydraulic correction.

## Time, inventory and cost semantics

Units are binary64 elapsed seconds, bbl, bpd, mcfd and synthetic currency. Water
rates divide by 86400; daily oil/gas revenue is computed before that division,
matching E3. Repairs are represented as configured durations after explicit
synthetic onsets. A repair and new failure may touch but cannot overlap on one pump.

Continuous algebraic threshold events precede repairs/rate changes/pickups at an
exact clock tie. Repairs clear their old masks before a new failure at that clock.
The controller's external command executes after `advance()` has processed the
events at its clock. First/repeat arrivals and half-open final-return occupancy
use the configured elapsed seconds directly, without hour rounding.

As in E3, a derived affine crossing is anchored at its algebraically known boundary.
This is an event equation, not a correction of invalid input/control inventory.
The signed discrepancy from the materialized affine volume at its rounded timestamp
is reported as `event_boundary_rounding_bbl`. The **raw** physical mass residual
is also reported and is never zeroed by that audit term. Observation samples and
zero-effect forcing do not reset physical affine anchors. Per-well economic
anchors are independent of pump/pickup anchors, so harmless breakpoints do not
split a continuous shut-in loss into different rounded intervals.

Unrepresentable rates/products/times and nonfinite derived values return explicit
unavailability (or reject unsupported construction). Running/completed supported
prefixes have aggregate costs. Invalid, unavailable, bounded or cancelled runs
have no certified aggregate cost; their finite retained prefix remains evidence.
No formal proof, real-number arithmetic or universal physical accuracy is claimed.

Bounds: horizon at most 31 days; 64 systems, 256 facilities, 128 wells/facility,
16,384 wells, 128 trucks/system, 16,384 failures and rate changes each, 4,095
trips/job, 16,384 dispatched trips, 262,144 retained points and one million event
iterations. Smaller explicit bounds are supported. A bound stops the session;
histories are not silently decimated. Native event processing is cooperative, not
a hard real-time guarantee.

## Executed evidence and next boundary

`science/evidence/eog-simulation/` pins source/binaries and actual normal/ASan logs.
Both modes pass **12/12** native CTests; the new suite executes **57,498 checks**.
It includes analytical hysteresis/trip/repair/loss/load cases, persistent shared
fleet and sequential jobs across repairs, 100 long-run observation-partition
comparisons, 100 actual E3 evaluator comparisons, a 31-day periodic-cycle oracle,
exact reset repeatability, cancellation, output bounds and rejected invalid input.

The original month-scale probe failed with 2.5875685963683281e-5 bbl cycle error.
Algebraic events reduced it to **4.9828259562900712e-7 bbl** without changing the
1e-5 acceptance tolerance. Raw mass residual is **-8.5472594690827464e-7 bbl**;
signed event-boundary rounding is **6.9979188310753671e-7 bbl**. Initial source
68686c1 and the unsuccessful predecessor-time attempt are retained, not erased.

Still required: real imported model/identity mapping, seeded scenario/provenance
generation, the causal sensor projector, stateful planner commitment support,
an adopted B-owned simulation boundary, recorded replay/counterfactual/extended
application controls, installed/rendered checks and broader unseen-network
qualification. Leakage/degradation forcing and arbitrary dynamic economics are
not silently supplied. This native increment alone completes none of those gates.
Lucas owns desktop/contracts/root delivery and must publish/coordinate that mapping
before concurrent application implementation; A owns its numerical bridge/evidence.
