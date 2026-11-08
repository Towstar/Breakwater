# F1b: experimental front/outlet accuracy comparison

**Experiment completed; 58/63 synthetic targets met. Accuracy remains unqualified.**
The private MC/SSPRK2 candidate reduces field and outlet errors substantially, but still misses the extended inlet's observed 0.9 mg/L crossing and fails the peak budget. The production solver remains unchanged.

## Revisions, declaration and preservation

- Baseline: `487d80dd54df7d7b6567dfed1170dea9a5e59cd7` (merged PR #28).
- Pre-execution declaration: `a766396610e37fd3ccad201d1df498da56dfc327`; implementation/tests: `0cfbabd656b83103d53282a1388343808741681d`.
- Verified code tree: `b25324e5267e828ce029c86e50235f57872e3bdf`.
- [Pinned declaration](FRONT_COMPARISON_DECLARATION.md) and [configuration](f1b_front_comparison_config.json) were committed before candidate execution. No target or existing threshold changed.
- All **80 original F1 checksum entries** are unchanged. Production transport source/public headers, existing A9/F1 exporter, reference modules, schemas, pins and proofs are unchanged.
- Work used a new isolated `codex/person-a-front-comparator` worktree. The root checkout, other branches and uncommitted work were preserved. Local continuation material is excluded.

Main advanced to `7f2bb7f598f960b60a1d9cc7cb362a7ff233db7f` while this study ran, adding
Person B's PR #27 integration. A [non-checkout merge check](evidence/f1b/integration-main.json)
is clean: its engine/science/proofs/contracts/root-CMake inputs equal the tested code,
and its B-owned application/documentation paths equal the newer main. No existing
checkout or B file was rewritten. Verification remains identified by the frozen
study baseline and exact tested-source hashes.

## Candidate, supported domain and observables

The uninstalled private native target uses MC-limited piecewise-linear reconstruction and SSPRK(2,2), with zero endpoint slopes and a prescribed inlet. Its identity is `fvm-muscl-mc-ssprk2-experimental-0.1.0`. The production method remains `fvm-upwind-euler-0.1.0`.
Interior slopes are `minmod(2*left_difference, (left_difference+right_difference)/2, 2*right_difference)`. Both stages use the current flow interval, even when their endpoint is a flow knot. Inlet data use stage times. The signed ledger uses the same half-weighted stage fluxes as the field update.
Only nonnegative-flow pure advection with prescribed inlet is supported: dispersion, decay and source must be zero. Requested CFL must be in `(0,0.5]`; every step stops at forcing/output events. Invalid/unsupported data, nonfinite arithmetic, clock failure and exhausted resources are rejected. No clipping or tail renormalization occurs.
The first and last cells retain first-order closure. The exported outlet observable remains the last cell average proxy. Its error against the continuum last-cell average is measured separately from error against the physical outlet point value. During zero flow the discharge observation is unavailable.
All 22 F1 physical configurations, initial cell averages, CFL settings and output clocks are paired exactly. Six additional N512 candidate runs use pulse CFL .08/.04 alongside .02 and extended-inlet .4/.2 alongside .1. All cases use the original 1 m pipe, 0.2 m2 area and forcing/horizons. Each accepted candidate step consumes two cell-stage updates per cell; existing ceilings are unchanged.

## Field and outlet measurements

Field mean-L1/RMS are spatial norms at each saved output. The table gives the largest such norm over the full run; maximum errors may occur at different frames. Outlet mean/RMS use available saved discharge samples. Full per-frame data, terminal errors, signed peak/last-cell/point errors and ledgers are retained in raw reports.

| N512 passage | Method | Worst mean-L1 mg/L | Worst RMS mg/L | Worst maximum mg/L | Outlet point maximum mg/L |
|---|---|---:|---:|---:|---:|
| front_smooth | Baseline | 0.00896743178 | 0.0164831511 | 0.0478059047 | 0.0463293329 |
| front_smooth | Candidate | 9.27032203e-05 | 0.000340279601 | 0.00597644254 | 0.00151663855 |
| front_box | Baseline | 0.0396327874 | 0.109047473 | 0.493526492 | 0.493526492 |
| front_box | Candidate | 0.00882997274 | 0.0520190519 | 0.512499824 | 0.449520138 |
| scheduled_inlet | Baseline | 0.0410578453 | 0.0960348159 | 0.565991374 | 0.513749973 |
| scheduled_inlet | Candidate | 0.00681024728 | 0.0329166284 | 0.319094979 | 0.255275341 |

All six 50%-reduction targets and all 44 paired non-regression targets pass. The box maximum field error increases from 0.493526 to 0.512500 mg/L while mean/RMS improve. Bounded concentrations and good mass balance do not establish pointwise/front accuracy.

| Candidate N512 passage | Terminal mean-L1 mg/L | Terminal RMS mg/L | Terminal maximum mg/L | Outlet sample mean abs. error mg/L | Outlet sample RMS error mg/L | Maximum last-cell-average error mg/L |
|---|---:|---:|---:|---:|---:|---:|
| front_smooth | 1.63919991e-31 | 3.70907979e-30 | 8.3926895e-29 | 0.000129294749 | 0.000280184593 | 0.00597644254 |
| front_box | 1.81111604e-27 | 4.09807273e-26 | 9.27288007e-25 | 0.0104781744 | 0.0557001653 | 0.449520138 |
| scheduled_inlet | 1.31703585e-21 | 2.94439338e-20 | 6.66216137e-19 | 0.00501552938 | 0.0274736876 | 0.250625043 |

The N512 extended inlet peak improves from **0.610813894 to 0.834465965 mg/L**. The physical reference peak on the same saved clock is **0.950555556 mg/L**, and its continuous supremum is **1 mg/L**. The candidate's same-clock peak error is **0.116089591 mg/L**, exceeding the unchanged synthetic 0.10 target. The reference sampling gap of 0.049444444 mg/L remains separate. No unsaved native peak is inferred.
The legacy scheduled-inlet window ends at 0.6 h before physical outlet arrival; it supplies no outlet passage qualification. Its candidate N512 worst field mean-L1/RMS/maximum are 0.003620550/0.023762828/0.285262871 mg/L.

## Threshold timing and observation sampling

The decision gates use stride 1 and both bracket endpoints, with a 0.025 h budget. Crossings are observed brackets, never interpolated exact times. Counts include repeated upward events; missing crossings remain explicit.

| N512 passage | Threshold mg/L | Continuous first h | Baseline first bracket h | Candidate first bracket h | Reference/candidate counts |
|---|---:|---:|---|---|---|
| front_smooth | 0.1 | 1.70483276 | [1.6625, 1.675] | [1.7, 1.7125] | 1/1 |
| front_smooth | 0.5 | 2 | [1.9875, 2] | [1.9875, 2] | 1/1 |
| front_smooth | 0.9 | 2.29516724 | [2.325, 2.3375] | [2.2875, 2.3] | 1/1 |
| front_box | 0.1 | 1.5 | [1.3625, 1.375] | [1.4625, 1.475] | 1/1 |
| front_box | 0.5 | 1.5 | [1.4875, 1.5] | [1.4875, 1.5] | 1/1 |
| front_box | 0.9 | 1.5 | [1.6375, 1.65] | [1.525, 1.5375] | 1/1 |
| scheduled_inlet | 0.1 | 1.64957143 | [1.5875, 1.6] | [1.6375, 1.65] | 1/1 |
| scheduled_inlet | 0.5 | 1.67471429 | [1.775, 1.7875] | [1.6625, 1.675] | 2/2 |
| scheduled_inlet | 0.9 | 1.69985714 | not observed | not observed | 1/0 |

The candidate recovers both extended-inlet 0.5 mg/L events. It still has no observed 0.9 crossing. Box 0.1/0.9 first-bracket endpoint errors are about 0.0375 h, exceeding 0.025 h. All stride-2/4 views retain original indices and the endpoint from the same master trajectory; their outcomes are in the compact/full reports.

## Refinement, reference uncertainty and mass

Continuum cell averages, physical outlet characteristics and ledgers come from declared physical inputs independently of native arrays. Split 32/64-point quadrature cross-checks every saved continuum frame. Candidate analysis uses no upwind Poisson kernel. The unchanged baseline analysis retains its independent semidiscrete oracle.
Maximum candidate reference disagreement is **8.0380147e-14 mg/L** (rejection limit 1e-10); maximum uncertainty estimate is **1.48077106e-11 mg/L**. These are numerical estimates, not interval enclosures. Ratios/orders near ten times the larger paired uncertainty are unresolved.
Maximum independently reconstructed candidate signed mass residual is **2.97717406e-11 mg**. Maximum candidate work is **26,542,080 cell-stage updates**, below 50,000,000. The largest saved field has 164,352 values, below 4,000,000.
Candidate spatial refinements are explicitly coupled space/time. Time-step differences below are empirical sensitivity; they neither furnish an exact temporal/spatial split nor certify the remaining error.

| Candidate family/window | Coupled mean-L1 refinement orders |
|---|---|
| front_smooth/space | 1.74468972, 1.8850274, 1.94401274 |
| front_box/space | 0.696916025, 0.690976497, 0.694848305 |
| scheduled_inlet/space | 0.961903327, 1.03932077 |
| scheduled_inlet/extended_space | 0.899871317, 0.979342523 |

| Time-sensitivity study | Mean-L1 difference orders | Finest pair worst mean-L1 difference mg/L |
|---|---|---:|
| front_smooth.time_probe.n512.cfl0.08 | 2.02756365 | 1.79667931e-07 |
| front_box.time_probe.n512.cfl0.08 | 1.9900884 | 5.37992811e-06 |
| scheduled_inlet.time_probe.n512.cfl0.4 | 2.03929944 | 0.00017517468 |
| front_smooth.time.n128.cfl0.4 | 2.02870579, 2.02393082 | 1.50404362e-05 |
| front_box.time.n128.cfl0.4 | 2.06786023, 2.01772171 | 6.09519523e-05 |

The box maximum-error refinement orders remain negative; smooth mean-error behavior does not imply uniform front accuracy. F1's six original failed time-contamination targets remain preserved with their original reference and method.

## Executed verification

| Check | Observed result |
|---|---|
| Normal headless CTests | 9/9; 8.50 s |
| MSVC ASan headless CTests | 9/9; 65.89 s; zero sanitizer reports |
| Individual native checks, each build | Storage 22; search 22; influence 15; original transport 25; candidate 21 |
| Full final Python discovery | 225/225; 74.526 s; zero skips; PhysicsNeMo available |
| New Python checks | 39/39 |
| Actual study, each build | Baseline 22 cases; candidate 28; 58/63 targets; intentional exit 1 |
| Refined A9, each build | 45 cases / 580 gates passed |
| Original coarse A9, each build | 579/580; original scheduled-inlet refinement failure retained |
| Native compatibility | Both A9 modes byte-identical to retained PR #28 outputs; F1 raw SHA unchanged |
| Normal/ASan comparison | Baseline and candidate native data byte-identical; candidate measurements/decisions equal |

Native tests include fixed hand-calculated two-cell and interior SSP updates, stage-time inlet quadrature, flow-knot pauses, shared-face cancellation, litre units, constant/zero/sharp/seeded profiles, input immutability, nonfinite/unsupported cases, CFL equality and actual exact output/work limits. Parser tests reject corrupted fields/times/identities, forged stage counts and inconsistent ledgers. Tests distinguish target failure from integrity failure.
The original coarse orders remain 0.2765063820077894 / 0.20370157974971287 against minimum 0.25. No other A9 gate fails.
Toolchain: Windows x64, MSVC 19.51.36260.0 / VC 14.51.36231, SDK 10.0.26100.0; CMake 4.3.1-msvc1, Ninja 1.13.2. Builds use C++20, RelWithDebInfo `/O2 /Ob1 /DNDEBUG /Z7`, `/fp:precise`, `/W4 /permissive- /EHsc /utf-8`, `/debug /INCREMENTAL:NO`; ASan adds `/fsanitize=address`. Exact generated flags and registration are captured.
Pinned runtime: Python 3.12.13, NumPy 2.5.4, Torch 2.15.0+cpu, PhysicsNeMo 2.2.2 and jsonschema 4.25.1. Interpreter and installed-module origins are captured. Prebuilt Qt/Python/Torch/vendor libraries are outside established ASan coverage. Elapsed times are shared-machine observations, not benchmarks.
No new desktop/installed-app, hosted-model, 1,800-update training or Lean run is claimed. Public application/worker interfaces and approval behavior have no implementation change.

## Failed attempts and reproducibility

The first normal/ASan studies (attempts 04/06) completed at 58/63. Review found that the initial candidate reference checker unnecessarily also evaluated the legacy upwind Poisson kernel. Attempt 09 adds a regression and retains its failing 39-test run, source snapshot and traceback. The correction adds a continuum-only candidate cross-check; attempt 10 passes all 39 tests. Final attempts 11/12 repeat actual studies and attempt 13 repeats all 225 Python tests. Native solver bytes, raw fields, thresholds and measured decisions are unchanged. All earlier attempts remain retained.
[Evidence summary](evidence/f1b/summary.json), [source/executable manifest](evidence/f1b/manifest.json), compact normal/ASan reports, native/Python logs and [retained-attempt index](evidence/f1b/retained-attempt-files.json) record commands, lengths and SHA-256 values. Raw fields/full per-frame reports remain under ignored `out/f1b/`; compact copies omit per-frame arrays and long index lists but retain their full report hashes, extrema, uncertainty, every gate and crossing bracket.
From an x64 Visual Studio Developer PowerShell at the worktree root:

```powershell
$pythonExe = 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe'
cmake -S . -B out/f1b/normal -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded -DBUILD_TESTING=ON -DBREAKWATER_BUILD_DESKTOP=OFF -DBREAKWATER_ENABLE_ASAN=OFF '-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/debug /INCREMENTAL:NO'
cmake --build out/f1b/normal --parallel 4
ctest --test-dir out/f1b/normal --output-on-failure
& $pythonExe science/compare_front_accuracy.py --native out/f1b/normal/engine/breakwater_front_comparison_reference.exe --baseline-native out/f1b/normal/engine/breakwater_transport_qualification_reference.exe --config science/f1b_front_comparison_config.json --output out/f1b/reproduced-normal-01
& $pythonExe -m unittest discover -s science/tests -v
```

Use a fresh output directory. Exit 0 means integrity and all synthetic targets passed; 1 means a completed experiment with unmet targets; 2 means execution/input/reference/integrity failure. Repeat the configure in a separate `asan` directory with ASan ON and its runtime on PATH. [Captured command scripts](evidence/f1b/commands/run-study.ps1) and execution JSON contain the actual normal/ASan/A9 arguments; choose new attempt names.

Native output SHA-256 values:

| Method | SHA-256 |
|---|---|
| Baseline, both builds | `14a086a64342931ea95a6c1d53f67d63e679c34720dc1d82cedb0aada5981477` |
| Candidate, both builds | `91119de26f4baddda20a8a394788c33263220eacb9aa9597caf8afa3a94f4caf` |

## Proof mapping and Person B handoff

The prior 54 audited propositions, foundational allowlist and pins are unchanged. A10a shared-face cancellation motivates the weighted ledger, but this batch supplies no formal mapping of the new two-stage stencil. A10b's original Euler nonnegativity coefficients do not cover MC reconstruction or SSP stages. The declaration provides the conditional exact-real argument; native tests, ASan and accuracy measurements remain separate evidence.
B can report an experimental improvement on these declared pure-advection cases, with 58/63 synthetic targets met. Production transport, history provenance, contracts and approval remain unchanged. No candidate is selectable through the application or worker.
A further bounded experiment is justified before promotion: isolate endpoint closure versus interior limiter smearing using the same independent front/outlet cases and explicit positivity/ledger checks. These measurements do not identify the first-order boundary as the sole cause. Preserve the missed 0.9 event, box timing failures and discontinuity maximum-error limitation. Any production adoption needs separately agreed application error budgets and B-owned integration/provenance review.
G2/G3 mappings and detection, RBMs, solver promotion, new proofs and application release acceptance remain subsequent increments. Shared STATUS and delivery documentation are left for B to integrate from this A-owned handoff.
