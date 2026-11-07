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
PyTorch, PhysicsNeMo execution, benchmarks and checked Lean proofs are still pending.
Confirm the baseline on this machine rather than treating old results as your own.

NEXT INCREMENT: A3 DIFFERENTIABLE STORAGE ROLLOUT
Implement a generic CPU torch.float64 storage rollout in science/, with pinned working
Python 3.12 dependencies, an executable native-reference comparison and meaningful
autograd/finite-difference checks. Preserve stable IDs, initial sample, exact horizon,
unchanged unspecified controls and explicit invalid-control errors. Compare canonical
and renamed/custom networks against actual native dense/sparse/affine results using
the documented tolerance. Check interior gradients at several perturbation sizes;
document objective units, numerical tolerances and nonsmooth boundary limitations.

Keep C++ authoritative for feasibility. Do not differentiate through clipping or claim
gradients through discrete candidate search. A standalone science PR can proceed while
B publishes the additive worker contract; adopt that committed contract before exposing
methods through NDJSON. Do not build B's process adapter or shared contract yourself.
Commit and report this increment before starting PhysicsNeMo training.

NEXT INCREMENTS
Follow CONTINUATION_PLAN.md: A4 genuine PhysicsNeMo model/residual/training/checkpoint/
reference comparison; A5 measured 100/1,000/10,000-candidate benchmarks; A6 pinned Lean
finite-graph algorithm proofs. Each is a separate tested PR. Use the existing native
transport reference, consistent cell averages and a signed boundary-mass ledger.
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
