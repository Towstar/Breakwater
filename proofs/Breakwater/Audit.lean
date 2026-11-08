module

public import Breakwater.Network
public import Breakwater.ControlInfluence
public import Breakwater.Transport
public import Breakwater.TransportNonnegativity
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
#print axioms Breakwater.Transport.inventory_eq_scale_sum
#print axioms Breakwater.Transport.face_differences_telescope
#print axioms Breakwater.Transport.euler_sum_balance
#print axioms Breakwater.Transport.euler_inventory_balance
#print axioms Breakwater.Transport.openFlux_inlet
#print axioms Breakwater.Transport.openFlux_outlet
#print axioms Breakwater.Transport.openFlux_interior
#print axioms Breakwater.Transport.closedFlux_inlet
#print axioms Breakwater.Transport.closedFlux_outlet
#print axioms Breakwater.Transport.closedFlux_interior
#print axioms Breakwater.Transport.open_euler_inventory_balance
#print axioms Breakwater.Transport.closed_euler_inventory_balance
#print axioms Breakwater.Transport.zero_flow_inlet_loss
#print axioms Breakwater.Transport.rollout_residual_preserved
#print axioms Breakwater.Transport.rollout_mass_residual_zero
#print axioms Breakwater.Transport.rollout_inventory_balance

#print axioms Breakwater.Transport.open_first_expansion
#print axioms Breakwater.Transport.open_last_expansion
#print axioms Breakwater.Transport.open_interior_expansion
#print axioms Breakwater.Transport.closed_first_expansion
#print axioms Breakwater.Transport.closed_last_expansion
#print axioms Breakwater.Transport.closed_interior_expansion
#print axioms Breakwater.Transport.dimensionless_numbers_nonnegative
#print axioms Breakwater.Transport.open_budget_identity
#print axioms Breakwater.Transport.closed_budget_identity
#print axioms Breakwater.Transport.open_coefficient_bounds
#print axioms Breakwater.Transport.closed_coefficient_bounds
#print axioms Breakwater.Transport.open_euler_nonnegative
#print axioms Breakwater.Transport.closed_euler_nonnegative
#print axioms Breakwater.Transport.native_step_nonnegative
#print axioms Breakwater.Transport.native_rollout_nonnegative
#print axioms Breakwater.Transport.native_rollout_nonnegative_mass_balance
#print axioms Breakwater.Transport.interior_only_bound_counterexample

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
  ``Breakwater.ControlInfluence.precomputed_network_trajectory,
  ``Breakwater.Transport.inventory_eq_scale_sum,
  ``Breakwater.Transport.face_differences_telescope,
  ``Breakwater.Transport.euler_sum_balance,
  ``Breakwater.Transport.euler_inventory_balance,
  ``Breakwater.Transport.openFlux_inlet,
  ``Breakwater.Transport.openFlux_outlet,
  ``Breakwater.Transport.openFlux_interior,
  ``Breakwater.Transport.closedFlux_inlet,
  ``Breakwater.Transport.closedFlux_outlet,
  ``Breakwater.Transport.closedFlux_interior,
  ``Breakwater.Transport.open_euler_inventory_balance,
  ``Breakwater.Transport.closed_euler_inventory_balance,
  ``Breakwater.Transport.zero_flow_inlet_loss,
  ``Breakwater.Transport.rollout_residual_preserved,
  ``Breakwater.Transport.rollout_mass_residual_zero,
  ``Breakwater.Transport.rollout_inventory_balance,
  ``Breakwater.Transport.open_first_expansion,
  ``Breakwater.Transport.open_last_expansion,
  ``Breakwater.Transport.open_interior_expansion,
  ``Breakwater.Transport.closed_first_expansion,
  ``Breakwater.Transport.closed_last_expansion,
  ``Breakwater.Transport.closed_interior_expansion,
  ``Breakwater.Transport.dimensionless_numbers_nonnegative,
  ``Breakwater.Transport.open_budget_identity,
  ``Breakwater.Transport.closed_budget_identity,
  ``Breakwater.Transport.open_coefficient_bounds,
  ``Breakwater.Transport.closed_coefficient_bounds,
  ``Breakwater.Transport.open_euler_nonnegative,
  ``Breakwater.Transport.closed_euler_nonnegative,
  ``Breakwater.Transport.native_step_nonnegative,
  ``Breakwater.Transport.native_rollout_nonnegative,
  ``Breakwater.Transport.native_rollout_nonnegative_mass_balance,
  ``Breakwater.Transport.interior_only_bound_counterexample
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
