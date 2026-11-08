# A-001: exact requested storage horizons

Person A follow-up to [AUDIT_PERSON_A.md](AUDIT_PERSON_A.md), on
`codex/person-a-engine`, based on audit commit `dff133c`. The A-001 implementation
and regression checks are complete. A-002, A-003 and A-004 remain open.

## Behavior and implementation

An accepted storage forecast starts at zero, retains every rounded float64
`k*step_h` strictly before the requested horizon, then appends that exact horizon
once. It contains at least two samples and at most 10,000 updates. No regular
interior sample is shifted to absorb a small terminal interval.

Native simulation, search request validation and benchmark sampling use the private
`engine/src/storage_grid.hpp` policy. The work bound tests the actual rounded
10,000th multiple, not `ceil(horizon/step)`. Division is used only for an allocation
hint. A positive subnormal horizon and a larger step yield `[0,horizon]`; a tail
requiring update 10,001 fails explicitly before candidate screening. Invalid
native configurations throw `std::invalid_argument` rather than producing no plan.

The independent native validator requires exact equality with the original
requested horizon and recomputes storage bounds using that original horizon.
A one-ULP-short or extended result fails with `forecast_horizon`, even for a
stationary tank whose volumes would otherwise pass.

PyTorch and the worker mapper use `science/storage_grid.py`, a standard-library-only
helper. The mapper rejects excessive work before loading PyTorch. Worker publication
independently reconstructs the full requested grid and verifies exact preview
timestamps, original sample indices and both endpoints. A mapper that omits the
terminal interval fails with `numerical_failure` and cannot mark execution verified.
Previews remain samples from the actual full forward rollout, including the final
state; no output values are clipped.

Selected-control derivatives still use one constant-rate interval over the original
horizon. The forward grid now includes that same endpoint; tests verify the selected
terminal Jacobian is exactly `-horizon` (or the corresponding signed tank entries)
in the one-ULP-tail fixture. Ordinary gradient checks retain their documented
finite-difference tolerances.

## Regression cases

| Requested horizon / step (h) | Accepted full grid / result |
|---|---|
| `.9000000000000001 / .1` | 11 samples; retains `.9`, then the exact one-ULP-later horizon |
| `.30000000000000004 / .1` | 4 samples; final time occurs exactly once |
| Smallest positive double / `24` or `1e308` | 2 samples `[0,horizon]`; no ratio-underflow omission |
| `1.1 / .3`, step greater than horizon, ULP neighbors | Exact horizon and strictly increasing samples |
| `1 / .0001` | Exactly 10,000 updates, 10,001 samples |
| `.0082 / 8.2e-7` | Exactly 10,000 updates even though division rounds above 10,000 |
| `nextafter(1,+inf) / .0001` | Explicit work-limit failure; no lost terminal interval |

The original false-feasibility network has source flow `2^50` m3/h, initial tank
inventory zero and capacity `.9 * 2^50` m3. At the requested
`.9000000000000001` h horizon, **all three native solvers, PyTorch and the actual
worker return the requested endpoint, 0.125 m3 above capacity**. Native validation
reports `storage_capacity`; bounded search returns no feasible recommendation.
The deliberately extreme finite magnitude remains part of the reproduction.

Six new executed native fixtures join the existing twelve. The verifiers compare
full native/PyTorch timestamps exactly, and wire preview timestamps against exact
full-grid positions. The extreme-flow endpoint and native infeasibility receive
strict assertions: relative trajectory tolerances cannot hide the 0.125 m3 error.
Native tests also reject altered one-ULP-short/extended runs and check that invalid
configurations fail even when screening would reject every candidate.

## Verification

All checks use the existing Windows x64 MSVC C++20 and CPU Python 3.12.13 / PyTorch
2.15.0+cpu environment. No dependency or public protocol schema changed. Native
and ASan builds are isolated under `out/audit-person-a/`; generated binaries,
logs and JSON evidence remain ignored under `out/a001/`.

| Check | Result |
|---|---|
| MSVC native build and CTest | 8/8 suites passed, including added engine/search regressions |
| Separate MSVC AddressSanitizer build and CTest | 8/8 suites passed; no sanitizer reports |
| Python science suite | 79/79 tests passed; no skipped tests |
| Native/PyTorch parity | 18 executed native cases, all 3 solvers; exact grids; 44 interior controls with 132 central-difference checks |
| Actual worker/native parity | 302 NDJSON requests across 18 cases; offline schema checks passed; same 44 controls / 132 checks |
| Optimized benchmark rerun | 100 / 1,000 / 10,000 candidates on both graph shapes; every row/reference/ranking check passed |

Maximum volume disagreement on the parity cases was `7.67386154621e-13` m3
(absolute tolerance 1e-9 plus relative 1e-10). Gradient tolerances remain absolute
1e-6 plus relative 1e-4. Timestamp comparisons and the extreme endpoint are exact.

The optimized benchmark ran separately from tests on the existing AMD Ryzen 7 7730U
host, with precise floating point, two warmups and seven repetitions. Median batch
times below separate equal-work sampled kernels from affine screening (which avoids
trajectories/final validation) and full public search. They are measurements, not
timing acceptance thresholds or evidence of a universal speedup.

| Graph | Candidates | Dense sampled (ms) | Sparse sampled (ms) | Affine screen (ms) | Public full search (ms) |
|---|---:|---:|---:|---:|---:|
| canonical | 100 | .108964 | .111619 | .004431 | .110586 |
| canonical | 1,000 | .980919 | 1.044744 | .046041 | .382531 |
| canonical | 10,000 | 9.367800 | 9.795400 | .431900 | 3.569700 |
| custom, 24 tanks | 100 | 10.956600 | 1.509300 | .027638 | 1.415213 |
| custom, 24 tanks | 1,000 | 112.667300 | 15.142600 | .266631 | 1.913900 |
| custom, 24 tanks | 10,000 | 1182.685800 | 161.097200 | 2.929775 | 7.544000 |

The 121-sample default grid and public dispatch are unchanged: route walk for small
workloads, precomputed influence for the 10,000-row custom workload. Raw JSON
retains paired route/influence phases, spread and checksums. This rerun did not
repeat the separate 2,048-row timing; its dispatch was already exercised in native
regression tests. Benchmark source hashes now include the grid helper.

Reproduce in an MSVC x64 Developer PowerShell with the existing pinned environment:

```powershell
cmake --build out/audit-person-a/native --parallel 2
ctest --test-dir out/audit-person-a/native --output-on-failure
cmake --build out/audit-person-a/asan --parallel 2
ctest --test-dir out/audit-person-a/asan --output-on-failure
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/verify_rollout.py --native out/audit-person-a/native/engine/breakwater_rollout_reference.exe --report out/a001/rollout-verification.json
science/.venv/Scripts/python.exe science/verify_worker.py --native out/audit-person-a/native/engine/breakwater_rollout_reference.exe --report out/a001/worker-verification.json
.\engine\benchmarks\run.ps1 -Influence
```

For first-time build configuration, use the isolated CMake commands in the audit
report. Current logs are `out/a001/native-build.log`, `native-tests.log`,
`asan-build.log`, `asan-tests.log`, `python-tests.log`, `rollout-verification.log`, `worker-verification.log` and their
JSON reports. Worker evidence records the helper and native source hashes.

Benchmark outputs are `out/a7-benchmarks/results.json` and `environment.json`,
copied to `out/a001/benchmark-matrix.json` and `benchmark-environment.json`;
the command log is `out/a001/benchmarks.log`. Evidence was generated before the
fix commit, so the benchmark records parent `dff133c` with `source_dirty=true`
and hashes of the actual changed sources.

| Local report | SHA-256 |
|---|---|
| rollout-verification.json | `1b6ce4c06d41766125eff87ecc89408a7ba8be8841e6c6d87f645e2c9f65d69d` |
| worker-verification.json | `718b8ca8b452fdbff6e83758822756e16903c390658b366c69b56ba03cb61b73` |
| benchmark-matrix.json | `cf7e6816c5586be6ae466e4c351e6c23c1b13d9df42a1677d306df9b7a0b6491` |

## Ownership, provenance and integration

Only `engine/` and `science/` changed. The equations, scenario constants, ownership,
quality/volume separation and approval authority remain the same. No Person B
application, contract, root build/solution or shared documentation was edited.

The Torch implementation version is `torch-constant-flow-0.1.1`; numeric worker
protocol/method versions remain 1. The native `solver_version` string lives in
Person B's shared types and was left untouched (`constant-flow-0.1.0`). Identify
this implementation using the fix commit and executable/source hashes; do not use
that unchanged string to treat old forecasts as newly validated. B can reconcile
shared provenance/version presentation when integrating.

B's deployment must include the new standard-library `science/storage_grid.py`
beside `worker_storage.py` and the `breakwater_science` package. A standalone
`worker.py` still supports health/capabilities without Torch. The v1 wire shape is
unchanged and already requires the exact requested horizon. B should retain the
original request and full native timestamp grid without normalizing its endpoint.

No Lean source or numerical recurrence was changed. The existing exact-real proofs
do not certify floating-point grid construction; this increment checks that gap
with implementation regressions and actual cross-language executions. Lean and the
separate PhysicsNeMo training smoke were not rerun for this storage-grid-only fix.
