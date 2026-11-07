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
