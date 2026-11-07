module

public import Mathlib.Basic.Real.Basic
public import Mathlib.Algebra.BigOperators.Group.List.Basic
public import Mathlib.Tactic.Linarith
public import Mathlib.Tactic.Ring

@[expose] public section

/-!
# Constant-rate rollout and interval bounds

This module formalizes exact-real Euler updates over a finite set of storage
coordinates. Rates may have either sign. The variable-step theorem includes a
shortened final step, as used by the native forecast sample grid. It does not model
IEEE arithmetic, clipping, input validation, or changing rates.
-/

namespace Breakwater.Affine

set_option autoImplicit false

noncomputable section

abbrev State (n : ℕ) := Fin n → ℝ

variable {n : ℕ}

/-- One actual constant-rate Euler update, with no state projection. -/
def eulerStep (state rate : State n) (dt : ℝ) : State n :=
  fun v => state v + dt * rate v

/-- Repeated Euler updates, defined recursively rather than by the closed form. -/
def repeatedEuler (initial rate : State n) (dt : ℝ) : ℕ → State n
  | 0 => initial
  | steps + 1 => eulerStep (repeatedEuler initial rate dt steps) rate dt

/-- The exact affine trajectory associated with a fixed initial state and rate. -/
def trajectory (initial rate : State n) (t : ℝ) : State n :=
  fun v => initial v + t * rate v

/-- Fixed-step repeated updates equal the affine expression at every coordinate. -/
theorem repeatedEuler_closed_form
    (initial rate : State n) (dt : ℝ) (steps : ℕ) (v : Fin n) :
    repeatedEuler initial rate dt steps v =
      initial v + (steps : ℝ) * dt * rate v := by
  induction steps with
  | zero => simp [repeatedEuler]
  | succ steps ih =>
      simp only [repeatedEuler, eulerStep, ih, Nat.cast_succ]
      ring

/-- Function-level form of the fixed-step rollout theorem. -/
theorem repeatedEuler_eq_trajectory
    (initial rate : State n) (dt : ℝ) (steps : ℕ) :
    repeatedEuler initial rate dt steps =
      trajectory initial rate ((steps : ℝ) * dt) := by
  funext v
  exact repeatedEuler_closed_form initial rate dt steps v

/-- A chronological list of updates, applying each step to the preceding state. -/
def variableEuler (initial rate : State n) : List ℝ → State n
  | [] => initial
  | dt :: rest => variableEuler (eulerStep initial rate dt) rate rest

/-- Arbitrary step sizes accumulate only their total elapsed time at constant rate. -/
theorem variableEuler_closed_form
    (initial rate : State n) (steps : List ℝ) (v : Fin n) :
    variableEuler initial rate steps v = initial v + steps.sum * rate v := by
  induction steps generalizing initial with
  | nil => simp [variableEuler]
  | cons dt rest ih =>
      simp only [variableEuler, ih, eulerStep, List.sum_cons]
      ring

/-- Native-style differences between consecutive sample times. -/
def timeSteps (previous : ℝ) : List ℝ → List ℝ
  | [] => []
  | t :: rest => (t - previous) :: timeSteps t rest

/-- Last sample time, retaining the previous time for an empty grid. -/
def finalTime (previous : ℝ) : List ℝ → ℝ
  | [] => previous
  | t :: rest => finalTime t rest

/-- Consecutive differences telescope, including a shortened last interval. -/
theorem timeSteps_sum (previous : ℝ) (times : List ℝ) :
    (timeSteps previous times).sum = finalTime previous times - previous := by
  induction times generalizing previous with
  | nil => simp [timeSteps, finalTime]
  | cons t rest ih =>
      simp only [timeSteps, List.sum_cons, ih, finalTime]
      ring

/-- A nonuniform sampled rollout equals the affine trajectory at its final time. -/
theorem variableEuler_on_grid
    (initial rate : State n) (previous : ℝ) (times : List ℝ) (v : Fin n) :
    variableEuler initial rate (timeSteps previous times) v =
      initial v + (finalTime previous times - previous) * rate v := by
  rw [variableEuler_closed_form, timeSteps_sum]

/-- For an affine scalar trajectory, bounds at both endpoints bound every point
of the interval. No sign restriction on the rate and no positive-horizon division
are needed, so the zero-horizon case is included. -/
theorem scalar_endpoint_bounds
    (initial rate lower upper horizon t : ℝ)
    (ht0 : 0 ≤ t) (htT : t ≤ horizon)
    (initial_bounds : lower ≤ initial ∧ initial ≤ upper)
    (final_bounds : lower ≤ initial + horizon * rate ∧
      initial + horizon * rate ≤ upper) :
    lower ≤ initial + t * rate ∧ initial + t * rate ≤ upper := by
  rcases initial_bounds with ⟨h0lower, h0upper⟩
  rcases final_bounds with ⟨hTlower, hTupper⟩
  by_cases hr : 0 ≤ rate
  · have hnonnegative : 0 ≤ t * rate := mul_nonneg ht0 hr
    have hscaled : t * rate ≤ horizon * rate :=
      mul_le_mul_of_nonneg_right htT hr
    constructor
    · linarith only [h0lower, hnonnegative]
    · linarith only [hTupper, hscaled]
  · have hrnonpositive : rate ≤ 0 := by linarith
    have hnonpositive : t * rate ≤ 0 :=
      mul_nonpos_of_nonneg_of_nonpos ht0 hrnonpositive
    have hscaled : horizon * rate ≤ t * rate :=
      mul_le_mul_of_nonpos_right htT hrnonpositive
    constructor
    · linarith only [hTlower, hscaled]
    · linarith only [h0upper, hnonpositive]

/-- Pointwise endpoint bounds imply coordinate bounds throughout the full interval.
The interval hypotheses imply a nonnegative horizon whenever an interior time exists. -/
theorem trajectory_bounds
    (initial rate lower upper : State n) (horizon : ℝ)
    (initial_bounds : ∀ v, lower v ≤ initial v ∧ initial v ≤ upper v)
    (final_bounds : ∀ v, lower v ≤ trajectory initial rate horizon v ∧
      trajectory initial rate horizon v ≤ upper v) :
    ∀ t, 0 ≤ t → t ≤ horizon →
      ∀ v, lower v ≤ trajectory initial rate t v ∧
        trajectory initial rate t v ≤ upper v := by
  intro t ht0 htT v
  exact scalar_endpoint_bounds (initial v) (rate v) (lower v) (upper v)
    horizon t ht0 htT (initial_bounds v) (final_bounds v)

/-- Explicit degenerate-interval check: zero horizon needs only the initial bounds. -/
theorem trajectory_zero_horizon_bounds
    (initial rate lower upper : State n)
    (initial_bounds : ∀ v, lower v ≤ initial v ∧ initial v ≤ upper v) :
    ∀ t, 0 ≤ t → t ≤ 0 →
      ∀ v, lower v ≤ trajectory initial rate t v ∧
        trajectory initial rate t v ≤ upper v := by
  intro t ht0 htT v
  have ht : t = 0 := le_antisymm htT ht0
  subst t
  simpa only [trajectory, zero_mul, add_zero] using initial_bounds v

end

end Breakwater.Affine
