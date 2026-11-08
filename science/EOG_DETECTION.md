# Causal EOG input/state/detection core

Person A increment following PR #36. Requirements revision:
`nihaal419/HackUTD-ZeroDay@135ba4c8348357457420a1111df80ec1906bcbdb`, rechecked
November 8, 2026. This is an initial deterministic detector with declared heuristics,
not calibrated confidence or a production process/client integration.

## Implemented scope

- `eog_model.py`: immutable numerical configuration from decoded official network
  and operations dictionaries; exact imported IDs, bbl/bpd/mcfd/psi meanings,
  thresholds, per-well synthetic revenues, repair/truck/fleet parameters. Geometry
  is checked metadata and does not determine flow/travel/pressure physics.
- `eog_inputs.py`: only the three fixed input names are opened, without enumerating
  the directory. Bounded strict JSON/CSV, duplicate keys/rows, exact UTC/Unix agreement,
  identity/metric/unit validation, finite values and post-parse byte/stat rechecks.
  BOM bytes remain in content hashes. Unknown status measurements remain raw
  unsupported evidence, never healthy defaults.
- `eog_detection.py`: causal failure, leak and degradation episodes, current
  coverage/ambiguity, independent event categories, supported shutdown countdowns,
  repair estimates and hourly opportunity exposure. No file/model API access.
- `inspect_eog.py`: bounded **offline** CLI for a real fixed-input run and exact
  source/input identities. It reads no labels and makes no accuracy/optimality claim.

Public EOG contract 72cadd1 has separate numerical acceptance. Lucas's reciprocal
exact adoption/main integration is still required before provider/client binding.
This core does not introduce a substitute wire protocol. The reader's raw-file-set
identity is not a historical capture's original source dataset identity. A production
caller must first execute the accepted capture verifier; an optional private reader
cutoff is a caller claim, not authentication.

## Frozen initial evidence rules

The following rules are declared **before first execution on the official full-week
recording in this increment**. Later evidence must identify changes explicitly.
There is no hidden-answer tuning or calibrated probability. Custom `Rules` values
and the model/observation policies have separate retained fingerprints.

| Condition | Initial rule / semantics |
|---|---|
| Fault | Typed pump status 2 starts a failure episode immediately. Contradictory delivery creates ambiguous evidence and no countdown. Status 0 alone never starts failure. |
| Near-zero pump delivery | `abs(flow_bpd) <= max(5, 0.02 * configured healthy pump flow)`. Raw flow is retained; zero failed-pump delivery is a named forecast assumption. |
| Normal delivery recovery | Typed running status, delivery above the zero band and at most 5% above configured healthy flow; or stopped status with near-zero delivery, below-start level and interpretable well statuses. A configured repair deadline never resolves an observed fault. |
| Leak | While running, pump minus receiving flow exceeds `max(15 bpd, 0.10 * measured pump flow)`. Negative or grossly contradictory receipt cannot establish healthy delivery. |
| Degradation | While running, delivery is more than 15% below configured healthy pump flow. This is a threshold on nominal performance, not an inferred hydraulic model. |
| Persistence | Four consecutive compatible running acquisitions, covering at least 900 seconds; gaps exceed neither 300 seconds nor the caller's stricter gap policy. Rapid frames cannot fake a sustained signal. Normal off/fault frames reset pending confirmation. |
| Inspection recovery | The same sustained confirmation of restored running-flow agreement. Off/failure masks an existing condition and does not prove repair. A degradation event and a later failure stay separate; the masked degradation recommendation is unavailable. |
| Snapshot | Required physical channels share an exact acquisition time and satisfy freshness. A relaxed display skew policy cannot manufacture a physical snapshot. |
| Inflow consistency | Nonnegative measured rates, known well states and well/facility agreement within `max(20 bpd, 5% of measured facility inflow)`. Stopped wells' near-zero water allowance is 5 bpd. Inconsistent states remain ambiguous. |
| Already shut in | Fault, all wells stopped, near-shutdown level (within 0.5 percentage point below threshold or above) and near-zero inflow. Preserve pre-trip opportunity revenue. Unknown pre-trip states give unavailable exposure, never zero. |

These thresholds are initial engineering choices, not organizer-mandated noise bounds.
Very low deliveries at/below the zero band, missing/ambiguous observations and
unrepresentable arithmetic remain unavailable. Four samples imply at least a
15-minute inspection confirmation delay; earliest gradual physical wear/leak onset
is not established by the threshold-crossing timestamp.

## Time, provenance and economics

Only causal `View` methods are queried. Appending/changing future data cannot change
an event, forecast or causal evidence fingerprint at the same cutoff. Inspection
onset marks the first sustained threshold breach. Failure onset uses first observed
fault feedback, with the preceding interpretable normal reading as an uncertainty
bound; absent preceding evidence is explicitly left-censored. Source samples that
justify onset, productive wells and current conclusions remain retained.
`detected_unix` is the actual first causal emission time, distinct from an estimated
earlier onset and a later update/resolution. This provenance field was added after
the initial unlabeled smoke; it does not change detection decisions or thresholds.

Ongoing repair remaining is configured duration minus time since the stated onset
estimate. Left-censored/expired horizons are unknown; a persistent fault is not
silently assigned zero hours or declared repaired. Physical countdown availability
is distinct from economic-horizon/exposure availability. A computable countdown
does not authorize planning when the latter are unknown.

The forecast holds measured inflow, assumes a compatible failed pump delivers zero,
and uses the configured shutdown threshold. If the cutoff is after the last coherent
sample, that sample's true age advances the conditional forecast. A crossing between
sample and cutoff is labeled **predicted reached**, distinct from observed automatic
shut-in. The threshold hold is an explicit automatic state transition, not volume
clipping. Finite invalid raw readings remain evidence; unrepresentable derived
inventories/countdowns are unavailable.

Hourly exposure uses input oil/gas economics and observed pre-trip productive wells.
Preexisting inactive wells contribute no opportunity loss. An already tripped
facility retains exposure until observed recovery; its zero current water rate
does not erase it. This detector does not yet compute intervention-plan costs,
dispatch trucks, optimize a fleet or independently validate a numerical plan.

## Execution and evidence

Use Python 3.12; the new modules require only the standard library:

```powershell
python -S -m unittest discover -s science/tests -p 'test_eog*.py' -v
python -S science/inspect_eog.py --dataset-folder <folder> --report out/eog-detection.json
python -S science/inspect_eog.py --dataset-folder <folder> --cutoff-unix <integer> --report out/eog-historical.json
```

Limits: 64 systems, 256 facilities total, 128 wells/facility, 8,192 catalog channels,
168-hour configured repair duration, 4 MiB JSON inputs/report, 128 MiB CSV, 2,000,000
rows, 8,192 samples per queried channel, 1,024 events by default (at most 4,096).
Budget exhaustion fails explicitly and never returns successful truncated evidence.
No original arrival-jitter reconstruction, OS sandbox isolation or physical validity
is claimed. Process cancellation and timeout enforcement remain Lucas's adapter work.

Generated checks cover normal sawtooth operation, status/flow contradictions,
missing/stale/skewed inputs, exact/ULP thresholds, persistence timing, separate
degradation/failure, masked inspection, observed recovery, repeated/concurrent
failures, known-value/renamed networks, revenue exposure, overflow/underflow and
future-prefix equivalence. Initial adversarial failures and corrected reruns are
retained when evidence is published. Full scientific regression and official-input
smoke results are recorded separately after execution, without calling smoke
execution a labeled accuracy qualification.

The separately invoked `evaluate_eog_detection.py` reads a frozen detector report
and an explicit offline label file. No runtime module imports it, and it never
invokes the detector. It reports one-to-one exact-ID/category interval matching,
misses/unmatched and early alarms, actual emission delay, onset/resolution error
and lifecycle confusion. Reference actions are not copied into its output. For
this first practice evaluation, the **predeclared event-presence gate** requires
all labeled events matched, no unmatched/early alarms and correct matched lifecycle.
Timing errors remain separate measurements; this gate does not qualify countdowns,
plans, cost, optimality or independent held-out accuracy.

```powershell
python -S science/evaluate_eog_detection.py --detection-report out/eog-detection.json --offline-labels <separate-offline-label-file> --report out/eog-offline-evaluation.json
```

Exit 2 means the presence gate failed and preserves the report. It is not a passed
qualification or an execution crash. The full-week, historical cuts and mock sample
are correlated. The judging network was absent at the source revision check.

Next: label-isolated offline qualification, real protected-capture/provider mapping,
deterministic response evaluation, joint search and independent plan validation;
then actual desktop integration and the remaining simulation/application gates.
