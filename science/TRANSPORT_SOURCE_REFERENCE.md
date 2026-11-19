# Closed dispersion, reaction and spatial-source reference

This follow-up adds eight independent modal reference runs to the native
transport suite, which now passes 26/26 cases in normal MSVC and MSVC ASan.
The production `fvm-upwind-euler-0.1.0` implementation and public contracts are
unchanged. It complements the dated A9/F1 findings; it does not replace their
retained failures or promote the separate research solver.

## Physical problem and independent solutions

On a one-metre closed pipe with area 0.2 m² and zero flow:

```text
c_t = 0.03*c_xx - 0.7*c + 0.25 + 0.1*cos(pi*x)
c_x(0,t) = c_x(1,t) = 0
c(x,0) = 0.6 + 0.2*cos(pi*x)
```

Concentration is mg/L, time is hours, dispersion is m²/h, decay is 1/h and source
is mg/L/h. Native initial/source profiles use exact integrals over each cell.
No future reference is derived from native result buffers.

The continuum solution is `b(t) + a(t)*cos(pi*x)`, where
`b(t)=0.6*exp(-k*t)+(0.25/k)*(1-exp(-k*t))` and
`a(t)=0.2*exp(-lambda*t)+(0.1/lambda)*(1-exp(-lambda*t))`, with
`k=0.7` and `lambda=k+0.03*pi²`. Cell comparisons use the exact integral of that
solution. The independent semidiscrete solution uses the same two modal ODEs
with `lambda_N=k+4*D*sin²(pi/(2N))/dx²`. Native-minus-semidiscrete measures time
error at a fixed mesh. `expm1` avoids subtractive cancellation in the reference.

All runs end at 0.4 hours with only initial/final output frames. Before execution,
the declared checks required:

- Coupled mesh/time refinement at N16/32/64/128, safety 0.01: observed mean-L1
  orders above 1.8, finest mean-L1 below 2e-5 mg/L and maximum below 3e-5 mg/L.
- Fixed N32 with safety 0.8/0.4/0.2/0.1: native-versus-semidiscrete time-error
  orders above 0.85.
- Existing finite/positivity, exact horizon, source/decay mass-ledger and work
  bounds, plus unchanged initial/source input arrays.

No acceptance threshold, forcing, scheme or CFL policy was changed after seeing
the results. A later output-only addition records actual substeps/cell updates;
the complete suite was rerun in both modes after that addition.

## Results

Normal and ASan produce identical eight-row numerical CSV data. The retained
[source-bound report](evidence/v03/transport-source-reference.json) contains exact
values, observed orders, source hashes, logs and limits.

| Coupled mesh/time | Continuum mean-L1, mg/L | Continuum maximum, mg/L |
| --- | ---: | ---: |
| N16 | 3.17751e-5 | 5.95350e-5 |
| N32 | 7.91698e-6 | 1.49753e-5 |
| N64 | 1.97961e-6 | 3.74959e-6 |
| N128 | 4.94804e-7 | 9.37757e-7 |

Coupled orders are approximately 2.00. Fixed-mesh time errors decrease from
2.32341e-4 to 2.89377e-5 mg/L, with orders approximately 1.00. Maximum observed
mass-balance residual is 1.93445e-11 mg. The largest run uses 39,350 substeps and
5,036,800 cell updates, within the unchanged one-million/50-million work limits.

The coupled study refines the explicit time step along with the mesh. Its
near-second-order trend is **not isolated spatial order**, and does not make the
upwind advection scheme second order. The separately reported semidiscrete error
is material in these coupled rows; no five-percent time-contamination gate or
formal interval enclosure is claimed here.

## Reproduce and interpret

From an initialized MSVC developer PowerShell:

```powershell
cmake --build out/portfolio-desktop --target breakwater_transport_tests
ctest --test-dir out/portfolio-desktop --output-on-failure --no-tests=error -R '^engine.transport$'
```

Repeat with `out/portfolio-desktop-asan`. `SOURCE_REFERENCE` lines in the retained
CTest details give all eight rows. The new group is one native test case with
eight numerical runs, not eight additional CTest suites.

This qualifies only these smooth closed scalar-reference points. It does not
qualify a moving inlet front, outlet peak, threshold-crossing time, industrial
chemistry, pressure hydraulics or concentration-to-forecast observation mapping.
The earlier [front-accuracy report](evidence/f1/normal/f1-summary.md), including
the documented nonsmooth-front limitations and unqualified peak/threshold scope,
remains applicable. No research method is promoted and no approval rule changes.
