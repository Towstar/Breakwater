from pathlib import Path
import json,re

root=Path(__file__).resolve().parents[1]
normal=root/'out/f1/attempt-11-normal';asan=root/'out/f1/attempt-12-asan'
d=json.loads((normal/'f1/report.json').read_text());other=json.loads((asan/'f1/report.json').read_text())
assert d['status']==other['status']=='completed' and d['native_data_sha256']==other['native_data_sha256']
cases={c['id']:c for c in d['cases']}
selected=[('Smooth pulse','front_smooth.space.n512.cfl0.02'),('Box pulse','front_box.space.n512.cfl0.02'),
          ('Legacy inlet, 0.6 h','scheduled_inlet.space.n512.cfl0.1'),('Extended inlet, 2.2 h','scheduled_inlet.extended_space.n512.cfl0.1')]
field_rows=[];peak_rows=[];timing_rows=[]
for label,key in selected:
    c=cases[key];e=c['worst_continuum_error'];o=c['outlet']
    field_rows.append(f"| {label} | {e['mean_l1_mg_l']:.8g} | {e['rms_mg_l']:.8g} | {e['maximum_mg_l']:.8g} | {o['point_error']['maximum_mg_l']:.8g} |")
    peak_rows.append(f"| {label} | {o['sampled_native_peak_mg_l']:.8g} | {o['sampled_continuum_point_peak_mg_l']:.8g} | {o['continuum_peak_mg_l']:.8g} |")
    for r in c['observations']:
        if r['stride']!=1:continue
        ref=r['continuum']['first_upward_h'];b=r['native']['first_upward_bracket_h']
        reference='not reached' if ref is None else f'{ref:.9f}'
        bracket='not reached' if b is None else f'[{b[0]:.7g}, {b[1]:.7g}]'
        timing_rows.append(f"| {label} | {r['threshold_mg_l']:g} | {reference} | {bracket} | {r['continuum']['upward_crossing_count']} / {r['native']['upward_crossing_count']} |")
study_rows=[]
for s in d['studies']:
    orders=', '.join('unresolved' if v is None else f'{v:.5f}' for v in s['observed_orders'])
    study_rows.append(f"| {s['regime']} | {s['study']} | {s['scope']} | {orders} | {s['mean_l1_errors_mg_l'][-1]:.9g} |")
ratios=[]
for regime in ['front_smooth','front_box']:
    values=[cases[f'{regime}.space.n{n}.cfl0.02']['maximum_resolved_time_to_space_ratio'] for n in [64,128,256,512]]
    ratios.append('| '+regime+' | '+' | '.join(f'{v:.6f}'+(' (exceeds)' if v>.05 else '') for v in values)+' |')
cross=max(c['reference']['crosscheck_disagreement_mg_l'] for c in d['cases'])
uncertainty=max(c['reference']['uncertainty_estimate_mg_l'] for c in d['cases'])
mass=max(c['maximum_signed_mass_residual_mg'] for c in d['cases'])
native_times={v:float(re.search(r'Total Test time \(real\) = ([0-9.]+)',(root/f'out/f1/attempt-{a}-{v}/ctest.log').read_text()).group(1)) for v,a in [('normal','06'),('asan','07')]}
python_time=re.search(r'Ran 186 tests in ([0-9.]+)s',(normal/'python-tests.log').read_text()).group(1)
content=f'''# F1: front, outlet and threshold accuracy

**The 22-case study is completed; application accuracy is not qualified.** Normal
and MSVC ASan execution produced byte-identical native data. All 186 Python tests
passed without skips, including 59 new focused checks. Both native builds passed
8/8 suites. The unchanged refined A9 matrix passed 45 cases/580 gates in both;
the original coarse matrix retained its one known failure in both.

The extended inlet case exposes a material limitation at N512: the saved native
outlet proxy peaks at **0.610814 mg/L**, versus **0.950556 mg/L** for the physical
reference on the same output times and **1 mg/L** for its continuous peak. It misses
the 0.9 mg/L upward crossing. The box front still has approximately **0.493526 mg/L**
maximum field error. These results are retained, not repaired or qualified by mean
error or mass conservation. The original **0.40191 mg/L** legacy front limitation
is reproduced and remains valid for its original case/window.

## Revisions, ownership and delivery

- Baseline: `de06aa1860ccbd3af2e4091557a50920ad16d423` (merged PR #26).
- G1 numerical guidance: `9941859b202d804b024d788b1238d94166b33309`;
  [observation semantics and worked examples](OBSERVATION_SEMANTICS.md).
- F1 implementation/tests: `4841c042d81eb43fe06871a89c4b096c778707b9`.
- Final metric correction/tests: **`fffe5c342caf64725c6242215a8858280ca75441`**.
  It changes only the F1 diagnostic norm and its regression. Final F1 normal/ASan
  execution and full Python discovery were repeated at this revision. The already
  passing native/A9 checks at `4841c04` retain their exact compiled C++/executable
  bytes; those unchanged checks were reused. Reports/evidence are a separate commit.
- Worktree: `C:/Projects/Breakwater/.worktrees/person-a-front-accuracy`, branch
  `codex/person-a-front-accuracy`. Original checkouts, other sessions' changes and
  applications remain untouched. No local continuation file or credential is included.

Only A-owned engine/science files change. The numerical solver, transport method
version `fvm-upwind-euler-0.1.0`, public APIs/types, worker/project contracts, root
build/solution, dependency pins, existing acceptance thresholds and proofs are
unchanged. The private existing exporter gains `--front-accuracy`; default and
`--refined-inlet` output remain **byte-identical to the retained pre-F1 verified
executable**, in both new builds. [Identity evidence](evidence/f1/legacy-byte-identity.json)
records the baseline executable hash and actual comparisons.

G1 delivery is A's numerical guidance, not adoption of a new shared contract or
implementation of sensors. B must publish the reviewed contract and both workstreams
adopt it before dependent mappings. The note covers calibration, physical observation
support, measured/commanded state, uncertainty and missing/conflicting coverage.

## Predeclared matrix and independent references

The [pinned configuration](f1_front_accuracy_config.json) was written before the
first native F1 measurement. Its hash stayed unchanged through all attempts:
`{d['config_sha256']}`. No study gate, forcing, mesh or CFL was adjusted to pass.
All pipes have length 1 m, area 0.2 m2, zero dispersion/decay/source and unit-height
initial/inlet pulses. These are synthetic test points, not a guaranteed parameter box.

| Family | Runs | Configuration |
|---|---:|---|
| Raised-cosine initial pulse | 8 | Support [0.125,0.625] m; velocity 0.25 m/h; horizon 4 h |
| Rectangular initial pulse | 8 | Same support, amplitude, velocity and horizon |
| Legacy scheduled inlet | 3 | Original forcing, N128/256/512, CFL 0.1, horizon 0.6 h, outputs 0.05 h |
| Extended scheduled inlet | 3 | Same forcing/meshes/CFL; horizon 2.2 h, outputs 0.0125 h |

Each initial family uses spatial N64/128/256/512 at CFL 0.02 and temporal CFL
0.4/0.2/0.1/0.05 at fixed N128. Its output cadence is 0.0125 h. Scheduled velocity
is 0.4 m/h until 0.137 h, zero until 0.293 h, then 0.7 m/h; the unchanged inlet
knots are (0,0), (0.11,1), (0.29,0.2), (0.47,0.8), (0.63,0).

Continuum initial-pulse fields use analytic translated cell integrals. Scheduled
fields/ledgers use A9's characteristic integration; an independently implemented
inverse-characteristic point function supplies split 32/64-point Gauss quadrature
checks at **every saved frame**, including pause interfaces. Native initial or
result buffers never construct a reference. Box interface point values use
right-continuous arrival time, while integration endpoints have zero measure.

For initial pulses, the open upwind semidiscrete solution is
`c[j,t] = sum(exp(-z)*z^k/k! * c[j-k,0], k=0..j)`, with `z=v*t/dx`.
Stable zero-order/modal recurrences preserve representable probabilities; the
truncated kernel is **never renormalized**, because its missing tail is physical
escape. An independent 80-digit Decimal recurrence checks selected clocks at
time zero, the middle saved frame and the horizon. An additional regression checks
an underflowing zero-order probability at clock 800 without erasing its modal terms.

Maximum observed reference disagreement was **{cross:.12g} mg/L**, below the
unchanged declared `1e-10 mg/L` cross-check gate. The largest reported uncertainty
scale was **{uncertainty:.12g} mg/L**: the larger of observed discrepancy and
`128*epsilon*(cells+flow_knots+inlet_knots+1)` for unit-height data. These are numerical
checks/roundoff scales, **not formal enclosures**. Orders below ten times the reference
scale are unresolved. Scheduled inlet has no semidiscrete oracle in this batch;
its refinement remains coupled space/time.

## Field accuracy, separated errors and unmet interpretation target

Mean-L1, RMS and maximum field errors compare physical cell averages. The following
N512 values are each metric's worst over saved frames; their maxima may occur at
different times. Terminal and per-frame metrics remain in the full local reports.
Finite predictions are never clipped. Error-norm scaling prevents auxiliary RMS
underflow; it does not normalize concentrations or the semigroup kernel.

| Family/window | Mean-L1 mg/L | RMS mg/L | Maximum mg/L | Outlet point maximum mg/L |
|---|---:|---:|---:|---:|
{chr(10).join(field_rows)}

Spatial orders for initial pulses use **semidiscrete minus continuum** errors;
temporal orders use **native minus semidiscrete** errors at a fixed mesh. The table
uses worst-in-time mean-L1 errors, so it is distinct from A9's terminal-error studies.

| Family | Study | Error scope | Observed orders | Finest mean-L1 mg/L |
|---|---|---|---|---:|
{chr(10).join(study_rows)}

The predeclared native time-contamination target is at most 5% on resolved frames.
It is **not met in six of eight initial-pulse spatial runs**. Maximum measured
ratios, including the uncertainty scale, are:

| Family | N64 | N128 | N256 | N512 |
|---|---:|---:|---:|---:|
{chr(10).join(ratios)}

Both N512 maxima occur at the 4 h washout endpoint. The separate semidiscrete spatial
and native temporal errors are still reported, but these native whole-window results
do not meet the 5% interpretation target. CFL was not reduced to obtain a pass.
Small mean error does not establish pointwise front accuracy or threshold reliability.

## Outlet peaks, arrival times and sampling

The native outlet flux uses the last cell average as a face approximation. It is
compared separately with the continuum last-cell average and the physical point
outlet; signed temporal/spatial/observation-projection components are retained per
frame. During a pause there is no discharged-effluent observation; missing discharge
is not converted to a measured zero or bridged when finding crossings.

| N512 family/window | Saved native peak mg/L | Same-grid physical peak mg/L | Continuous physical peak mg/L |
|---|---:|---:|---:|
{chr(10).join(peak_rows)}

The extended reference's 0.0494444 mg/L peak sampling gap is distinguished from
the native-versus-same-grid deficit of 0.339742 mg/L. No peak of the unsaved native
trajectory is claimed. The legacy 0.6 h horizon ends before physical outlet arrival;
its tiny native outlet tail is below reference resolution, not an observed event.

Thresholds are **0.1, 0.5 and 0.9 mg/L**. Continuous reference crossings are solved
from the pulse profile or emitted characteristics. Native results are the first
*observed* upward brackets, not invented interpolated timestamps or guarantees that
an earlier unsampled pulse did not occur. Counts expose merged/repeated crossings.

| N512 family/window | Threshold mg/L | Reference first upward h | Native saved bracket h | Reference/native upward counts |
|---|---:|---:|---|---:|
{chr(10).join(timing_rows)}

At N64 the smooth pulse also misses the 0.9 mg/L crossing: saved native peak
0.736571 mg/L against reference peak 1. The extended inlet at N512 resolves only one
0.5 mg/L upward crossing where the reference has two, and its first saved bracket
is approximately 0.1003–0.1128 h later than the physical first crossing.

Observation strides 1/2/4 select original samples from the **same** computed native
trajectory, always retaining the endpoint. This isolates sampling effects without
rerunning or changing integration. Ordinary output times constrain Euler substeps,
so the master cadence is held fixed within spatial/temporal comparisons. Initial
equality, no crossing, repeated crossings and gaps have distinct tested outcomes.

## Verification, failures and reproducibility

| Executed check | Result |
|---|---|
| Normal headless CTests | 8/8, {native_times['normal']:.2f} s |
| MSVC ASan headless CTests | 8/8, {native_times['asan']:.2f} s; zero sanitizer reports |
| Individual native cases, each build | Storage 22; search 22; influence 15; transport 25 |
| Full final Python discovery | 186/186, {python_time} s; no skips; PhysicsNeMo available |
| New focused checks | 59/59 |
| Final F1 actual execution | 22 cases in each build; raw native data byte-identical |
| Refined A9, each build | 45 cases / 580 gates passed |
| Original coarse A9, each build | 579/580; expected scheduled-inlet refinement failure retained |
| Legacy exporter compatibility | Both modes byte-identical to the retained pre-F1 verified executable |

Largest F1 work was 25,898 substeps / 13,259,776 cell updates; maximum saved values
per case were 164,352, below unchanged native limits. Maximum independent signed
mass residual was **{mass:.12g} mg**. Good mass balance does not qualify the errors above.
The original coarse orders remain **0.2765063820 / 0.2037015797**, failing the
unchanged minimum 0.25 on the second pair. Its reports and nonzero exits are retained.

Toolchain: Windows x64, MSVC 19.51.36260.0 / VC 14.51.36231, SDK 10.0.26100.0,
CMake 4.3.1-msvc1, Ninja 1.13.2; C++20 `RelWithDebInfo`, `/fp:precise`, `/Z7`,
non-incremental linking, separate ASan `/fsanitize=address`. Python 3.12.13,
NumPy 2.5.4, Torch 2.15.0+cpu and PhysicsNeMo 2.2.2 use the existing pinned environment.
Prebuilt Python/Torch/vendor libraries are outside established ASan coverage.
Timings are shared-machine acceptance observations, not benchmarks. No new desktop,
installed-app, hosted-model, 1,800-update training or Lean execution is claimed.

Failed attempts were retained before correction:

- Attempt 01: two new reference-test failures. A near-jump subtraction rounded a
  just-before-arrival point onto the interface; arrival-distance comparisons fix it.
  Independent log probabilities also exceeded a tight semigroup-composition check;
  recurrence weights fix the roundoff without relaxing tolerances.
- Attempt 08: the legacy comparison harness looked up slash-separated paths in a
  Windows-backslash manifest. Lookup normalization fixes it; no product change.
- Attempt 10: direct squaring reported RMS zero for finite `1e-200` errors. A failing
  regression preceded the scaled-norm correction; the tolerance/config stayed fixed.
  Both F1 executions and full Python discovery were repeated afterward.

The [evidence summary](evidence/f1/summary.json), [manifest](evidence/f1/manifest.json),
normal/ASan compact case reports, exact commands, flags, CMake caches, full native test
logs and failed source/test snapshots are committed. Full raw fields, per-frame
reports and all attempts stay under ignored `out/f1/`; their paths/lengths/SHA-256
values are indexed in [retained attempts](evidence/f1/retained-attempt-files.json).
Compact reports omit per-frame arrays and long decimation-index lists; they retain
their full report hashes, extrema, all crossing outcomes and reference uncertainty.

From the x64 Visual Studio Developer PowerShell, using the existing full pinned
science interpreter, the study entry point is:

```powershell
$python = 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe'
& $python science/qualify_front_accuracy.py --native out/f1/normal/engine/breakwater_transport_qualification_reference.exe --config science/f1_front_accuracy_config.json --output out/f1/reproduced-normal-01
& $python science/qualify_front_accuracy.py --native out/f1/asan/engine/breakwater_transport_qualification_reference.exe --config science/f1_front_accuracy_config.json --output out/f1/reproduced-asan-01
& $python -m unittest discover -s science/tests -v
```

Use a fresh output path; an existing directory is refused without overwriting it.
Expanded configure/build/CTest/legacy commands are recorded beside the reports.
Copies of the local orchestration scripts are execution records; their root paths
assume their original `out/` location. Copy back there before replaying them as-is,
and choose new attempt names. Each CLI captures its raw configuration, preflight
source/executable hashes, stdout/stderr, final report and explicit failure status.

## Decision and B handoff

A subsequent bounded advection-accuracy improvement study is **justified** if peak or
threshold decisions are required. Predeclare application-specific error/timing budgets
with B first, then compare a conservative, less diffusive discretization against these
same oracles and failure cases. This batch selects or implements no replacement scheme.
Any future change needs positivity/CFL/resource checks, mass-ledger regressions and
review of the existing exact-real proof mapping; existing proofs do not certify a
different stencil, compiled scheduler or peak accuracy.

B can use the G1 note now to review calibration/measurement support and missing-data
semantics, and incorporate this A-owned result into shared status. Sensor ingestion,
G2 synthetic generation, G3 detection, RBM training and broader learned/robustness
work remain subsequent increments. Agreement, historical evidence and these accuracy
measurements never authorize a simulated control.
'''
(root/'science/FRONT_ACCURACY.md').write_bytes(content.encode())
print('F1 report generated with measured tables, failed targets and explicit source/verification scopes')
