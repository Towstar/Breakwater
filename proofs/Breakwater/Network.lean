module

public import Breakwater.Graph
public import Breakwater.Affine

@[expose] public section

/-! Connect finite-graph kernels to the constant-rate trajectory results. -/

set_option autoImplicit false
open scoped BigOperators

namespace Breakwater
noncomputable section
variable {n m : ℕ}

/-- Captured external net inflow plus all internal edge contributions. -/
def networkRate (G : Graph n m) (flow : Fin m → ℝ) (external : Fin n → ℝ) :
    Fin n → ℝ := sparseFold G flow external

theorem networkRate_eq_dense (G : Graph n m) (flow : Fin m → ℝ)
    (external : Fin n → ℝ) :
    networkRate G flow external = fun v => external v + denseRate G flow v :=
  sparse_eq_dense G flow external

/-- Repeated actual graph Euler steps; graph, flows and external rates stay fixed. -/
def networkRollout (G : Graph n m) (flow : Fin m → ℝ) (external : Fin n → ℝ)
    (dt : ℝ) (initial : Fin n → ℝ) : ℕ → Fin n → ℝ
  | 0 => initial
  | steps + 1 => graphEuler G flow external dt (networkRollout G flow external dt initial steps)

theorem networkRollout_eq_repeatedEuler (G : Graph n m) (flow : Fin m → ℝ)
    (external : Fin n → ℝ) (dt : ℝ) (initial : Fin n → ℝ) (steps : ℕ) :
    networkRollout G flow external dt initial steps =
      Affine.repeatedEuler initial (networkRate G flow external) dt steps := by
  induction steps with
  | zero => rfl
  | succ steps ih =>
      simp only [networkRollout, Affine.repeatedEuler, ih]
      rfl

/-- Closed-form trajectory for any finite captured network, not just two tanks. -/
theorem network_rollout_affine (G : Graph n m) (flow : Fin m → ℝ)
    (external : Fin n → ℝ) (dt : ℝ) (initial : Fin n → ℝ) (steps : ℕ) :
    networkRollout G flow external dt initial steps =
      Affine.trajectory initial (networkRate G flow external) ((steps : ℝ) * dt) := by
  rw [networkRollout_eq_repeatedEuler]
  exact Affine.repeatedEuler_eq_trajectory initial (networkRate G flow external) dt steps

/-- Internal flows cancel across any number of fixed-rate graph Euler steps. -/
theorem network_rollout_total_balance (G : Graph n m) (flow : Fin m → ℝ)
    (external : Fin n → ℝ) (dt : ℝ) (initial : Fin n → ℝ) (steps : ℕ) :
    (∑ v : Fin n, networkRollout G flow external dt initial steps v) =
      (∑ v : Fin n, initial v) + (steps : ℝ) * dt * ∑ v : Fin n, external v := by
  induction steps with
  | zero => simp [networkRollout]
  | succ steps ih =>
      rw [networkRollout, graphEuler_total_balance, ih]
      simp only [Nat.cast_add, Nat.cast_one]
      ring

/-- Endpoint bounds for a finite network imply storage bounds over its whole horizon. -/
theorem network_interval_bounds (G : Graph n m) (flow : Fin m → ℝ)
    (external initial lower upper : Fin n → ℝ) (horizon : ℝ)
    (initial_bounds : ∀ v, lower v ≤ initial v ∧ initial v ≤ upper v)
    (final_bounds : ∀ v,
      lower v ≤ Affine.trajectory initial (networkRate G flow external) horizon v ∧
      Affine.trajectory initial (networkRate G flow external) horizon v ≤ upper v) :
    ∀ t, 0 ≤ t → t ≤ horizon → ∀ v,
      lower v ≤ Affine.trajectory initial (networkRate G flow external) t v ∧
      Affine.trajectory initial (networkRate G flow external) t v ≤ upper v := by
  exact Affine.trajectory_bounds initial (networkRate G flow external) lower upper horizon
    initial_bounds final_bounds

end
end Breakwater
