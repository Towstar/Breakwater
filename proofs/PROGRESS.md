# Person A proof evidence

## A10a — discrete FVM mass balance (November 7, 2026)

Implemented on `codex/person-a-engine` from integrated main `7785d1d`. New work is
confined to `proofs/`, three native transport regression examples and the A-owned
transport/science notes. Person B's application, contracts, root build/solution and
shared `docs/` are unchanged from that baseline. Earlier A6/A7 records below remain
historical evidence rather than the current theorem count.

`Breakwater/Transport.lean` delivers **16 new checked statements**, preserving the
existing 21 storage/control-influence statements for **37 audited results**:

1. Finite shared-face cancellation and the concentration/inventory sum conversion.
2. One explicit Euler step's signed inlet/outlet/source/old-inventory decay balance,
   including `1000 * area * dx` litres and `1000 * area` boundary-flux conversion.
3. Native open half-cell inlet, upwind/diffusive interior and advective outlet laws;
   closed zero-exchange boundaries with zero-velocity interior diffusion.
4. A strict negative-inlet-flux result for zero flow, positive dispersion/width and
   a higher first-cell concentration than the prescribed inlet.
5. Actual recursive state-dependent variable-step rollout, an independently defined
   four-term cumulative ledger, preserved residual, zero initial-ledger residual
   and telescoped final inventory balance. Per-step conservation is derived, not
   assumed as a rollout hypothesis.

The [formal/native mapping](FVM_MASS_BALANCE.md) catalogs every theorem, equation,
unit, finite-index domain and scope limit. Generic cancellation permits empty
cells; boundary constructors are nonempty. Native at-least-two-cell inputs are
included. The mass conversion requires `dx != 0`; CFL, nonnegative flux and fixed
step/rates are not required for this exact-real identity. Native event-shortened
steps are covered as a sequence; scheduler construction itself is not proved.

Actual checks on November 7, 2026:

```powershell
./proofs/verify.ps1 -LeanBin 'C:/Projects/Breakwater/.tools/lean-4.35.0-windows/bin'
```

- Direct new-module compilation with warnings as errors passed before integration.
- Final `lake build` completed successfully (**1,062 jobs**), checking the new
  transport module, combined audit and root library with cached dependencies reused.
- Fresh direct audit passed all **37 proposition-typed results**. All reported
  transitive assumptions are the unchanged standard foundations `propext`,
  `Classical.choice`, `Quot.sound`; the allowlist was not expanded.
- The owned-source prohibited-token scan, Lean/Mathlib pin checks and deliberate
  unapproved-assumption rejection passed. The full verifier exited **0**.
- Lean remains **4.35.0**, Mathlib remains
  `56a005855ea17e0b0958f73ec801ae140b0a1679`, and the nine resolved dependency pins
  are unchanged. Proof verification ran 02:22:34–02:24:28 UTC on November 8
  (November 7 in America/Chicago); exact timestamps/hashes are in the record.
- The targeted MSVC transport suite passed **21/21 cases** normally (**3.38 s**)
  and with AddressSanitizer (**21.16 s**), no sanitizer report. Three new dyadic,
  hand-computed examples check signed open exchange/nonunit geometry, closed
  interior cancellation, and unequal event steps with old-inventory decay.

Committed evidence:

- [Lake build](evidence/2026-11-07-a10a-lake-build.log).
- [Fresh transitive assumption audit](evidence/2026-11-07-a10a-axiom-audit.log).
- [Expected unapproved-assumption rejection](evidence/2026-11-07-a10a-negative-audit.log).
- [Proof versions, pins and checked-source hashes](evidence/2026-11-07-a10a-verification.json).
- [Native regression log](evidence/2026-11-07-a10a-native-transport.log),
  [ASan regression log](evidence/2026-11-07-a10a-asan-transport.log) and
  [native source/executable hashes and scope](evidence/2026-11-07-a10a-native-verification.json).

This is an exact-real discrete conservation/correctness result, not a proof of
PDE accuracy, positivity, IEEE arithmetic, compensated summation/tolerances, C++
memory safety, event selection, hydraulic fidelity, optimization feasibility or
global optimality. The solver and public contracts are unchanged. No new full
desktop/Python suite, learned training, transport accuracy qualification or benchmark
is claimed. A10b positivity and the separate front-accuracy study remain later work.

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
