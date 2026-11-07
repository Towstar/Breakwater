# Lean workstream: planned

The minimum base supplies executable C++ reference/sparse/affine implementations and
agreement/conservation tests. No Lean proof has been authored or checked yet.

Next Person A increment:

1. Pin a compatible Lean/Mathlib project and record `lake build` evidence.
2. Formalize valid finite storage graphs and the signed incidence update.
3. Prove sparse-update equivalence and internal transfer conservation.
4. Prove constant-rate rollout and endpoint-bound results.
5. Audit theorem dependencies; no `sorry` or unreviewed extra axioms.

These future exact-arithmetic proofs will not automatically establish IEEE rounding,
compiled C++ memory safety, hydraulic accuracy, or global optimizer optimality.
