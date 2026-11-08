# F1b pre-execution scientific declaration

Baseline: merged PR #28, `487d80dd54df7d7b6567dfed1170dea9a5e59cd7`.
This declaration and its pinned JSON configuration precede candidate execution.
They are scientific study inputs, separate from any local continuation plan.

## Method and supported domain

The private comparator uses MC-limited piecewise-linear finite-volume advection
and SSPRK(2,2). Interior slopes are
`minmod(2*(Cj-Cprev), (Cnext-Cprev)/2, 2*(Cnext-Cj))`; endpoint slopes are zero.
At positive velocity the outgoing face value is `Cj+slopej/2`. The inlet face is
prescribed and the outlet face uses the last cell average. End-cell closure is
first order. No global second-order accuracy claim follows from the method name.

For a spatial operator L, the stages are `Y=U+dt*L(t,U)` and
`Unew=U/2+(Y+dt*L(t+dt,Y))/2`. Shared face fluxes supply the ledger with the same
half-weighted stage quadrature. Both stages use the velocity of the current event
interval, including an endpoint at a flow knot; inlet concentrations use stage times.
Output, inlet and flow events bound every step. The candidate supports only finite
nonnegative initial/inlet data and nonnegative flow, with zero dispersion, decay and
source and an open prescribed inlet. Requested CFL must be in `(0,0.5]`.

MC outgoing reconstructions are at most twice the cell value and incoming
reconstructions are nonnegative for nonnegative data. Hence forward Euler is
nonnegative at CFL <= 1/2 in exact arithmetic; the SSP convex combination preserves
that property. Native stage/domain tests independently check the implementation.
This is mathematical reasoning, not a new Lean theorem or an IEEE guarantee.
Values are never clipped. All existing native resource ceilings remain; each
accepted step consumes `2*cells` cell-stage updates before publication.

Method references: [Clawpack MC limiter](https://www.clawpack.org/pyclaw/evolve/limiters.html)
and [Gottlieb, Shu and Tadmor, SSP time discretizations, equation 4.1](https://www.math.umd.edu/~tadmor/pub/linear-stability/Gottlieb-Shu-Tadmor.SIREV-01.pdf).

## Matrix and measurements

Run the 22 unchanged F1 cases on each method. Add six candidate-only N512 time
probes: smooth/box CFL .08/.04 alongside .02, and extended inlet .4/.2 alongside .1.
Keep physical inputs, output clocks and every other setting fixed. Record all
field norms at every output, terminal/worst errors, physical-point versus
last-cell-average outlet errors, sampled/continuous reference peaks, signed mass
residuals, reference uncertainty and threshold brackets/counts. Strides 1/2/4
decimate one master trace. Never interpolate a claimed exact crossing.

Continuum references remain independent of native initial/result fields. Preserve
F1 cross-check and integrity tolerances. The upwind Poisson reference is valid only
for the baseline. Candidate refinement is coupled space/time; three-level native
time-step differences give empirical sensitivity, with unresolved orders near the
reference floor. No extrapolated value is substituted for the continuum oracle.

## Fixed synthetic decision targets

- N512 smooth, box and extended-inlet worst-over-time mean-L1 and RMS errors each
  must be <= .5 times their paired baseline errors.
- All 22 paired cases must have no increase in either worst mean-L1 or RMS beyond
  ten times the larger paired reference-uncertainty estimate. Ratios at/below that
  floor are unresolved and cannot establish a resolved improvement.
- N512 extended-inlet sampled native outlet peak must be within .10 mg/L of the
  physical point reference peak sampled at the same output times.
- N512 extended inlet must recover 1/2/1 upward crossings at .1/.5/.9 mg/L.
- For each threshold and each N512 smooth/box/extended case, both endpoints of the
  first observed bracket must be within .025 h of the continuous first crossing.

These targets use the master cadence (stride 1). Coarser observation views and
maximum errors at jumps are reported separately. No uniform maximum-norm
convergence gate is asserted for discontinuities. Integrity failures invalidate
the attempt; unmet accuracy targets remain completed negative experiments.

## Preservation and delivery boundaries

Preserve production transport source/public headers, original A9/F1 exporter,
original configurations/references/evidence, dependency pins and all thresholds.
Retain refined A9's 45-case/580-gate result, its original coarse scheduled-inlet
failure and F1's six time-contamination failures. Existing 54-proposition proof
evidence concerns the original stencil and does not cover the experimental one.

Use separate normal/MSVC ASan builds, full pinned Python discovery without skips,
actual baseline/candidate executions, independent numerical tests and corruption
tests. Retain exact commands, flags, versions, hashes, raw data and failed attempts.
Delivery separates declaration, implementation/tests and measured evidence commits.
Production adoption, B-owned integration/contracts/shared documentation, G2/G3,
RBMs, new proofs and further numerical tuning remain subsequent increments.
