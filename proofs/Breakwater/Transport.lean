module

public import Breakwater.Graph
public import Mathlib.Tactic.Linarith

@[expose] public section

/-!
# Exact discrete finite-volume tracer mass balance

Cell values are concentrations in mg/L; all arithmetic here is over the reals.
Face fluxes are oriented from inlet to outlet and may have either sign. The
state-dependent flux function is evaluated at the old field at each Euler step.
The statements concern the discrete update and signed ledger, not scheduler
selection, differential-equation error, or floating-point implementation.
-/

namespace Breakwater.Transport

set_option autoImplicit false

noncomputable section

open scoped BigOperators

abbrev CellField (n : ℕ) := Fin n → ℝ
abbrev FaceField (n : ℕ) := Fin (n + 1) → ℝ

variable {n : ℕ}

/-- Uniform cell volume in litres, including the cubic-metre conversion. -/
def cellLiters (area dx : ℝ) : ℝ := (area * dx) * 1000

/-- Boundary flux to tracer mass-rate scale. -/
def boundaryMassScale (area : ℝ) : ℝ := area * 1000

/-- Sum each cell's concentration times its volume, as in native `mass_sum`. -/
def inventory (area dx : ℝ) (field : CellField n) : ℝ :=
  ∑ i : Fin n, field i * cellLiters area dx

theorem inventory_eq_scale_sum (area dx : ℝ) (field : CellField n) :
    inventory area dx field = cellLiters area dx * ∑ i : Fin n, field i := by
  simp only [inventory, Finset.sum_mul, mul_comm]

/-- Interior faces appear with both signs; only the two boundary faces remain. -/
theorem face_differences_telescope (flux : FaceField n) :
    (∑ i : Fin n, (flux i.castSucc - flux i.succ)) =
      flux 0 - flux (Fin.last n) := by
  rw [Finset.sum_sub_distrib]
  have first := Fin.sum_univ_succ flux
  have last := Fin.sum_univ_castSucc flux
  linarith

/-- The native explicit cell-average update, without projection or clipping. -/
def eulerStep (dx dt decay : ℝ) (source : CellField n) (flux : FaceField n)
    (field : CellField n) : CellField n :=
  fun i => field i + (dt / dx) * (flux i.castSucc - flux i.succ) -
    (dt * decay) * field i + dt * source i

theorem euler_sum_balance (dx dt decay : ℝ) (source field : CellField n)
    (flux : FaceField n) :
    (∑ i : Fin n, eulerStep dx dt decay source flux field i) =
      (∑ i : Fin n, field i) + (dt / dx) * (flux 0 - flux (Fin.last n)) -
      (dt * decay) * (∑ i : Fin n, field i) + dt * ∑ i : Fin n, source i := by
  simp only [eulerStep, Finset.sum_add_distrib, Finset.sum_sub_distrib, ← Finset.mul_sum]
  rw [← Finset.sum_sub_distrib, face_differences_telescope]

/-- One step equals the independently defined inlet/outlet/source/old-field decay ledger. -/
theorem euler_inventory_balance (area dx dt decay : ℝ) (dx_nonzero : dx ≠ 0)
    (source field : CellField n) (flux : FaceField n) :
    inventory area dx (eulerStep dx dt decay source flux field) =
      inventory area dx field + (dt * boundaryMassScale area) * flux 0 -
      (dt * boundaryMassScale area) * flux (Fin.last n) +
      dt * inventory area dx source - (dt * decay) * inventory area dx field := by
  have scaled_flux : cellLiters area dx * (dt / dx) = dt * boundaryMassScale area := by
    unfold cellLiters boundaryMassScale
    calc
      (area * dx) * 1000 * (dt / dx) = (area * 1000) * ((dt / dx) * dx) := by ring
      _ = dt * (area * 1000) := by rw [div_mul_cancel₀ dt dx_nonzero]; ring
  simp only [inventory_eq_scale_sum, euler_sum_balance]
  rw [mul_add, mul_sub, mul_add, ← mul_assoc, scaled_flux]
  ring

/-- Half-cell Dirichlet boundary flux. `inletAverage` is the native substep inlet average. -/
def inletFlux (velocity dispersion dx inletAverage firstCell : ℝ) : ℝ :=
  velocity * inletAverage - 2 * (dispersion / dx) * (firstCell - inletAverage)

/-- First-order upwind advection and centred diffusion at an interior face. -/
def interiorFlux (velocity dispersion dx : ℝ) (field : CellField (n + 1))
    (i : Fin n) : ℝ :=
  velocity * field i.castSucc - (dispersion / dx) * (field i.succ - field i.castSucc)

/-- Open boundaries have a half-cell inlet and advective-only outlet. -/
def openFlux (velocity dispersion dx inletAverage : ℝ) (field : CellField (n + 1)) :
    FaceField (n + 1) :=
  Fin.cases (inletFlux velocity dispersion dx inletAverage (field 0))
    (Fin.lastCases (velocity * field (Fin.last n)) (interiorFlux velocity dispersion dx field))

/-- Closed pipes use zero boundary flux and zero-velocity interior diffusion. -/
def closedFlux (dispersion dx : ℝ) (field : CellField (n + 1)) : FaceField (n + 1) :=
  Fin.cases 0 (Fin.lastCases 0 (interiorFlux 0 dispersion dx field))

theorem openFlux_inlet (velocity dispersion dx inletAverage : ℝ)
    (field : CellField (n + 1)) :
    openFlux velocity dispersion dx inletAverage field 0 =
      inletFlux velocity dispersion dx inletAverage (field 0) := by
  simp [openFlux]

theorem openFlux_outlet (velocity dispersion dx inletAverage : ℝ)
    (field : CellField (n + 1)) :
    openFlux velocity dispersion dx inletAverage field (Fin.last (n + 1)) =
      velocity * field (Fin.last n) := by
  have last_index : Fin.last (n + 1) = (Fin.last n).succ := rfl
  unfold openFlux
  rw [last_index, Fin.cases_succ, Fin.lastCases_last]

theorem openFlux_interior (velocity dispersion dx inletAverage : ℝ)
    (field : CellField (n + 1)) (i : Fin n) :
    openFlux velocity dispersion dx inletAverage field i.castSucc.succ =
      interiorFlux velocity dispersion dx field i := by
  simp [openFlux]

theorem closedFlux_inlet (dispersion dx : ℝ) (field : CellField (n + 1)) :
    closedFlux dispersion dx field 0 = 0 := by
  simp [closedFlux]

theorem closedFlux_outlet (dispersion dx : ℝ) (field : CellField (n + 1)) :
    closedFlux dispersion dx field (Fin.last (n + 1)) = 0 := by
  have last_index : Fin.last (n + 1) = (Fin.last n).succ := rfl
  unfold closedFlux
  rw [last_index, Fin.cases_succ, Fin.lastCases_last]

theorem closedFlux_interior (dispersion dx : ℝ)
    (field : CellField (n + 1)) (i : Fin n) :
    closedFlux dispersion dx field i.castSucc.succ = interiorFlux 0 dispersion dx field i := by
  simp [closedFlux]

/-- Open-pipe balance with the actual half-cell inlet and advective outlet. -/
theorem open_euler_inventory_balance (area dx dt decay velocity dispersion inletAverage : ℝ)
    (dx_nonzero : dx ≠ 0) (source field : CellField (n + 1)) :
    inventory area dx
        (eulerStep dx dt decay source (openFlux velocity dispersion dx inletAverage field) field) =
      inventory area dx field +
      (dt * boundaryMassScale area) * inletFlux velocity dispersion dx inletAverage (field 0) -
      (dt * boundaryMassScale area) * (velocity * field (Fin.last n)) +
      dt * inventory area dx source - (dt * decay) * inventory area dx field := by
  rw [euler_inventory_balance area dx dt decay dx_nonzero, openFlux_inlet, openFlux_outlet]

/-- Closed-pipe inventory changes only by source and old-inventory decay. -/
theorem closed_euler_inventory_balance (area dx dt decay dispersion : ℝ)
    (dx_nonzero : dx ≠ 0) (source field : CellField (n + 1)) :
    inventory area dx
        (eulerStep dx dt decay source (closedFlux dispersion dx field) field) =
      inventory area dx field + dt * inventory area dx source -
      (dt * decay) * inventory area dx field := by
  rw [euler_inventory_balance area dx dt decay dx_nonzero, closedFlux_inlet, closedFlux_outlet]
  ring

/-- With zero flow, diffusion can carry tracer out through the inlet. -/
theorem zero_flow_inlet_loss (dispersion dx inletAverage firstCell : ℝ)
    (dispersion_positive : 0 < dispersion) (dx_positive : 0 < dx)
    (higher_inside : inletAverage < firstCell) :
    inletFlux 0 dispersion dx inletAverage firstCell < 0 := by
  have diffusion_positive : 0 < dispersion / dx := div_pos dispersion_positive dx_positive
  have difference_positive : 0 < firstCell - inletAverage := sub_pos.mpr higher_inside
  have product_positive : 0 < 2 * (dispersion / dx) * (firstCell - inletAverage) :=
    mul_pos (mul_pos (by norm_num) diffusion_positive) difference_positive
  simp only [inletFlux, zero_mul, zero_sub]
  linarith

/-- Substep inputs; the flux law is evaluated on the current old field. -/
structure StepData (n : ℕ) where
  dt : ℝ
  decay : ℝ
  source : CellField n
  flux : CellField n → FaceField n

/-- Four separately accumulated signed tracer mass contributions, in mg. -/
structure Ledger where
  inlet : ℝ
  outlet : ℝ
  source : ℝ
  decay : ℝ

def zeroLedger : Ledger := ⟨0, 0, 0, 0⟩

def addLedger (left right : Ledger) : Ledger :=
  ⟨left.inlet + right.inlet, left.outlet + right.outlet,
   left.source + right.source, left.decay + right.decay⟩

/-- Native ledger increments: decay uses inventory before applying this step. -/
def stepLedger (area dx : ℝ) (step : StepData n) (field : CellField n) : Ledger :=
  { inlet := (step.dt * boundaryMassScale area) * step.flux field 0
    outlet := (step.dt * boundaryMassScale area) * step.flux field (Fin.last n)
    source := step.dt * inventory area dx step.source
    decay := (step.dt * step.decay) * inventory area dx field }

/-- Apply each substep to the preceding field and accumulate its separate ledger. -/
def rollout (area dx : ℝ) (field : CellField n) (ledger : Ledger) :
    List (StepData n) → CellField n × Ledger
  | [] => (field, ledger)
  | step :: rest =>
      rollout area dx
        (eulerStep dx step.dt step.decay step.source (step.flux field) field)
        (addLedger ledger (stepLedger area dx step field)) rest

/-- Signed native residual, without scale-dependent numerical tolerance. -/
def massResidual (area dx initialInventory : ℝ) (field : CellField n) (ledger : Ledger) : ℝ :=
  inventory area dx field - initialInventory - ledger.inlet + ledger.outlet -
    ledger.source + ledger.decay

/-- Any finite sequence of variable, state-dependent steps preserves the ledger residual. -/
theorem rollout_residual_preserved (area dx initialInventory : ℝ) (dx_nonzero : dx ≠ 0)
    (steps : List (StepData n)) (field : CellField n) (ledger : Ledger) :
    massResidual area dx initialInventory (rollout area dx field ledger steps).1
        (rollout area dx field ledger steps).2 =
      massResidual area dx initialInventory field ledger := by
  induction steps generalizing field ledger with
  | nil => rfl
  | cons step rest ih =>
      simp only [rollout]
      rw [ih]
      unfold massResidual
      rw [euler_inventory_balance area dx step.dt step.decay dx_nonzero]
      simp only [addLedger, stepLedger]
      ring

/-- From a zero initial ledger, the full finite rollout has exactly zero signed residual. -/
theorem rollout_mass_residual_zero (area dx : ℝ) (dx_nonzero : dx ≠ 0)
    (steps : List (StepData n)) (initial : CellField n) :
    massResidual area dx (inventory area dx initial)
        (rollout area dx initial zeroLedger steps).1
        (rollout area dx initial zeroLedger steps).2 = 0 := by
  rw [rollout_residual_preserved area dx (inventory area dx initial) dx_nonzero]
  simp [massResidual, zeroLedger]

/-- Telescoped inlet/outlet/source/decay balance for the actual recursively computed field. -/
theorem rollout_inventory_balance (area dx : ℝ) (dx_nonzero : dx ≠ 0)
    (steps : List (StepData n)) (initial : CellField n) :
    inventory area dx (rollout area dx initial zeroLedger steps).1 =
      inventory area dx initial + (rollout area dx initial zeroLedger steps).2.inlet -
      (rollout area dx initial zeroLedger steps).2.outlet +
      (rollout area dx initial zeroLedger steps).2.source -
      (rollout area dx initial zeroLedger steps).2.decay := by
  have balance := rollout_mass_residual_zero area dx dx_nonzero steps initial
  unfold massResidual at balance
  linarith

end

end Breakwater.Transport
