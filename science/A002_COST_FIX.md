# A-002: finite costs and independent validation

Person A follow-up to [AUDIT_PERSON_A.md](AUDIT_PERSON_A.md), implemented on
`codex/person-a-engine` after `d602160`. A-002 is fixed and regression-verified.
A-001 remains fixed; A-003 and A-004 remain open.

## Changes

The native tolerant comparison rejects nonfinite arguments. It can no longer
accept a supplied finite result cost as equal to an independently computed
infinity through `inf <= inf`.

Independent validation reconstructs hourly cost in the original source-then-route
order from `CompiledNetwork.network`, then multiplies by the requested horizon.
It explicitly reports `cost_overflow` for a nonfinite hourly or total cost and
`cost_mismatch` for a supplied nonfinite, negative or disagreeing result cost.
It does not trust the cached `CompiledNetwork.cost_per_h` or the supplied result's
cost as an oracle. All prior cost terms, ordering and tolerances for finite values
are preserved.

The control catalog cannot publish an infinite unit price. `inspect_network`
reports `cost_overflow` for an overflowing combined route/destination price on a
tank-origin discharge; `control_catalog` and `compile` reject it explicitly with
`std::invalid_argument`. Independent validation checks this unit-price domain again,
including when the actual charge at a tiny flow happens to be finite.

This is an intentional representability limit: `DBL_MAX + DBL_MAX` is an invalid
catalog unit price even if the route currently has zero flow. Prices are not
clamped, rescaled or replaced with zero. A single unit price equal to `DBL_MAX`
remains supported where its computed charge is finite.

An overflowing *operating* cost with finite catalog unit prices is handled
differently: compilation remains available so a search can lower controls and
find finite candidates. Native simulation retains the infinite cost as evidence;
validation rejects it. Search numerical screening rejects nonfinite candidate
costs while retaining and ranking finite repairs. No nonfinite cost can be returned
as a validated recommendation.

PyTorch's captured network and the worker's standard-library mapper enforce the
same combined-unit-price rule. The worker returns `invalid_network` before loading
Torch for an unrepresentable catalog price. An actual nonfinite rollout cost still
returns `numerical_failure`; finite explicit overrides of an overflowing baseline
remain executable. Numeric protocol/method versions stay 1. The Torch implementation
version is now `torch-constant-flow-0.1.2`.

## Regression coverage

| Area | Cases and expected behavior |
|---|---|
| Catalog | Finite individual route/destination prices whose sum overflows are rejected, including zero and small flow; treatment and trucking covered in science tests |
| Independent cost reconstruction | Route product, destination product, source-curtailment product, sum of finite charge terms, and finite hourly cost times horizon overflow all reject supplied finite/Inf/NaN costs |
| Cache independence | Overflow in source-of-truth prices cannot be hidden by cached zero; genuine finite/zero costs still validate when the unrelated cached value is zero, Inf or NaN |
| Nonfinite comparison | Overflow cases require both explicit `cost_overflow` and `cost_mismatch` diagnostics, including with supplied cost zero |
| Finite controls/results | Single `DBL_MAX` price, finite `DBL_MAX` total, zero flow and ordinary zero-cost operation remain valid; altered finite/Inf/NaN and negative supplied costs fail |
| Search | Overflowing rows are counted as numerical rejections; finite lower-flow candidates and exact cost/index ordering survive |
| Automatic influence path | 32 routes and 2,048 candidates: 512 overflow rejections, 1,536 screen passes, five independently validated zero-cost recommendations in original index order; agrees with route walk |
| Worker boundary | Actual NDJSON execution with Python site packages disabled rejects invalid combined prices before backend loading, then continues to answer health |
| Existing science | Canonical mixed plan stays 80/117 m3 with 12,780 synthetic cost; A-001 exact-horizon regressions remain passing |

The independent-validation tests intentionally compile/simulate safe physical
inputs, then change only the stored source-of-truth prices while keeping cached
cost zero. This isolates independent reconstruction instead of allowing a new
compile-time rejection to hide the original validator defect. Separate tests
compile a genuinely overflowing operating setting, retain its infinite simulated
cost, replace supplied result costs, and require rejection for every solver.

Executable regressions are in `engine/tests/engine_tests.cpp`,
`engine/tests/search_tests.cpp`, `engine/tests/control_influence_tests.cpp` and
the science rollout/mapper/protocol tests. The existing test executables reproduce
the cases directly; no generated fixture or external hosted model is required.

## Fresh verification

Existing Windows x64 MSVC C++20 / precise-floating-point builds and Python 3.12.13,
PyTorch 2.15.0+cpu environment were used. Dependencies were not upgraded.

| Check | Actual result |
|---|---|
| MSVC native build and CTest | 8/8 suites; 4.61 s |
| Separate MSVC AddressSanitizer build and CTest | 8/8 suites; 28.83 s; no sanitizer reports |
| Full Python science suite | 84/84 tests; 41.834 s; no skipped tests |
| Native/PyTorch parity | 18 executed cases, 3 native solvers, exact timestamp comparisons |
| Actual worker/native parity | 302 NDJSON requests across 18 cases; offline schema checks pass |
| Gradients | 44 selected interior controls and 132 central-difference checks |

Maximum observed volume disagreement remains `7.67386154621e-13` m3 with the
existing absolute 1e-9 / relative 1e-10 forward tolerances. Gradient tolerances
remain absolute 1e-6 / relative 1e-4. Extreme cost assertions check exact finite
values or explicit rejection, so relative tolerances cannot conceal overflow.

Reproduce from the repository root in an MSVC x64 Developer PowerShell with the
existing pinned environment and isolated build configurations from the audit:

```powershell
cmake --build out/audit-person-a/native --parallel 2
ctest --test-dir out/audit-person-a/native --output-on-failure
cmake --build out/audit-person-a/asan --parallel 2
ctest --test-dir out/audit-person-a/asan --output-on-failure
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/verify_rollout.py --native out/audit-person-a/native/engine/breakwater_rollout_reference.exe --report out/a002/rollout-verification.json
science/.venv/Scripts/python.exe science/verify_worker.py --native out/audit-person-a/native/engine/breakwater_rollout_reference.exe --report out/a002/worker-verification.json
```

Local evidence is under ignored `out/a002/`: native/ASan build and test logs,
`python-tests.log`, parity command logs and JSON reports. These outputs are not
Git deliverables. The worker report records actual source/executable hashes.
Local report SHA-256 values:

| Report | SHA-256 |
|---|---|
| rollout-verification.json | `83620f762a624ed17b28c30a1756622f90c8e30981a1d163f1665ee0234e47e0` |
| worker-verification.json | `ca96bca0d8540f4160693c545ac2758135d7cd679b8db646305cc8df713c06ae` |

The existing benchmark-correctness CTest and the added 2,048-row differential
regression passed; no new performance timing claim is made here.

## Scope and integration

Only A-owned engine/science files changed. Person B's application, contracts,
solution, root configuration and shared documentation remain untouched. The native
version string is defined in B-owned shared types and remains unchanged; use this
fix's source commit and executable/source hashes to identify the implementation.

Read-only review found existing exception handling in B's gateway and history
paths; full desktop/installed-app acceptance remains B's responsibility. As already
required by A-001, deployment includes the mapper, `storage_grid.py`, the science
package and pinned runtime. The wire shape and native approval authority are
unchanged. No live Nemotron call was needed for this fix.

No Lean or FVM algorithm changed. The exact-real proofs do not certify IEEE cost
overflow or the compiled comparator; these changes are checked by implementation
regressions and sanitizer executions. Lean and the separate PhysicsNeMo training
smoke were not rerun for this cost-validation increment.
