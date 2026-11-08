module

public import Breakwater.Network
public import Breakwater.ControlInfluence
meta import Lean.Util.CollectAxioms
meta import Lean.Meta.InferType

/-! Build-visible transitive assumption reports for the delivered algorithm results. -/

#print axioms Breakwater.sparse_eq_dense
#print axioms Breakwater.sparse_permuted_eq_dense
#print axioms Breakwater.denseRate_total_zero
#print axioms Breakwater.sparse_total_conserved
#print axioms Breakwater.graphEuler_total_balance
#print axioms Breakwater.Affine.repeatedEuler_closed_form
#print axioms Breakwater.Affine.variableEuler_closed_form
#print axioms Breakwater.Affine.timeSteps_sum
#print axioms Breakwater.Affine.variableEuler_on_grid
#print axioms Breakwater.Affine.scalar_endpoint_bounds
#print axioms Breakwater.Affine.trajectory_bounds
#print axioms Breakwater.Affine.trajectory_zero_horizon_bounds
#print axioms Breakwater.network_rollout_affine
#print axioms Breakwater.network_rollout_total_balance
#print axioms Breakwater.network_interval_bounds
#print axioms Breakwater.ControlInfluence.weightedFold_eq_sum
#print axioms Breakwater.ControlInfluence.weighted_candidate_decomposition
#print axioms Breakwater.ControlInfluence.precomputed_eq_direct
#print axioms Breakwater.ControlInfluence.precomputed_zero_delta
#print axioms Breakwater.ControlInfluence.precomputed_network_rate
#print axioms Breakwater.ControlInfluence.precomputed_network_trajectory

/-- The advertised results are checked transitively, not just scanned for placeholders. -/
public meta def Breakwater.auditedAlgorithms : Array Lean.Name := #[
  ``Breakwater.sparse_eq_dense,
  ``Breakwater.sparse_permuted_eq_dense,
  ``Breakwater.denseRate_total_zero,
  ``Breakwater.sparse_total_conserved,
  ``Breakwater.graphEuler_total_balance,
  ``Breakwater.Affine.repeatedEuler_closed_form,
  ``Breakwater.Affine.variableEuler_closed_form,
  ``Breakwater.Affine.timeSteps_sum,
  ``Breakwater.Affine.variableEuler_on_grid,
  ``Breakwater.Affine.scalar_endpoint_bounds,
  ``Breakwater.Affine.trajectory_bounds,
  ``Breakwater.Affine.trajectory_zero_horizon_bounds,
  ``Breakwater.network_rollout_affine,
  ``Breakwater.network_rollout_total_balance,
  ``Breakwater.network_interval_bounds,
  ``Breakwater.ControlInfluence.weightedFold_eq_sum,
  ``Breakwater.ControlInfluence.weighted_candidate_decomposition,
  ``Breakwater.ControlInfluence.precomputed_eq_direct,
  ``Breakwater.ControlInfluence.precomputed_zero_delta,
  ``Breakwater.ControlInfluence.precomputed_network_rate,
  ``Breakwater.ControlInfluence.precomputed_network_trajectory
]

-- These are the standard foundations of the pinned classical-real Mathlib development.
-- Any additional transitive assumption makes the default project build fail.
public meta def Breakwater.auditAlgorithmTheorems
    (declarations : Array Lean.Name) : Lean.Elab.Command.CommandElabM Unit := do
  let allowed : Array Lean.Name := #[``propext, ``Classical.choice, ``Quot.sound]
  for decl in declarations do
    let info ← Lean.getConstInfo decl
    -- Public module exports can hide theorem bodies as opaque declarations.
    -- Check the proposition type rather than relying on the storage constructor.
    let isProposition ← Lean.Elab.Command.liftTermElabM (Lean.Meta.isProp info.type)
    unless isProposition do
      throwError "Audited algorithm does not have a proposition type: {decl}"
    let dependencies ← Lean.collectAxioms decl
    for dependency in dependencies do
      unless allowed.contains dependency do
        throwError "Unapproved transitive assumption in {decl}: {dependency}"
  Lean.logInfo m!"BREAKWATER assumption audit passed for {declarations.size} algorithm theorems"

run_cmd Breakwater.auditAlgorithmTheorems Breakwater.auditedAlgorithms
