module

public import Mathlib.Algebra.BigOperators.Group.Finset.Basic
public import Mathlib.Algebra.BigOperators.Group.List.Basic
public import Mathlib.Algebra.BigOperators.Fin
public import Mathlib.Basic.Real.Basic
public import Mathlib.Tactic.Ring

@[expose] public section

/-!
# Finite-graph dense/sparse algorithm equivalence

`Graph n m` uses bounded indices for every edge endpoint. Parallel edges are distinct
indices; self-loops are also allowed and cancel. The C++ input validator excludes
self-loops, so the proved class is slightly broader than its accepted inputs.

`sparseFold` is an actual sequential left fold of source-subtract/target-add updates.
`denseRate` is independently defined as an incidence-matrix sum. All flows are fixed
throughout a fold and arithmetic is exact over the real numbers. No statement here
identifies exact arithmetic with C++ floating-point execution or establishes memory
safety, route capacities or a global search optimum.
-/

set_option autoImplicit false

open scoped BigOperators

namespace Breakwater

/-- A finite directed multigraph with valid, bounded endpoint indices. -/
structure Graph (n m : ℕ) where
  source : Fin m → Fin n
  target : Fin m → Fin n

variable {n m : ℕ}

/-- A column has +1 at its target and -1 at its source, including self-loop cancellation. -/
def incidence (G : Graph n m) (v : Fin n) (e : Fin m) : ℝ :=
  (if G.target e = v then 1 else 0) - (if G.source e = v then 1 else 0)

/-- An edge's signed contribution, without materializing a dense column. -/
def edgeDelta (G : Graph n m) (q : Fin m → ℝ) (e : Fin m) (v : Fin n) : ℝ :=
  (if G.target e = v then q e else 0) - (if G.source e = v then q e else 0)

/-- Sequential source subtraction followed by target addition, matching the sparse kernel. -/
def edgeStep (G : Graph n m) (q : Fin m → ℝ) (state : Fin n → ℝ)
    (e : Fin m) : Fin n → ℝ :=
  fun v => (state v - (if G.source e = v then q e else 0)) +
    (if G.target e = v then q e else 0)

/-- The incidence-matrix/vector product, defined independently of the sparse fold. -/
def denseRate (G : Graph n m) (q : Fin m → ℝ) (v : Fin n) : ℝ :=
  ∑ e : Fin m, incidence G v e * q e

/-- Process edge indices `0, ..., m-1` once each, matching the C++ vector loop. -/
noncomputable def sparseFold (G : Graph n m) (q : Fin m → ℝ)
    (state : Fin n → ℝ) : Fin n → ℝ :=
  (List.finRange m).foldl (edgeStep G q) state

theorem edgeDelta_eq_incidence (G : Graph n m) (q : Fin m → ℝ)
    (e : Fin m) (v : Fin n) : edgeDelta G q e v = incidence G v e * q e := by
  by_cases ht : G.target e = v <;> by_cases hs : G.source e = v <;>
    simp [edgeDelta, incidence, ht, hs]

theorem edgeStep_apply (G : Graph n m) (q : Fin m → ℝ)
    (state : Fin n → ℝ) (e : Fin m) (v : Fin n) :
    edgeStep G q state e v = state v + edgeDelta G q e v := by
  dsimp [edgeStep, edgeDelta]
  ring

/-- Fold induction exposes the actual sequential loop rather than defining it as a sum. -/
theorem sparse_list_apply (G : Graph n m) (q : Fin m → ℝ)
    (order : List (Fin m)) (state : Fin n → ℝ) (v : Fin n) :
    order.foldl (edgeStep G q) state v =
      state v + (order.map (fun e => edgeDelta G q e v)).sum := by
  induction order generalizing state with
  | nil => simp
  | cons e tail ih =>
      rw [List.foldl_cons, ih]
      simp only [List.map_cons, List.sum_cons, edgeStep_apply]
      ring

/-- The sequential sparse update equals the independently defined dense incidence update. -/
theorem sparse_eq_dense (G : Graph n m) (q : Fin m → ℝ) (state : Fin n → ℝ) :
    sparseFold G q state = fun v => state v + denseRate G q v := by
  funext v
  unfold sparseFold
  rw [sparse_list_apply, ← Fin.sum_univ_def]
  simp only [denseRate, edgeDelta_eq_incidence]

/-- Exact-arithmetic equivalence is independent of the complete edge enumeration's order. -/
theorem sparse_permuted_eq_dense (G : Graph n m) (q : Fin m → ℝ)
    (order : List (Fin m))
    (complete : order.Perm (List.finRange m))
    (state : Fin n → ℝ) :
    order.foldl (edgeStep G q) state = fun v => state v + denseRate G q v := by
  funext v
  rw [sparse_list_apply]
  have mapped := complete.map (fun e => edgeDelta G q e v)
  rw [mapped.sum_eq, ← Fin.sum_univ_def]
  simp only [denseRate, edgeDelta_eq_incidence]

/-- Every internal transfer is included once with opposite signs. -/
theorem edgeDelta_total_zero (G : Graph n m) (q : Fin m → ℝ) (e : Fin m) :
    (∑ v : Fin n, edgeDelta G q e v) = 0 := by
  classical
  simp [edgeDelta, Finset.sum_sub_distrib]

/-- Internal edge flows cannot change the sum of the storage rates. -/
theorem denseRate_total_zero (G : Graph n m) (q : Fin m → ℝ) :
    (∑ v : Fin n, denseRate G q v) = 0 := by
  simp only [denseRate, ← edgeDelta_eq_incidence]
  rw [Finset.sum_comm]
  simp only [edgeDelta_total_zero, Finset.sum_const_zero]

/-- The actual sparse loop preserves total volume for internal transfers. -/
theorem sparse_total_conserved (G : Graph n m) (q : Fin m → ℝ)
    (state : Fin n → ℝ) :
    (∑ v : Fin n, sparseFold G q state v) = ∑ v : Fin n, state v := by
  rw [sparse_eq_dense, Finset.sum_add_distrib, denseRate_total_zero, add_zero]

/-- One constant-rate Euler step, after accumulating external net inflow and internal edges. -/
noncomputable def graphEuler (G : Graph n m) (q : Fin m → ℝ)
    (external : Fin n → ℝ) (dt : ℝ) (state : Fin n → ℝ) : Fin n → ℝ :=
  fun v => state v + dt * sparseFold G q external v

/-- Only external net inflow/discharge changes total inventory in an Euler step. -/
theorem graphEuler_total_balance (G : Graph n m) (q : Fin m → ℝ)
    (external : Fin n → ℝ) (dt : ℝ) (state : Fin n → ℝ) :
    (∑ v : Fin n, graphEuler G q external dt state v) =
      (∑ v : Fin n, state v) + dt * ∑ v : Fin n, external v := by
  unfold graphEuler
  rw [Finset.sum_add_distrib, ← Finset.mul_sum, sparse_total_conserved]

end Breakwater
