# A5: measured native candidate performance

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
