# A10a: discrete finite-volume tracer mass balance

This increment proves an algebraic correctness property of the implemented
single-pipe finite-volume update. It is separate from storage-volume feasibility,
candidate optimization and the PDE accuracy studies in
[A9](../science/TRANSPORT_QUALIFICATION.md).

## Mathematical statement and units

For `n` cells and `n+1` faces, let `C[j]` be the old cell-average concentration in
mg/L, `S[j]` the source in mg/L/h, `dx` the cell width in m, `A` the area in m2,
`dt` the step in h, and `k` the decay rate in 1/h. Define

```text
cell_liters = 1000 * A * dx
boundary_mass_scale = 1000 * A
M(C) = sum_j cell_liters * C[j]                              [mg]
C'[j] = C[j] + (dt/dx)*(F[j]-F[j+1]) - dt*k*C[j] + dt*S[j] [mg/L]
```

Flux `F` has units mg/L * m/h. Each interior face is a single stored value,
subtracted from its left cell and added to its right cell. Finite-sum telescoping
gives `sum_j (F[j]-F[j+1]) = F[0]-F[n]`. It follows that

```text
delta_inlet  = dt * boundary_mass_scale * F[0]
delta_outlet = dt * boundary_mass_scale * F[n]
delta_source = dt * M(S)
delta_decay  = dt * k * M(C)     # OLD inventory, before the update
M(C') = M(C) + delta_inlet - delta_outlet + delta_source - delta_decay
```

Applying that result to the recursively computed fields and cumulative ledger
gives, after any finite sequence of steps,

```text
M_final - M_initial - inlet + outlet - source + decay = 0.
```

The conclusion is derived from the actual discrete recurrence; a mass-balance
conclusion is not supplied as an assumption. The list may contain unequal steps,
changing fluxes, sources and decay rates. Native fixed source/decay parameters and
event-shortened substeps form a supported subset.

## Native boundary formulas

For an open pipe, the formal model includes the native half-cell inlet, upwind
advection/centered interior diffusion and zero-diffusive-gradient outlet:

```text
F[0] = v * C_in_average - 2 * (D/dx) * (C[0] - C_in_average)
F[i] = v * C[i-1] - (D/dx) * (C[i] - C[i-1])       # 1 <= i < n
F[n] = v * C[n-1]
```

For closed mode both boundary fluxes are zero; interior diffusion is retained.
The native input validator also requires zero flow in that mode. Zero flow alone
does not close a Dirichlet boundary: when the first cell is more concentrated
than the inlet, diffusion can make `F[0]` negative. Signed inlet exchange must
remain in the identity and cumulative ledger.

The native scheduler stops at flow/inlet/output events and uses the mean of the
two inlet values on a substep. The proofs accept the resulting step sizes and
flux values. They do not prove scheduler event selection, interpolation accuracy,
or exact execution of floating-point time alignment.

## Assumptions and scope

- Cell/face indices are bounded finite types, so neighboring face indices exist.
  Generic statements use `Fin n` cells and `Fin (n+1)` faces, including `n=0`:
  the sole face then serves as both endpoints and cancels. Open/closed boundary
  constructors use `n+1` cells, so are nonempty. Native requests with at least two
  cells form a stricter supported subset.
- Face cancellation needs no geometry hypothesis. Converting the cell update to
  boundary mass balance requires nonzero `dx` to cancel its division by `dx`.
  Native accepted geometry is stricter: positive area/width and finite derived
  quantities. The factor 1000 is the native m3-to-litre conversion.
- There is no nonnegative-flux assumption. Algebraic balance does not require CFL,
  positivity of concentrations, a fixed timestep or fixed fluxes over a rollout.
  Those are separate physical/numerical validity questions.
- Arithmetic is over exact real numbers. The proof does not certify compensated
  summation, IEEE roundoff/overflow, residual tolerances, C++ memory safety or the
  construction of validated runtime indices.
- This is not a PDE convergence/accuracy theorem, positivity theorem, hydraulic
  validation, quality-threshold guarantee, optimization-feasibility certificate
  or global-optimality proof. A10b positivity and the front-accuracy study remain
  separate work. No solver algorithm or public contract is changed here.

## Checked statements

All names below are in `Breakwater.Transport`. The audit includes all 16 transport
statements and preserves the existing 21 storage/control-influence statements.

| Statements | Meaning |
|---|---|
| `inventory_eq_scale_sum` | Per-cell inventory sum equals its exact-real factored form; no floating-point reassociation claim. |
| `face_differences_telescope` | Interior shared faces cancel over any finite number of cells. |
| `euler_sum_balance`, `euler_inventory_balance` | Explicit concentration update yields the boundary/source/old-state decay inventory identity. |
| `openFlux_inlet`, `openFlux_outlet`, `openFlux_interior` | Boundary/interior values of the native open flux constructor. |
| `closedFlux_inlet`, `closedFlux_outlet`, `closedFlux_interior` | Closed endpoints are zero, while zero-velocity interior diffusion is retained. |
| `open_euler_inventory_balance`, `closed_euler_inventory_balance` | Specialize inventory balance to the actual two boundary laws. |
| `zero_flow_inlet_loss` | Positive dispersion/width and a higher first-cell concentration imply negative inlet flux at zero velocity. |
| `rollout_residual_preserved` | Every recursively evaluated, state-dependent variable step preserves the signed residual, even from a nonzero starting ledger. |
| `rollout_mass_residual_zero`, `rollout_inventory_balance` | Starting with zero ledger yields zero residual and the telescoped final mass identity. |

## Implementation mapping

| Formal term or step | Native source |
|---|---|
| Finite cell/face arrays | `field`, `next`, `flux` in `engine/src/transport.cpp` |
| `M(C)` and `M(S)` | `mass_sum`: scales each cell before compensated summation |
| `dx`, cell litres, boundary scale | Geometry setup in `transport()` |
| Open/closed face laws | `flux.front()`, the interior face loop and `flux.back()` |
| One explicit cell step | `transport_change`, `source_change`, `decay_change`, `next[j]` |
| Signed cumulative terms | Four `ledger.*.add` calls, before replacing inventory/field |
| Stepwise inventory | `inventory = mass_sum(next, cell_liters)` |
| Zero exact residual | `ledger_residual` sign convention; the native check allows roundoff |
| Variable-step rollout | Repeated accepted substeps, including shortened event/output steps |

See [transport.cpp](../engine/src/transport.cpp) and the independently hand-computed
examples in [transport_tests.cpp](../engine/tests/transport_tests.cpp). This is a
reviewed correspondence, not verified extraction or a compiler refinement proof.

The three added native regressions use exactly representable dyadic inputs:

| Case | Independent expectations |
|---|---|
| Open two-cell step: `dx=2`, `A=0.25`, `dt=1/8`, `v=1`, `D=1`, `k=0.5`, old field `[4,2]`, inlet `1`, source `[1,3]` | Faces `[-2,5,2]`; final field `[3.4375,2.4375]`; inlet `-62.5`, outlet `62.5`, source `250`, decay `187.5`, final inventory `2937.5` mg. |
| Closed three-cell step: `dx=2`, `A=0.125`, `dt=1/4`, `D=2`, `k=0.25`, old field `[1,4,2]`, source `[0,1,2]` | Faces `[0,-3,2,0]`; interior transport changes concentrations but cancels in inventory; source `187.5`, decay `109.375`, final inventory `1828.125` mg. |
| Closed two-cell variable-step source/decay: `dt=[1/8,3/8,1/2]`, old field `[2,4]`, source `[1,0]`, `k=0.5`, 500 L/cell | Schedule events force three steps between the two saved frames; final second-cell value `585/256` mg/L, cumulative decay `1357.421875`, source `500`, final inventory `2142.578125` mg. |

## Reproduction and evidence

Use the unchanged pinned Lean 4.35.0/Mathlib environment described in
[README.md](README.md). The default `lake build` imports the assumption audit;
[verify.ps1](verify.ps1) also re-executes it and requires rejection of a deliberately
unapproved assumption. Actual theorem names, checked-source hashes and executed
results are recorded in [PROGRESS.md](PROGRESS.md) after verification.
