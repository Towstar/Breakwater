# Person A proof evidence

## A6 — checked algorithms (November 7, 2026)

Implemented on `codex/person-a-engine`, after A5 commit `eca890d`. Every incremental
change is under `proofs/`. Person B's files and the native/Python implementations
remain untouched. The root checkout stays on its unchanged `main` revision.

Delivered:

1. Finite directed multigraph with bounded source/target indices, an actual sequential
   source-subtract/target-add loop, and an independently defined dense incidence sum.
   `sparse_eq_dense` proves equality; `sparse_permuted_eq_dense` covers complete edge
   permutations without changing exact-real results.
2. Internal cancellation, full sparse-loop total conservation, one-step external
   balance and repeated graph-step total external-flow balance.
3. Actual recursive fixed/variable-step Euler rollouts and affine closed forms;
   telescoping consecutive time differences includes a shortened final sample step.
4. Scalar and finite-coordinate interval bounds from endpoint bounds, with arbitrary
   signed rates, capacity equality and an explicit zero-horizon corollary.
5. Network-level composition and a default-build transitive assumption guard for
   **15 advertised algorithm theorems**, plus a documented mapping to the C++ kernels.

Pins actually installed/resolved:

- Lean 4.35.0, x86_64 Windows GNU Release, commit
  `eb6cafb7290e8bd5f2937522d8d804f58f02bafc`.
- Lake `5.0.0-src+eb6cafb`, shipped in that verified archive.
- Mathlib `56a005855ea17e0b0958f73ec801ae140b0a1679` (`v4.35.0`), with all nine resolved
  Git dependency revisions recorded in `lake-manifest.json`.
- Official Windows ZIP SHA256 verified as
  `5fc033e93e1a7bc59c5ebf311103d2c9cd18d4b3eddda4ffa3414e8adb0ef79e`.
- Partial Mathlib cache fetched 1,039 transitive import artifacts; no full-library
  source build or unfiltered cache was needed. Toolchains/dependencies/cache stay ignored.

Actual verification, not a proposed proof sketch:

- The initial `lake build` compiled the graph, affine and network proofs and reported
  success. Lean 4.35 module-format import warnings were then removed using the pinned
  public/exposed module format.
- Final `lake build` **completed successfully (1,060 jobs)**. Graph, affine, network,
  audit and root modules are checked; unchanged dependencies were reused where cached.
  The final build/audit logs contain no proof errors or module-format warnings.
- `lake env lean -DwarningAsError=true -DautoImplicit=false Breakwater/Audit.lean`
  independently re-executed the audit and passed all 15 proposition-typed results.
- Every reported transitive axiom set is exactly
  `[propext, Classical.choice, Quot.sound]`. These are explicitly reviewed standard
  foundations; no custom assumptions or placeholder dependency is accepted.
- A generated ignored-file theorem depending on an unapproved assumption was rejected
  by the same audit function for that specific assumption. The verifier consumes that
  expected failure and returns success only after all positive/negative checks pass.
- Final `proofs/verify.ps1 -LeanBin <pinned bin>` exited **0**; source token checks,
  pinned-version/manifest checks, build, direct audit and rejection regression passed.
  Final evidence interval: 2026-11-07 23:09:00–23:10:00 UTC.

Committed run evidence:

- [Lake build](evidence/2026-11-07-lake-build.log).
- [Transitive axiom report](evidence/2026-11-07-axiom-audit.log).
- [Expected rejection](evidence/2026-11-07-negative-audit.log).
- [Versions, pins and source hashes](evidence/2026-11-07-verification.json).

The verifier was adapted to Lean 4.35's module exports: public proof bodies can appear
as opaque declarations, so the guard checks proposition types and exported transitive
assumption metadata rather than relying on an internal theorem-storage constructor.
This does not weaken the assumption allowlist. Proof source/hash evidence identifies
the actual files checked; documentation was added afterward.

Scope and handoff:

- The proofs use exact real arithmetic, valid finite endpoints and captured constant
  graph/flows/external rates. Initial/final storage bounds are explicit hypotheses;
  no interior bound or conservation conclusion is assumed.
- Parallel edges and self-loops are handled algebraically. The engine's stricter
  physical-input subset, nonnegative flows and increasing times fit that model.
- No IEEE error bound, nonfinite arithmetic behavior, compiled C++ memory safety,
  parser/index-construction proof, hydraulic/PDE correctness, cost/control/destination
  constraints, ML quality or global optimality follows from these results.
- The C++ correspondence is a manual reviewed mapping, not verified extraction or
  compiler refinement. Existing native tests, independent validation, ASan evidence
  and measured benchmarks remain separate checks.
- [README.md](README.md) gives installation/build commands, theorem assumptions and
  per-function native mapping. No root build integration is required for this standalone
  Lean project; Person B can incorporate this evidence into shared status at integration.

## Precomputed control influence — November 7, 2026

Implemented in `Breakwater/ControlInfluence.lean` on `codex/person-a-engine`, from
integrated baseline `5c47d4f`. The previous A6 results and dependency pins remain.

The new direct evaluator traverses every finite route, forms its affine candidate
flow, and sequentially adds its weighted contribution to each output. The optimized
evaluator traverses selected controls and sequentially applies precomputed columns.
Baseline totals and each column are themselves defined by route folds. Their
equality follows from actual fold induction and finite-sum distributivity/interchange,
rather than an equivalence assumption or identical top-level definitions.

Six additional advertised results are now part of the default-build audit:

1. `weightedFold_eq_sum`: actual weighted route traversal equals its finite sum.
2. `weighted_candidate_decomposition`: the direct weighted route sum decomposes
   into baseline plus selected-control influence columns.
3. `precomputed_eq_direct`: prepared-column evaluation equals route recomputation,
   including direct control terms such as source-curtailment synthetic cost.
4. `precomputed_zero_delta`: unchanged selected controls reproduce the baseline.
5. `precomputed_network_rate`: incidence-weight columns equal the original sparse
   finite-graph rate for the same candidate flows and fixed external terms.
6. `precomputed_network_trajectory`: substituting the prepared rate preserves every
   exact-real affine sample, including the endpoint used for screening.

Arbitrary finite dimensions and signed real weights/changes are supported, including
empty selected-control sets. Tank rates, shared-destination totals, incident flow
magnitudes and hourly cost can all be output coordinates of the generic theorem.
The native sparse `+1` route-setpoint and `-1` source-feed-curtailment coefficients
are a supported specialization. Baseline, coefficient matrix and weights must all
belong to the same captured network/control selection.

Actual checks, November 7, 2026 (23:25:47–23:27:05 UTC):

```powershell
.\proofs\verify.ps1 -LeanBin 'C:\Projects\Breakwater\.tools\lean-4.35.0-windows\bin'
```

- The new module first passed a direct warning-as-error Lean check.
- `lake build` then completed successfully with **1,061 jobs**, including the new
  module, composed audit and root library. No proof errors or warnings were reported.
- The independent direct audit passed **21 algorithm theorems**, preserving all
  original 15 and adding the six above.
- All six new results report exactly `[propext, Classical.choice, Quot.sound]`, the
  unchanged standard foundational allowlist. Owned-source placeholder/unchecked-token
  checks passed.
- The deliberate unapproved-assumption probe was rejected for the expected reason.
- `verify.ps1` exited **0** and recorded source hashes, versions, pins and probe status.

New evidence is retained separately from the historical A6 run:

- [Lake build](evidence/2026-11-07-control-influence-lake-build.log).
- [Transitive assumption report](evidence/2026-11-07-control-influence-axiom-audit.log).
- [Expected rejection](evidence/2026-11-07-control-influence-negative-audit.log).
- [Versions, pins and checked-source hashes](evidence/2026-11-07-control-influence-verification.json).

These are exact-real algorithm equivalence results. No C++ extraction/refinement,
IEEE rounding bound, finite-input validation, ranking/tie behavior, feasibility
classification, memory safety, optimization speed or global optimum is formally
proved by this increment. The C++ correspondence and numerical guard/fallback behavior
require independent implementation tests; performance requires measured benchmarks.
