# Hypothetical sensors, synthetic live telemetry and fault detection

Accepted planning extension, November 7, 2026 (America/Chicago).
All features in this document are **planned**, unless current implementation is
explicitly identified. This document does not claim that sensors, ingestion,
detection or a learned detector have been implemented or tested.

Read [CONTINUATION_PLAN.md](CONTINUATION_PLAN.md) for the complete next-step order,
[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) for the desktop architecture and
[PARALLEL_WORK.md](PARALLEL_WORK.md) for ownership.

## 1. Scope and operating modes

Sensors are hypothetical equipment placed in the synthetic facility. "Synthetic
live" means a running synthetic plant publishes time-stamped readings continuously;
it does not imply a connection to physical equipment. Provide distinct Manual,
Synthetic live, Replay and Historical views with visible source and time status.
No hardware, industrial-network access or real actuation is required for acceptance.

Use one typed ingestion boundary for the synthetic publisher and recorded replay.
A future physical adapter can adopt that boundary through a separately reviewed
extension. MQTT, OPC UA, remote listeners and device discovery are deferred.
Start with bounded local NDJSON input and an in-process test adapter.

Keep these separate:

1. Editable project design, nominal configuration and commanded simulated controls.
2. Synthetic plant truth, including disturbances known only to the evaluator.
3. Reported sensor measurements, their quality and acquisition history.
4. Accepted observed-state snapshots used for numerical forecasts.
5. Immutable forecasts, investigations, validations, approvals and historical records.

A measurement never overwrites a commanded setpoint. A synthetic plant may fail to
deliver a commanded flow; that difference is evidence to investigate. New readings
do not mutate existing runs or create project undo entries on every packet.

## 2. Configurable sensor objects

Person B owns serialization, shared schemas and editor behavior. Person A reviews
the physical interpretation before the contract is committed and adopted.

Each sensor definition needs:

- Stable sensor ID, label, measurement type, declared units and source/channel.
- Attachment to a stable asset or route ID, with a visible selectable scene marker.
- Layout anchor/offset, independent of physical measurement coordinates.
- An explicit physical location where relevant: for example pipe distance in metres.
- A versioned calibration/mapping definition, instrument limits, expected sampling
  interval, maximum acceptable age, declared uncertainty and enabled state.
- Optional alarm settings and an explicit primary/redundant-sensor policy.

Place, select, move, configure and delete sensors; include configuration in atomic
project save/load and undo/redo. Moving a display marker is layout-only. Changing its
attachment, physical location, calibration or numerical mapping retires dependent
evidence. Deleting an asset/route handles attached sensors explicitly. Conflicting
sensors cannot silently compete through last-writer-wins assignment.

| Type | Canonical unit | Initial numerical use |
|---|---|---|
| Tank inventory | m3 | Observed initial storage for a new forecast. |
| Tank level | m | Convert using a configured, validated level-volume relationship; rendered cylinders do not establish physical dimensions. |
| Source/route flow | m3/h | Measured flow evidence, balance residuals and explicitly declared forecast assumptions. |
| Pump fault/availability/status | Typed Boolean or enumerated status | Explicit mapping to observed state; commanded-off, running feedback and unavailable are distinct. |
| Concentration | mg/L | Observation at a declared pipe location compared with the native transport field under a documented observation operator. |
| Pressure/temperature | kPa / degC | Display, history and configured alarms; current storage/transport physics does not predict these quantities. |
| Custom numeric channel | Explicit configured unit | Display/history only until A defines and B publishes a supported model mapping. |

For concentration, specify cell-average versus point/interval measurement semantics,
interpolation/averaging and observation uncertainty. An arbitrary scene coordinate
cannot silently become a PDE boundary condition or a physical pipe length.

## 3. Ingestion, clocks, quality and bounded storage

Publish a small additive contract before concurrent implementation. Proposed concepts
include sensor configuration, reading/batch, observed snapshot, detection event and
telemetry provenance. These are planning concepts, not existing wire schemas.

An illustrative synthetic reading is:

~~~json
{
  "telemetry_version": 1,
  "stream_id": "synthetic-run-001",
  "sequence": 42,
  "sensor_id": "tank-a-volume",
  "event_time_s": 12.5,
  "value": 63.1,
  "unit": "m3",
  "quality": "good",
  "source_kind": "synthetic"
}
~~~

Before adoption, B specifies exact fields/types, enum values, errors and byte/count/
rate/history limits with valid and invalid examples. Retain receive time separately
from event time. Bind relative times to a stream epoch; replay/restart creates a
distinct epoch. Forecast horizons remain relative to their captured observation time.

- Validate IDs, attachment/type compatibility, finite values, units, conversions and
  configuration versions. Retain original values alongside canonical conversions.
- Define duplicate, reordered, late, future-dated and restarted-stream behavior.
  Use bounded buffering/watermarks and explicit age/skew limits for coherent snapshots.
- Show good, stale, missing, rejected and inconsistent states. Missing required data
  produces an explicit unavailable/degraded forecast with named assumptions, not a
  zero-flow or healthy-equipment default.
- Preserve finite observations outside model bounds as alarm evidence. Malformed
  readings are excluded visibly. Do not clamp observations, inventories or controls.
- Retain bounded recent traces and immutable event/snapshot evidence; expose dropped
  packet/backpressure counters and storage failures. Keep acquisition off the UI thread.
- Validate a complete snapshot before using it; report unrepresentable/contradictory
  state instead of silently repairing the network to obtain a feasible forecast.

New optional serialization must keep existing sensor-free projects and historical
archives readable. B decides the extension/version policy in the contract increment;
do not change version-1 semantics in place.

## 4. Synthetic publisher and replay

A owns deterministic plant/sensor generation under science/ and scenarios/, plus any
pure numerical support in engine/. B owns launch wrappers, lifecycle, ingestion and UI.
No runtime networking, Qt or model API dependency enters the numerical engine.

The publisher has a declared plant configuration, clock, seed, sensor placements,
sample intervals, noise/bias model and fault schedule. Provide pause/resume, bounded
speed changes and restart/replay. Publish observations through the same ingestion
path used by tests; no direct writes into ProjectController's current run.

Truth evolves separately from the forecasting capture. Include pump delivery loss,
changing inflow and an unmetered discharge, then independent sensor bias, drift,
stuck values, noise, dropouts, delay, duplication, reordering and recovery. Distinguish
plant faults, sensor faults and normal operating changes. Any plant dynamics beyond
the existing constant-flow segment model need explicit A-owned equations and tests.

Ground-truth fault labels and onset times are available only to the evaluation
harness, not to detectors or Nemotron. Reusing engine mathematics is acceptable for
declared nominal cases, but also test parameter mismatch and unmodelled disturbances.
Identical generator and predictor output alone does not validate detection.

## 5. Detection architecture and uncertainty

The storage model is currently a constant-flow ODE/balance model; selected-pipe
transport adds a scalar PDE. Solver correctness and small PDE discretization residuals
do not establish agreement with incoming observations. Detection needs an explicit
measurement comparison and a decision rule.

Use a layered implementation, with each layer reporting evidence and availability:

1. Sensor integrity: timestamps, missing/stale data, invalid units, implausible jumps,
   disagreement and stuck-signal evidence. A flat signal alone does not prove failure.
2. Direct rules: high/high-high inventory, observed pump faults and sustained delivery
   shortfall, with configured persistence/hysteresis and documented operating modes.
3. Physical residuals: measured inventory versus a prior prediction, integrated flow
   balance and compatible selected-pipe concentration observations.
4. Statistical change detection: evaluate EWMA/CUSUM on suitable residuals for small
   persistent departures, with regime handling and explicit noise/model-error budgets.
5. Learned detector comparison: include an RBM as an experimental candidate, alongside
   simpler baselines, after the dataset and evaluation harness exist (section 6).

An initial tank residual, in m3, can be defined as:

~~~text
r[k] = V_measured(t[k]) - V_measured(t[k-1])
       - integral(t[k-1], t[k], Q_in_measured - Q_out_measured) dt
~~~

A must specify event alignment, integration, required sensors and error budgets.
Missing flow measurements cannot be replaced with setpoints and still be described
as an independently measured balance. Label assumed terms explicitly.

Compute forecast innovations against the prediction saved before assimilating the
new measurement. Overwriting the initial state with the same reading and then
comparing it to that state would conceal discrepancies.

Report fault detection separately from fault isolation. A balance error can indicate
unmetered loss, a biased sensor, a timing error or model mismatch. Publish a
detectability matrix showing which sensor combinations support which conclusions.
Return "abnormal, cause unresolved" when evidence cannot distinguish hypotheses.
Thresholds/scales include declared sensor noise, timing and model error; a configured
uncertainty band is not a calibrated fault probability.

B implements event IDs, onset/update/clear/acknowledgement, severity, sensor/asset
links, persistence, hysteresis and evidence views from A's specified quantities.
Acknowledging an event does not make the system healthy or authorize a control.
Nemotron can investigate captured evidence through bounded tools; numerical rules
and validation retain their authority.

## 6. RBM and other learned-detector experiment

No detector has been shown optimal for BREAKWATER. An RBM is a valid candidate for
learning multivariate operating patterns. Existing nuclear-plant research includes
RBM-pretrained deep belief networks on simulated accident data; that is relevant
precedent, not BREAKWATER acceptance evidence or proof that a standalone RBM wins.

Person A owns a bounded offline comparison after G2/G3:

- Predeclare the task: anomaly detection from healthy training data versus supervised
  fault classification. Do not substitute known-class accuracy for unseen-fault tests.
- Compare direct/residual rules, an appropriate EWMA or CUSUM baseline, and an RBM;
  add a PCA or autoencoder baseline only if the declared comparison needs it.
- Consider a hybrid feature vector of physical residuals, measured values, commands
  and operating mode. Specify actual units/normalization and continuous/discrete
  visible-unit choices; binary RBMs are not the only RBM formulation.
- A plain RBM over one vector has no explicit chronology. Declare causal lag windows,
  trend features or a conditional/temporal extension when detecting dynamic faults.
  Treat fixed sensor order/topology and missing-channel handling as model contracts.
- Specify the anomaly score and calibration. Free energy/reconstruction scores are
  not automatically calibrated fault probabilities. Low reconstruction error alone
  does not establish a well-trained distribution or detection performance.
- Split entire runs, seeds, parameter settings and operating regimes before fitting
  preprocessing or window construction. Tune only on training/validation sets; freeze
  held-out tests. Prevent overlapping-window leakage and generator-label leakage.
- Include healthy transients, operating-mode changes, sensor loss, model mismatch,
  unseen fault severities/types and new supported network configurations. State
  unsupported topologies and abstain outside the declared domain.
- Compare event-level recall/misses, false alarms per simulated hour, detection delay,
  diagnosis confusion/abstention, latency and memory using the same evaluation rules.
  Report seeds/variation, actual commands, failures and thresholds selected beforehand.

Adopt a learned method only on demonstrated benefit at an agreed false-alarm/latency
budget. An inconclusive or negative comparison is a valid result. No training,
dependency installation or learned production integration is authorized merely by
reading this plan; each is a separately scoped implementation increment.

B publishes a distinct advisory detector-result contract before runtime integration,
including feature/model versions, score meaning, domain and unavailable states.
Neither RBM nor Nemotron may waive a constraint or apply a control.

## 7. Forecasts, incidents, history and simulated approval

Capture accepted observations at a controlled cadence or explicit incident/manual
trigger, with bounded background work. Keep ongoing acquisition responsive without
starting a model request or cancelling/restarting all jobs on every reading.

Each observed snapshot retains project/network and sensor-configuration hashes,
stream/epoch, sensor sample IDs/times/quality, calibration version, command values,
measured values, derived state and all persistence assumptions. Keep observed-state
identity distinct from the project physical hash and existing numerical revision.
Candidate runs inherit that exact snapshot and original horizon/grid.

The first forecast holds supported observed rates constant over its declared horizon.
This is a forecast assumption, not a prediction of future pump recovery or hydraulics.
Measured delivery and an intervention's intended setpoint require an explicit
actuation/performance assumption; A must not silently equate them after a fault.

Numerical/sensor-mapping edits, new accepted state, stream-epoch changes and freshness
expiry retire dependent live recommendations/approvals under a published policy.
Incoming packets never modify immutable evidence. Display layout moves preserve it.
Before simulated application, independently validate the candidate and check current
snapshot/freshness eligibility; stale runs remain reviewable as historical evidence.
Project/telemetry imports cannot restore live approval.

Record incidents and supporting bounded traces with original timing, thresholds,
detector versions and snapshot/run links. Missing data, contradictory sensors or
unavailable physics remain visible. All control application is explicitly human
approved and limited to the synthetic plant.

## 8. Delivery gates and ownership

| Gate | Person A | Person B | Exit evidence |
|---|---|---|---|
| G0: integrated baseline | Supply relevant numerical oracles/reviews. | Review/merge verified PR #26, preserve existing science/history/credentials, establish Windows PR checks. | Exact resulting revision and actual normal/checked checks; inherited results identified. |
| G1: common contract | Specify measurement operators, calibration, required coverage, state semantics and missing-data policy. | Publish additive sensor/reading/snapshot/event contracts and compatibility examples. | Both workstreams adopt the same committed contract before dependent code. |
| G2: synthetic monitoring | Deliver deterministic plant/sensor publisher, observation mapping and known-value fixtures. | Deliver sensor editor, local input lifecycle, quality/clock handling, bounded live traces and replay. | Canonical and renamed/custom networks; identical replay from seed/recording; fault truth inaccessible to detectors. |
| G3: detection and forecasts | Deliver physical residuals, uncertainty/coverage matrix, baseline rules and statistical evaluation. | Deliver events, immutable observed captures, native forecast adapter, bounded investigation trigger and freshness guards. | Healthy/noisy/missing and fault scenarios; no stale publication or approval; event metrics reported. |
| G4: operator/release acceptance | Review numerical meaning and telemetry/science parity. | Complete installed/relocated and clean-machine acceptance, manual dialogs/cancel and canonical/custom review rehearsals. | Three clean canonical runs and custom follow-up, recorded simulated-only approval/reset and honest unavailable states. |
| D1: detector comparison | Run the bounded RBM/baseline experiment and report findings. | Publish/adopt runtime result contract only for a separately selected method. | Held-out event metrics at declared budgets, including negative/inconclusive outcomes. |

F1 front/threshold accuracy proceeds independently while B publishes G1. Later
candidate science, automatic pipe history, robustness screening and conditioned
PhysicsNeMo remain in the complete continuation plan.

## 9. Acceptance scenarios

- Place/configure/save/reopen hypothetical volume, flow and pump-status sensors on
  canonical and custom networks; retain IDs and physical attachments through edits.
- Stream normal synthetic operation, preserve commands versus actual delivery, then
  inject pump failure. Detect it from emitted evidence, forecast the volume limit and
  investigate actual candidate runs. Keep true labels out of detector inputs.
- Exercise small persistent loss/drift, bias/stuck sensors, coherent normal changes,
  timing skew, drops, duplicate/reordered input and recovery. Record missed or
  ambiguous events instead of assigning a fabricated cause.
- Compare finite below-zero/above-capacity observations without clipping; reject
  malformed/unsupported readings visibly. Missing coverage must not display healthy.
- Verify pause/speed/restart/replay clocks, deterministic seeds, bounded memory,
  backpressure, asynchronous cancellation and no source-checkout dependency on install.
- Confirm numerical/calibration/attachment changes retire evidence, layout does not,
  expiry prevents stale approval and imported history cannot act on the synthetic plant.
- Predeclare detector thresholds and metrics; maintain separate healthy validation
  and held-out fault runs. Synthetic acceptance does not qualify physical deployment.

## 10. Technical references

- [Isermann, model-based fault detection and diagnosis](https://doi.org/10.1016/j.arcontrol.2004.12.002):
  process-model/measurement residuals and diagnosis.
- [NIST CUSUM](https://www.itl.nist.gov/div898/handbook/pmc/section3/pmc313.htm)
  and [NIST EWMA](https://www.itl.nist.gov/div898/handbook/pmc/section3/pmc314.htm):
  statistical shift monitoring; parameters require application-specific validation.
- [Ahmed, Heo and Kassim, 2017](https://www.kns.org/files/pre_paper/37/17S-275Ibrahim-Ahmed.pdf):
  RBM-pretrained DBN fault classification using simulated nuclear-plant sensor data.
- [Hinton, 2010, practical RBM guide](https://www.cs.toronto.edu/~hinton/absps/guideTR.pdf):
  continuous visible units, training, free energy and reconstruction-error caveats.

These references motivate candidate methods; none validates BREAKWATER's implementation.
