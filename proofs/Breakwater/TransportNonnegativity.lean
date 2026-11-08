module

public import Breakwater.Transport

@[expose] public section

/-!
# Conditional exact-real nonnegativity of the native FVM recurrence

The mesh has `n + 2` cells, including the two-cell case `n = 0`. The expansions
below are derived from the existing face laws and Euler update. A budget at most
one is an exact-real hypothesis, not a statement about the native epsilon guards.
-/

namespace Breakwater.Transport

set_option autoImplicit false

noncomputable section

variable {n : ℕ}

def advectionNumber (dt velocity dx : ℝ) : ℝ := dt * velocity / dx
def diffusionNumber (dt dispersion dx : ℝ) : ℝ := dt * dispersion / dx ^ 2
def reactionNumber (dt decay : ℝ) : ℝ := dt * decay

def Nonnegative (field : CellField n) : Prop := ∀ i, 0 ≤ field i

theorem open_first_expansion (dx dt decay velocity dispersion inlet : ℝ)
    (source field : CellField (n + 2)) :
    eulerStep dx dt decay source (openFlux velocity dispersion dx inlet field) field 0 =
      (1 - advectionNumber dt velocity dx - 3 * diffusionNumber dt dispersion dx -
        reactionNumber dt decay) * field 0 +
      diffusionNumber dt dispersion dx * field (0 : Fin (n + 1)).succ +
      (advectionNumber dt velocity dx + 2 * diffusionNumber dt dispersion dx) * inlet +
      dt * source 0 := by
  change field 0 + (dt / dx) *
      (openFlux velocity dispersion dx inlet field 0 -
        openFlux velocity dispersion dx inlet field (0 : Fin (n + 1)).castSucc.succ) -
      (dt * decay) * field 0 + dt * source 0 = _
  rw [openFlux_inlet, openFlux_interior]
  simp only [inletFlux, interiorFlux, advectionNumber, diffusionNumber, reactionNumber,
    div_eq_mul_inv, Fin.castSucc_zero]
  ring

theorem open_last_expansion (dx dt decay velocity dispersion inlet : ℝ)
    (source field : CellField (n + 2)) :
    eulerStep dx dt decay source (openFlux velocity dispersion dx inlet field) field
        (Fin.last (n + 1)) =
      (advectionNumber dt velocity dx + diffusionNumber dt dispersion dx) *
        field (Fin.last n).castSucc +
      (1 - advectionNumber dt velocity dx - diffusionNumber dt dispersion dx -
        reactionNumber dt decay) * field (Fin.last (n + 1)) +
      dt * source (Fin.last (n + 1)) := by
  change field (Fin.last (n + 1)) + (dt / dx) *
      (openFlux velocity dispersion dx inlet field (Fin.last n).castSucc.succ -
        openFlux velocity dispersion dx inlet field (Fin.last (n + 2))) -
      (dt * decay) * field (Fin.last (n + 1)) + dt * source (Fin.last (n + 1)) = _
  rw [openFlux_interior, openFlux_outlet]
  simp only [interiorFlux, advectionNumber, diffusionNumber, reactionNumber,
    div_eq_mul_inv, Fin.succ_last]
  ring

theorem open_interior_expansion (dx dt decay velocity dispersion inlet : ℝ)
    (source field : CellField (n + 2)) (i : Fin n) :
    eulerStep dx dt decay source (openFlux velocity dispersion dx inlet field) field
        i.castSucc.succ =
      (advectionNumber dt velocity dx + diffusionNumber dt dispersion dx) *
        field i.castSucc.castSucc +
      (1 - advectionNumber dt velocity dx - 2 * diffusionNumber dt dispersion dx -
        reactionNumber dt decay) * field i.castSucc.succ +
      diffusionNumber dt dispersion dx * field i.succ.succ + dt * source i.castSucc.succ := by
  change field i.castSucc.succ + (dt / dx) *
      (openFlux velocity dispersion dx inlet field i.castSucc.castSucc.succ -
        openFlux velocity dispersion dx inlet field i.succ.castSucc.succ) -
      (dt * decay) * field i.castSucc.succ + dt * source i.castSucc.succ = _
  rw [openFlux_interior, openFlux_interior]
  simp only [interiorFlux, advectionNumber, diffusionNumber, reactionNumber,
    div_eq_mul_inv, Fin.castSucc_succ]
  ring

theorem closed_first_expansion (dx dt decay dispersion : ℝ)
    (source field : CellField (n + 2)) :
    eulerStep dx dt decay source (closedFlux dispersion dx field) field 0 =
      (1 - diffusionNumber dt dispersion dx - reactionNumber dt decay) * field 0 +
      diffusionNumber dt dispersion dx * field (0 : Fin (n + 1)).succ + dt * source 0 := by
  change field 0 + (dt / dx) *
      (closedFlux dispersion dx field 0 -
        closedFlux dispersion dx field (0 : Fin (n + 1)).castSucc.succ) -
      (dt * decay) * field 0 + dt * source 0 = _
  rw [closedFlux_inlet, closedFlux_interior]
  simp only [interiorFlux, diffusionNumber, reactionNumber,
    div_eq_mul_inv, Fin.castSucc_zero]
  ring

theorem closed_last_expansion (dx dt decay dispersion : ℝ)
    (source field : CellField (n + 2)) :
    eulerStep dx dt decay source (closedFlux dispersion dx field) field (Fin.last (n + 1)) =
      (1 - diffusionNumber dt dispersion dx - reactionNumber dt decay) *
        field (Fin.last (n + 1)) +
      diffusionNumber dt dispersion dx * field (Fin.last n).castSucc +
      dt * source (Fin.last (n + 1)) := by
  change field (Fin.last (n + 1)) + (dt / dx) *
      (closedFlux dispersion dx field (Fin.last n).castSucc.succ -
        closedFlux dispersion dx field (Fin.last (n + 2))) -
      (dt * decay) * field (Fin.last (n + 1)) + dt * source (Fin.last (n + 1)) = _
  rw [closedFlux_interior, closedFlux_outlet]
  simp only [interiorFlux, diffusionNumber, reactionNumber,
    div_eq_mul_inv, Fin.succ_last]
  ring

theorem closed_interior_expansion (dx dt decay dispersion : ℝ)
    (source field : CellField (n + 2)) (i : Fin n) :
    eulerStep dx dt decay source (closedFlux dispersion dx field) field i.castSucc.succ =
      diffusionNumber dt dispersion dx * field i.castSucc.castSucc +
      (1 - 2 * diffusionNumber dt dispersion dx - reactionNumber dt decay) *
        field i.castSucc.succ +
      diffusionNumber dt dispersion dx * field i.succ.succ + dt * source i.castSucc.succ := by
  change field i.castSucc.succ + (dt / dx) *
      (closedFlux dispersion dx field i.castSucc.castSucc.succ -
        closedFlux dispersion dx field i.succ.castSucc.succ) -
      (dt * decay) * field i.castSucc.succ + dt * source i.castSucc.succ = _
  rw [closedFlux_interior, closedFlux_interior]
  simp only [interiorFlux, diffusionNumber, reactionNumber,
    div_eq_mul_inv, Fin.castSucc_succ]
  ring

theorem dimensionless_numbers_nonnegative (dx dt velocity dispersion decay : ℝ)
    (dx_positive : 0 < dx) (dt_nonnegative : 0 ≤ dt) (velocity_nonnegative : 0 ≤ velocity)
    (dispersion_nonnegative : 0 ≤ dispersion) (decay_nonnegative : 0 ≤ decay) :
    0 ≤ advectionNumber dt velocity dx ∧ 0 ≤ diffusionNumber dt dispersion dx ∧
      0 ≤ reactionNumber dt decay := by
  exact ⟨div_nonneg (mul_nonneg dt_nonnegative velocity_nonnegative) (le_of_lt dx_positive),
    div_nonneg (mul_nonneg dt_nonnegative dispersion_nonnegative) (sq_nonneg dx),
    mul_nonneg dt_nonnegative decay_nonnegative⟩

theorem open_budget_identity (dx dt velocity dispersion decay : ℝ) :
    dt * (velocity / dx + 3 * dispersion / dx ^ 2 + decay) =
      advectionNumber dt velocity dx + 3 * diffusionNumber dt dispersion dx +
        reactionNumber dt decay := by
  simp only [advectionNumber, diffusionNumber, reactionNumber, div_eq_mul_inv]
  ring

theorem closed_budget_identity (dx dt dispersion decay : ℝ) :
    dt * (2 * dispersion / dx ^ 2 + decay) =
      2 * diffusionNumber dt dispersion dx + reactionNumber dt decay := by
  simp only [diffusionNumber, reactionNumber, div_eq_mul_inv]
  ring

theorem open_coefficient_bounds (a b r safety : ℝ) (ha : 0 ≤ a) (hb : 0 ≤ b)
    (budget : a + 3 * b + r ≤ safety) (safety_le_one : safety ≤ 1) :
    0 ≤ 1 - a - 3 * b - r ∧ 0 ≤ 1 - a - 2 * b - r ∧
      0 ≤ 1 - a - b - r ∧ 0 ≤ a + b ∧ 0 ≤ a + 2 * b := by
  constructor
  · linarith
  constructor
  · linarith
  constructor
  · linarith
  constructor <;> linarith

theorem closed_coefficient_bounds (b r safety : ℝ) (hb : 0 ≤ b)
    (budget : 2 * b + r ≤ safety) (safety_le_one : safety ≤ 1) :
    0 ≤ 1 - b - r ∧ 0 ≤ 1 - 2 * b - r := by
  constructor <;> linarith

theorem open_euler_nonnegative (dx dt decay velocity dispersion inlet safety : ℝ)
    (dx_positive : 0 < dx) (dt_nonnegative : 0 ≤ dt) (decay_nonnegative : 0 ≤ decay)
    (velocity_nonnegative : 0 ≤ velocity) (dispersion_nonnegative : 0 ≤ dispersion)
    (inlet_nonnegative : 0 ≤ inlet) (safety_le_one : safety ≤ 1)
    (budget : dt * (velocity / dx + 3 * dispersion / dx ^ 2 + decay) ≤ safety)
    (source field : CellField (n + 2)) (source_nonnegative : Nonnegative source)
    (field_nonnegative : Nonnegative field) :
    Nonnegative (eulerStep dx dt decay source (openFlux velocity dispersion dx inlet field) field) := by
  obtain ⟨ha, hb, _⟩ := dimensionless_numbers_nonnegative dx dt velocity dispersion decay
    dx_positive dt_nonnegative velocity_nonnegative dispersion_nonnegative decay_nonnegative
  rw [open_budget_identity] at budget
  obtain ⟨first, interior, last, neighbor, entering⟩ :=
    open_coefficient_bounds _ _ _ safety ha hb budget safety_le_one
  intro cell
  refine Fin.cases ?_ (fun j => Fin.lastCases ?_ (fun i => ?_) j) cell
  · rw [open_first_expansion]
    exact add_nonneg (add_nonneg (add_nonneg (mul_nonneg first (field_nonnegative _))
      (mul_nonneg hb (field_nonnegative _))) (mul_nonneg entering inlet_nonnegative))
      (mul_nonneg dt_nonnegative (source_nonnegative _))
  · rw [Fin.succ_last, open_last_expansion]
    exact add_nonneg (add_nonneg (mul_nonneg neighbor (field_nonnegative _))
      (mul_nonneg last (field_nonnegative _))) (mul_nonneg dt_nonnegative (source_nonnegative _))
  · rw [open_interior_expansion]
    exact add_nonneg (add_nonneg (add_nonneg (mul_nonneg neighbor (field_nonnegative _))
      (mul_nonneg interior (field_nonnegative _))) (mul_nonneg hb (field_nonnegative _)))
      (mul_nonneg dt_nonnegative (source_nonnegative _))

theorem closed_euler_nonnegative (dx dt decay dispersion safety : ℝ)
    (dx_positive : 0 < dx) (dt_nonnegative : 0 ≤ dt) (decay_nonnegative : 0 ≤ decay)
    (dispersion_nonnegative : 0 ≤ dispersion) (safety_le_one : safety ≤ 1)
    (budget : dt * (2 * dispersion / dx ^ 2 + decay) ≤ safety)
    (source field : CellField (n + 2)) (source_nonnegative : Nonnegative source)
    (field_nonnegative : Nonnegative field) :
    Nonnegative (eulerStep dx dt decay source (closedFlux dispersion dx field) field) := by
  obtain ⟨_, hb, _⟩ := dimensionless_numbers_nonnegative dx dt 0 dispersion decay
    dx_positive dt_nonnegative (le_refl 0) dispersion_nonnegative decay_nonnegative
  rw [closed_budget_identity] at budget
  obtain ⟨boundary, interior⟩ := closed_coefficient_bounds _ _ safety hb budget safety_le_one
  intro cell
  refine Fin.cases ?_ (fun j => Fin.lastCases ?_ (fun i => ?_) j) cell
  · rw [closed_first_expansion]
    exact add_nonneg (add_nonneg (mul_nonneg boundary (field_nonnegative _))
      (mul_nonneg hb (field_nonnegative _))) (mul_nonneg dt_nonnegative (source_nonnegative _))
  · rw [Fin.succ_last, closed_last_expansion]
    exact add_nonneg (add_nonneg (mul_nonneg boundary (field_nonnegative _))
      (mul_nonneg hb (field_nonnegative _))) (mul_nonneg dt_nonnegative (source_nonnegative _))
  · rw [closed_interior_expansion]
    exact add_nonneg (add_nonneg (add_nonneg (mul_nonneg hb (field_nonnegative _))
      (mul_nonneg interior (field_nonnegative _))) (mul_nonneg hb (field_nonnegative _)))
      (mul_nonneg dt_nonnegative (source_nonnegative _))

inductive NativeBoundary where
  | openInlet
  | closed

/-- Physical substep inputs, with no arbitrary face-law field. -/
structure NativeStep (n : ℕ) where
  boundary : NativeBoundary
  dt : ℝ
  velocity : ℝ
  dispersion : ℝ
  decay : ℝ
  inletAverage : ℝ
  source : CellField (n + 2)
  safety : ℝ

/-- Embed the physical laws into the existing A10a recurrence and ledger. -/
def NativeStep.toStepData (dx : ℝ) (step : NativeStep n) : StepData (n + 2) :=
  { dt := step.dt
    decay := step.decay
    source := step.source
    flux := match step.boundary with
      | .openInlet => openFlux step.velocity step.dispersion dx step.inletAverage
      | .closed => closedFlux step.dispersion dx }

/-- Each step carries its actual physical parameters and sufficient budget. -/
structure AdmissibleStep (dx : ℝ) (step : NativeStep n) : Prop where
  dt_nonnegative : 0 ≤ step.dt
  velocity_nonnegative : 0 ≤ step.velocity
  dispersion_nonnegative : 0 ≤ step.dispersion
  decay_nonnegative : 0 ≤ step.decay
  inlet_nonnegative : 0 ≤ step.inletAverage
  source_nonnegative : Nonnegative step.source
  safety_le_one : step.safety ≤ 1
  boundary_condition : match step.boundary with
    | .openInlet =>
        step.dt * (step.velocity / dx + 3 * step.dispersion / dx ^ 2 + step.decay) ≤ step.safety
    | .closed => step.velocity = 0 ∧
        step.dt * (2 * step.dispersion / dx ^ 2 + step.decay) ≤ step.safety

theorem native_step_nonnegative (dx : ℝ) (dx_positive : 0 < dx) (step : NativeStep n)
    (admissible : AdmissibleStep dx step) (field : CellField (n + 2))
    (field_nonnegative : Nonnegative field) :
    Nonnegative (eulerStep dx (step.toStepData dx).dt (step.toStepData dx).decay
      (step.toStepData dx).source ((step.toStepData dx).flux field) field) := by
  have condition := admissible.boundary_condition
  cases boundary_eq : step.boundary with
  | openInlet =>
      simp only [boundary_eq] at condition
      simpa only [NativeStep.toStepData, boundary_eq] using
        open_euler_nonnegative dx step.dt step.decay step.velocity step.dispersion
          step.inletAverage step.safety dx_positive admissible.dt_nonnegative
          admissible.decay_nonnegative admissible.velocity_nonnegative
          admissible.dispersion_nonnegative admissible.inlet_nonnegative admissible.safety_le_one
          condition step.source field admissible.source_nonnegative field_nonnegative
  | closed =>
      simp only [boundary_eq] at condition
      simpa only [NativeStep.toStepData, boundary_eq] using
        closed_euler_nonnegative dx step.dt step.decay step.dispersion step.safety
          dx_positive admissible.dt_nonnegative admissible.decay_nonnegative
          admissible.dispersion_nonnegative admissible.safety_le_one condition.2 step.source field
          admissible.source_nonnegative field_nonnegative

/-- Finite unequal physical steps preserve nonnegativity in the actual A10a rollout. -/
theorem native_rollout_nonnegative (area dx : ℝ) (dx_positive : 0 < dx)
    (steps : List (NativeStep n)) (field : CellField (n + 2)) (ledger : Ledger)
    (admissible : ∀ step ∈ steps, AdmissibleStep dx step)
    (field_nonnegative : Nonnegative field) :
    Nonnegative (rollout area dx field ledger (steps.map (NativeStep.toStepData dx))).1 := by
  induction steps generalizing field ledger with
  | nil => exact field_nonnegative
  | cons step rest ih =>
      have head : AdmissibleStep dx step := admissible step (by simp)
      have tail : ∀ item ∈ rest, AdmissibleStep dx item := by
        intro item member
        exact admissible item (List.mem_cons_of_mem step member)
      simp only [List.map_cons, rollout]
      exact ih _ _ tail (native_step_nonnegative dx dx_positive step head field field_nonnegative)

/-- Compose nonnegativity with A10a's unchanged independently accumulated mass ledger. -/
theorem native_rollout_nonnegative_mass_balance (area dx : ℝ) (dx_positive : 0 < dx)
    (steps : List (NativeStep n)) (initial : CellField (n + 2))
    (admissible : ∀ step ∈ steps, AdmissibleStep dx step)
    (initial_nonnegative : Nonnegative initial) :
    let result := rollout area dx initial zeroLedger (steps.map (NativeStep.toStepData dx))
    Nonnegative result.1 ∧
      inventory area dx result.1 = inventory area dx initial + result.2.inlet - result.2.outlet +
        result.2.source - result.2.decay := by
  exact ⟨native_rollout_nonnegative area dx dx_positive steps initial zeroLedger
      admissible initial_nonnegative,
    rollout_inventory_balance area dx (ne_of_gt dx_positive) _ initial⟩

def inletCounterexampleField : CellField 2 := fun i => if i = 0 then 1 else 0

/-- An interior-only diffusion budget permits a negative half-cell inlet update. -/
theorem interior_only_bound_counterexample :
    (1 / 2 : ℝ) * (2 * 1 / 1 ^ 2) = 1 ∧
    Nonnegative inletCounterexampleField ∧
    eulerStep 1 (1 / 2) 0 (fun _ => 0) (openFlux 0 1 1 0 inletCounterexampleField)
      inletCounterexampleField 0 = -(1 / 2) ∧
    ¬ Nonnegative (eulerStep 1 (1 / 2) 0 (fun _ => 0)
      (openFlux 0 1 1 0 inletCounterexampleField) inletCounterexampleField) := by
  have first : eulerStep 1 (1 / 2) 0 (fun _ => 0)
      (openFlux 0 1 1 0 inletCounterexampleField) inletCounterexampleField 0 = -(1 / 2) := by
    rw [open_first_expansion]
    norm_num [advectionNumber, diffusionNumber, reactionNumber, inletCounterexampleField]
  refine ⟨by norm_num, ?_, first, ?_⟩
  · intro i
    simp only [inletCounterexampleField]
    split <;> norm_num
  · intro nonnegative
    have at_inlet := nonnegative 0
    rw [first] at at_inlet
    norm_num at at_inlet

end

end Breakwater.Transport
