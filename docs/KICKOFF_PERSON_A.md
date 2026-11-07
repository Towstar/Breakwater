# Person A kickoff: PDE, simulation and science

Copy the following prompt into Person A's coding session.

```text
You are implementing Person A's workstream for BREAKWATER. I am the project owner and
the PDE/simulation developer. Continue the existing C++20/MSVC numerical foundation.

REPOSITORY
https://github.com/Towstar/Breakwater
Use a separate clone/worktree and branch codex/person-a-engine from current origin/main.
Inspect git status first and preserve existing work. Do not change another session's branch.

READ FIRST
AGENTS.md
docs/PARALLEL_WORK.md
docs/STATUS.md
docs/IMPLEMENTATION_PLAN.md
For Dev Agents.md, Part A
contracts/README.md
engine/include/breakwater/engine.hpp and the existing numerical tests

OWNERSHIP
You may edit engine/, science/, proofs/ and scenarios/, including their local tests,
build files, scientific scripts and dependency locks. Do not edit desktop/, contracts/,
root build/configuration, root scripts, Breakwater.slnx, README or shared docs/.
Put your evidence in engine/PROGRESS.md and science/proof-local notes. Person B updates
shared documentation and integrates the two branches.

CURRENT STATE
The engine already supports generic fixed-flow storage networks, immutable candidate
compilation, dense/sparse/affine forecasts, a control catalog and independent validation.
The canonical cases and native ASan checks have passed on the original machine.
Search, transport, PyTorch, PhysicsNeMo and Lean proofs are still planned.
Confirm the baseline on this machine rather than treating old results as your own.

FIRST INCREMENT
Implement bounded generic candidate evaluation/search in engine/, preserving existing
APIs and shared types. Use an additive engine-owned header for new native search types.
Accept catalog control IDs and explicit candidate values; bound search to four varying
controls and 10,000 combinations. Keep unspecified controls unchanged and all candidates
on the same immutable incident state. Use affine endpoints to screen candidates, then
independently reference-simulate and validate recommendations. Rank actual feasible
candidates by computed cost and return an explicit no-feasible-plan result.

Test arbitrary asset/control IDs, invalid and unavailable controls, candidate limits,
concurrent isolation, cost ordering and no-feasible-plan behavior. Preserve the existing
analytic outcomes and avoid global-optimality claims. Add an executable usage example.
Commit and report this working increment before taking on the whole science roadmap.

NEXT INCREMENTS
Implement authoritative single-pipe finite-volume transport with boundary-flux accounting
and stable stepping; then PyTorch rollout/gradient checks and genuine PhysicsNeMo
execution/evaluation. Pin a real Lean/Mathlib project and check the agreed kernel proofs.
Keep these as separate tested increments. Do not start a broad hydraulic engine or GPU rewrite.

BOUNDARIES AND VALIDATION
Keep the engine independent of Qt, Python and model APIs. Preserve units, time-zero
semantics and capacity equality. Never clip predicted volumes or silently relax bounds.
Do not alter canonical fixture meanings or existing shared contracts without coordination.
Request root dependency/contract changes from B; do not make them yourself.
Use scripts/build.ps1 -Preset headless and -Preset asan. Run relevant Python tests when
scientific methods are added. Generating a Visual Studio solution rewrites a B-owned file;
do not include that generated change in your commits.

DELIVERY
Push only your feature branch and open a PR for B to integrate. Report commit SHA,
touched files, interface additions, actual tests/results, limitations and a usage example.
No force pushes, direct feature pushes to main, or fabricated proof/model/benchmark claims.
Start with inspection and baseline checks, then implement the first increment.
```
