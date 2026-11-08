# Single-pipe finite-volume reference

The bounded A9 [qualification report](../science/TRANSPORT_QUALIFICATION.md) adds
independent analytic/semidiscrete comparisons, separated error studies and explicit
limits for tested regimes. It preserves the failed coarse inlet-front study; its
refined mean-L1 pass does not qualify local peak accuracy or every accepted input.

The A10a [checked mass-balance proofs](../proofs/FVM_MASS_BALANCE.md) formalize
interior-face cancellation, one-step inventory balance and the variable-step signed
ledger identity over exact reals. They do not certify PDE accuracy, positivity,
floating-point implementation behavior or the scheduler.

Include `breakwater/transport.hpp` and link `breakwater_engine`. This is a synthetic,
cross-section-averaged scalar concentration model, separate from the tank-volume model:

`dc/dt + v(t) dc/dx = D d²c/dx² - k c + s`, with `v = q/A >= 0`.

Units are m, h, m3/h, m2, m2/h, 1/h, mg/L, and mg/L/h. Concentration values represent
cell averages on a uniform mesh. Geometry is physical pipe input, not editor layout.

## Discretization and boundaries

For cell width `dx`, each internal face uses upwind advection and centered diffusion:

`F[j+1/2] = v*c[j] - D*(c[j+1]-c[j])/dx`.

The fixed-concentration inlet is a half-cell from the first center:

`F[left] = v*c_in - 2*D*(c[0]-c_in)/dx`.

The outlet has zero diffusive gradient, so `F[right] = v*c[last]`.
Every update uses the same stored face flux on both neighboring cells:

`c_new[j] = c[j] + dt/dx*(F[left_face]-F[right_face]) - dt*k*c[j] + dt*s[j]`.

Flow knots are right-continuous, piecewise constant; inlet knots are piecewise linear.
Both start at zero, are strictly increasing, and hold their last value thereafter.
Substeps stop at flow/inlet knots and output times. The affine inlet forcing uses its
interval average; state-dependent flux and reaction use the old field. This preserves
first-order explicit time integration while integrating linear boundary forcing exactly.

The inlet makes the sufficient positivity bound stricter than the interior-cell bound:

`dt * (v/dx + 3D/dx² + k) <= cfl_safety`, with `0 < cfl_safety <= 1`.

The optional `closed_no_flux` mode requires zero flow and sets both boundary fluxes to
zero; it uses the sufficient `2D/dx² + k` bound and is useful for closed diffusion tests.
The method enforces stability through substeps and never increases the declared CFL
to satisfy a resource budget. It does not clip concentration. Negative values beyond
roundoff tolerance fail; tiny negative values remain visible in the field/diagnostics.

## Tracer-mass ledger

Tracer inventory is `M = 1000*A*dx*sum(c)` mg. The factor 1000 converts m3 to L.
Inventory/source terms are scaled per cell before compensated summation to avoid
overflow of an otherwise unnecessary concentration sum. The solver records:

- Signed cumulative inlet exchange: `1000*A*integral(F[left])`.
- Cumulative outlet exchange: `1000*A*integral(F[right])`.
- Cumulative source mass and explicit reaction/decay mass.
- Residual: `M - M_initial - inlet + outlet - source + decay`.

The ledger uses the exact step flux/source/reaction terms used by the update. Inlet
exchange may be negative: a zero-flow pipe with dispersion can lose tracer to a lower
Dirichlet inlet. Zero flow therefore does not imply zero boundary exchange.
Residual evaluation and tolerance scales avoid overflow of auxiliary sums. Nonfinite
physical results and mass residual above `1e-9 * max(1, ledger magnitudes)` fail explicitly.

## API and limits

`transport(pipe, initial, flow, inlet, config, optional_source)` returns time-major
concentration fields, cell-center coordinates, mass time series, solver version and
diagnostics. Initial/source profiles contain one value per cell; empty source means zero.
Closed mode permits an empty inlet. Inputs are finite/nonnegative and are not mutated.

Outputs include time zero and the exact forecast horizon. Invalid parameters, shapes,
schedules and output budgets throw `invalid_argument`. Numerical failure, non-advancing
time or exhausted integration budget throws `runtime_error`; no partial run is returned.
Limits: 2..4096 cells, at most 10001 frames/knots, four million saved concentrations,
one million substeps and fifty million cell updates. Horizon is at most 24 hours.
These bounds do not assert accuracy for every accepted mesh/parameter combination.

## Executable example and evidence

```powershell
.\scripts\build.ps1 -Preset headless
.\out\headless\engine\breakwater_transport_example.exe --csv out/transport_reference.csv
.\scripts\build.ps1 -Preset asan
```

The example uses a 100 m pipe, area 0.1 m2, dispersion 1 m2/h, decay 0.1/h, 64 cells,
a Gaussian-shaped piecewise-linear inlet, and a flow change from 20 to 15 m3/h at 0.4 h.
An executed one-hour run produced 41 frames and 136 substeps, maximum CFL 0.9,
final inventory approximately 1994.108 mg, and maximum mass residual about 1.37e-12 mg.
CSV columns are `time_h,x_m,concentration_mg_l`; they contain computed reference data.

Tests cover analytic decay/time convergence, cosine-mode cell-average diffusion at
32/64/128 cells, a discrete CFL-one advection shift, constant fields, signed open and
closed mass balances, source units, inlet positivity, event timing, concurrent inputs,
invalid requests, resource exhaustion, numerical overflow and extreme finite norms.

This function does not add tank chemistry, account for network-wide pipe inventory,
enforce plan quality thresholds, or certify industrial chemistry. A learned model must
be trained/evaluated separately against these reference fields before claiming ML readiness.
