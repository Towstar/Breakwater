# A8: agreed scientific worker mapping

`worker.py` now implements `capabilities` and `storage_rollout` from the published
[B3 contract](../docs/SCIENCE_CONTRACTS.md) and [schemas](../contracts/science/).
It uses the existing CPU float64 PyTorch model. Native C++ remains authoritative
for feasibility, stored run identity and simulated operator approval.

## Run and verify

Use the pinned Python 3.12/A3 environment in [README.md](README.md), or the executed
A4 environment. The worker needs Torch for storage; health and capabilities use
only the standard library. Add the separate hash-locked schema test dependencies
without replacing the scientific environment:

```powershell
uv pip install --python science/.venv/Scripts/python.exe --require-hashes -r science/requirements-worker-tests-win-py312.txt
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/verify_worker.py --native out/headless/engine/breakwater_rollout_reference.exe --report out/a8-worker-verification.json
```

`jsonschema==4.25.1` and its dependencies are test-only; runtime mapping does not
depend on jsonschema. The test lock constrains shared versions to the existing A4
lock. Refresh it deliberately with `uv pip compile science/requirements-worker-tests.in
--python science/.venv/Scripts/python.exe --generate-hashes --output-file
science/requirements-worker-tests-win-py312.txt`, then repeat verification.

For a real contract-example request, serialize the pretty JSON to one line first:

```powershell
$request = Get-Content -Raw contracts/science/examples/storage-rollout-request.json | ConvertFrom-Json
$request | ConvertTo-Json -Depth 32 -Compress | & science/.venv/Scripts/python.exe science/worker.py
```

Each UTF-8 input/output frame is exactly one JSON object followed by a newline.
The process accepts multiple requests sequentially. Closing/retiring the process
is the caller's cancellation mechanism; v1 introduces no cancellation method.

## Methods and execution state

| Method | Behavior |
|---|---|
| `health` | Preserves the existing five-field result, including `methods: ["health"]` and `scientific_execution: "not_implemented"`, for the strict desktop health client |
| `capabilities` | Reports mapper presence, supported methods/limits, no artifact support, and process-local scientific execution verification |
| `storage_rollout` | Executes the agreed project-network mapping and optional selected-control derivatives |
| `pipe_transport` | Explicit `not_implemented`; the desktop pipe viewer already runs authoritative native FVM directly |

New callers use `capabilities` for science availability. The legacy health field
is preserved for compatibility, rather than expanded into a different response
shape. Storage's `execution_verified` starts false and becomes true only after an
actual result passes identity/shape/numeric checks and the serialized byte budget.
It records successful execution in this process, not qualification of future inputs.
Restart resets it. Package discovery never establishes scientific execution.

For storage, `implemented: true` currently means only that `worker_storage.py`
exists beside `worker.py`. It does not prove that the helper/package is complete,
Torch imports successfully, or a numerical job has executed. A missing runtime can
therefore coexist with that implementation flag and yield `runtime_unavailable`
on a storage request. A successful, checked result in this process is required for
storage's `execution_verified: true`; neither flag grants native validation or approval.

Scientific imports are lazy. Python/library stdout and stderr are discarded during
imports/calculation, keeping normal stdout protocol-only and errors bounded. Missing
runtime/deployment and unsupported operations return explicit schema-defined errors.
No generic PhysicsNeMo inference is advertised: A4 remains a separate fixed-case smoke.

## Mapping, controls and derivatives

Storage method version 2 adds [explicit source-feed allocations](../contracts/SOURCE_FEEDS.md).
The worker advertises its highest storage version (2), accepts legacy method 1,
and echoes the requested method version. Method 1 rejects the new share field.
Shares are integer hundredths of a percent totaling exactly 100%; branch flows
and costs use the configured fraction of production after curtailment. Source
curtailment derivatives affect every receiving tank and every positive-share
branch capacity must admit the selected interior value. Zero-share branches do
not create false derivative boundaries. Shares are not new control IDs.
Project format version 3 and scientific method/result version 2 are independent;
G1's reserved project-v2 telemetry envelope is unchanged.

Run the independent split-flow check with a fresh output directory:

```powershell
science/.venv/Scripts/python.exe science/verify_source_feeds.py --native out/headless/engine/breakwater_source_feed_reference.exe --output out/source-feed-attempt-01
```

`worker_storage.py` validates the envelope payload, identity, project network and
selected controls before loading Torch. It translates destination `capacity_m3h`
into the private model's kind-dependent `capacity_m3` field; tank capacities remain
m3. Project defaults and permitted extra asset/route properties follow the existing
desktop format. IDs are preserved whole, including embedded colons and Unicode.
Invalid captured drafts cannot be repaired by an override.

All unspecified controls keep their captured values. Output tank order follows the
captured asset order. Volumes are `volumes_m3[tank][time]`; costs are synthetic units
over the requested horizon. Finite negative/over-capacity predictions are retained
as evidence. No `feasible` verdict or native run ID is created.

With `derivatives: true`, derivative columns are **exactly the explicit `controls`
list in request order**. An empty list returns empty columns. Selected controls
must lie strictly inside their positive-width admissible bounds; source curtailment
must also lie inside the feed-capacity domain. Boundary/fixed selections fail with
`invalid_controls`, rather than shifting the requested values or dropping columns.
Ordinary forward requests accept admissible boundary settings.

Terminal-volume Jacobians have units h (m3 per m3/h); cost gradients have units
synthetic units per m3/h. The full requested-step forward rollout runs under
`torch.no_grad()`. Derivatives use autograd through one constant-rate interval,
avoiding a 10,000-step backward graph. One-step/full-rollout AD and three central
difference sizes are compared in tests. This is the continuous constant-rate model's
derivative, not a derivative through rejection, discrete search, approval or JSON
rounding; neither gradients nor package availability establish feasibility.

Snapshot/run IDs, numerical revision and hashes are validated and echoed. The worker
does not authenticate their source, reproduce Qt's hash serialization, access the
desktop run store or grant approval. The desktop must bind results to its captured
settings and current job generation before publication.

## Work and output bounds

The method retains the native 128-asset/256-route, 24-hour and 10,000-update limits.
Requests support at most 384 explicit control entries, subject to real catalog and
uniqueness checks. Invalid/nonrepresentable grids and nonfinite outputs fail explicitly.

The combined route/destination unit price for each tank-origin discharge must be
finite. Overflow here is `invalid_network`, including at zero/tiny flow, and is
rejected before loading PyTorch. A finite unit price with a nonfinite hourly or
forecast cost yields `numerical_failure`; finite lower-flow overrides remain
available. See [A-002 resolution](A002_COST_FIX.md) for native/worker parity checks.

Storage preserves each rounded float64 `k*step_h` strictly before the horizon,
then appends the original horizon exactly once. A one-ULP terminal interval counts
toward the 10,000-update limit; no interior sample is shifted to hide it. Division
rounding does not determine the sample count. Even a positive subnormal horizon
with a larger step has both endpoints. Publication independently reconstructs this
grid and requires exact timestamps and original indices, including the final index.
See [A-001 resolution](A001_HORIZON_FIX.md) for the native/Torch/worker checks.

Previews contain every tank and at most the requested 2..256 times. They retain time
zero, the exact horizon and original `time_indices`; values come from the full forward
grid. If needed, publication reduces only preview times to fit the **1 MiB including
newline** budget. It preserves requested derivatives and returns `output_too_large`
if even two frames do not fit. JSON is never truncated; values are never clipped.

Malformed UTF-8/JSON, duplicate keys, nonfinite JSON constants, excessive nesting and
unterminated/oversized frames fail visibly. Valid request IDs survive semantic errors,
including finite-range overflow in otherwise well-formed JSON. Protocol versions
accept JSON numeric representations of one and reject booleans.

## Delivery boundary for Person B

In the desktop source inherited by this Person A branch from baseline `6fb8c29`,
the adapter requests health only; it does not negotiate these science methods or
publish their results. The installer in that snapshot copies only `worker.py`.
These claims describe this branch, not the status of Person B's parallel branches
or an independently installed application. That worker file remains self-contained:
an installed copy still supports health/capabilities, truthfully reporting storage
unavailable when its mapper is absent.

To deploy science, B must additionally install `worker_storage.py`, the standard-library
`storage_grid.py` helper and the `breakwater_science` package beside it, select a Python environment with the pinned
Torch runtime, and extend the asynchronous adapter for capability/result negotiation.
The repo worker already has those modules beside it. Keep identity, staleness,
timeouts/retirement, output bounds and independent volume approval in that adapter.
No B-owned application, contract, packaging or root configuration files are changed
by this A workstream.

## Historical A8 delivery evidence

On November 7, 2026, the complete scientific suite passed **69/69 tests**, including
16 real subprocess protocol tests and 14 mapper/derivative tests. The actual native
verification driver sent **290 NDJSON requests** across **12 native cases**, compared
all three native solvers, and checked **44 selected interior controls** with
**132 central-difference comparisons**. Request/response schemas were validated
offline against the committed Draft 2020-12 schema registry.

Maximum volume difference was `7.67386154621e-13 m3`; maximum finite-difference
terminal/cost derivative errors were `4.44836967262e-9 h` / `1.94024778466e-8`
synthetic units per m3/h. The largest response in that fixture run was 1,684 bytes;
separate protocol tests exercise byte-budget failure/reduction. Fixture IDs and
fingerprints are verification-only, not application run-store records.

The [versioned report](evidence/2026-11-07-worker-verification.json) records Python,
Torch/schema versions, native executable and source/lock/schema hashes, forward and
gradient tolerances, endpoint values and verification transitions. No checkpoint,
environment, credential or generated native executable is committed.

These counts describe A8 delivery. Subsequent numerical fixes and executions are
recorded in [A-001](A001_HORIZON_FIX.md), [A-002](A002_COST_FIX.md) and
[A-003](A003_RESIDUAL_FIX.md). The [A-004 documentation check](A004_STATUS_FIX.md)
separately verifies current worker status without changing runtime behavior.
