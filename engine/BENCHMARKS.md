# A5: measured native candidate performance

The original A5 experiment below is retained as historical evidence. The current
version-2 runner also compares the precomputed control-influence evaluator; see
[A7 measurements](#a7-precomputed-control-influence) and run it with `-Influence`.

The native benchmark measures production dense/sparse sampled kernels, production
affine screening, independent retained-run validation and the complete public search
API separately. It does not introduce SIMD, parallel search, GPU execution or a new
physical model. Public engine contracts and Person B's files are unchanged.

## Reproduce

From the repository root on Windows with MSVC C++ tools and CMake/Ninja:

```powershell
.\engine\benchmarks\run.ps1
```

This creates a separate optimized `out/a5-release` build, runs native tests, and then
executes both graphs with **100, 1,000 and 10,000 candidates**. It writes raw timing
samples to `out/a5-benchmarks/results.json` and CPU/OS/compiler/build/source evidence
to `environment.json`. It initializes the supported x64 developer shell when needed.
Qt, Python and model/API access are not required.

```powershell
.\engine\benchmarks\run.ps1 -SkipBuild -Repeats 11 -Warmups 3
.\out\a5-release\engine\breakwater_benchmark.exe --count 1000 --repeats 7 --warmups 2 --output out/one-benchmark.json
```

`-SkipBuild` still checks the cached Release/ASan-OFF settings and runs CTest. Keep
the CPU otherwise idle when collecting results. No core affinity, power-mode control
or isolation from background system activity is imposed by the runner.

For correctness under MSVC AddressSanitizer:

```powershell
.\scripts\build.ps1 -Preset asan
.\out\asan\engine\breakwater_benchmark.exe --verify-only --count 100
```

Run checked executables in the same initialized developer session so the ASan runtime
DLL is available. Checked and Debug binaries refuse performance measurement. The new
CTest `engine.benchmark_correctness` runs this correctness-only mode on both graphs.
Sanitizer/verification timings are not used as performance evidence.

## Comparable workloads and correctness

The canonical incident has two tanks and one internal transfer. The custom graph has
24 tanks, 60 internal edges, 28 assets and 66 routes, including cycles, branching, two
sources and shared treatment/trucking destinations. IDs contain colons and source-feed,
route and destination costs are nonzero. All data and costs are synthetic.

Four catalog controls vary in each graph. Rows use deterministic Halton values with
a feasible seed at index zero; batches are distinct nested prefixes of the same
sequence, not different graphs or repeated rows. Horizon is three hours, sampled at
0.025 h: 120 updates and 121 samples, including the initial state. Counts are exact.
Fingerprints in the result are FNV-1a identifiers, not security hashes.

Before timing, **every candidate** is compiled, dense-reference simulated and passed
through the independently coded native validator. Both sampled benchmark kernels and
affine endpoints must agree with reference trajectories within the existing
`1e-9 + 1e-10 * scale` tolerance. The production screen must not reject a reference-
feasible candidate. The public search's best-five cost/index ranking is compared with
the entire independently evaluated feasible set, including ties. These checks are
outside performance timers, so an incorrect result cannot earn a faster score.

The private production helpers were extracted without changing their arithmetic or
operation order. `simulate()` and the benchmark call the same dense/sparse/Euler
helpers. `evaluate_candidates()` and the benchmark call the same affine screen.
Compilation, public types, independent validation and mathematical semantics remain.

## Timing scopes

| Phase | Included | Excluded |
|---|---|---|
| `dense_sampled_kernel` | Dense incidence rate updates, Euler, full trajectory writes and observable full-buffer checksum | Network compilation, incidence construction, workspace allocation, public `simulate()` finite-output checks and validation |
| `sparse_sampled_kernel` | Same samples, state resets, trajectory writes and checksum as dense; sparse edge updates | Same setup/public checks as dense |
| `affine_screen_kernel` | Actual production source/route cost, endpoint bounds and shared-destination screening | Setup, sampled fields, survivor sorting and independent validation |
| `reference_validate_retained` | Actual `validate()` and its map/report allocations on retained dense runs | Compilation and dense simulation; all-candidate validation is not timed here |
| `public_search_total` | Full `evaluate_candidates()` call and destruction: preparation, input checks, screen, sorting, finalist compile/dense simulation/validation and materialization | Caller generation of candidate input rows |

Dense/sparse isolate the effect of the rate-update representation on the **same sampled
task**, including common trajectory/checksum overhead. Affine screening removes the
sampled-trajectory task and adds screening checks; a sparse/affine ratio is a workload
reduction comparison, not a pure sparse-kernel speedup or a full validator speedup.
The measured phases overlap and must not be added or subtracted as a stage accounting.

Reference validation applies to the reported retained count, at most five; it does not
validate 10,000 runs inside that timer. In the recorded matrix every search returned
five recommendations. Full search independently validates only its finalists, while
the separate benchmark correctness pass checks all candidates.

Candidate compilation is outside kernel timers and packs only numeric in/out arrays
and internal edge data. Topology/incidence is shared, and one trajectory workspace is
reused. No 10,000 full string-rich network snapshots or sampled trajectories are kept.
`prepared_numeric_bytes` reports these numeric buffers; it is **not total process peak
memory** and excludes candidate strings, search preparation, result objects and allocator
overhead. `preparation_once_ms` measures only this numeric packing once; it is not a
repeated full API preparation benchmark. Allocation scopes are recorded, but an allocator
counter is not installed, so no measured allocation count is claimed.

## Measurement method

The recorded run uses MSVC Release `/O2 /Ob2 /DNDEBUG`, C++20 and `/fp:precise`, with
ASan off. The runner hashes the executable, compilation-command evidence and relevant
source files. The CPU model, architecture, reported logical processors, OS and software
versions are recorded. No custom SIMD or parallelism is enabled.

Each phase gets two warmup batches. Calibration doubles iterations up to 1,024 to aim
for at least 10 ms per sample. Seven repetitions rotate phase order. Each recorded
sample is milliseconds per complete batch after dividing by that phase's iteration
count. Raw samples, min/median/mean/p90/max, population standard deviation and median
nanoseconds per row are retained. P90 uses nearest rank; with seven samples it equals
the maximum, not an inferred long-run latency percentile.

Results are descriptive measurements on this machine, not timing-based CI assertions
or a universal speedup guarantee. Mobile CPU scheduling, power/thermal behavior and
background load affect observed variation. Compiler fences plus a non-inlined consumer
of every dense/sparse output prevent unused work elimination; identical checksum cost
is included in both sampled timings.

## Recorded run: November 7, 2026

Machine: AMD Ryzen 7 7730U, 16 reported logical processors, Windows x64 build 26300;
MSVC 19.51.36260, CMake 4.3.1 / Ninja. Seven repeats, two warmups, serial Release.
All numbers below are **median milliseconds per batch**, not per candidate.

| Graph | Candidates | Dense sampled | Sparse sampled | Affine screen | Validate retained 5 | Full search |
|---|---:|---:|---:|---:|---:|---:|
| Canonical | 100 | .14549 | .14072 | .00680 | .01121 | .14130 |
| Canonical | 1,000 | 1.24415 | 1.08080 | .05488 | .00904 | .49090 |
| Canonical | 10,000 | 18.63820 | 15.53550 | .71158 | .01461 | 7.72950 |
| Custom 24 tanks | 100 | 14.91610 | 1.67358 | .03489 | .12435 | 1.81436 |
| Custom 24 tanks | 1,000 | 119.74160 | 16.59250 | .32001 | .10296 | 2.24113 |
| Custom 24 tanks | 10,000 | 1110.62110 | 143.15390 | 2.63185 | .09170 | 8.25825 |

At 10,000 rows the sampled dense/sparse median ratio is about **1.20x** for the tiny
canonical graph and **7.76x** for the custom graph. The sparse-sampled/affine-screen
ratios are about 21.83 and 54.39 respectively, reflecting different amounts/types of
work. These larger ratios must not be attributed to changing sparse representation.
Raw minima/tails and iteration counts are retained so the medians can be assessed.

Reference-feasible counts at 100/1,000/10,000 were 5/41/381 for canonical and
23/221/2,191 for custom; screen counts matched and each public search reference-checked
five finalists. Maximum forward discrepancy across the whole matrix was approximately
1.14e-12 m3. Best evaluated synthetic costs/indices are in the raw artifact. They are
finite-batch results, not global optima, robustness or sustainability guarantees.

Committed evidence: [raw timings](benchmarks/results/2026-11-07-msvc-release.json) and
[environment/source hashes](benchmarks/results/2026-11-07-msvc-environment.json).
The run correctly records the A4 parent commit and a dirty A5 working tree; source
hashes identify the actual measured files. No source changed between this measurement
and the A5 commit; documentation/evidence was added afterward.

## A7: precomputed control influence

See [CONTROL_INFLUENCE.md](CONTROL_INFLUENCE.md) for the algorithm, proof mapping,
floating-point fallback and exact survivor-cost ordering. Reproduce the new matrix:

```powershell
.\engine\benchmarks\run.ps1 -Influence
.\engine\benchmarks\run.ps1 -Influence -SkipBuild -Count 2048
```

These use separate optimized `out/a7-release` and `out/a7-benchmarks` directories.
The first command measures the requested 100/1,000/10,000 rows on both graphs; the
second measures the dispatch boundary. All use the same candidate generation,
three-hour/121-sample forecast, seven rotated repetitions, two warmups and calibration
as A5. Both commands run all eight Release CTest suites before timing.

Additional version-2 phases:

| Phase | Scope |
|---|---|
| `influence_precompute` | Construct one map and reusable workspace; consume a checksum of all numeric preparation; includes allocation, destruction and checksum work |
| `route_walk_screen_kernel` | Original endpoint/storage/shared-destination/cost screen; consumes all rates, destination totals, status and cost |
| `influence_screen_kernel` | Same rows and output consumption using prepared columns; includes boundary fallbacks and original-order cost recomputation for every potential survivor |
| `route_walk_search_total` | Private baseline using shared production request processing, original screen, ranking, retained simulation and independent validation |
| `public_search_total` | Current public API with automatic workload dispatch; otherwise the same full-search work |

The paired screen ratio compares the same numeric task and includes matching checksum
overhead. Rejected influence-row costs are affine estimates; potential-row costs are
bitwise canonical before ranking. The old A5 `affine_screen_kernel` remains present
with its smaller status/cost checksum and should not be substituted for the paired
route-walk phase. Preparation is measured once, not once per candidate. These phases
overlap and cannot be added or subtracted to account for full-search time.

Before timers, every row is independently compiled, dense-simulated and validated.
Checks cover all-row rate/destination/cost agreement, no discarded reference-feasible
row, and exact best-five cost/index rankings from both full evaluators. The final
matrix and boundary run check 26,296 candidate rows; nested prefixes reuse earlier
rows. Exactly one forced-map fallback occurred per case. Maximum discrepancies were
`7.11e-15 m3/h` in rates, `8.88e-16 m3/h` in destination totals, and `1.14e-12` in
rejected-row cost estimates. Survivor costs and recommendation ordering matched exactly.

### Recorded final run: November 7, 2026

Same Ryzen 7 7730U / 16 reported logical processors, Windows build 26300, MSVC
19.51.36260 and CMake 4.3.1. Release `/O2`, C++20, `/fp:precise`, ASan OFF. Values
are median **milliseconds per complete batch**, not per candidate.

| Graph | Candidates | Old screen | Influence screen | Old full search | Public full search | Public evaluator |
|---|---:|---:|---:|---:|---:|---|
| Canonical | 100 | .00633 | .00651 | .11660 | .13140 | Route walk |
| Canonical | 1,000 | .05948 | .05719 | .43818 | .42309 | Route walk |
| Canonical | 2,048 | .12022 | .11788 | .71847 | .76078 | Route walk |
| Canonical | 10,000 | .50069 | .49457 | 2.95328 | 3.38780 | Route walk |
| Custom 24 tanks | 100 | .02949 | .01638 | 1.33835 | 1.35293 | Route walk |
| Custom 24 tanks | 1,000 | .30001 | .15752 | 1.81665 | 1.84916 | Route walk |
| Custom 24 tanks | 2,048 | .62973 | .32630 | 2.47959 | 2.22734 | Influence |
| Custom 24 tanks | 10,000 | 3.36940 | 1.54706 | 7.84100 | 7.16745 | Influence |

For custom 10,000 rows the paired screen ratio is **2.178x**, while full search is
**1.094x**; input processing, sorting and retained validation limit the total gain.
At the selected 2,048 boundary, the ratios are **1.930x** and **1.113x**. Preparation
including workspace/checksum costs approximately 1.45–1.82 microseconds for canonical
and 4.08–4.23 microseconds for custom, excluded from paired screen timers and included
in full searches that select the influence path.

The initial all-influence implementation showed no canonical full-search benefit
(10,000 rows: 4.20275 ms old versus 5.37553 ms influence). The final policy selects
influence only with at least 32 routes and 2,048 rows; smaller requests retain the
original algorithm. This heuristic was checked at the boundary and is not a universal
break-even rule. Even route-walk-selected public/direct-baseline timings differ in
the final sample. Wrapper/code-generation effects and system variation are not
isolated; no universal zero-overhead or no-regression claim is made. Mobile power,
thermal and background-load conditions remain uncontrolled. Raw distributions are
retained instead of asserting performance in CI.

Final evidence:

- [100/1,000/10,000 raw matrix](benchmarks/results/2026-11-07-control-influence-msvc-release.json).
- [Machine, binary and source hashes](benchmarks/results/2026-11-07-control-influence-msvc-environment.json).
- [2,048 boundary timings](benchmarks/results/2026-11-07-control-influence-msvc-threshold.json).
- [Boundary environment/source evidence](benchmarks/results/2026-11-07-control-influence-msvc-threshold-environment.json).
- Initial [timings](benchmarks/results/2026-11-07-control-influence-msvc-release-initial.json)
  and [environment](benchmarks/results/2026-11-07-control-influence-msvc-environment-initial.json)
  preserve the measurements that motivated dispatch.

The records identify parent `5c47d4f` plus the actual dirty implementation's source
hashes; documentation/evidence followed measurement. A separate MSVC ASan build
passed all eight suites, including 14 influence acceptance groups; sanitizer timing
is not used here. Lean's 21-theorem audit proves the exact-real algebra, not the
measured speed, floating-point classification, ranking or global optimality.
