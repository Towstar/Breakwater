# Person A kickoff: PDE, simulation and science

Copy the following prompt into Person A's coding session.

```text
You are implementing Person A's workstream for BREAKWATER. I am the project owner and
the PDE/simulation developer. Continue the existing C++20/MSVC numerical foundation.

STATUS AFTER PR #14: A3–A8, including the agreed scientific worker mapping, are merged.
Do not repeat those increments. The worker-mapping instructions below are an earlier
handoff; see science/WORKER.md and docs/STATUS.md for current evidence. B owns the next
deployment/desktop-adapter step; A continues science/proof maintenance.

REPOSITORY
https://github.com/Towstar/Breakwater
Use a separate clone/worktree and branch codex/person-a-engine from current origin/main.
Inspect git status first and preserve existing work. Do not change another session's branch.

READ FIRST
AGENTS.md
docs/PARALLEL_WORK.md
docs/STATUS.md
docs/CONTINUATION_PLAN.md
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
Main includes generic fixed-flow storage networks, dense/sparse/affine forecasts,
independent validation, bounded search and authoritative single-pipe finite-volume
transport. Native Nemotron tools and the editor are integrated. See engine/SEARCH.md,
engine/TRANSPORT.md and engine/PROGRESS.md for APIs and actual evidence.
This workstream now includes A3 differentiable storage, A4's fixed-case PhysicsNeMo
smoke, A5's recorded benchmarks and A6's checked exact-real algorithms through 7c99c68.
B3 contracts and B4 native viewing are implemented downstream. Worker science mapping remains pending.
Confirm the baseline on this machine rather than treating old results as your own.

NEXT INCREMENT: AGREED WORKER MAPPING AND PROOF MAINTENANCE
A6 already supplies pinned Lean/Mathlib finite-graph proofs and audited assumptions.
Preserve their explicit scope and C++ correspondence as algorithms change; do not
repeat A3–A6 or claim compiled IEEE arithmetic, PDE, or ML qualification from them.

Keep C++ authoritative for feasibility. Do not differentiate through clipping or claim
gradients through discrete candidate search. Review docs/SCIENCE_CONTRACTS.md and
contracts/science/ with B before mapping A3 into worker methods. The private native
fixture JSON is not the public project/wire format. Keep health-v1 compatible; advertise
only executed implementations. Do not build B's process adapter or change shared contracts.

NEXT INCREMENTS
Follow CONTINUATION_PLAN.md for later science integration. Retain A3/A4/A5's executed
evidence and limitations. A4 is fixed-case experimental evidence, not arbitrary-route
quality inference. Each new increment is a separate tested PR. Use the existing native
transport reference, consistent cell averages and signed boundary-mass ledger.
Do not start a broad hydraulic engine or GPU rewrite.

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
