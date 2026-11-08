# A10b: conditional FVM nonnegativity

Implemented after the [post-A10a audit](../science/AUDIT_POST_A10A.md), whose gate
closed in `631f783`. This extends A10a's existing update and flux laws; it does not
change the native solver or public contracts. The claim is **nonnegativity**:
zero concentrations, coefficients and steps are allowed.

## Exact-real hypotheses and coefficients

`TransportNonnegativity.lean` works with `n+2` cells, for arbitrary natural `n`.
Thus the minimum native two-cell mesh has no interior cells, without a special
exception or an assumed third neighbor. `Nonnegative field` means every cell
value is at least zero. The physical preservation theorems require:

- `dx > 0`, `dt >= 0` and nonnegative velocity `v`, dispersion `D`, decay `k`,
  old field, source and prescribed substep inlet average.
- Open mode: `dt*(v/dx + 3*D/dx^2 + k) <= safety <= 1`.
- Closed mode: zero velocity and `dt*(2*D/dx^2 + k) <= safety <= 1`.

The native scheduler uses a strictly positive safety factor; the theorem also
permits safety zero when its nonnegative budget is zero. Positive area is required
by native geometry, but is unnecessary for concentration nonnegativity. The
composed mass identity inherits A10a's nonzero-width condition from `dx > 0` and
does not weaken any existing A10a statement.

Set `a = dt*v/dx`, `b = dt*D/dx^2`, `r = dt*k`. The following identities are
**proved from `eulerStep`, `openFlux` and `closedFlux`**, including their shared
interior faces, half-cell inlet and advective-only outlet:

| Cell | Updated concentration |
|---|---|
| Open first | `(1-a-3*b-r)*C0 + b*C1 + (a+2*b)*Cin + dt*S0` |
| Open interior | `(a+b)*Cprev + (1-a-2*b-r)*C + b*Cnext + dt*S` |
| Open last | `(a+b)*Cprev + (1-a-b-r)*Clast + dt*Slast` |
| Closed first | `(1-b-r)*C0 + b*C1 + dt*S0` |
| Closed interior | `b*Cprev + (1-2*b-r)*C + b*Cnext + dt*S` |
| Closed last | `(1-b-r)*Clast + b*Cprev + dt*Slast` |

`dimensionless_numbers_nonnegative` proves `a,b,r >= 0` from physical data.
The budget identities connect the physical timestep expressions to `a+3*b+r`
and `2*b+r`. The coefficient lemmas prove all diagonal and entering/neighborhood
coefficients nonnegative; `b >= 0` and `dt >= 0` supply the remaining factors.
One-step nonnegativity follows by nonnegative products and sums for every finite
cell index, allowing equality throughout. Algebraic expansion identities themselves
need no positivity premises; physical interpretation/preservation uses positive width.

## Variable steps and mass-balance composition

`NativeStep n` contains boundary mode, actual `dt`, `v`, `D`, `k`, inlet average,
source and safety. It contains **no arbitrary flux function**.
`NativeStep.toStepData dx` selects the original `openFlux` or `closedFlux` and
embeds those inputs in A10a's existing `StepData`. `AdmissibleStep dx step`
records nonnegative physical data, safety at most one and that mode's actual
budget; its closed branch explicitly requires velocity zero.

`native_step_nonnegative` derives preservation from these physical hypotheses.
`native_rollout_nonnegative` inducts over a finite list of individually admissible
steps, using A10a's actual `rollout` with the mapped physical inputs. Every flux
is evaluated on the recursively computed old field. Unequal/event-shortened steps
and changing coefficients, sources, inlet averages and safety factors are permitted;
fixed native parameters are a supported subset. The empty list is the identity.
The result applies to any admissible prefix as well as the whole finite list.

`native_rollout_nonnegative_mass_balance` pairs that result with the unchanged
`rollout_inventory_balance`: starting from a zero ledger, the same computed field
is nonnegative and its inventory equals initial inventory plus signed inlet minus
outlet plus source minus old-state decay. The ledger is independently defined in
A10a; conservation and arbitrary-flux preservation are not supplied as hypotheses.

## Required inlet counterexample

`interior_only_bound_counterexample` proves, in exact arithmetic,

```text
dx=1, v=0, D=1, k=0, dt=1/2, C=[1,0], Cin=0, S=0
dt*(2*D/dx^2) = 1
Cnew[0] = -1/2
```

The old field is nonnegative and the interior-only bound holds, but the resulting
field is not nonnegative. The correct open budget is `3/2 > 1`, so this does not
satisfy the preservation theorem. This is a deliberately unsafe mathematical
single step, not a native execution. The existing native inlet-CFL regression is
strengthened to check that the scheduler executes `dt=1/3,1/6` instead, obtaining
`[0,1/3]` and then `[1/18,5/18]` mg/L within floating-point tolerance.

## Advertised statements and assumption policy

All 17 new names are in `Breakwater.Transport`, in addition to the unchanged 37
storage/control-influence/A10a statements, for **54 audited propositions**:

| Statements | Guarantee |
|---|---|
| `open_first_expansion`, `open_interior_expansion`, `open_last_expansion` | Actual open-law coefficients, including the half-cell inlet |
| `closed_first_expansion`, `closed_interior_expansion`, `closed_last_expansion` | Actual zero-flow closed-law coefficients |
| `dimensionless_numbers_nonnegative` | Nonnegative `a,b,r` from physical data |
| `open_budget_identity`, `closed_budget_identity` | Physical timestep budgets equal the coefficient budgets |
| `open_coefficient_bounds`, `closed_coefficient_bounds` | Nonnegative stencil coefficients under the sufficient bounds |
| `open_euler_nonnegative`, `closed_euler_nonnegative` | Every cell remains nonnegative in one step, including two-cell meshes |
| `native_step_nonnegative` | Physical/admissible step embedding preserves the field |
| `native_rollout_nonnegative` | Actual recursively evaluated admissible variable-step rollout preserves the field |
| `native_rollout_nonnegative_mass_balance` | Same field has nonnegativity and A10a's independent signed inventory balance |
| `interior_only_bound_counterexample` | Interior diffusion bound alone permits a negative open first cell |

Both `#print axioms` and the proposition/transitive-assumption guard cover all 54
names. The wrapper expects 54, rejects prohibited proof tokens in owned sources,
checks the unchanged Lean 4.35.0/Mathlib
`56a005855ea17e0b0958f73ec801ae140b0a1679` pins, and permits only the existing
foundations `propext`, `Classical.choice`, `Quot.sound`. The unexpected-assumption
probe must fail for its actual dependency, not an unrelated parse/import failure.

## Native mapping, units and implementation regressions

| Mathematical input or operation | `engine/src/transport.cpp` |
|---|---|
| `dx` in m; at least two cells | Validated pipe geometry and uniform `length_m/cells` |
| `v` in m/h | Selected substep flow `flow_m3h / area_m2` |
| `D` in m2/h, `k` in 1/h | `dispersion_m2_h`, `decay_per_h` |
| `dt` in h and mode budget | `next_time-time`, `stability_rate` and reported `max_cfl_number` |
| `Cin` in mg/L | Half-weighted values at both ends of the substep; event stops keep inlet forcing affine |
| Nonnegative `S` in mg/L/h | Validated source profile; empty source maps to zeros |
| Old-state concentration step | Shared `flux` buffer and `next[j]` expression before `field.swap(next)` |
| Native physical step sequence | Accepted event/output-shortened steps; theorem assumes their exact-real admissibility |
| Inventory and ledger, mg | Original A10a `1000*A*dx` cell litres and `1000*A` boundary scaling |

The correspondence is reviewed manually; there is no verified extraction or
machine-checked refinement to the C++ binary. The
[native regression record](../engine/A10B_REGRESSIONS.md) supplies independently
specified equality-bound impulse responses on minimum and interior meshes,
nonnegative inlet/source forcing, zero coefficients/data, pure decay and unequal
substeps in both boundary modes. It preserves the original inlet-CFL test instead
of adding a duplicate request. See [PROGRESS.md](PROGRESS.md) and the committed
evidence there for actual commands, source hashes and results.

## Limits and stopping point

This is an **exact-real conditional guarantee**. The native floating-point
scheduler accepts epsilon margins, evaluates reciprocals/products in a particular
order and may retain tiny negative concentrations with diagnostics. A real budget
at most one does not prove compiled nonnegativity, scheduler correctness, inlet
interpolation behavior or an IEEE rounding/overflow bound. No clipping, guard,
timestep or public-contract change is used to fit the theorem. Runtime guards,
independent tests and ASan remain separate implementation evidence.

Nonnegativity and mass balance do not establish PDE accuracy, peak/threshold
accuracy, real chemistry/hydraulics, learned-model quality, storage feasibility or
simulated approval. A9's retained coarse failure and approximately 0.40191 mg/L
refined front maximum error remain unchanged. The separate front-accuracy study
is deferred; no new accuracy experiment or scheme is bundled here.
