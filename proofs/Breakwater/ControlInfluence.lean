module

public import Breakwater.Network

@[expose] public section

/-!
# Precomputed control influence

The direct evaluator walks every route, forms that route's candidate flow, and adds
its weighted contribution. The optimized evaluator instead walks selected controls
and adds columns prepared once from the same route weights. Output coordinates may
represent tank rates, shared-destination totals, or hourly synthetic cost. Direct
control terms include the cost of source curtailment. Both loops are actual folds.

All quantities are exact real numbers, and all coefficients belong to one captured
network. This is not a statement about rounding, runtime input checks or rankings.
-/

set_option autoImplicit false
open scoped BigOperators

namespace Breakwater.ControlInfluence
noncomputable section

variable {outputs routes controls : ℕ}

/-- One route's independently weighted contribution to every output coordinate. -/
def weightedStep (weight : Fin outputs → Fin routes → ℝ) (flow : Fin routes → ℝ)
    (state : Fin outputs → ℝ) (route : Fin routes) : Fin outputs → ℝ :=
  fun output => state output + weight output route * flow route

/-- Sequential route traversal, not defined as a matrix product. -/
def weightedFold (weight : Fin outputs → Fin routes → ℝ) (flow : Fin routes → ℝ)
    (initial : Fin outputs → ℝ) : Fin outputs → ℝ :=
  (List.finRange routes).foldl (weightedStep weight flow) initial

theorem weighted_list_apply (weight : Fin outputs → Fin routes → ℝ)
    (flow : Fin routes → ℝ) (order : List (Fin routes))
    (initial : Fin outputs → ℝ) (output : Fin outputs) :
    order.foldl (weightedStep weight flow) initial output =
      initial output + (order.map (fun route => weight output route * flow route)).sum := by
  induction order generalizing initial with
  | nil => simp
  | cons route rest ih =>
      rw [List.foldl_cons, ih]
      simp only [List.map_cons, List.sum_cons, weightedStep]
      ring

/-- Fold induction connects actual sequential traversal to its independent finite sum. -/
theorem weightedFold_eq_sum (weight : Fin outputs → Fin routes → ℝ)
    (flow : Fin routes → ℝ) (initial : Fin outputs → ℝ) (output : Fin outputs) :
    weightedFold weight flow initial output =
      initial output + ∑ route : Fin routes, weight output route * flow route := by
  unfold weightedFold
  rw [weighted_list_apply, ← Fin.sum_univ_def]

/-- A selected route setpoint contributes +1; curtailment on its feed contributes -1.
The algebra permits any fixed coefficient matrix, including these sparse columns. -/
def candidateFlow (base : Fin routes → ℝ)
    (coefficient : Fin routes → Fin controls → ℝ) (delta : Fin controls → ℝ) :
    Fin routes → ℝ :=
  fun route => base route + ∑ control : Fin controls, coefficient route control * delta control

/-- General direct evaluator: direct control terms followed by the full route loop. -/
def directOutputs (offset : Fin outputs → ℝ)
    (direct : Fin outputs → Fin controls → ℝ)
    (weight : Fin outputs → Fin routes → ℝ) (base : Fin routes → ℝ)
    (coefficient : Fin routes → Fin controls → ℝ) (delta : Fin controls → ℝ) :
    Fin outputs → ℝ :=
  weightedFold weight (candidateFlow base coefficient delta)
    (fun output => offset output + ∑ control : Fin controls, direct output control * delta control)

/-- Prepared baseline totals are computed once using the original route traversal. -/
def baselineOutputs (offset : Fin outputs → ℝ)
    (weight : Fin outputs → Fin routes → ℝ) (base : Fin routes → ℝ) : Fin outputs → ℝ :=
  weightedFold weight base offset

/-- Each prepared column accumulates the route effects of one selected control. -/
def controlColumn (direct : Fin outputs → Fin controls → ℝ)
    (weight : Fin outputs → Fin routes → ℝ)
    (coefficient : Fin routes → Fin controls → ℝ) (control : Fin controls) :
    Fin outputs → ℝ :=
  weightedFold weight (fun route => coefficient route control) (fun output => direct output control)

/-- Candidate evaluation walks only prepared control columns, in index order. -/
def evaluateColumns (baseline : Fin outputs → ℝ)
    (column : Fin controls → Fin outputs → ℝ) (delta : Fin controls → ℝ) :
    Fin outputs → ℝ :=
  (List.finRange controls).foldl
    (fun state control output => state output + column control output * delta control) baseline

theorem evaluateColumns_eq_sum (baseline : Fin outputs → ℝ)
    (column : Fin controls → Fin outputs → ℝ) (delta : Fin controls → ℝ)
    (output : Fin outputs) :
    evaluateColumns baseline column delta output =
      baseline output + ∑ control : Fin controls, column control output * delta control := by
  exact weightedFold_eq_sum (fun output control => column control output) delta baseline output

/-- Distributivity and finite-sum interchange justify moving route work out of the batch. -/
theorem weighted_candidate_decomposition (weight base : Fin routes → ℝ)
    (coefficient : Fin routes → Fin controls → ℝ) (delta : Fin controls → ℝ) :
    (∑ route : Fin routes, weight route * candidateFlow base coefficient delta route) =
      (∑ route : Fin routes, weight route * base route) +
        ∑ control : Fin controls,
          (∑ route : Fin routes, weight route * coefficient route control) * delta control := by
  simp only [candidateFlow, mul_add, Finset.sum_add_distrib, Finset.mul_sum, ← mul_assoc]
  rw [Finset.sum_comm]
  simp only [Finset.sum_mul]

/-- Full route recomputation and the prepared-column loop produce equal output vectors. -/
theorem precomputed_eq_direct (offset : Fin outputs → ℝ)
    (direct : Fin outputs → Fin controls → ℝ)
    (weight : Fin outputs → Fin routes → ℝ) (base : Fin routes → ℝ)
    (coefficient : Fin routes → Fin controls → ℝ) (delta : Fin controls → ℝ) :
    evaluateColumns (baselineOutputs offset weight base)
      (controlColumn direct weight coefficient) delta =
      directOutputs offset direct weight base coefficient delta := by
  funext output
  rw [evaluateColumns_eq_sum]
  simp only [baselineOutputs, controlColumn, directOutputs, weightedFold_eq_sum]
  rw [weighted_candidate_decomposition]
  simp only [add_mul, Finset.sum_add_distrib]
  ring

/-- No selected-control change reproduces the captured baseline, including no controls. -/
theorem precomputed_zero_delta (baseline : Fin outputs → ℝ)
    (column : Fin controls → Fin outputs → ℝ) :
    evaluateColumns baseline column (fun _ => 0) = baseline := by
  funext output
  simp [evaluateColumns_eq_sum]

/-- Incidence weights recover the existing sparse finite-graph network rate. -/
theorem direct_incidence_eq_network (G : Graph outputs routes)
    (external : Fin outputs → ℝ) (base : Fin routes → ℝ)
    (coefficient : Fin routes → Fin controls → ℝ) (delta : Fin controls → ℝ) :
    directOutputs external (fun _ _ => 0) (incidence G) base coefficient delta =
      networkRate G (candidateFlow base coefficient delta) external := by
  funext output
  simp only [directOutputs, weightedFold_eq_sum, zero_mul, Finset.sum_const_zero, add_zero]
  rw [networkRate_eq_dense]
  rfl

/-- Prepared incidence columns generate exactly the original graph rate. -/
theorem precomputed_network_rate (G : Graph outputs routes)
    (external : Fin outputs → ℝ) (base : Fin routes → ℝ)
    (coefficient : Fin routes → Fin controls → ℝ) (delta : Fin controls → ℝ) :
    evaluateColumns (baselineOutputs external (incidence G) base)
      (controlColumn (fun _ _ => 0) (incidence G) coefficient) delta =
      networkRate G (candidateFlow base coefficient delta) external := by
  rw [precomputed_eq_direct]
  exact direct_incidence_eq_network G external base coefficient delta

/-- The prepared rate leaves every exact-real affine trajectory sample unchanged. -/
theorem precomputed_network_trajectory (G : Graph outputs routes)
    (external initial : Fin outputs → ℝ) (base : Fin routes → ℝ)
    (coefficient : Fin routes → Fin controls → ℝ) (delta : Fin controls → ℝ) (time : ℝ) :
    Affine.trajectory initial
      (evaluateColumns (baselineOutputs external (incidence G) base)
        (controlColumn (fun _ _ => 0) (incidence G) coefficient) delta) time =
      Affine.trajectory initial (networkRate G (candidateFlow base coefficient delta) external) time := by
  rw [precomputed_network_rate]

end
end Breakwater.ControlInfluence
