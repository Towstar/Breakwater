# Sparse tracer-decay calibration

The desktop workbench's **Pipe transport → Learn decay · PhysicsNeMo** tab estimates
the unknown first-order coefficient in `dC/dt = -k C` from a closed, well-mixed
tracer experiment. Initial concentration is known; sample times are known elapsed
hours. The test must have no inflow, dilution or additional source. The learned
coefficient is a physical input to the existing pipe transport model. Neither
editor layout nor EOG production-water observations provide these tracer data.

PhysicsNeMo's actual `FullyConnected` module parameterizes `k = 10 sigmoid(f(0))`
and PyTorch float64 LBFGS minimizes log-concentration residuals against
`C(t) = C0 exp(-k t)`. This is a small bounded inverse problem with an analytical
forward characteristic, not a learned general-purpose transport surrogate.
The implementation imports and executes PhysicsNeMo; it does not relabel the
native solver or substitute an analytical answer for model optimization.

A separate stdlib calculation finds
`k_reference = -sum(t log(C/C0))/sum(t²)`. The offered estimate must agree within
`1e-6 + 1e-5 abs(k_reference)` per hour and have concentration RMSE at most 5% of
the supplied initial concentration. Unsupported rate ranges or inadequate time
spread fail explicitly. These are deterministic acceptance thresholds, not
statistical confidence intervals or evidence of physical model adequacy.

Input bounds: positive initial concentration 1e-9–1e9 mg/L; 3–64 strictly
increasing positive times up to 1000 h spanning at least 0.001 h; positive
concentrations between 1e-12 times initial and initial. Supported reference decay
is `[0,10)` per hour. No malformed data or inferred parameter is silently clipped.
The private worker has a 64 KiB one-line input/output budget; desktop execution
has a 45-second runtime budget, supports cancellation and uses a fresh interpreter
without passing hosted credentials. CPU float64 execution is explicit.

The estimate binds to the selected route, complete pipe parameters, network hash
and numerical revision. Editing observation text discards it. Changed project
inputs prevent application. **Apply estimate** uses the existing parameter-edit
path, which invalidates prior numerical results and approval. A new native
forecast and transport calculation are required. Estimation alone changes no
project input and cannot approve or apply an operating plan.

Example synthetic measurements for `C0 = 1 mg/L`, `k = 0.2 /h`:

```text
0.25, 0.951229424500714
0.50, 0.9048374180359595
0.75, 0.8607079764250578
1.00, 0.8187307530779818
```

Focused science verification:

```powershell
& $env:BREAKWATER_PYTHON -B -m unittest discover -s science/tests -p test_transport_calibration.py -v
& $env:BREAKWATER_PYTHON -B science/qualify_transport_calibration.py -v
```

The first command runs three stdlib input/reference tests. The explicit second
command runs three real PhysicsNeMo runtime qualification tests; a missing
runtime fails this command rather than adding an unexpected skip to CPU CI.

Actual application-boundary test, once the combined desktop test target is built:

```powershell
$env:BREAKWATER_CALIBRATION_TEST = '1'
& .\out\desktop\desktop\breakwater_pipe_transport_tests.exe actualPhysicsNeMoEstimateRequiresExplicitApply
```

The optional test requires a real `BREAKWATER_PYTHON` containing PhysicsNeMo and
PyTorch; it checks that fitting does not modify the project, the explicit apply
updates the decay coefficient, the numerical revision advances and prior
approval eligibility is invalidated. No such desktop result is claimed until
that executable actually passes on the combined build.
