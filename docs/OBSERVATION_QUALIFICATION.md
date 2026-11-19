# Independent synthetic observation qualification

This study exercises `bounded-volume-monitor-0.1.1`. It adds evidence to the
earlier 72,020-capture healthy study without replacing its dated report. It is
an offline numerical study, not operational monitoring or field accuracy.

## Declared study

`engine/examples/observation_qualification.cpp` supplies independently calculated
affine and step balances. It never calls the native volume forecast or the Python
plant publisher. The detector receives ordinary measured snapshots, a separate
command catalog and the declared temporal assumption. It receives no preset,
seed, hidden fault label, expected onset or future truth.

- Two topologies: one tank with separate supply/discharge; two tanks with an
  internal pumped transfer, two discharges and different external supply.
- Ten new seeds, 2001 through 2010, per topology; two absolute uncertainty scales,
  1 and 10. Calibration offsets are fixed per instrument and bounded separately
  from bounded uniform measurement error. A synthetic zero-flow instrument
  reports exact zero; the receiver does not clip readings.
- Ten scenarios, 40 cases each: healthy, 0.2-second clock skew, pump delivery
  loss, 18 m³/h unmetered loss, 6 m³/h marginal loss, 0.1 m³/h sub-bound loss,
  inventory drift, stuck inventory, missing inventory and delayed inventory.
- Healthy/skew cases run for one hour. Fault cases run for ten minutes, with
  onset at 60 seconds and recovery at 180 seconds. All sample at 1 Hz.
- Total: 400 cases, 480,400 captures and 80 healthy/skew hours. Healthy instrument
  values receive 1,296,360 checks against their declared absolute error bounds.

The independent reference records physical onset separately from the first
30-second residual whose noiseless instrument signal exceeds the complete
declared absolute threshold. Inventory drift/stuck cases concern instrument
consistency, not proof of a physical leak. An onset that remains below the bound
is explicitly unobservable under this policy.

Before execution, acceptance required no physical alerts in healthy, clock-skew,
missing, delayed or sub-bound cases, or before fault onset. Coverage must match
the declared freshness/coherence policy. Observable pump loss must activate
within ten seconds; observable residual faults within sixty seconds. Both need
clearing after recovery and by 220 seconds. Retained windows must stay within
4,096 points and 32,768 numerical scalars. EWMA/CUSUM outcomes are recorded,
but are not used to decide whether the native detector passes.

## Results

The retained table and summary are under `evidence/v03/detection-study/`. The
normal and AddressSanitizer outputs are checked separately before publication.

| Scenario | Cases | Native detected | Worst delay from physical onset | Worst delay from observable onset |
| --- | ---: | ---: | ---: | ---: |
| Pump delivery loss | 40 | 40 | 2 s | 2 s |
| Unmetered loss | 40 | 40 | 12 s | 3 s |
| Marginal loss | 40 | 40 | 30 s | 3 s |
| Inventory drift | 40 | 40 | 25 s | 3 s |
| Stuck inventory | 40 | 40 | 13 s | 3 s |
| Sub-bound loss | 40 | 0 | Unobservable | Unobservable |
| Healthy and clock skew | 80 | 0 | Not applicable | Not applicable |
| Missing and delayed | 80 | 0 | Coverage unavailable | Coverage unavailable |

There were zero false native activations in the declared no-alert cases, zero
coverage classification mismatches and zero healthy absolute-bound misses.
Missing and delayed readings each produced 4,760 unavailable captures, reported
as coverage failures rather than physical faults. All 200 observable cases met
the activation and clearing gates. Individual clearing times and per-case
uncertainty/resource counts are in the CSV.

## Offline comparators

The EWMA recurrence is `z = 0.2*x + 0.8*z_previous`, with absolute alarm limit 1.
The two-sided CUSUM uses `upper = max(0, upper + x - 0.5)` and
`lower = max(0, lower - x - 0.5)`, with alarm limit 5. Both start at zero and reset
after missing coverage. Here `x` is the same signed independent residual divided
by its declared absolute bound. Pump cases use command-minus-delivery and its
absolute tracking bound. Comparator first alarms have no additional debounce.

The recurrence choices follow the [NIST EWMA description](https://www.itl.nist.gov/div898/handbook/pmc/section3/pmc324.htm)
and [NIST two-sided CUSUM description](https://www.itl.nist.gov/div898/handbook/pmc/section3/pmc323.htm).
This study does **not** normalize by a standard deviation. Its overlapping windows
are correlated and its errors are bounded uniform samples with fixed calibration
offsets. NIST's independent-normal average-run-length interpretations therefore
do not apply. Parameters were fixed before execution, not fitted to these cases.

Both comparators alarmed in all 200 observable cases and none of the no-alert
cases. Their worst physical-onset delays were respectively 0/0 seconds for pump
loss, 13/14 for unmetered loss, 31/30 for marginal loss, 27/26 for drift and 14/15
for stuck inventory. This is a small synthetic policy comparison, not evidence
that one method or model is universally superior. No comparator replaces the
production detector in this increment.

## Coverage and resource reporting

The updated detector counts point times and tank timing bounds in its scalar
budget. A valid dense 128-instrument network at 6 Hz can exhaust that bounded
window before accumulating thirty seconds. It reports `window_resource_limit`
instead of remaining ambiguously in warmup. A lower sampling rate restores a
complete window without treating resource exhaustion as a physical fault.

A manual observed capture freezes the detector's source identity, as-of time,
temporal model, balance status and retained window counts beside its events.
Gateway tools and v2 history reject foreign, future, malformed or unsupported
coverage reports. Older/direct captures explicitly report `not_captured` when
queried. Empty events never establish health or complete detection coverage.
G1 frames, project documents and public scientific protocols remain unchanged.

## Reproduce

From an initialized MSVC developer PowerShell in the repository:

```powershell
cmake --build out/portfolio-desktop --target breakwater_observation_qualification breakwater_observation_detection_tests
ctest --test-dir out/portfolio-desktop --output-on-failure --no-tests=error -R '^engine.observation_(detection|qualification)$'
& './out/portfolio-desktop/engine/breakwater_observation_qualification.exe' --output out/new-observation-study
```

Use a new output directory; the exporter refuses to overwrite prior evidence.
Repeat with `out/portfolio-desktop-asan` from an ASan-enabled developer shell.
`desktop.observed_forecast` covers frozen coverage validation and real publisher
captures. Initial and final local logs, source checksums and exact counts are
listed in `evidence/v03/detection-qualification.json`.

## Limits

These are selected supported mappings and simple synthetic flow histories.
The study does not qualify arbitrary nonlinear hydraulics, correlated field
sensor errors, realistic noise distributions, chemical transport detection,
mechanical diagnosis, probabilistic confidence or robust forecast feasibility.
The supplied temporal model and uncertainty declarations remain assumptions.
Increased uncertainty can make small faults unobservable. Generic external
publishers without the temporal declaration retain unavailable balance detection.
Forecast calculations remain nominal and advisory; this work adds no actuation.
