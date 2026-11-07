# A3: differentiable fixed-flow storage

This is the standalone Person A science module. It implements a CPU `torch.float64`
rollout for captured finite storage networks, with actual native-reference comparisons
and continuous-control gradient checks. C++ remains the independent feasibility
authority. The desktop, shared project schema and health-only worker are unchanged.

A4 now adds a separate, actually executed PhysicsNeMo pipe smoke. Its larger pinned
environment, training/evaluation commands and experimental scope are documented in
[PHYSICSNEMO.md](PHYSICSNEMO.md). The A3 instructions below remain independently usable.

## Reproduce on Windows x64

Use Python **3.12.13** and the hash-locked CPU environment. From the repository root:

```powershell
uv python install 3.12.13
uv venv science/.venv --python 3.12.13
uv pip sync science/requirements-win-cpu-py312.txt --python science/.venv/Scripts/python.exe --index-url https://download.pytorch.org/whl/cpu --require-hashes
.\scripts\build.ps1 -Preset headless
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/verify_rollout.py --native out/headless/engine/breakwater_rollout_reference.exe --report out/a3-verification.json
```

The lock resolves Torch **2.15.0+cpu**, NumPy **2.5.4**, and all transitive dependencies.
It was executed on CPython 3.12.13 / Windows x64 using uv 0.11.9. It is not a promise
of compatibility with a future PhysicsNeMo environment or other platforms. No CUDA
runtime is needed. Environments and generated reports stay in ignored `.venv/` / `out/`.

To deliberately refresh the environment, review the resulting pins/hashes and rerun
all checks; do not silently update the dependencies during ordinary verification:

```powershell
uv pip compile science/requirements.in --python science/.venv/Scripts/python.exe --index-url https://download.pytorch.org/whl/cpu --generate-hashes --output-file science/requirements-win-cpu-py312.txt
```

The native engine requires C++20/MSVC and no Torch dependency. Its additive
`breakwater_rollout_reference` executable generates current C++ results rather than
reading a saved golden file. Do not regenerate the B-owned root solution in A's PR;
B can include this new target during the next integration.

For checked native execution, run both commands in the same PowerShell session.
The build script initializes the MSVC developer environment, whose PATH supplies the
AddressSanitizer runtime DLL. A new session must initialize that environment again:

```powershell
.\scripts\build.ps1 -Preset asan
science/.venv/Scripts/python.exe science/verify_rollout.py --native out/asan/engine/breakwater_rollout_reference.exe --report out/a3-verification-asan.json
```

AddressSanitizer covers the owned native executable/library, not PyTorch's prebuilt
Python extension or the Python interpreter.

## API and equations

Add `science/` to the Python module path and import
`from breakwater_science import StorageModel`. Construct a model from `assets` and
`routes` using native C++ member names, including `kind`, `capacity_m3`, `initial_m3`,
`inflow_m3h`, `curtailment_m3h`, `curtailment_max_m3h`, `setpoint_m3h`, availability and
costs. Destination `capacity_m3` retains the native kind-dependent meaning of m3/h.
This normalized mapping and the exporter's `fixture_version: 1` are **private science
test inputs**, not a replacement project/worker wire format.

- `model.controls` / `model.control_ids` retain stable full IDs. Sources come first in
  asset order, then tank-origin routes in route order. IDs are never split on colons.
- `model.default_controls()` returns a new CPU float64 vector.
- `model.rollout({control_id: scalar}, horizon_h=3, step_h=.025)` overrides only listed
  settings. Scalar CPU float64 tensors preserve their autograd graph; numbers are
  constants. Omitted settings preserve the captured operating state.
- `model.rollout_vector(vector, ...)` explicitly overrides the full catalog in that
  order; it requires a one-dimensional CPU float64 tensor with strict control bounds.
- Results contain tank-major `volumes_m3`, `time_h`, `rates_m3h`, resolved `control_values`
  and a scalar synthetic `cost`. No feasibility verdict is supplied by Torch.

All inputs are captured independently of later mutations to the input mapping.
Time zero is the initial state. `ceil(horizon/step)` updates include the exact final
horizon, including a shortened last interval. The supported horizon is `(0,24]` hours
with at most 10,000 steps, 128 assets and 256 routes, matching the native base.

An incidence matrix over all routes maps nonnegative flows to storage rates:

```text
source route: q = nominal production - source curtailment
tank route:   q = route setpoint
dV/dt = incidence * q
V[k+1] = V[k] + (t[k+1]-t[k]) * dV/dt
cost = T * (source curtailment costs + route costs + destination discharge costs)
```

Source-route setpoints are not independent controls. Reducing production also saves
the associated feed-route cost, so curtailment's cost derivative includes both terms.
Topology and input/control validation follow the native compiler; invalid drafts
cannot be rescued by overrides. Source curtailment rechecks actual feed capacity.

Native structural checks allow a stored route flow up to `capacity + 1e-9`; an omitted
stored setting preserves that tolerance. Explicit overrides, including a full vector,
must satisfy catalog bounds strictly. This distinction is tested without clipping.
Shared-destination excess, negative storage and storage overflow remain forecast
evidence. Native validation decides their feasibility separately.

## Verification and gradient scope

`verify_rollout.py` executes the supplied native exporter in the current run. For each
case it compares all samples, costs and stable tank IDs against actual dense, sparse
and affine executions. Exact sample times and tank order are required; forward error
uses `1e-9 + 1e-10 * max(abs(actual), abs(reference))`, in the quantity's units.

The cases cover canonical healthy/failure/bad/mixed plans, escaped/renamed IDs, feed
costs, branching/shared destinations, nondividing steps, zero controls, capacity
equality, tolerated stored defaults and deliberately infeasible unclipped forecasts.
The report records native executable hash, input hashes, versions, each comparison
and the native validator's independent verdict.

Gradient checks use the fixture controls with positive-width bounds at their
midpoints. Fixed controls stay fixed. Current fixtures leave those midpoints within
source-feed capacity; the command does not search for differentiable interiors of
arbitrary user-supplied constraint sets. Every selected control is checked at all
three perturbation scales, plus PyTorch `gradcheck`:

```text
epsilon = relative_step * max(1, abs(control in m3/h))
relative_step = 1e-3, 1e-4, 1e-5; cap epsilon at one eighth of bound width
derivative = (f(u + epsilon) - f(u - epsilon)) / (2 * epsilon)
acceptance = 1e-6 absolute + 1e-4 relative
```

Checked outputs are the terminal-volume Jacobian (hours), cost gradient (synthetic
units per m3/h), and gradient of the dimensionless smooth objective
`sum((terminal_m3 / 100 m3)^2) + cost / 10000 synthetic units`. Scales are fixed rather
than chosen from the candidate. Unit tests also check independent physical derivative
signs, internal-transfer conservation and the source/feed-cost derivative.

Bounds/structure rejection is outside the differentiable calculation. Autograd does
not establish gradients through boundary projection, max/min constraints, failed
controls, discrete search or approvals. These gradients are not an optimizer or a
learned model. There is no transport differentiation, PhysicsNeMo execution, network
chemistry or quality qualification in A3. Worker method exposure waits for B3's agreed
contract; package detection alone does not claim scientific execution by the worker.

References: [official PyTorch installation](https://pytorch.org/get-started/locally/),
[gradcheck mechanics](https://docs.pytorch.org/docs/stable/notes/gradcheck.html).
Actual run evidence is recorded in [PROGRESS.md](PROGRESS.md).
