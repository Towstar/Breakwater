# A9: bounded transport qualification

Implemented on `codex/person-a-engine` after `367dcf1`, November 7, 2026
(America/Chicago). The native algorithm and public contracts are unchanged.

**The refined 45-case matrix passes all 580 gates in normal MSVC and MSVC ASan
builds.** The original coarse scheduled-inlet study fails its declared convergence
gate and remains unqualified. No threshold, forcing, CFL or scheme was relaxed.

**The finest inlet-front maximum cell-average error remains 0.40191 mg/L. Its
mean-L1 pass does not establish accurate local peaks or peak-based quality decisions.**

## Existing coverage and new evidence

The previous 18 transport cases already covered conservation, positivity, half-cell
inlet CFL, exact CFL-one shifts, diffusion refinement, source equilibrium, schedules,
limits and invalid/extreme inputs. A4 supplied one mixed ADR eigenmode comparison.
A9 adds a systematic matrix, independent references, separated smooth-family
space/time errors, transient source/reaction, three Péclet settings and an explicitly
measured nonsmooth-front limitation.

| Regime | Physical case and independent reference | Refinement |
|---|---|---|
| Smooth advection | v=.25 m/h, D=k=0, zero inlet; raised-cosine pulse on [.125,.625] m | N32/64/128 at CFL .01; fixed N64 at CFL .8/.4/.2/.1; T=.5 h |
| Closed diffusion | D=.05, no flow/decay/boundary flux; .6+.3 cos(pi*x) initial profile | Same meshes/CFL levels; T=.2 h |
| Open zero-flow diffusion | D=.05, k=0; .7 sin(pi*x/2), zero Dirichlet inlet/Neumann outlet | Same meshes/CFL levels; T=.2 h; negative inlet exchange |
| Mixed ADR | D=.1, k=.1; eigenmode fractions .6/.75/.85, Pe=1.22492/4.71239/10.48173 | N32/64/128 at CFL .01 for each; N64 temporal study for .75; T=.5 h |
| Reaction/source | Closed uniform c0=1, k=1, s=.25 | N8, CFL .5/.25/.125/.0625; T=1 h |
| Source only | Closed uniform c0=.1, k=0, s=.25 | N8, exact linear-in-time check; T=1 h |
| Scheduled pulse | Smooth pulse, zero D/k/inlet; v=.4 until .137 h, paused until .293 h, then .7 | N32/64/128 at CFL .01, outputs every .05 h through .6 h |
| Scheduled inlet | Initially empty; same flow; inlet knots (0,0),(.11,1),(.29,.2),(.47,.8),(.63,0) | Refined N128/256/512 at CFL .1, outputs every .05 h through .6 h |

All pipes have length 1 m and area .2 m2. Concentrations are **cell averages** in
mg/L; flow is m3/h, dispersion m2/h, decay 1/h, source mg/L/h, and mass/ledgers mg.
These are tested points, not a guaranteed-accuracy parameter box. Arbitrary
time-dependent/negative manufactured sources remain outside the current API.

## Methods, separation and reference uncertainty

`breakwater_science/transport_reference.py` uses physical inputs and forcing,
never native initial/result buffers. Sixteen reference tests check independent
quadrature, physical ledgers, characteristic reconstruction and face-flux generators.

- Pulse advection uses exact translated cell integrals. Its semidiscrete upwind
  solution is a finite Poisson convolution with integrated displacement/dx.
- Diffusion uses exact cosine/sine cell averages and continuum rates, with
  semidiscrete rate `4D sin²(beta*dx/2)/dx²`. The open mode tests the half-cell inlet.
- Mixed ADR uses `bL=f*pi`, `a=-b*cot(bL)`, `v=2Da`,
  `lambda=k+D(a²+b²)` and `phi=exp(a(x-L))*sin(bx)/sin(bL)`.
  An independently assembled tridiagonal generator is transformed to a symmetric
  matrix and evaluated with NumPy eigendecomposition. Residual, orthogonality,
  matrix/initial reconstruction and conditioning are checked. Its roundoff estimate
  is numerical conditioning evidence, not a formal interval enclosure.
- Uniform source/reaction uses its exact ODE solution and source/decay ledger.
- Scheduled inlet uses independent characteristic reconstruction: split at the
  union of flow/inlet knots and integrate affine entering concentrations over each
  cell overlap. Exact injected mass is checked separately.

At the endpoint of each smooth spatial study, native-minus-semidiscrete measures time error and
semidiscrete-minus-continuum measures spatial error. Time error, including the
mixed-reference roundoff estimate, must be <=5% of spatial error; the largest
measured ratio was **0.029998**. Temporal studies hold the mesh fixed.

**Exception:** scheduled inlet has no semidiscrete reference. It measures coupled
mesh/time refinement at fixed CFL .1, without isolated temporal error.

Mean-L1 is `(dx/L)*sum(abs(error))`; RMS and maximum errors are also recorded in
mg/L. Independently reconstructed mass is `1000*A*dx*sum(cell_average)` mg.

## Measured results

Spatial rows report terminal native-versus-continuum mean-L1. All smooth rows
also satisfy the time-contamination gate.

| Regime / mode | Meshes | Observed orders | Finest mean-L1 (mg/L) |
|---|---|---|---:|
| Smooth advection | 32/64/128 | .8901, .9436 | .01168897 |
| Closed diffusion | 32/64/128 | 1.9997, 1.9999 | 8.31575e-7 |
| Open diffusion | 32/64/128 | 1.9999, 2.0000 | 1.31936e-7 |
| Mixed ADR .6 | 32/64/128 | .9725, .9859 | .000236979 |
| Mixed ADR .75 | 32/64/128 | .9528, .9761 | .000793223 |
| Mixed ADR .85 | 32/64/128 | .9813, .9899 | .001030577 |
| Scheduled pulse | 32/64/128 | .7917, .8803 | .02415743 |
| Scheduled inlet, coupled | 128/256/512 | .3075, .4797 | .01930305 |

Temporal errors below are native-versus-semidiscrete on fixed meshes:

| Regime | Observed temporal orders | Finest mean-L1 (mg/L) |
|---|---|---:|
| Smooth advection | 1.0423, 1.0223, 1.0117 | .00211701 |
| Closed diffusion | .9982, .9999, .9995 | 1.02825e-6 |
| Open diffusion | .9986, .9999, .9996 | 1.07680e-7 |
| Mixed ADR .75 | 1.0003, 1.0001, 1.0000 | 7.18951e-6 |
| Reaction/source | 1.1954, 1.0846, 1.0398 | .00885398 |

The source-only branch meets its exactness tolerance. Maximum independently
reconstructed signed mass residual was **5.116e-13 mg**. The largest work counts
were 252,474 substeps and 32,316,672 cell updates, within existing limits. Three
requests explicitly reject substep, output-frame and output-value budget violations.

## Retained coarse failure

The original `a9_transport_config.json` was declared before native measurement.
It passed 579/580 gates. Scheduled-inlet orders on N32/64/128 were **.27651 and
.20370**, failing the unchanged minimum .25 on the second pair. Raw fields and
the failed report remain in `out/a9/qualification/`.

At .6 h, the resume interface is at .2149 m and the pre-pause peak at .2257 m.
Their .0108 m separation spans only 1.38 cells at N128, alongside a genuine front
discontinuity. Independent characteristic/quadrature and injected-mass checks
support unresolved/smeared structure rather than mass loss.

The follow-up `a9_transport_refined_config.json` was written before its own run.
It changes only the scheduled-inlet mesh tuple to 128/256/512 and selects the
exporter's `--refined-inlet` flag. Every threshold, physical input, forcing and CFL
is unchanged; regression tests enforce this. The original config stays intact.

The finest front still has **maximum cell-average error .40191 mg/L**. Passing
mean-L1 gates does not qualify local peaks, threshold crossings or second-order
accuracy at this discontinuity. Coarse N32/64 inlet-front accuracy remains unqualified.

## Reproduce and inspect

Use the existing pinned Windows science environment: Python 3.12.13 and NumPy
2.5.4, with the existing Torch-importing package initializer. No dependency upgrade,
SciPy or neural training is required.

From an MSVC x64 Developer PowerShell, with the isolated build configurations
documented in [AUDIT_PERSON_A.md](AUDIT_PERSON_A.md):

```powershell
cmake --build out/audit-person-a/native --parallel 2
ctest --test-dir out/audit-person-a/native --output-on-failure
science/.venv/Scripts/python.exe -m unittest discover -s science/tests -v
science/.venv/Scripts/python.exe science/qualify_transport.py --native out/audit-person-a/native/engine/breakwater_transport_qualification_reference.exe --config science/a9_transport_refined_config.json --output out/a9/reproduced-refined
```

Reproduce the retained coarse study separately; **exit 1 is expected**:

```powershell
science/.venv/Scripts/python.exe science/qualify_transport.py --native out/audit-person-a/native/engine/breakwater_transport_qualification_reference.exe --config science/a9_transport_config.json --output out/a9/reproduced-baseline
```

For checked execution, initialize the MSVC environment in the same session:

```powershell
cmake --build out/audit-person-a/asan --parallel 2
ctest --test-dir out/audit-person-a/asan --output-on-failure
science/.venv/Scripts/python.exe science/qualify_transport.py --native out/audit-person-a/asan/engine/breakwater_transport_qualification_reference.exe --config science/a9_transport_refined_config.json --output out/a9/reproduced-asan
```

Use distinct output directories to retain attempts. Each run records preflight
configuration/source/executable hashes before native execution, full native inputs,
fields and ledgers, stderr, individual gates, JSON and compact Markdown reports.
Failure returns nonzero and retains available evidence. No simulated field is
clipped or replaced by an oracle.

The exporter is a private fixture format, not application/worker JSON. B can
regenerate the root Visual Studio solution to include this additive CMake target;
A did not modify the B-owned solution.

## Verification and provenance

| Fresh check | Result |
|---|---|
| Native build and existing CTest suites | 8/8 passed, 4.07 s |
| MSVC ASan build and existing CTest suites | 8/8 passed, 53.59 s |
| Refined normal qualification | 45 cases / 580 gates passed |
| Refined MSVC ASan qualification | 45 cases / 580 gates passed; no sanitizer report |
| Full Python suite | 127/127 passed, 79.461 s; no skips |

The 33 new tests check reference mathematics and deliberately corrupted evidence:
wrong litre conversion/signs, conserving-but-wrong fields, diagnostics/work limits,
missing outputs, initial averages, matrix/variant identity and unchanged gates.
Runtime observations are not performance benchmarks.

Local evidence is in `out/a9/qualified/`, `qualified-asan/` and the separate
failed `qualification/`. Generated outputs, environments and binaries are ignored.

| Evidence/configuration | SHA-256 |
|---|---|
| Original config | `180b003f6534898bf244d4c5a09f44eba05dc4c4d6ab902ed84ce41edd9e7beb` |
| Refined config | `9f590848092c2681b73ed1f9f6713b1c2f7ec5889626503c05900607829b1a13` |
| Initial failed report | `aa711dcc18b63fdf30cc81903f90f01caf634bfab7cd0bef7b3d8a3666ab1d4c` |
| Refined normal report | `25db4330bd7123fc30c78c492d3235a6cde393bad95a3172739a0837d5eae059` |
| Refined ASan report | `36dedf6fbcdf58dbe0b224f18a75435fca207ae493e5c69068704227d42bdbfb` |

Preflight source/executable hashes identify the actual A9 working tree tested before
its commit. The algorithm remains `fvm-upwind-euler-0.1.0`; its implementation did
not change. Evidence qualifies only the declared synthetic cases, not hydraulic
fidelity, industrial chemistry, general network transport, learned inference or
native volume approval.

Only A-owned engine/science files changed. B's application, public contracts,
root build/solution and shared documentation are untouched. Lean and the separate
PhysicsNeMo training smoke were unchanged and not rerun. Optional A10 discrete
FVM proofs and broader parameter-family qualification remain subsequent work.
