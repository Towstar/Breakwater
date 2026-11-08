# A-003: constant and linear PDE residuals

Person A follow-up to [AUDIT_PERSON_A.md](AUDIT_PERSON_A.md), implemented on
`codex/person-a-engine` after `b24004b`. A-003 is fixed and regression-verified.
A-001 and A-002 remain fixed; A-004 remains open.

## Implementation

The physical residual remains
`c_t + v*c_x - D*c_xx + k*c`, with derivatives in metres and hours.

The private coordinate-gradient helper now returns coordinate-shaped zeros when
an output has no differentiation graph, or when autograd reports no dependency
on the coordinate tensor. Both constant fields and constant first derivatives
therefore work. It uses `create_graph=True` and `allow_unused=True` for genuine
dependencies; it does not detach the field or its derivatives, compare derivative
values against zero, or catch arbitrary autograd exceptions.

This distinction preserves trainable coordinate-independent amplitudes and affine
coefficients. It also preserves a nonzero parameter sensitivity when the current
parameter value and residual are both zero. The reaction term retains the field's
parameter graph even when its coefficient is zero; a legitimate zero parameter
gradient remains available.

Residual evaluation temporarily enables coordinate autograd under an outer
`torch.no_grad()`. Otherwise, an ordinary varying field could silently appear
constant and lose its derivative terms. Active `torch.inference_mode()` is rejected
explicitly because it cannot supply the required coordinate graph. Field callables
must be pointwise and retain the autograd dependencies of their coordinate-varying
terms; this is not a finite-difference replacement for detached callables.
Shape errors and exceptions from fields/custom backward implementations remain
explicit failures.

The A-owned quality method version is now `physicsnemo-pipe-pinn-0.1.1`.
Model architecture, seed, training configuration, physical constants and smoke
thresholds are unchanged. The storage worker's method/protocol are unchanged.

## Regressions and neural parameter gradients

The initial four defect regression methods failed in 14 subcases against the
unmodified implementation before the fix. The existing quadratic residual and
analytic eigenmode checks remain in place.

Ten new methods in `science/tests/test_physicsnemo.py` cover:

- Constant zero, positive and negative fields with zero/positive decay.
- `x+t`, x-only, t-only and general affine fields, checked against independent
  analytic residuals with zero spatial curvature.
- Coordinate-independent trainable amplitudes and affine coefficients, with
  independent analytic parameter gradients for zero/positive decay.
- `theta*x²` at `theta=0`: the residual is zero but its mean has the expected
  nonzero derivative with respect to theta. Squaring the residual here would hide
  a discarded dependency, so this test differentiates its mean directly.
- Evaluation under outer `no_grad`, including later parameter differentiation;
  explicit rejection of active inference mode.
- Propagation of field-call and deliberate custom-autograd backward errors.
- A genuine PhysicsNeMo neural field with a **residual-only loss**, so an
  initial-condition loss cannot conceal a detached PDE residual.

The neural regression constructs the real CPU float64 `FullyConnected` model,
backpropagates to every parameter, checks finite gradients and a nonzero component,
then compares the largest-magnitude component against a central finite difference
of the full residual loss. Perturbation is `1e-6 * max(1,abs(parameter))`, with
absolute tolerance 2e-8 and relative tolerance 5e-5. A small SGD step must change
parameters and lower that residual-only loss.

## Fresh verification

The existing pinned Python 3.12.13 environment contains PyTorch 2.15.0+cpu,
PhysicsNeMo 2.2.2 and NumPy 2.5.4. No dependencies were upgraded.

| Check | Actual result |
|---|---|
| Full science unittest discovery | **94/94 passed**, 43.721 s; no skipped tests |
| Analytic residual/parameter checks | All 19 mathematics tests passed, including the new degenerate-field cases |
| Genuine neural-gradient test | Finite-difference agreement, finite parameter gradients and residual-only SGD improvement passed |
| Actual PhysicsNeMo/native FVM smoke | **12/12 gates passed**, 1,800 Adam steps, CPU float64 |
| Checkpoint round-trip | Maximum inference difference 0 mg/L |

The actual smoke retrained the 2,241-parameter model and executed the native FVM
reference, refinement checks, held-out residual/field comparisons and checkpoint
reload. Training took 9.3953 s in this run; this is runtime evidence, not a
performance benchmark. Parameter update L2 was 2.80302703103; loss decreased from
2.86528132106 to .002261996361.

| Smoke quantity | Measured | Unchanged gate |
|---|---:|---:|
| Held-out field RMSE | .00336886618 mg/L | <= .03 |
| Held-out maximum field error | .0135691065 mg/L | <= .10 |
| Held-out PDE residual RMS | .0438580470 mg/L/h | <= .05 |
| Outlet maximum error | .0180117144 mg/L | <= .10 |
| Maximum inventory relative error | .0100422003 | <= .05 |
| Minimum sampled concentration | .00017309089 mg/L | >= -.005 |

This remains a fixed-case experimental quality smoke. It does not qualify an
arbitrary parameter family or establish volume feasibility.

## Reproduction and provenance

From the repository root with the existing pinned science environment and native
reference build:

```powershell
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/physicsnemo_smoke.py --native out/audit-person-a/native/engine/breakwater_physicsnemo_reference.exe --output out/a003/physicsnemo
```

The native reference executable was built during the preceding A-002 MSVC checks
and executed again by this smoke; no native source changed for A-003. Follow the
audit's isolated build configuration if that local executable is absent.
Its SHA-256 is
`0f9f3f5b519ff99e752ccb11729b51d4b22b365c1f4084fde777c306cfd1f57d`.

Local outputs are ignored: `out/a003/python-tests.log`,
`physicsnemo-smoke.log`, and `physicsnemo/` containing the report, preflight,
actual native reference, predictions and checkpoint.

| Artifact/source | SHA-256 |
|---|---|
| Python test log | `d7da94ad20343854518a7c018e3098366b76b58f4d250ef62b2157c2778db807` |
| PhysicsNeMo report.json | `6d9623a071782542fcc7a3845c1172dcbd3842a0c77f28879f78bb1fb166565a` |
| transport_pinn.py | `76e3b4c5c9b44d72d423474ff0c6641795d71ca6c1e47c176aa83a029e05daeb` |
| test_physicsnemo.py | `69ae783b97c78f573f36c69845ff9445d78a22ac2bd3a394ac5f7205eb669ad2` |

These identify this run's local evidence and tested source; generated log/checkpoint
bytes need not match across machines. The fresh full Python suite also includes
the existing storage/worker regressions. The dedicated native/PyTorch/worker
18-case parity drivers, full native/ASan suites, benchmarks and Lean were not
rerun for this Python residual-only change. Their preceding A-002 results remain
historical evidence, not new A-003 executions.

Only science-owned source, tests and documentation changed. Person B's application,
contracts, root build/solution and shared documentation remain untouched.
