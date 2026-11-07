# A4: actual PhysicsNeMo transport smoke

This increment executes NVIDIA PhysicsNeMo's `FullyConnected` model, physical PDE
residuals, bounded training, its `.mdlus` save/load, and held-out inference against
fresh native finite-volume transport results. It is a **fixed-case experimental
smoke**, separate from volume feasibility, approvals and industrial quality claims.

## Run on Windows x64 / Python 3.12.13

From the repository root, using the A3 Python environment:

```powershell
uv venv science/.venv --python 3.12.13
uv pip sync science/requirements-physicsnemo-win-cpu-py312.txt --python science/.venv/Scripts/python.exe --index-url https://pypi.org/simple --torch-backend cpu --require-hashes
.\scripts\build.ps1 -Preset headless
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/physicsnemo_smoke.py --native out/headless/engine/breakwater_physicsnemo_reference.exe --output out/a4-smoke
```

The resolved CPU environment pins PhysicsNeMo **2.2.2**, Torch **2.15.0+cpu**, NumPy
**2.5.4**, and its transitive dependencies/hashes. Native Windows was actually used;
WSL, Docker and a CUDA GPU were not required for this MLP smoke. PhysicsNeMo's imported
Warp dependency emits a missing-CUDA-driver warning on this machine; the recorded
model and all calculations use CPU. No GPU speedup or GPU execution is claimed.

Keep the base A3 lock for its smaller environment; the A4 lock extends it. Syncing
the smaller lock intentionally removes optional A4 packages. To deliberately refresh
the complete A4 lock and then rerun checks:

```powershell
uv pip compile science/requirements-physicsnemo.in --python science/.venv/Scripts/python.exe --index-url https://pypi.org/simple --torch-backend cpu --generate-hashes --output-file science/requirements-physicsnemo-win-cpu-py312.txt
```

PyTorch-specific CPU routing matters: using the CPU wheel index as an ordinary extra
index made older copies of unrelated dependencies shadow compatible PyPI packages.
The tested `--torch-backend cpu` workflow leaves those dependencies on PyPI.

For checked native execution, keep both commands in the same MSVC-initialized session:

```powershell
.\scripts\build.ps1 -Preset asan
science/.venv/Scripts/python.exe science/physicsnemo_smoke.py --native out/asan/engine/breakwater_physicsnemo_reference.exe --output out/a4-smoke-asan
```

MSVC AddressSanitizer checks the owned native reference, not prebuilt Torch,
PhysicsNeMo, or Python. The CLI configures PhysicsNeMo's supported `LOCAL_CACHE`
setting under its output directory **before importing PhysicsNeMo**, unless the
process already specifies a cache. This avoids writes to a protected user cache.
The checkpoint tests similarly use a temporary cache. No installed vendor code is
patched. Use a fresh CLI process if PhysicsNeMo was already imported elsewhere.

## Fixed physical case and independent reference

The authoritative native function is described in
[engine/TRANSPORT.md](../engine/TRANSPORT.md). The new private scientific exporter
`breakwater_physicsnemo_reference` executes it at 32, 64 and 128 cells with CFL 0.9,
then repeats 128 cells at CFL 0.45. Every run has the same 41 output times through 1 h.
It emits actual fields, signed mass ledgers and diagnostics; no saved field is used
in place of execution. Its fixture JSON is not an application/worker contract.

All units are physical: metres, hours, m3/h, m2/h, 1/h and mg/L. The case has
`L=1 m`, `A=1 m2`, `D=.1 m2/h`, `k=.1/h`, `b=3*pi/(4*L)`, `a=b`,
`v=2*D*a`, and `q=A*v`. The PDE is

```text
c_t + v*c_x - D*c_xx + k*c = 0
c(0,t)=0; c_x(L,t)=0
phi(x) = exp(a*(x-L))*sin(b*x)/sin(b*L)
c_exact(x,t) = phi(x)*exp(-lambda*t)
lambda = k + v*v/(4*D) + D*b*b
```

This is a nonuniform positive initial profile with genuine advection, dispersion,
decay and time evolution. It has no source and no schedule jumps. Its exact boundary
conditions avoid an inlet interpolation approximation. Cell initialization uses the
analytic integral of `phi` over each cell, not its center value.

The native refinement check compares **cell averages** with the analytic integral.
Neural inference uses eight-point Gauss quadrature over each cell before comparison
with native cell averages. Unit tests independently compare the integrals/quadrature,
check the residual on a nonzero polynomial, and verify analytic PDE cancellation.

The left tracer flux is `v*c(0,t)-D*c_x(0,t)`, which is negative here despite positive
water flow. The signed inlet is retained. Native inventory/outlet/decay and cumulative
inlet are compared with analytic ledger terms; volume-to-litre conversion is 1000.
Mesh errors must decrease, temporal sensitivity is measured separately, and the
native discrete mass residual must remain within its declared tolerance.

## Genuine model, training and checkpoint

The model is `physicsnemo.models.mlp.FullyConnected`, with two inputs, one output,
three 32-unit hidden layers, `tanh`, 2,241 parameters and CPU float64 tensors.
Autograd computes first time/spatial and second spatial derivatives in physical units.
The residual is implemented explicitly with Torch; this increment does not use or
claim execution of the optional PhysicsNeMo Sym/PhysicsInformer package.

Let `h(x)=sin(pi*x/(2*L))`. The prediction is
`c_theta=h(x)*MLP(2*h(x)-1, 2*t/T-1)`. This enforces the zero inlet and zero-gradient
outlet without embedding `phi` or its exponential time decay. The network still has
to learn the initial profile and transient using the PDE. Smooth derivatives include
the full chain through `h`, and no predicted concentration is clipped.

The checked default in [a4_config.json](a4_config.json) uses seed 20261107, one CPU
thread, 256 fixed scrambled Sobol collocation points, 64 initial-condition samples,
and 1,800 Adam steps at learning rate .002. Loss is residual MSE plus ten times IC
MSE. Training has a 120-second wall-clock budget and explicit nonfinite/timeout failure.
Native fields and the analytic transient are **not training targets**. Only analytic
initial-condition samples provide concentration targets.

Held-out residual points use a different recorded seed and are checked to be disjoint
from training coordinates. The field gate uses times after zero and quadrature points
different from the training IC samples. It is held-out space/time evaluation of this
one physical case. Changed flow, boundary forcing, geometry or reaction needs model
conditioning or separately reported training; cross-setting generalization is absent.

The actual PhysicsNeMo `.save()` / `.load()` methods round-trip `transport.mdlus`.
Reload first constructs a CPU float64 model, then loads weights strictly; this avoids
rounding saved double weights through a default float32 constructor. Predictions
before and after reload must agree. Checkpoints and generated fields stay ignored.

## Gates and evidence

Thresholds are loaded from the versioned config, hashed, and written to `preflight.json`
before training. All gates must pass for CLI exit 0. None are relaxed after evaluation.

| Gate | Declared threshold |
|---|---:|
| Native 128-cell RMSE vs analytic cell averages | <= .01 mg/L |
| Native spatial error | strictly decreases at 32/64/128 cells |
| Native time refinement RMSE | <= .002 mg/L |
| Held-out neural/native cell-average RMSE / maximum | <= .03 / .10 mg/L |
| Held-out PDE residual RMS | <= .05 mg/L/h |
| Outlet maximum error vs analytic boundary trace | <= .10 mg/L |
| Maximum inventory relative error vs native | <= .05 |
| Minimum sampled prediction | >= -.005 mg/L; no clipping |
| Checkpoint maximum inference difference | <= 1e-10 mg/L |

The loss must decrease, parameters must actually change, and native mass/mesh/time
checks must pass. These are predefined toy-smoke tolerances, not chemical-quality
qualification thresholds. Low training loss alone cannot pass the smoke.

Each output directory contains preflight config/environment/native-executable hash,
the actual native reference, prediction field, `.mdlus` checkpoint and `report.json`
with individual gates, losses, seeds, runtime, errors, ledger differences and checkpoint
hash. Failure returns nonzero and records available evidence; a failed run is not
silently presented as a successful model. Actual measured results are in
[PROGRESS.md](PROGRESS.md).

Installation followed NVIDIA's [official workflow](https://docs.nvidia.com/physicsnemo/latest/getting-started/installation.html)
and [requirements](https://docs.nvidia.com/physicsnemo/latest/getting-started/system_requirements.html).
The real model/checkpoint API is documented in
[PhysicsNeMo modules](https://docs.nvidia.com/physicsnemo/latest/physicsnemo/api/models/modules.html).

The scientific worker remains health-only pending B's contract/adapter. No desktop,
public contract, root configuration or solution files are modified by A4.
