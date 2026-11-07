# BREAKWATER — For Dev Agents

Consolidated engineering handoff, prepared November 7, 2026.

**Read Part A first. Part B preserves the complete original transfer document verbatim.** This file contains both the original proposal and the decisions, corrections, recommendations, and observed repository state from the subsequent conversation. Do not mistake proposed features, example outputs, code stubs, or acceptance targets for implemented functionality.

## Navigation

- [A.1 — Authority, user intent, and confirmed context](#a1--authority-user-intent-and-confirmed-context)
- [A.2 — Observed repository and environment state](#a2--observed-repository-and-environment-state)
- [A.3 — Product, scope, and unresolved requirements](#a3--product-scope-and-unresolved-requirements)
- [A.4 — Two-person ownership and conflict prevention](#a4--two-person-ownership-and-conflict-prevention)
- [A.5 — Contracts and integration boundary](#a5--contracts-and-integration-boundary)
- [A.6 — Scenario corrections and analytical checks](#a6--scenario-corrections-and-analytical-checks)
- [A.7 — Immediate work, background checks, and dependencies](#a7--immediate-work-background-checks-and-dependencies)
- [A.8 — Execution schedule and cutoffs](#a8--execution-schedule-and-cutoffs)
- [A.9 — C++, Lean, and proof-backed optimization](#a9--c-lean-and-proof-backed-optimization)
- [A.10 — Windows compilers, memory checking, and development checks](#a10--windows-compilers-memory-checking-and-development-checks)
- [A.11 — Benchmarks and numerical validation](#a11--benchmarks-and-numerical-validation)
- [A.12 — NVIDIA integration requirements](#a12--nvidia-integration-requirements)
- [A.13 — Acceptance evidence and truthful status](#a13--acceptance-evidence-and-truthful-status)
- [A.14 — Suggested assignments for implementation sessions](#a14--suggested-assignments-for-implementation-sessions)
- [A.15 — Reference provenance and preservation](#a15--reference-provenance-and-preservation)
- [Part B — Complete original transfer document](#part-b--complete-original-transfer-document)

---

# Part A — Conversation addendum and current handoff

## A.1 — Authority, user intent, and confirmed context

The original user request was to assess the transfer document and work out a two-person split, prerequisites, immediate work, and integration strategy. The instructions embedded in the attached transfer document were reference material for that planning request, not authorization to execute its implementation kickoff. The user subsequently explicitly requested local repository initialization, discussed C++/Lean/performance/memory checking, and requested this comprehensive Markdown handoff.

Confirmed user context:

- There are **two human developers**. Both are strong in mathematics.
- AI coding assistance is expected to perform most implementation work. Humans should direct the work, review its evidence, check mathematical assumptions, and rehearse the product.
- **Minimizing merge conflicts is a primary preference.** Organize work by exclusive file ownership and stable interfaces rather than overlapping feature edits.
- **Both developers use Windows.** MSVC is the likely compiler, but other compilers may be used. Do not assume Linux, WSL, Docker, Clang, a particular Visual Studio installation, or GPU access is working merely because a command exists.
- At the time of the planning clarification, the user said the 24-hour clock had started **one hour earlier**. The ensuing schedule assumed roughly 23 hours remained **at that point**. That is a historical reference, not a live countdown or a known absolute deadline. Recalculate remaining time from the real event deadline before scheduling new work.
- The working directory is `C:\Projects\Breakwater`.
- The user explicitly authorized initializing a local Git repository. That was completed.
- The user is interested in Lean formalization connected to speedups and asked whether a C++ foundation and memory checkers should be used. This establishes an engineering interest; it does not establish that a native kernel or Lean proof has already been commissioned, implemented, compiled, or benchmarked.

How to use this handoff:

1. Current explicit user instructions and applicable repository instructions govern the active task.
2. Part A records the latest conversation context and recommended planning baseline. It updates the original proposal where noted, particularly API ownership, Windows tooling, fixture caveats, and the bounded C++/Lean experiment.
3. Part B is the complete original design and implementation proposal. Its imperative kickoff is preserved as source text; reading this file does not itself require beginning every listed task.
4. Items labeled **confirmed**, **observed**, **analytical**, **recommended**, **proposed**, **experimental**, or **unresolved** have different evidentiary status. Preserve those distinctions.
5. When an implementation task refers to reading `BREAKWATER_CODEX_TRANSFER.md`, Part B contains that complete document. The original external source path and hash are recorded below.

## A.2 — Observed repository and environment state

Observed at handoff preparation:

| Item | Actual state |
|---|---|
| Local repository | Initialized in `C:\Projects\Breakwater\.git` |
| Initial branch | `main` |
| Commits | None |
| Remote | None configured |
| Existing application | None; the project directory was empty before initialization |
| Existing project files before this handoff | `.gitignore` only |
| This request's added artifact | `For Dev Agents.md` |
| Implemented simulator/API/frontend/agent/optimizer/PINN/native kernel/proof | None verified or present at the start of this handoff |
| Tests and scientific benchmarks | None implemented or executed |
| Initial application commit or GitHub repository | Not created |

The `.gitignore` excludes local `.env` files, Python environments/caches, JavaScript dependencies/build output, native/Lean build output, and generated scientific checkpoints. It intentionally allows `.env.example`, scenario JSON, and small demo-run fixtures. Representative ignore behavior was checked. Do not put API keys into tracked configuration or frontend code.

Environment discovery on the first developer's agent session found Git, Node/npm, uv, WSL, and Docker commands on PATH. The `python` command resolved to the WindowsApps alias; `py` and `nvidia-smi` were not found on PATH. An attempt to inspect uv-managed Python installations failed because the sandbox could not persist an interpreter-cache file. WSL enumeration and GPU/CIM inspection were denied by the agent's environment.

**Interpretation:** Python versions, CUDA support, GPU hardware, Docker operation, installed WSL distributions, C++ compilers, and usable scientific dependencies remain unverified. Those restricted inspections do not establish that Python, WSL, or an NVIDIA GPU is absent. Do not repeat permission-sensitive discovery endlessly or claim a missing GPU from a missing PATH entry. Verify the actual development machines with the available permissions when implementation requires it.

Do not infer that the Python path mentioned by a failed discovery command is a project-approved interpreter. Select and pin a compatible project interpreter deliberately.

## A.3 — Product, scope, and unresolved requirements

BREAKWATER is a **synthetic industrial fluid-infrastructure decision-support digital twin**, not a coastal-erosion or storm-surge application. The story is a pump failure threatening upstream storage, followed by investigation, simulation, rejection of a downstream-infeasible workaround, a better independently validated plan, and explicit simulated operator approval.

The minimum integrated product should demonstrate:

1. A recognizable network and explicit healthy operating settings.
2. Main-pump failure while production continues.
3. An authoritative quantitative forecast of time to a capacity limit.
4. A locally plausible response that fails downstream, with asset and time identified.
5. A nominally feasible mixed response from the same incident snapshot.
6. Genuine Nemotron tool calls whose results reference actual runs.
7. Comparison of cost, constraints, storage headroom, and forecast horizon.
8. Explicit operator approval before the displayed operating plan is applied in simulation.
9. A bounded PhysicsNeMo transport demonstration compared with a numerical reference, if required scope can be met.

The original priority ladder remains useful: deterministic model/validator/API/UI/agent first; differentiable controls and scientific ML next; broad platform work last. Preserve the small reusable PyTorch system interface without building a general-purpose PDE compiler or simulation framework.

**Unresolved scope ambiguity:** The original document calls a functioning PhysicsNeMo module a hard requirement, but other passages make it conditional on P0 and time. Confirm the actual judging requirements promptly. Plan a small genuine module and keep it outside the authoritative validation path. If mandatory integration fails, report incomplete scope or change the submission plan; do not silently redefine a hard requirement as optional or claim it worked.

Latest position on C++ and Lean:

- An earlier recommendation to cut all C++/Lean work was revised after the user raised proof-backed speedups as an interest.
- Design a replaceable numerical-kernel boundary now.
- Consider **one bounded kernel/proof/benchmark experiment**, with explicit time allocation and proof assumptions.
- Do not rewrite the API, UI, or agent infrastructure in C++ merely to support that experiment.
- Core correctness, integrated demo behavior, and any mandatory NVIDIA integrations retain priority. If a proof/performance feature becomes a firm requirement, acknowledge its budget and remove another stretch item.

The default cut list is still: broad C++ rewrites, extensive Lean verification, SpacetimeDB, physical hardware, FNO/PINO, Neural ODE correction, multi-agent distribution, and elaborate infrastructure. These remain source proposals or future work unless explicitly prioritized later.

## A.4 — Two-person ownership and conflict prevention

Recommended split: **Person A owns the numerical engine and science. Person B owns the application and is the sole integrator.** Both humans can review mathematics, but coding sessions should have disjoint write ownership.

This updates the original section 15 split by moving API and application-contract ownership to Person B. Keeping the simulator owner responsible for every API adjustment would make frontend and agent integration wait on the scientific workstream.

The following paths are proposed ownership boundaries, not existing modules or a request to scaffold empty files:

| Area | Sole write owner | Responsibilities |
|---|---|---|
| `backend/app/sim/**` | A | Generic ODE interface, reference solver, graph model, constraints, search, gradients, transport reference |
| `backend/app/ml/**` | A | PhysicsNeMo model, residuals, evaluation, readiness evidence |
| `scenarios/**` | A | Canonical fixtures, operating settings, stress cases, units |
| `native/**` | A | Optional native numerical kernel and native test harness |
| `proofs/**` | A | Optional pinned Lean/Mathlib project and checked theorems |
| `backend/tests/science/**` or clearly assigned science test files | A | Numerical, conservation, optimizer, gradient, transport, native equivalence tests |
| Training/evaluation/science benchmark scripts | A | Reproducible scientific artifacts and performance evidence |
| `docs/SCIENCE_VALIDATION.md` | A | Equations, assumptions, tolerances, proof scope, measured errors |
| `frontend/**` | B | Network display, timelines, controls, comparison, trace, approval, physics panel |
| `backend/app/api/**` | B | HTTP routing and adapters around engine functions |
| `backend/app/contracts/**` | B | Published HTTP/tool payload schemas and examples |
| `backend/app/agents/**` | B | NVIDIA client, tools, dispatcher, bounded orchestration |
| `backend/app/store/**`, startup and configuration | B | Immutable incident snapshots, runs, approval records, launch behavior |
| Application/API/agent/approval/end-to-end tests | B | Contracts, actual tool execution, reset, errors, approval state |
| Root configuration, dependency declarations and lockfiles | B | Consistent project setup; integrate A's dependency requests |
| README, API docs, demo script, launch/replay scripts | B | Reproducibility, presentation, fallback behavior |
| `AGENTS.md`, status, decisions, and this consolidated handoff | B after initial handoff | Single editor incorporating reports from both workstreams |

The initial shared baseline should contain only what both sessions need: package layout, a minimal environment, ownership rules, and an agreed engine interface with illustrative payloads. Do not create dozens of empty files from the original proposed tree.

Workflow:

1. Use one remote repository when one is configured, with separate local checkouts/worktrees and separate branches for the coding sessions. Do not have two sessions change branches or edit files in the same working tree.
2. Create the common baseline before divergent implementation. A usable shared baseline needs an initial commit; initialization alone does not provide one. No commit has been made yet.
3. Person B integrates small increments from A, normally every 45–60 minutes. Start integration with the first real result rather than waiting for complete subsystems.
4. Each increment reports the commit, touched files, interface changes, tests actually run, limitations, and an executable usage example.
5. A requests root dependency/lockfile changes; B makes them. The same applies to published contracts and shared documentation.
6. Change a public contract only after coordination, with its request/response examples updated in the same increment.
7. Rebalance by handing over an entire module and its ownership. Do not have both sessions opportunistically fix the same files.
8. Keep the integration branch runnable and the required tests green. Do not push independently to shared `main` without integration review.
9. A can review UI/API behavior and B can review equations without crossing write boundaries.

## A.5 — Contracts and integration boundary

Freeze a small scientific interface first. These are conceptual signatures; agree concrete dataclasses/Pydantic models and array shapes before implementation:

```text
simulate(scenario, incident_snapshot, controls, configuration) -> run
validate(run, declared_constraints, optional_stress_cases) -> validation_report
search(scenario, incident_snapshot, bounds, objective) -> ranked_candidates
transport(selected_flow_schedule, pipe_configuration, inlet) -> transport_run
```

Person A implements the scientific functions. Person B exposes them through HTTP and agent tools. Frontend and Nemotron must consume the same underlying engine; do not implement separate numerical truth in a UI slider handler or tool wrapper.

Keep engine internals independent of FastAPI, Nemotron, and frontend code. B can translate engine results into published API models. A owns internal scientific types; B owns external schemas. Coordinate their mapping once, rather than importing mutable application state into the solver.

Contracts must make the following explicit:

- Scenario, immutable incident/start-state identity, and canonical units.
- Actual main/backup pump setpoints and availability, rather than assuming all available equipment runs at maximum.
- Bounded controls: bypass, backup treatment discharge, trucking, production reduction, and horizon.
- Time convention: `times[0]` is the incident state; each trajectory sample is the state at that time.
- Run ID, scenario/start-state hashes, solver/version, parameters, seed, units, and status.
- Validation status, asset-specific violations, first limit-crossing time, numerical tolerances, and minimum margins.
- Whether quality checks were enabled, available, and validated. Disabled or unavailable checks are not a passing scientific result.
- Whether feasibility is nominal-only or robust over specified stress cases.
- Synthetic cost and forecast horizon, plus modeled limitations.
- Approval state distinct from recommendation or plan staging.

The minimal HTTP surface should support health/scenario retrieval, incident injection, simulation, validation, run retrieval, agent execution, and simulated approval. Add optimization and transport endpoints when their functions work. The fuller proposed route list and request/response examples are preserved in original section 13.

All what-if candidates must begin at the **same immutable incident-time state**. A candidate cannot mutate the canonical scenario, another candidate's state, or live displayed operating settings. Human approval is a separate state transition. Test concurrent candidates for state leakage.

Fixtures may unblock frontend work, but label fixture/replay results. Once the reference engine works, generate demo artifacts from it. No hand-authored result may be presented as an executed simulation or live tool call.

## A.6 — Scenario corrections and analytical checks

Use the original synthetic constants as a reproducible starting point:

| Quantity | Value |
|---|---:|
| Tank A capacity / initial inventory | 120 / 80 m³ |
| Tank B capacity / initial inventory | 130 / 114 m³ |
| A / B external inflow | 52 / 16 m³/h |
| Main A → treatment capacity | 55 m³/h, unavailable after incident |
| A → B bypass maximum | 25 m³/h |
| Backup B → treatment maximum | 35 m³/h |
| Combined treatment capacity | 60 m³/h |
| A trucking maximum | 22 m³/h |
| A production-reduction maximum | 15 m³/h |
| Forecast horizon | 3 h |
| Suggested graph step | 0.025–0.05 h |
| Trucking / curtailment cost | 80 / 250 synthetic cost units per m³ |

### Healthy-state consistency

Total external inflow is **68 m³/h**, exceeding treatment capacity by **8 m³/h**. Both tanks cannot remain indefinitely steady with treatment as the only external outlet and no production reduction. Do not display an indefinitely balanced healthy network using incompatible setpoints.

Before freezing the fixture, choose and document one convention:

- Preserve treatment capacity 60 and show temporary accumulation with explicit headroom and time horizon.
- Include at least 8 m³/h of additional trucking/production reduction for steady operation.
- Deliberately change the synthetic treatment capacity or source flows, record the decision, and update all tests and UI fixtures.

One **proposed, not yet adopted** bounded healthy preview is main discharge 44 and backup discharge 16 m³/h, with bypass/trucking/curtailment zero. Treatment totals 60; A accumulates 8 m³/h and reaches 104 m³ after three hours; B stays at 114 m³. This is feasible over the preview, not an indefinitely steady operating condition.

The original JSON fixture sets main-pump availability to zero and therefore represents an incident state. Use a separate healthy state or a properly implemented incident toggle. Reset must restore availability, setpoints, volumes, and approval/selection state consistently.

### Incident examples

The following are **analytical expectations**, checked with arithmetic during planning. They are not executed simulator test results.

| Candidate after main-pump failure | Bypass | Truck | Curtail | B discharge | Expected behavior |
|---|---:|---:|---:|---:|---|
| Baseline example | 0 | 0 | 0 | 16 | A reaches 120 at 40/52 ≈ 0.76923 h, about 46 minutes; B stays level |
| Bypass alone, backup at maximum | 25 | 0 | 0 | 35 | A reaches capacity at 40/27 ≈ 1.48148 h; B reaches capacity at 16/6 ≈ 2.66667 h |
| Isolated downstream-failing response | 25 | 22 | 5 | 35 | A stays at 80; B grows 6 m³/h and reaches 130 at ≈ 2.66667 h |
| Known nominal mixed response | 20 | 22 | 10 | 35 | A stays at 80; B grows 1 m³/h, ending at 117 after 3 h; treatment 35 ≤ 60 |

All flow controls in the table are m³/h. The original also uses a baseline B discharge of 35 in one candidate example; that is a valid distinct baseline in which B drains. Always supply the actual setpoint and never silently switch conventions between comparisons.

The isolated downstream-failing response is the clearest judge-facing bad plan. Bypass alone fails upstream first, so it does not isolate the hidden downstream bottleneck. The original document already explains the isolated case; the conversation emphasized using it consistently in the demonstration.

For the known nominal mixed response:

```text
dVA/dt = 52 - 20 - 22 - 10 = 0 m³/h
dVB/dt = 16 + 20 - 35 = 1 m³/h
VA(3 h) = 80 m³
VB(3 h) = 117 m³
cost = 3 × (80 × 22 + 250 × 10) = 12,780 synthetic cost units
```

Define whether capacity equality is permitted and distinguish the exact time a limit is reached from the first sampled value strictly exceeding it. Use interpolation/analytical checks as appropriate so a coarse time grid does not silently turn the expected crossing time into a different assertion.

### Objective and horizon decisions

"Feasible for the next three hours" is different from "sustainable indefinitely." A lowest-cost finite-horizon solution may deliberately consume storage headroom. Decide whether to require reserve margins or terminal conditions, or clearly display the horizon and remaining headroom.

The supplied mixed response is a known feasible nominal baseline, **not a proven cheapest solution**. Rank only independently validated candidates. If search covers a finite grid, describe the result as best among evaluated candidates unless global optimality is established.

The constant-flow toy system is linear enough to have analytical checks and, if useful, a small linear-program oracle. Use these as validation tools; do not add optimizer sophistication before the basic candidate outcomes agree.

For robustness, the proposed stress cases are +10% inflow to both tanks and −10% backup capacity. State whether these are separate scenarios or a combined stress case. A nominal pass does not imply a stress pass. Under infeasibility, return no feasible plan rather than relaxing constraints silently.

## A.7 — Immediate work, background checks, and dependencies

Work that can start without NVIDIA access, GPU training, external industrial data, or a literature review:

- Reference volume model and conservation checks.
- Hard constraint validator and the three principal incident outcomes.
- Deterministic candidate search and synthetic cost calculation.
- Generic PyTorch system boundary and later gradient checks.
- Frontend shell, network diagram, charts, controls, and labeled temporary fixtures.
- API adapters and schema validation.
- Allowlisted tool dispatcher and fake-model contract tests executing real engine functions.
- Approval-state logic, run provenance, reset, and replay design.

Run these background checks early and in parallel with independent implementation:

| Check | Owner | Required evidence | Work it must not block |
|---|---|---|---|
| Hosted NVIDIA access | B | Authenticated response and genuine model-issued tool call | Deterministic UI/API workflow |
| Scientific environment | A | Compatible interpreter, Torch import, PhysicsNeMo import, model forward pass and one training step | Reference engine and constraints |
| Windows native toolchain | A | Actual compiler/version and a tiny build/run; sanitizer support checked when native work begins | Python reference or application work |
| Event requirements | B | Track eligibility, required integrations, deadline/submission format | Core implementation |
| Canonical fixture | A, reviewed by B | Healthy setpoints, units, immutable incident, analytical expectations | UI shell |
| Demo machine | B | Frontend/backend launch together, browser connectivity, local assets | Isolated engine work |

Time-box environment investigation. A command found on PATH is not proof that its runtime is usable. An inaccessible local API, GPU, or compiler is not grounds to fabricate success. Continue independent work, record the specific missing integration, and choose a concrete supported environment when needed.

Do not postpone PhysicsNeMo environment verification until the middle of the event. Verify the small execution path early; perform substantial transport training only after the core is integrated. Hosted Nemotron avoids local LLM deployment work. Keep its key on the backend and use a configurable model ID/base URL.

Both developers use Windows. Native MSVC development is the primary plan. WSL, Linux CI, Docker, or another compiler are optional execution environments, not assumed prerequisites for either person's initial contribution.

## A.8 — Execution schedule and cutoffs

This was the proposed schedule **from the planning reference point when about 23 hours remained**. It is a resource-allocation target, not a promise or a live deadline. Rebase it to the actual remaining time before use.

| Elapsed from that reference point | Person A | Person B | Integration gate |
|---|---|---|---|
| 0–0.5 h | Scenario checks, engine interface, environment smoke check | Repository baseline, contracts, credentials | Sessions can work independently |
| 0.5–3 h | Reference solver, validator, candidate outcomes | Network, charts, API adapter | Real numerical outcomes displayed |
| 3–6 h | Search, provenance, numerical tests | Nemotron tools, trace, comparisons, approval | Complete incident → recommendation → approval flow |
| 6–10 h | Differentiable rollout, gradient tests, transport reference | Errors, reset, units, evidence display | Stable core; science extensions isolated |
| 10–16 h | Small PhysicsNeMo training and held-out comparison | Physics panel, replay, presentation | Actual scientific result or clearly stated limitation |
| 16–19 h | Stress tests and numerical fixes | Integration fixes and packaging | Feature freeze |
| Final 4 h | Joint verification | Joint rehearsal | Three clean-reset demonstrations |

If the integrated core flow is absent six hours into that plan, both people prioritize it while respecting ownership. Keep deterministic search if gradient optimization is unstable. If P0 is not integrated by the event midpoint, do not begin a new stretch subsystem.

Reserve an explicit bounded slice for a Lean/C++ experiment if prioritized. It must displace another stretch task or fit genuinely available time; do not quietly add it on top of every existing scientific commitment. Protect final integration and rehearsal time. Running training asynchronously does not eliminate implementation, validation, or integration work.

Do not begin hardware, SpacetimeDB, large neural operators, or a broad compiler/verification project in the last hours. Prefer one compiling theorem and one measured kernel to a wide set of unexecuted claims.

## A.9 — C++, Lean, and proof-backed optimization

### Architectural decision

Design for a replaceable compute kernel now. Keep API/orchestration in Python and frontend in TypeScript unless later requirements justify a change. A C++ numerical extension does not require a C++ web application.

Potential hotspots are sparse flow updates on larger networks, batches of rollouts, and fine-mesh finite-volume transport. The two-tank demo is too small to assume a measurable native advantage; API/model latency, allocation, plotting, and boundary overhead may dominate. Measure before migrating the critical path.

Pass a full rollout or candidate batch across the Python/native boundary. Avoid calling C++ once per scalar operation or time step. Specify shapes, dtype, strides/contiguity, buffer ownership, lifetimes, mutability, and error behavior at the boundary. Keep native code isolated so the application owner need not touch it.

### Concrete bounded experiment

1. Retain the readable matrix reference:

   `V_next = V + dt * (s - d + B @ q)`.

2. Implement a sparse edge-wise update: for each internal edge, subtract its transfer from its source and add the same transfer to its destination.
3. Formalize equivalence and internal-volume conservation in Lean under explicit graph/arithmetic assumptions.
4. Implement the optimized update in a small C++ kernel with a standalone native test harness and Python adapter if needed.
5. Compare reference/native trajectories within declared floating-point tolerances and run native checks.
6. Benchmark algorithmic and language effects separately, then evaluate whole-application latency.

A dense `n × m` incidence multiplication costs O(nm); an edge-list update with vector initialization can cost O(n+m). This is an operation-count argument for that representation, not a measured speedup or a claim that all matrix libraries behave identically.

Benchmark dense versus sparse **within the same language/build** to isolate the algorithmic change. Comparing Python dense code with sparse C++ simultaneously changes language, representation, and implementation. Report that comparison honestly rather than attributing its entire result to a theorem or one optimization.

### Proof scope and assumptions

Start with the original two-tank transfer-cancellation theorem, then consider general edge-update equivalence. The original proposed Lean source is preserved in Part B and has not been compiled here.

State at least:

- Vertex indices and edge endpoints are valid.
- The same graph, flows, source/sink conventions, and time step are used in both algorithms.
- Each internal transfer is accounted for once with opposite source/target contributions.
- Parallel edges and self-loops are either excluded by the graph contract or modeled consistently.
- Flows are supplied from the same state snapshot for the update; sequentially recomputing state-dependent flows is a different algorithm.
- The theorem's number system is explicit, such as rationals or reals.

Compile the proof in an actual pinned Lean/Mathlib project and record the exact theorem statement, toolchain, imports, and successful checking command. No `sorry`, unreviewed extra assumptions, or screenshot-only proof counts as completed verification.

Distinguish four claims:

1. The formalized mathematical algorithm has a proved property.
2. An implementation agrees with reference examples/property tests.
3. Native code passes the chosen runtime/static checks.
4. A measured workload runs faster under a documented setup.

None automatically establishes the other three. A theorem over exact arithmetic does not certify IEEE floating-point execution, a handwritten C++ port, memory safety, real industrial behavior, or global optimizer optimality. Further refinement/error analysis would be needed for stronger claims. Lean can compile executable code to C/native code, but that alone does not prove an independently written C++ implementation equivalent.

### Differentiation boundary

Keep the PyTorch differentiable rollout available. An independent C++ forward solver is initially a forward-computation/benchmark path. Converting tensors to NumPy or ordinary native buffers and returning values does not preserve an autograd graph automatically. An autograd-aware operator needs a correct backward rule/adjoint or a supported differentiable implementation plus numerical gradient checks.

Do not replace the differentiable path just because a native forward benchmark is faster. Validate the actual forward and backward workload. Discrete failures, topology changes, hard validation, the LLM, and operator approval are not differentiated.

## A.10 — Windows compilers, memory checking, and development checks

### Primary platform and build setup

Both humans develop on Windows. Use the installed supported compiler deliberately and record it. **MSVC is the likely default, not a verified installation.** Clang/clang-cl are options when available; they are not interchangeable with MSVC for every sanitizer/runtime combination.

If native code is prioritized, use a small CMake project with a native test target. Maintain separate build directories/configurations for:

- Checked development/testing.
- Optimized release benchmarking.
- Optional additional compiler or race/memory diagnostics.

Record compiler/version, architecture, C++ standard, flags, and runtime-library configuration. Confirm the chosen Visual Studio C++ workload and sanitizer component actually exist. Do not copy Linux flags into an MSVC build.

Use ownership-oriented C++ (`std::vector`, RAII, explicit views/lifetimes) and validate external indices, shapes, and sizes. Add compiler warnings appropriate to owned code, for example MSVC `/W4` or Clang `-Wall -Wextra -Wconversion -Wshadow`; introduce warnings-as-errors with judgment so third-party warnings do not consume the event. Static analysis such as MSVC `/analyze` or targeted clang-tidy checks can complement runtime tests. No static analysis configuration has been installed or executed yet.

### Recommended diagnostic tools

| Problem | Tool | Platform/use |
|---|---|---|
| Heap/stack bounds errors, use-after-free, double-free and related native corruption | AddressSanitizer (ASan) | Primary native Windows/MSVC checked build; also available in Clang environments |
| Undefined behavior such as signed integer overflow, invalid shifts, misaligned/null accesses | UndefinedBehaviorSanitizer (UBSan) | Clang configuration; a Linux/WSL/CI configuration is a straightforward optional path. Do not assume MSVC supports `/fsanitize=undefined` |
| Native data races | ThreadSanitizer (TSan) | Separate supported environment, such as Linux, if multithreading is introduced. Not part of the assumed MSVC Windows baseline |
| Leaks and uninitialized-value investigations | Valgrind Memcheck | Targeted Linux runs; no native Windows Valgrind dependency |
| Uninitialized native memory with compiler instrumentation | MemorySanitizer (MSan) | Optional advanced investigation in a supported environment; instrumenting dependencies makes this an expensive initial choice |
| CUDA out-of-bounds or misaligned accesses | NVIDIA Compute Sanitizer `memcheck` | When custom CUDA kernels or a relevant GPU fault exist |
| CUDA shared-memory hazards / uninitialized global reads / synchronization faults | Compute Sanitizer `racecheck`, `initcheck`, `synccheck` | Choose the tool matching the failure; racecheck is not a universal detector for all GPU races |
| Retained Python allocations | Python `tracemalloc` snapshots | Repeated simulation/reset/search tests; complement with process/native/tensor memory observations |

References: [MSVC ASan](https://learn.microsoft.com/en-us/cpp/sanitizers/asan), [Clang ASan](https://clang.llvm.org/docs/AddressSanitizer.html), [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html), [TSan](https://clang.llvm.org/docs/ThreadSanitizer.html), [MSan](https://clang.llvm.org/docs/MemorySanitizer.html), [Valgrind platforms](https://valgrind.org/info/platforms.html), [Compute Sanitizer](https://docs.nvidia.com/compute-sanitizer/ComputeSanitizer/index.html), [Python tracemalloc](https://docs.python.org/3/library/tracemalloc.html).

### Windows/MSVC ASan configuration

The recommended starting compiler flags for an ASan-enabled native target are:

```text
/fsanitize=address /Zi
```

MSVC documents incompatibility with Edit and Continue, incremental linking, and `/RTC`. Check generated flags rather than assuming the ordinary Debug preset is compatible. `/Zi` debug information is different from `/ZI` Edit and Continue. A dedicated ASan configuration or appropriately configured `RelWithDebInfo` build can avoid incompatible defaults. Ensure incremental linking is disabled for that configuration.

Run the standalone native test harness first; that provides a smaller and clearer diagnostic target than the complete Python/Torch/server process. Test the language boundary afterward. Instrument owned native targets and understand that a clean run does not establish coverage of every uninstrumented dependency.

### Optional Clang/Linux or WSL configuration

When a supported environment is actually available, a starting checked build uses:

```text
-O1 -g -fno-omit-frame-pointer
-fsanitize=address,undefined
-fno-sanitize-recover=all
```

Supply the appropriate instrumentation flags at compilation and linking. Run TSan in a separate configuration; do not combine it with ASan. Use an ordinary non-ASan native build for a separate Valgrind investigation, for example:

```sh
valgrind --tool=memcheck --leak-check=full --track-origins=yes \
  --error-exitcode=1 ./native_tests
```

These are proposed commands for future native targets, not commands already run or existing executables. Windows development must not stall on installing WSL solely to run every possible checker. Start with Windows ASan and numerical tests; add supported supplemental checks when they address actual code or concerns.

### Python, tensor, and application memory

- Compare `tracemalloc` snapshots before/after repeated operations to locate growing Python allocations. It does not account for all native or GPU tensor memory.
- Observe process and tensor memory separately when investigating growth. Caches and retained results can explain growth; distinguish them from a leak.
- Do not persist full autograd graphs accidentally in logs/run stores. Detach data intended only for saved artifacts while preserving the live graph where optimization needs gradients.
- Bound candidate counts, histories, tool calls, and stored demo artifacts. Keep repeated reset and rerun behavior under test.
- Do not run heavyweight GPU diagnostic suites for ordinary high-level model use without a relevant fault or custom kernel; use them where they can inspect the code at issue.

### Numerical and input checks remain separate

Memory-safe code can produce wrong physics. Alongside diagnostics, test:

- Finite values and explicit rejection of NaN/Inf.
- Capacity, nonnegativity, availability, treatment-throughput and control-bound rules.
- Correct source/target signs and external boundary accounting.
- Conservation at each step with integration-consistent quadrature.
- Analytic scenario results and reference/native/Torch agreement.
- Valid graph indices, array shapes, dtypes, and empty/invalid graph cases.
- Numerical gradient checks away from nonsmooth control/constraint transitions.
- Malformed model tool arguments, unknown tools, over-limit requests and approval bypass attempts.
- Property-based/randomized graph tests, and focused native fuzzing if boundary parsing/index handling warrants it.

Do not claim memory safety from a successful mathematical proof, numerical correctness from ASan, or production safety from either. None of the proposed compiler/sanitizer configurations has been configured or run in this repository yet.

## A.11 — Benchmarks and numerical validation

Benchmark only a defined workload. Include the actual two-tank demo and larger graphs/batches if the optimization claim concerns scaling. The original suggests 100, 1,000, and 10,000 candidate rollouts; use practical sizes and disclose them rather than treating all as mandatory.

Record:

- Hardware, OS, compiler, package versions, optimization flags, thread count, and device.
- Graph size, edge count, candidate batch, horizon, time step, integrator, dtype, seed, and controls.
- Warm-up policy, repeated measurements, startup/compilation inclusion, and data-conversion/allocation costs.
- Kernel time, complete rollout time, per-candidate time, and end-to-end application time separately.
- Maximum trajectory difference, conservation residual, feasibility agreement, and any changed arithmetic/reduction order.
- Gradient correctness and forward/backward time when claiming an optimization benefit for differentiable controls.
- Whether inference or API/network time is included. Live model latency is a distinct source of variation.

Compare equivalent numerical methods and tolerances. Avoid weak baselines chosen only to inflate results. A C++ kernel that wins on a large synthetic batch may not improve the actual demo; label the supported claim accordingly.

Amdahl's law gives a useful sanity check:

```text
overall_speedup = 1 / ((1 - p) + p / s)
```

Here `p` is the measured fraction of runtime accelerated, and `s` is that part's speedup. If numerical compute occupies 20% of runtime, making it arbitrarily fast caps whole-application speedup at 1.25×. This is an illustrative mathematical limit, not a measured BREAKWATER result.

Keep optimized benchmark builds separate from sanitizer builds. Do not use instrumented timings as release performance evidence, or include compilation/warm-up in one competitor and exclude it from another without disclosure.

For scientific correctness, use float64 initially and a separate reference implementation. For RK4/time-dependent flows, conservation compares against the same integration quadrature of boundary flux, not a mismatched Euler expression. Retain unrounded values for validation and round only for display.

## A.12 — NVIDIA integration requirements

### Nemotron

Use hosted access as the hackathon default, with server-side `NVIDIA_API_KEY`, configurable `NVIDIA_BASE_URL`, and configurable model identifier. The referenced model endpoint exists in NVIDIA documentation, but the project has not made an authenticated request or verified actual account access.

The agent must genuinely inspect state, propose/test candidates, receive constraint feedback, revise, compare, and request approval. A UI macro or replay is not live agent tool use. Use one bounded orchestration loop rather than introducing several independently hosted agents.

Use allowlisted function dispatch and server-side argument bounds. Tools must execute real simulator/search/validator functions. The model cannot overwrite failed validation or approve its own plan. Cap rounds, tool interactions, and total request duration; preserve completed runs on failure.

Most automated tests can fake the model client while executing real tools. A separately recorded authenticated smoke test establishes live integration. Label manual/deterministic/replay operation truthfully if the hosted model is unavailable.

Official references:

- [Nemotron 3.5 Lightning hosted endpoint](https://docs.api.nvidia.com/nim/reference/nvidia-nemotron-3-5-lightning-30b-a3b-infer)
- [NIM model-specific guide](https://docs.nvidia.com/nim/large-language-models/2.0.10/get-started/advanced/get-started-nemotron-3.5-lightning.html)

The local NIM guide and hosted endpoint are different deployment contexts. Do not assume a local parser flag is configurable on a hosted API. Check the actual endpoint behavior and installed client version rather than copying unverified API details.

### PhysicsNeMo

Verify import plus actual execution early. The documented pip/container workflows and current `physicsnemo.sym` integration were checked during planning; no package has been installed or executed for this project. NVIDIA lists Windows/native or WSL among its system options, so Linux is not an assumed requirement for both developers. Select a compatible Torch/driver/package stack based on the actual machine and package version.

Use a small 1D transport problem, a real PhysicsNeMo model, executed training/checkpoint loading, and held-out comparison against finite-volume reference. Reduce conditioning to one or two parameters first. Split held-out cases by parameter configuration, not only neighboring points from one field.

Record seed, normalization, loss/residual definition, runtime, checkpoint provenance, validation metrics, readiness state, and limitations. If supervised FVM labels enter the loss, describe it as physics-informed supervised training rather than pure residual-only training. Low PDE residual alone is not a validation result.

Keep FVM authoritative for quality until the relevant learned model is validated for its intended domain. A separate concentration visualization is acceptable; do not claim a network-wide chemistry model or validated quality constraints when only a single-pipe panel exists. Display poor or unavailable learned results honestly.

Official references:

- [PhysicsNeMo installation](https://docs.nvidia.com/physicsnemo/latest/getting-started/installation.html)
- [System requirements](https://docs.nvidia.com/physicsnemo/latest/getting-started/system_requirements.html)
- [Physics-informed residuals and current symbolic utilities](https://docs.nvidia.com/physicsnemo/latest/user-guide/physics_addition.html)

Pin the actual working versions. Original code snippets and links are design references, not evidence that a future installed release has matching signatures.

## A.13 — Acceptance evidence and truthful status

Use the original sections 20, 23, and 28 for detailed tests/tickets/definition of done. Do not mark any unchecked source checkbox complete merely because it appears in this handoff.

Minimum integrated acceptance evidence:

- Healthy settings are coherent; incident injection/reset affect actual model state.
- Baseline A limit time agrees with the analytic fixture.
- The isolated downstream-failing response identifies B and its limit time.
- The known mixed plan passes the declared nominal three-hour constraints and reports actual computed cost.
- All candidates use the same incident state and publish numerical provenance.
- Bounds and malformed inputs are rejected, including unavailable equipment requests.
- Validator decisions come from deterministic code, not model prose or frontend heuristics.
- Genuine Nemotron tool calls execute real functions and cite actual run IDs.
- Recommendation/staging alone does not apply or approve a plan.
- Reset clears stale recommendations, approvals and scenario modifications.
- Torch/reference/native comparisons and gradients are checked when those paths exist.
- FVM, PINN, proof, native, and performance claims each have their own execution/evaluation evidence.
- Replay remains clearly marked and is available without external model access.
- The final demo runs three times from clean reset with locally available assets.

Feature status vocabulary should distinguish `working`, `experimental`, `planned`, and `blocked` where useful. Record the supporting command/run/checkpoint/proof artifact and limitation. The initial scientific and application components are planned, not working.

All data, operating costs, equipment rules, telemetry, quality limits, and interventions in this prototype are synthetic. It is educational/research decision support, not validated industrial control. Operator approval in the demo authorizes only simulated application. No real hardware actuation or implied partner safety sign-off is part of the current implementation request.

Delivery documentation should eventually include setup, actual package/toolchain requirements, a local launch command, concise status, numerical assumptions/evidence, API contracts, a 90-second/three-minute demo script, and a fallback/replay procedure. These are future implementation deliverables, not existing files.

## A.14 — Suggested assignments for implementation sessions

These are handoff assignments to use **when implementation is requested**. They do not override the user's active task or grant permission to execute every original instruction just because the document was opened.

### Person A / scientific-engine coding session

Read Part A and original sections 5–10, 12, 16–17, 20, 23, and 28. Respect the ownership table. Start with the canonical incident fixture, independent reference simulation, hard validation, and analytic candidate tests. Deliver one callable numerical result early to the application owner. Then add deterministic search, generic Torch rollout/gradient checks, and the scoped transport/PhysicsNeMo work. If proof-backed speedups are prioritized, isolate one native kernel and theorem with measured evidence. Do not change HTTP/tool schemas, frontend, app startup, root lockfiles, or integration-owned docs without coordination.

### Person B / application-integrator coding session

Read Part A and original sections 2–4, 11, 13–15, 20, 22–23, and 28. Own the published schemas and root setup. Build the console with clearly labeled fixture data while A delivers the engine, then integrate the real functions. Implement actual bounded NVIDIA tool calling, run provenance, comparison and explicit approval. Keep the model key backend-only. Own launches, replay, reset, integration review and final presentation. Do not recreate numerical logic in the frontend or rewrite A's science modules.

### Required increment report

```text
Commit / branch:
Touched files and owner:
Implemented behavior:
Public interface changes:
Tests / commands actually executed and results:
Known limitations / unavailable dependencies:
Executable usage example:
Next integration or decision needed:
```

Communicate concrete evidence rather than claiming completion from generated code alone. If another human/session is using the same repository, inspect current status before editing and preserve their uncommitted work.

## A.15 — Reference provenance and preservation

Original source supplied by the user:

- Path: `C:\Users\EthanCochran\Downloads\BREAKWATER_CODEX_TRANSFER.md`
- Title: `BREAKWATER — Codex Engineering Transfer Document`
- Version/date stated in source: 1.0, November 7, 2026.
- Source size: **108,820 bytes**.
- Source SHA-256: `F23EDE5A2F75F12634D5E18446A3A821156D73B83D730973460C52F3ADAFB974`.

Part B is appended from the original file bytes to preserve every section, equation, JSON example, code stub, proposed repository path, research reference, optional feature, kickoff prompt, and unchecked definition-of-done item. It is deliberately included in full so this handoff does not depend on the Downloads file remaining available. The source file is not modified.

The conversation also supplied the planning corrections, Windows context, repository observations, compiler/sanitizer recommendations, proof/performance distinctions, and revised work ownership collected in Part A. Current official documentation was consulted for the principal NVIDIA, Lean, sanitizer, Valgrind, and Python points. Links retained inside the original document are source references; preserving them does not claim that every cited paper, eligibility statement, API example, or performance assertion was independently verified during this handoff.

The original contains its own heading hierarchy and imperative instructions. The unchanged text below is the preserved original proposal; use the authority/status distinctions at the start of Part A when interpreting it.

---

# Part B — Complete original transfer document

<!-- BEGIN ORIGINAL BREAKWATER TRANSFER (VERBATIM) -->

# BREAKWATER — Codex Engineering Transfer Document

**Project type:** 24-hour, two-person hackathon; scientific computing + industrial operations + agentic AI  
**Version:** 1.0 — November 7, 2026  
**Status:** Implementation-ready architecture and build plan; **not** a claim that the proposed components are already implemented  
**Primary intended challenge:** EOG's **“Beat the Shutdown”** industrial-operations scenario  
**Primary technologies:** NVIDIA Nemotron, NVIDIA PhysicsNeMo, Python, PyTorch, React/TypeScript, FastAPI; optional C++, Lean 4, SpacetimeDB, ESP32/3D printing  
**Audience:** Codex coding agents, the two human builders, and any integrator inheriting the repository

> **IMPORTANT IDENTITY CHECK.** BREAKWATER is **not** a coastal-erosion or municipal storm-surge simulator. It is an **industrial fluid infrastructure / produced-water-style operations digital twin** for pumps, pipes, storage tanks, treatment destinations, and emergency transport. A pump failure threatens to cause a shutdown. The system forecasts the impact, explores interventions, checks downstream constraints, and recommends an operator-approved response. All data, operating rules, and equipment models in the demo are synthetic. Do not portray this prototype as an industrially validated controls system.

---

## 0. Instructions for Codex: read this first

You are implementing **BREAKWATER**, a working, polished, scientifically credible, agent-driven industrial fluid digital twin. **Do not** begin by building an elaborate agent framework, general PDE compiler, 3D game engine, or production multi-tenant platform. First make the **end-to-end pump-failure demonstration** work reliably.

### Hard requirements

1. **Working demo before sophistication.** The system must show a live/simulated industrial network, inject a pump failure, predict tank overflow/shutdown risk, evaluate several candidate interventions, reject an infeasible reroute, and return an operator-approvable plan.
2. **Nemotron does real work.** It must call tools to inspect state, request simulations, propose alternatives, compare results, and obtain independent constraint validation. It is not merely a chatbot that describes an already-computed answer.
3. **PhysicsNeMo does real work.** Include at least one functioning PhysicsNeMo-backed scientific ML module—preferably a parameter-conditioned, one-dimensional contaminant-transport PINN—and demonstrate its prediction or residual computation against a conventional reference method. A dependency in `requirements.txt` alone does not count.
4. **Reusable differentiable core.** Implement a general PyTorch state-space interface with differentiable rollouts for continuous controls/parameters; implement the BREAKWATER flow graph as one instance. Do not claim differentiation through discrete pump-failure toggles or the LLM.
5. **Numerical validation is authoritative.** Hard capacities, physical non-negativity, quality limits, and conservation checks are evaluated by deterministic code. Nemotron cannot waive, redefine, or fabricate these constraints.
6. **Two-person / 24-hour constraint.** Keep the repository modular, maintain a green main branch, and allocate final hours to integration and demo rehearsal.
7. **Truthful presentation.** Label synthetic telemetry, modeled costs, learned forecasts, benchmark claims, and unavailable features accurately. Do not claim real industrial control, guaranteed safety, or achieved speedups without measurement.
8. **Explicit operator approval.** Plans are advisory. The UI can stage a simulated intervention only after the operator approves. No real hardware actuation without a separate, explicit safety layer and operator control.

### Codex operating priorities

- **P0:** A complete deterministic graph-flow simulator, scenario runner, constraint validator, frontend, and structured Nemotron tool loop.
- **P1:** Differentiable control optimization + genuine PhysicsNeMo transport prototype + useful agent experiment loop.
- **P2:** C++ backend, Lean proof, SpacetimeDB, physical model, neural operators, Neural ODE correction, sophisticated multi-agent distribution.
- **Stop rule:** If P0 is not integrated by the midpoint of the hackathon, **do not** start another P2 feature.
- Record implementation status as `working`, `experimental`, or `planned` in `docs/STATUS.md`. Don't imply that an optional component exists when it doesn't.

### One-line product statement

**“When a pump fails, BREAKWATER uses a differentiable digital twin and NVIDIA Nemotron to test response strategies, expose hidden downstream risks, and recommend the lowest-cost validated intervention before a shutdown.”**

---

## 1. Why this exists: problem, users, and advantage

### Problem

Industrial fluid networks couple many local decisions. A pump failure or line outage does not affect just one asset: it changes inventory in upstream tanks, capacity on alternate routes, load on downstream treatment facilities, and potentially fluid quality. A naive “reroute to the backup line” action can avert one immediate alarm but create a second overflow or exceed treatment capacity several hours later. Reduced production, temporary transport/trucking, deferred processing, and equipment repair have different operational costs and constraints.

An operator needs an answer to:

- **What fails first?** Which tank reaches its limit, and when?
- **What if?** Does moving flow from Tank A to Tank B genuinely resolve the incident?
- **What else breaks?** Could the backup pump, treatment destination, or quality threshold become the new bottleneck?
- **What is the least-bad feasible intervention?** How should rerouting, trucking, and temporary curtailment be combined?
- **Why should I trust this?** What solver, assumptions, conservation checks, and validation results support the recommendation?

### Target user

An industrial field operations coordinator or fluid-handling control-room operator. The demo is **not** intended for autonomous production control. Its primary value is decision support that links incident detection to quantitative what-if analysis and auditable justification.

### Why use agentic AI here?

A static optimizer needs a pre-specified optimization problem and known incident class. A Nemotron agent can take a messy operator observation (“the north transfer pump is dead; can we avoid shutting in production?”), identify what it needs to know, query asset state, formulate sensible candidate interventions, run several tests, notice unexpected downstream consequences, and request a refined plan. **The LLM generates and coordinates hypotheses; deterministic solvers and validators judge them.**

### Why use SciML?

- **Graph ODE digital twin** predicts tank inventories dynamically.
- **Advection–diffusion–reaction PDE** predicts transport of a concentration/quality signal through a critical line.
- **Differentiable simulation** supplies gradients of risk/cost with respect to continuous interventions.
- **PhysicsNeMo PINN** is a parallel learned physical-field model for interpolation, preview, or residual-based analysis.
- **Optional hybrid Neural ODE** learns pump-efficiency corrections from deviations between telemetry and a nominal model.
- **Optional neural operator** accelerates repeated PDE predictions if there is enough training data and validation.

### Hackathon narrative

This is best aligned with the EOG **“Beat the Shutdown”** concept, while NVIDIA's technology is substantive. Other specialized track eligibility should be checked against event rules; do not assume any particular set of up to three submissions has been approved.

---

## 2. North-star live demonstration (judge-facing)

The following must be runnable offline for the physical simulator and UI, and with a valid NVIDIA API key for the agent.

1. **Healthy network.** Show a recognizable piping/tank diagram: two accumulating storage tanks, a main transfer pump, a bypass/backup route, treatment destination, and emergency trucking option. Animate flows and tank levels.
2. **Inject incident.** Click **“Fail Main Pump”**. The pump turns red; inflow continues; the Tank A forecast crosses its capacity limit in approximately the next hour under the illustrative scenario.
3. **Nemotron investigates.** Agent calls `get_network_state`, `get_incident`, `forecast_baseline`, and optionally `get_asset_constraints`. The UI displays a concise, verifiable event trace.
4. **Bad plan gets rejected.** Agent (or the judge) tries **“Open bypass to maximum”**. Forward simulation shows that although some upstream volume is relieved, the downstream storage tank or treatment capacity exceeds its limit. The validator returns a machine-readable violated constraint and failure time.
5. **Agent tests a better plan.** Explore combined bypass, emergency trucking, and limited production reduction. Run the differentiable optimizer or grid-search fallback. Validate candidates under a nominal incident and two modified conditions if time permits.
6. **Final recommendation.** Display the chosen controls, estimated modeled cost, projected hours of remaining storage, comparison against alternatives, and **PASS/FAIL by constraint**. Evidence links to simulation run IDs, not natural-language assertions.
7. **Operator approves.** Clicking **“Approve simulated plan”** moves the display into a planned/control-preview state; do not silently change the current network before approval.
8. **PhysicsNeMo visual.** A secondary quality transport panel shows a time–distance contaminant concentration field in a selected pipe. Overlay PhysicsNeMo PINN prediction with finite-volume reference or error/residual to demonstrate genuine physics ML integration.
9. **Optional hardware flourish.** A 3D-printed miniature tank/pipe network, LEDs, or servo valve mirrors the simulated state through an ESP32. This is an illustration, not actual fluid-control hardware.

**Ideal 90-second judge sequence:** healthy network → one-click failure → predicted overflow timer → ask Nemotron “Find the cheapest safe response without exceeding downstream limits” → wrong reroute visibly fails → optimized mixed response passes → human approval → PhysicsNeMo field and validation tab.

---

## 3. Priority ladder and ruthless scope

| Priority | Must deliver | Notes |
|---|---|---|
| **P0-A** | Deterministic network graph + scenario JSON | 2 tanks, treatment, pumps/routes, trucking, production control |
| **P0-B** | Batch time-series simulator with tests | Stable reference model; no agent dependency |
| **P0-C** | Constraint engine and failure explanation | Must independently reject bad routes |
| **P0-D** | Interactive frontend + incident injection | Smooth visualization; no 3D prerequisite |
| **P0-E** | Real Nemotron tool-use loop | At least 3 distinct tool calls in a successful run |
| **P1-A** | Differentiable PyTorch rollout and gradient-based controls | Compare to fixed candidate baselines |
| **P1-B** | PhysicsNeMo-backed 1D transport PINN | Real imported model and training/inference; verified against a reference PDE solver |
| **P1-C** | Scenario robustness checks | Nominal, higher inflow, less backup capacity |
| **P1-D** | Replayable simulation/event log | Deterministic demo even when network/API flaky; label replay |
| **P2-A** | Optional C++ sparse graph kernel/pybind11 | Only after benchmarks show a bottleneck |
| **P2-B** | Lean 4 conservation/sparse-update theorem | Proof artifact; not a runtime claim about float safety |
| **P2-C** | SpacetimeDB real-time shared state | Only if a track advantage or multi-client demo warrants it |
| **P2-D** | Hybrid Neural ODE or FNO | Research extension, not required for safe demo |
| **P2-E** | 3D-printed hardware | Only if UI/core are already stable |

**Do not implement in 24 hours:** full transient 3D Navier–Stokes, compressible multiphase pipe hydraulics, real-world produced-water chemistry, a general-purpose symbolic PDE compiler, distributed simulation orchestration infrastructure, large FNO training from scratch, custom LLM training, or actual industrial PLC integration.

---

## 4. Architecture and dataflow

```text
┌───────────────────────────────────────────────────────────────────┐
│ React/TypeScript operations console                               │
│ graph view • telemetry • interventions • time scrub • agent trace │
└────────────────────────┬──────────────────────────────────────────┘
                         │ HTTP + optional SSE
┌────────────────────────▼──────────────────────────────────────────┐
│ FastAPI: scenarios, run IDs, telemetry, state, simulation, approval│
│ Pydantic contracts • bounded jobs • structured logs               │
├───────────────────────────────────────────────────────────────────┤
│ NEMOTRON ORCHESTRATION                                             │
│ diagnose → hypothesize → call tools → compare → revise → explain   │
│ Agent tools: get_state, simulate, optimize, validate, approve gate  │
├───────────────────────────────────────────────────────────────────┤
│ DETERMINISTIC SIMULATION / OPTIMIZATION                            │
│ Generic DifferentiableSystem abstraction                          │
│ BREAKWATER tank + flow graph ODE                                   │
│ Fixed-step torch rollout + independent numpy/scipy reference      │
│ Constraint evaluator + gradient optimizer + scenario tests        │
├──────────────────────────┬────────────────────────────────────────┤
│ PDE reference             │ NVIDIA PhysicsNeMo                     │
│ 1D FVM advection/         │ Parametric 1D PINN / PDE residual       │
│ diffusion/reaction        │ compare against FVM                     │
├──────────────────────────┴────────────────────────────────────────┤
│ Synthetic scenario + run store (JSON/SQLite first)                │
│ OPTIONAL: SpacetimeDB / C++ / Lean / physical miniature            │
└───────────────────────────────────────────────────────────────────┘
```

### Ownership of truth

- **Network configuration**: canonical versioned scenario JSON.
- **State/telemetry**: timestamped synthetic measurement feed plus controlled simulation state.
- **Predictions**: numerical run artifacts, each with solver/version/assumptions/scenario ID.
- **Safety decisions**: deterministic constraint checks, never a language model's judgment.
- **Agent reasoning**: proposals and calls attributable to Nemotron; they reference run IDs and violation reports.
- **Approval**: operator action with audit entry; no simulated setting is enacted before this.

### Pure functions and reproducibility

Every simulation should satisfy:

```python
result = simulate(scenario, incident, control_schedule, seed=42)
# same inputs -> same outputs, except explicitly marked stochastic sensor noise
```

Never mutate the canonical scenario in-place during a what-if run. All candidates start from the **same incident-time state**. A run stores `scenario_hash`, `start_state_hash`, `parameters`, `control_schedule`, `solver_version`, and `seed`.

---

## 5. Mathematical engine I: graph-based fluid volume dynamics

### 5.1 Graph formalization

Let the network be a directed graph \(G=(\mathcal V,\mathcal E)\). Each *storage* vertex \(i\) has fluid volume \(V_i(t)\) [m³], physical capacity \(V_i^{\max}\) [m³], exogenous inflow \(s_i(t)\) [m³/h], and externally consumed/discharged flow \(d_i(t)\) [m³/h]. Each pipe/pump edge \(e\) has flow \(q_e(t)\) [m³/h], nonnegative in its declared direction. Set the graph incidence matrix \(B\in\mathbb R^{n\times m}\):

\[
B_{ie}=\begin{cases}-1 & i=\operatorname{src}(e),\\+1 & i=\operatorname{dst}(e),\\0&\text{otherwise.}\end{cases}
\]

The storage ODE is:

\[
\boxed{\dot{\mathbf V}(t)=\mathbf s(t)-\mathbf d(t)+B\mathbf q(t).}
\]

For the simplified hackathon model, controlled flows are **capacity-limited transfer setpoints**, not a rigorous pressure/head-based hydraulic solution:

\[
q_e(t)=a_e(t)\,q_e^{\max}\,u_e(t),\qquad 0\le u_e\le1,\quad a_e\in\{0,1\}.
\]

Here \(a_e\) represents edge availability (e.g., failed pump → 0) and \(u_e\) a continuous operating setpoint. Add directional constraints, shared treatment throughput, and source/target inventory limits in the independent validator.

**Do not claim hydraulic realism.** Real networks also require pump curves, pressure, frictional head loss, phase effects, control transients, and equipment data. A future extension can make \(q_e\) a physically derived function of hydraulic heads rather than a control setpoint.

### 5.2 External nodes vs storage nodes

Production sources, trucking, and treatment/disposal can be represented as *external boundary terms* or as explicit graph vertices. **Choose a consistent convention**:

- Recommended MVP: only Tank A and Tank B are stateful storage nodes; production is `s`, trucking and treatment discharge are `d`, and the A→B bypass is a `Bq` internal edge.
- Display production, trucking, treatment, and main/backup pump as nodes/edges in the UI even when the mathematical boundary terms omit their storage states.
- Main pump A→treatment corresponds to Tank A external discharge; backup pump B→treatment corresponds to Tank B external discharge.

This avoids inadvertently adding a treatment discharge twice or claiming water volume vanished.

### 5.3 Mass/volume conservation invariant

For internal edges, every transfer exits exactly one storage node and enters another:

\[
\mathbf 1^T B=0.
\]

Hence, for any differentiable admissible control and any pump-failure pattern held fixed over a step,

\[
\boxed{\frac{d}{dt}\sum_i V_i(t)=\sum_i s_i(t)-\sum_i d_i(t).}
\]

Check numerically at each simulation step:

\[
\epsilon_{\mathrm{mass}}
=\left|\sum_iV_i^{n+1}-\sum_iV_i^n
-\Delta t\sum_i(s_i-d_i)\right|.
\]

For RK4/time-varying inputs, use the **matching RK4 quadrature of net boundary flux**, not blindly the Euler expression. When fixed flows/inputs are held constant within the step, the simpler expression is valid. For the synthetic constant-flow run, mass error should remain at floating-point noise levels with `float64`.

### 5.4 Numerical rollout

Use `torch.float64` and initially a fixed-step integrator (Euler for very simple constant flows, otherwise RK4). Recommended default: horizon 3 h, step 0.025–0.05 h, with exact event-time boundaries if pump failure occurs inside the horizon. Fixed-step PyTorch is straightforward to differentiate and debug; do not add `torchdiffeq` unless needed.

```python
# Illustrative pseudocode: fixed-step differentiable integration.
state = initial_volume_tensor.clone().to(torch.float64)
trajectory = [state]
for n in range(num_steps):
    t = n * dt_hours
    s, d, q = build_flows(t, state, controls, incident, params)
    derivative = s - d + incidence_matrix @ q
    state = state + dt_hours * derivative  # replace with RK4 if dynamics vary
    trajectory.append(state)
volumes = torch.stack(trajectory)
```

**Important:** do **not** `detach()` inside the differentiable rollout; do **not** `numpy()` the state before computing the objective. Clamping state volumes to `[0, capacity]` after a step conceals unsafe candidates and can destroy gradient information. Instead, explicitly report negative/overfull predictions as violations. If physical outflow must be limited by available volume, cap the *edge flux*, apply the identical internal transfer to both endpoints, and validate its effect; this piecewise differentiability must be documented.

### 5.5 Derivative checks

For continuous controls \(u\), verify the objective \(J(u)\) obeys:

\[
\nabla_u J\overset{?}{\approx}
\frac{J(u+\varepsilon e_i)-J(u-\varepsilon e_i)}{2\varepsilon}.
\]

Test at non-saturated, non-kink points, compare to finite differences with sensible tolerances, and inspect `grad is not None` plus finite gradient norms. Gradients through the continuous simulator are valid; discrete incident flags, topology choice, and Nemotron decisions are **not** differentiated.

---

## 6. Mathematical engine II: transport/quality PDE

To make BREAKWATER scientifically richer than a network-inventory calculator, model the evolution of one scalar constituent concentration \(c(x,t)\) along **one critical transfer pipe**. This could represent a synthetic tracer, salt, or simplified contaminant. It is a toy **quality-risk proxy**, not a certified produced-water chemistry or treatment model.

### 6.1 Governing equation

\[
\boxed{
\frac{\partial c}{\partial t}
+v(t)\frac{\partial c}{\partial x}
=D\frac{\partial^2 c}{\partial x^2}
-\kappa c+s_c(x,t).
}
\]

where:

- \(x\in[0,L]\): pipe distance [m]; \(t\): time [h].
- \(c\): dissolved tracer concentration [mg/L], or consistently another declared unit.
- \(v=q/A\): mean axial speed [m/h], given volumetric flow \(q\) [m³/h] and pipe area \(A\) [m²].
- \(D\): effective longitudinal dispersion coefficient [m²/h].
- \(\kappa\): first-order decay coefficient [1/h].
- \(s_c\): optional source term, with concentration/time units.

Equivalent flux form:

\[
\partial_t c+\partial_x J=-\kappa c+s_c,
\qquad J=v c-D\partial_x c.
\]

For **forward, nonnegative** flow, prescribe:

\[
c(0,t)=c_{\mathrm{in}}(t),\qquad
\partial_x c(L,t)=0,\qquad
c(x,0)=c_0(x).
\]

For numerical consistency, use a physically consistent inlet flux implementation in the reference FVM; a ghost-cell Dirichlet treatment is an acceptable demo approximation if documented. **Do not allow flow reversal in P0**; reversed direction requires changing the inflow boundary.

### 6.2 Reference finite-volume solver (authoritative for quality)

Use a 1D, cell-centered, conservative finite-volume discretization with an upwind convective flux and centered diffusion:

\[
F_{j+1/2}=v\,c_{\rm upwind}-D\frac{c_{j+1}-c_j}{\Delta x},
\]

\[
\boxed{
c_j^{n+1}=c_j^n
-\frac{\Delta t}{\Delta x}(F_{j+1/2}-F_{j-1/2})
+\Delta t(-\kappa c_j+s_{c,j}).
}
\]

Use a stable step satisfying a conservative CFL/diffusion condition for the chosen explicit scheme; for constant \(v\ge0\), a convenient *sufficient* practical condition is

\[
\Delta t\left(\frac{v}{\Delta x}+\frac{2D}{\Delta x^2}+\kappa\right)\le1,
\]

subject to discretization-specific boundary behavior. If time steps become too small, make diffusion implicit rather than pretending explicit integration is stable. Add grid refinement (e.g., 32 vs 64 vs 128 cells) to show the reference is at least numerically self-consistent.

**Coupling strategy for hackathon:** The graph ODE calculates transfer flows. One selected line supplies `q(t)` to the PDE through `v(t)=q(t)/A`; the PDE returns a **pipe outlet concentration/quality warning**. The P0 volume system does not require accurate multi-pipe contaminant mixing. If tank concentration bookkeeping is later added, evolve constituent **mass** \(M_i=V_i C_i\), not concentration independently, and handle pipe residence/inventory carefully to avoid double counting chemical mass.

### 6.3 Parameter-conditioning and physics loss

Train a neural approximation:

\[
\hat c_\phi(x,t;v,D,\kappa,a,\tau)
\]

where \(a\) and \(\tau\) can parameterize a synthetic inlet tracer pulse. For a first run, **reduce the conditioning** to one or two parameters, such as \(v\) and pulse amplitude, while fixing \(D,\kappa\). Do not take on a high-dimensional generalized neural operator as the P1 requirement.

Define PDE residual:

\[
\mathcal R_\phi
=\partial_t\hat c_\phi+v\partial_x\hat c_\phi
-D\partial_{xx}\hat c_\phi+\kappa\hat c_\phi-s_c.
\]

Suggested training objective:

\[
\mathcal L=
\lambda_{\rm PDE}\mathbb E_{\Omega\times[0,T]}[\mathcal R_\phi^2]
+\lambda_{\rm IC}\mathbb E_x[(\hat c_\phi(x,0)-c_0(x))^2]
+\lambda_{\rm BC}\mathbb E_t[(\hat c_\phi(0,t)-c_{\rm in}(t))^2]
+\lambda_{\rm out}\mathbb E_t[(\partial_x\hat c_\phi(L,t))^2]
+\lambda_{\rm data}\mathbb E[(\hat c_\phi-c_{\rm FVM})^2].
\]

Data loss may be omitted for a *pure PINN* demonstration. If FVM labels are used, honestly call it **physics-informed supervised training**. Normalize input coordinates and parameters. Report both PDE residual and **held-out FVM prediction error**, because a low residual alone is not proof of an accurate solution.

### 6.4 Limitations to discuss publicly

Advection-dominated fronts and poorly conditioned PINN losses can be difficult to train. Use smooth tracer pulses, modest Peclet numbers, curriculum on transport speed if necessary, and a strict training-time cap. Never call a visibly poor PINN “validated.” If the learned output is inaccurate, show its error map and fall back to FVM for decision support; that itself illustrates responsible hybrid scientific computing.

---

## 7. Mathematical engine III: intervention optimization

### 7.1 Decision vector and mixed discrete/continuous choices

Let continuous controls be

\[
\theta=(u_{A\to B},q_{\rm truck},r_{\rm curtail},u_{B\to T})
\]

with bounds and appropriate units. All configurations that add/remove a route, repair a pump, or change availability form **discrete scenarios**, not differentiable coordinates. For the hackathon, enumerate a few discrete modes and optimize the continuous variables separately in each.

### 7.2 Cost objective

Define synthetic, clearly labeled relative cost terms:

\[
J_{\rm operating}(\theta)
=\int_0^T\left[
 c_{\rm truck}q_{\rm truck}(t)
+c_{\rm curtail}r_{\rm curtail}(t)
+c_{\rm energy}(t)
\right]dt+c_{\rm repair}I_{\rm repair}.
\]

Example demo-only cost coefficients: trucking **80 cost units/m³**; production curtailment **250 cost units/m³**; optional energy cost. Avoid presenting these as real EOG prices or field engineering estimates.

### 7.3 Constraints: what “safe” means in the toy model

At every time step, enforce:

\[
0\le V_i(t)\le V_i^{\max},\quad
0\le q_e(t)\le a_e q_e^{\max},\quad
0\le q_{\rm truck}\le q_{\rm truck}^{\max},
\]

\[
0\le r_{\rm curtail}\le r_{\rm curtail}^{\max},\qquad
\sum_{e\to\mathrm{treatment}}q_e(t)\le Q_{\rm treatment}^{\max},
\]

plus any declared quality threshold when its transport/quality model is **actively validated for that run**. Report infeasible if the threshold is violated; never silently relax it. Include a conservation tolerance as a numerical integrity check.

### 7.4 Differentiable penalty optimization

For continuous relaxation, minimize:

\[
\widetilde J(\theta)=J_{\rm operating}(\theta)
+\lambda_V\sum_{i,n}\operatorname{softplus}\!\left(\frac{V_i^n-V_i^{\max}}{\sigma_V}\right)^2
+\lambda_-\sum_{i,n}\operatorname{softplus}\!\left(\frac{-V_i^n}{\sigma_V}\right)^2
+\lambda_Q\sum_n\operatorname{softplus}\!\left(\frac{Q_T^n-Q_T^{\max}}{\sigma_Q}\right)^2.
\]

Parameterize bounded control values using sigmoid transforms or explicitly project with Adam/L-BFGS. Penalties should be scaled to meaningful dimensionless magnitudes; do not mix raw huge volumes and small dollar-like costs without normalization.

**CRUCIAL:** Penalty minimization is **not** proof of feasibility. After optimizing, run the separate deterministic validator with declared numerical tolerances. If it fails, reject the candidate and try another. If the entire search is infeasible, report “no feasible plan found under these modeled options,” not a fabricated rescue.

### 7.5 Robustness check

Test recommendations under at least these synthetic cases if P1 time allows:

- **Nominal:** expected production inflow and pump availability.
- **High-inflow stress:** e.g., +10% external inflow to both tanks.
- **Degraded-backup stress:** e.g., -10% backup transfer capacity.

Either require feasibility across all selected cases or label the plan **nominal only** and visibly warn about failed stress cases. Do not mix these semantics. For small P0 use a deterministic grid of control candidates as a baseline; add PyTorch optimization after reference correctness.

---

## 8. Specific synthetic scenario: a good, reproducible starting point

All numbers below are **illustrative synthetic values**, intentionally chosen to produce a convincing failure and expose a downstream bottleneck. Use units and display them in the UI. Adjust them only if a solver/test demonstrates inconsistency, and record any change in the scenario file.

| Entity | Nominal value / behavior |
|---|---|
| Tank A capacity | 120 m³ |
| Tank A initial inventory | 80 m³ |
| Tank B capacity | 130 m³ |
| Tank B initial inventory | 114 m³ |
| External inflow into Tank A | 52 m³/h |
| External inflow into Tank B | 16 m³/h |
| Main pump A → treatment | 55 m³/h capability; **fails** at incident time |
| Bypass A → B | 25 m³/h maximum |
| Backup pump B → treatment | 35 m³/h maximum |
| Combined treatment capacity | 60 m³/h (other inputs absent in MVP) |
| Emergency trucking from Tank A | 22 m³/h maximum |
| Controllable production reduction at Tank A | 15 m³/h maximum |
| Primary forecast horizon | 3 h |
| Forecast step | 0.025–0.05 h |
| Base tracer input | 0.2 mg/L, with optional synthetic transient pulse |

### Simple reasoning check

After main-pump failure with no intervention:

- Tank A fills at 52 m³/h; its free capacity is 40 m³, so estimated time to limit is **40/52 ≈ 0.769 h**, around **46 minutes**.
- Tank B can remain stable if its backup pump continues discharging 16 m³/h under baseline, or will drain if it discharges more. The operator should see actual modeled setpoints rather than assuming every pump always runs at maximum.
- A naive plan that routes **25 m³/h to Tank B** without trucking/curtailment still leaves Tank A filling by 27 m³/h, and may fill Tank B if its backup outlet is already saturated or available capacity is low.
- One feasible **illustrative** mixed plan at fixed controls: **bypass 20 m³/h, truck 22 m³/h, curtail 10 m³/h**, while Tank B transfers 35 m³/h to treatment. Then Tank A has balance \(52-20-22-10=0\) and Tank B gains \(16+20-35=1\) m³/h, so over 3 h Tank B rises from 114 to 117 m³, below its capacity. Combined treatment flow is 35 m³/h, below 60.

For the **bad-reroute downstream-capacity** visual, first simulate the 25 m³/h bypass while the B→treatment pump runs at 35 m³/h. Tank B then accumulates \(16+25-35=6\) m³/h from 114 m³ and breaches 130 m³ after **16/6 ≈ 2.667 h**; Tank A also breaches earlier unless other controls are applied. To isolate a *specifically downstream* failure, try **bypass 25 + trucking 22 + curtail 5**: Tank A is balanced, but Tank B still breaches at ~2.667 h. That is an excellent judge-facing explanation of why locally plausible fixes fail.

### Data contract example

```json
{
  "scenario_id": "pump_failure_v1",
  "synthetic": true,
  "units": {"time": "hour", "volume": "m3", "flow": "m3_per_hour", "concentration": "mg_per_L"},
  "horizon_h": 3.0,
  "dt_h": 0.025,
  "tanks": [
    {"id": "tank_a", "capacity_m3": 120, "initial_m3": 80, "inflow_m3h": 52},
    {"id": "tank_b", "capacity_m3": 130, "initial_m3": 114, "inflow_m3h": 16}
  ],
  "assets": [
    {"id": "main_pump", "source": "tank_a", "target": "treatment", "capacity_m3h": 55, "availability": 0},
    {"id": "bypass", "source": "tank_a", "target": "tank_b", "capacity_m3h": 25, "availability": 1},
    {"id": "backup_pump", "source": "tank_b", "target": "treatment", "capacity_m3h": 35, "availability": 1}
  ],
  "treatment_capacity_m3h": 60,
  "trucking_max_m3h": 22,
  "curtailment_max_m3h": 15,
  "quality": {"enabled": false, "base_mg_L": 0.2, "max_mg_L": 1.0},
  "cost_units": {"truck_per_m3": 80, "curtail_per_m3": 250},
  "incident": {"kind": "pump_failure", "asset_id": "main_pump", "time_h": 0.0}
}
```

**Important detail:** scenario `availability=0` represents the **incident-state** configuration. Add a healthy-state copy or an incident toggle that restores `availability=1` while also specifying valid pre-incident operating setpoints. Avoid having the UI claim a pump is healthy while the solver has already set its capacity to zero. The backup pump's actual flow setpoint must be supplied explicitly in each candidate plan (default 16 m³/h when merely balancing the healthy Tank B inflow, or 35 m³/h when actively relieving storage).

---
## 9. Reusable differentiable simulation framework

The initial code should **not** hard-code business decisions into the integrator. Implement a minimal reusable SciML abstraction, then provide BREAKWATER-specific models and constraints.

### 9.1 Core protocol

```python
from dataclasses import dataclass
from typing import Protocol
import torch

Tensor = torch.Tensor

class DynamicalSystem(Protocol):
    def initial_state(self, parameters: dict) -> Tensor: ...
    def derivative(
        self, t: Tensor, state: Tensor, controls: Tensor, parameters: dict
    ) -> Tensor: ...
    def observe(self, state_history: Tensor, parameters: dict) -> dict: ...
    def diagnostics(self, state_history: Tensor, parameters: dict) -> dict: ...

@dataclass(frozen=True)
class RolloutConfig:
    horizon_h: float
    dt_h: float
    dtype: str = "float64"
    method: str = "rk4"

@dataclass
class RolloutResult:
    times: Tensor
    state_trajectory: Tensor
    observable_trajectory: dict
    diagnostics: dict
```

The protocol is a **recommended design**, not an existing library API. The implementation can refine shapes, add batch dimensions, and handle control schedules if tests prove it necessary. Never pass a mutable singleton scenario into concurrent what-if jobs.

### 9.2 Required model adapters

1. `TankNetworkSystem`: \(\dot V=s-d+Bq\) and scenario-based constraints.
2. `ToyLinearSystem` or `PredatorPreySystem`: independent two-state ODE used as a smoke test for the generic interface. This matters because a wrapper around only the BREAKWATER code is not meaningfully general.
3. `Transport1DSystem`: optional differentiable PDE update object that consumes selected edge-flow time series and returns concentration fields.

### 9.3 Simulation composition

```python
network = TankNetworkSystem(graph=graph_spec)
network_run = simulate_ode(network, controls=control_schedule, config=cfg)

# One pipe, coupled via network flow; can be turned off independently.
transport = AdvectionDiffusionReaction1D(pipe_spec)
quality_run = transport.simulate(
    velocity_schedule=network_run.observable_trajectory["pipe_velocity_mph"],
    inlet_concentration_schedule=inlet_tracer,
    config=transport_cfg,
)

report = validate_run(network_run, quality_run, constraints)
```

**Generalization contract:** adding a new ODE system should require implementing the protocol and a new UI adapter, not rewriting RK4, gradient checking, run management, or optimizer loops. A truly generic PDE engine is intentionally **out of scope**; `Transport1DSystem` is a focused PDE extension point.

### 9.4 Unit discipline

Do not use implicit conversions:

- Volume: `m3`; flow: `m3_per_hour`; time: `hour`.
- Pipe length: `meter`; area: `m2`; transport speed: `meter_per_hour`; effective dispersion: `m2_per_hour`.
- Concentration: `mg_per_L`; ensure the same units across loss, boundary data, plots, and threshold checks.
- If converting tracer concentration to constituent mass, **1 mg/L = 1 g/m³**; document any conversion explicitly.
- Reference chemistry/quality values are synthetic, not measured regulatory limits.
- If normalizing tensors for ML, record scales and reverse-transform before display or validation.

### 9.5 Differentiability contract

Continuous differentiable operations may include: smooth control parameterization, graph ODE integration, most torch-based PDE operations, learned field predictions, and objective accumulation. Operations **not** differentiated: natural-language planning, function dispatch, discrete incident injection, hard feasibility classification, topology changes, human approval, and files/database writes. This boundary must be explicit in docs and demo.

### 9.6 Numerical integrity safeguards

- Use `float64` for scientific reference and optimization where feasible; benchmark lower precision separately.
- Fail fast on NaN/Inf, negative volumes beyond tolerance, inconsistent arrays, unavailable edges, or impossible capacities.
- Save unrounded scientific values, round only in UI.
- The `reference_solver` must not import Nemotron, PhysicsNeMo, or frontend code.
- Batch candidate evaluation only after single-candidate correctness; batched tensors can hide cross-candidate state leakage.
- Adopt one consistent time indexing convention: `times[0]=incident_time`; `trajectory[n]` is the state **at** `times[n]`.

---

## 10. NVIDIA PhysicsNeMo integration: authentic, small, and demonstrable

### 10.1 Current API reality (November 2026)

PhysicsNeMo's newer v2-style stack consolidates physics-informed utilities under the `physicsnemo.sym` namespace installed with the **`sym` extra**. NVIDIA's current guidance emphasizes defining equations in the training script and using explicit PyTorch training loops and the `PhysicsInformer` machinery, rather than copying older `physicsnemo-sym` `Domain/Solver/Constraint` examples unchanged.

Official starting points:

- Installation: https://docs.nvidia.com/physicsnemo/latest/getting-started/installation.html
- Physics addition / PDE definitions: https://docs.nvidia.com/physicsnemo/latest/user-guide/physics_addition.html
- PINN walkthrough: https://docs.nvidia.com/physicsnemo/latest/user-guide/pinns-tutorials/lid_driven_cavity_flow.html
- Current examples: https://docs.nvidia.com/physicsnemo/latest/physicsnemo/examples_index.html

**Do not hallucinate a `PhysicsNeMo` method name.** Inspect the installed library and the matching, current documentation before implementing. Keep the package version locked once a demo works.

### 10.2 P1 target

A functioning `PhysicsNeMoTransportPINN` with:

- model created with an actual PhysicsNeMo model class (for example, `physicsnemo.models.mlp.fully_connected.FullyConnected`);
- residual / PDE loss computed through `physicsnemo.sym` (`PhysicsInformer`) where practical, or a plainly documented PyTorch autograd residual with an actual PhysicsNeMo model;
- real training or fine-tuning, real checkpoint load, and inference on held-out \((x,t,\text{flow})\) points;
- evaluation against the deterministic FVM reference;
- reproducible training script, seed, runtime, loss curve, and prediction-vs-reference visualization.

**Minimum honest PhysicsNeMo claim:** “We used NVIDIA PhysicsNeMo to define/train the transport-field neural model and evaluate physics-based residuals.” Do not say “all simulation is powered by PhysicsNeMo” when the network ODE reference is PyTorch/NumPy.

### 10.3 Minimal implementation sequence

**Step A — verify install.** On Python 3.10+ with a compatible PyTorch environment:

```bash
python -m pip install nvidia-physicsnemo
python -m pip install 'nvidia-physicsnemo[sym]'
python -c "import physicsnemo; print(physicsnemo.__version__)"
python -c "from physicsnemo.models.mlp.fully_connected import FullyConnected; print('OK')"
```

Check platform-specific CUDA/OS notes. NVIDIA's official container or Linux/WSL may be less friction than native Windows in some setups. If setup fails, keep P0 working in a separate environment and record PhysicsNeMo as blocked; do not claim integration based on unexecuted code.

**Step B — create reference data.** Generate a small, deterministic family of 1D transport solutions over constant positive flow rates \(v\), smooth inlet pulses, and fixed \(D,\kappa\). Split train/validation/test by **parameter configurations**, not merely by random neighboring points from the same trajectory.

**Step C — build PINN.** Use normalized inputs such as \((x/L,t/T,v/v_0,a/a_0)\), one concentration output, and autograd derivatives with the correct coordinate rescaling. Test residual implementation on an analytic function before training.

**Step D — train.** Use Adam first; optional L-BFGS once stable. Limit experiments to a small number of epochs/steps bounded by hardware budget. Save only the best validation checkpoint, plus a short JSON metrics file. If PINN fronts are poor, widen pulse duration and reduce advection dominance rather than falsifying the error.

**Step E — visualize and compare.** UI panel: `FVM truth`, `PhysicsNeMo estimate`, `absolute error`, optionally `PDE residual`, with time slider. Expose model readiness state (`training`, `ready`, `failed_validation`, `unavailable`).

### 10.4 Explicit PhysicsNeMo model stub

```python
# Check the installed PhysicsNeMo release before using API details.
from physicsnemo.models.mlp.fully_connected import FullyConnected

model = FullyConnected(
    in_features=4,   # e.g. x, t, flow parameter, tracer amplitude
    out_features=1,  # concentration
)
# Build a real training loop, loss, optimizer and checkpointing.
# The model alone, without executed training/inference, is not an integration.
```

A hand-built residual can be used while `PhysicsInformer` is being integrated, provided the model is genuinely from PhysicsNeMo and the result is described precisely. Prefer `physicsnemo.sym` for a stronger scientific/demo story once the baseline is stable.

### 10.5 Why PINN rather than FNO for 24 hours?

PINNs can be trained on physics constraints without first generating a massive labeled simulation dataset. A *parameter-conditioned FNO* would be a compelling later improvement for many rapid repeated field forecasts, but requires dataset generation, training, and out-of-distribution evaluation. If the team has ample GPU time and P0/P1 are already working, use PhysicsNeMo's FNO modules to investigate that extension. Otherwise a small, rigorously compared PINN wins on speed of implementation and honesty.

### 10.6 What to do if PhysicsNeMo cannot train in time

- Deliver the reference FVM and learned model code **without claiming a working physics ML result**.
- Display `PhysicsNeMo unavailable` or `not validated` rather than a fabricated curve.
- Preserve a separate `scripts/train_physicsnemo.py` with exact instructions and error diagnostics.
- If an initial PINN runs but performs poorly, show the error honestly and use the FVM as authoritative.
- Never make critical plan verification depend on a new, unvalidated PINN.

---

## 11. NVIDIA Nemotron: agent architecture, not chatbot decoration

### 11.1 Suggested model and access

For a high-volume tool-calling agent, start with **Nemotron 3.5 Lightning**, exposed via NVIDIA's OpenAI-compatible Chat Completions API (currently documented with model identifier `nvidia/nemotron-3.5-lightning-30b-a3b`). Use an environment variable for the model name so it can be swapped to Nemotron 3 Super/other supported variants without refactoring. Lightning is a sparse MoE, hybrid Mamba/attention model optimized for repeated agent execution; that matters to the project because most tokens describe compact tool calls and results, not because the MoE experts are numerical solvers.

Official API docs:

- https://docs.api.nvidia.com/nim/reference/nvidia-nemotron-3-5-lightning-30b-a3b-infer
- https://docs.nvidia.com/nim/large-language-models/2.0.10/get-started/advanced/get-started-nemotron-3.5-lightning.html
- https://developer.nvidia.com/blog/nvidia-nemotron-3-5-lightning-delivers-fast-accurate-specialized-task-execution-for-long-running-agents/

For hosted API, use `base_url="https://integrate.api.nvidia.com/v1"` and `NVIDIA_API_KEY`. For local NIM/vLLM, update the base URL and follow vendor documentation on reasoning/tool parsers. The hosted endpoint is the hackathon default; **do not self-host a 30B model** merely for engineering prestige.

### 11.2 Roles (may be one model with distinct passes)

**Coordinator / Incident Analyst**

- Parses operator goal and incident.
- Calls state/asset tools.
- Explains what threatens continuity and what evidence is needed.
- Selects next tool calls; no fabricated telemetry.

**Candidate Planner**

- Proposes diverse combinations: bypass, trucking, curtailment, delayed repair.
- Explicitly includes a baseline and a tempting but unsafe plan.
- Invokes `simulate_candidate`, not a made-up numerical estimate.

**Verification Reviewer**

- Consumes actual constraint reports, compares results, and asks for stress tests.
- Does not decide violations using natural language alone.
- Can flag limitations and recommend no action if model is insufficient.

**Optimization Specialist (deterministic tool, not necessarily an LLM)**

- Uses gradient optimization or a small grid search to find candidate continuous controls.
- Returns solutions with solver status, bounds, feasibility and objective values.

For 24 hours, use **a single Nemotron orchestration loop** with role-specific prompts or a short reviewer pass, not three fragile independently hosted agents. What matters is authentic closed-loop planning and experimentation.

### 11.3 Minimum tool registry

Create JSON-schema function tools with validation and strict bounds. Suggested tools:

| Tool | Required inputs | Output |
|---|---|---|
| `get_network_state` | `scenario_id` | Tank volumes, pump availability, current flow setpoints, timestamp |
| `get_asset_constraints` | `scenario_id` | capacities, treatment limits, permitted interventions |
| `get_incident` | `incident_id` | failure type, affected assets, time, provenance |
| `forecast_baseline` | horizon, optional incident | forecast run ID, time-to-violation, timeline |
| `simulate_candidate` | bounded controls, scenario/horizon | run ID, predicted series summary, operating cost, violations |
| `optimize_intervention` | objective, declared constraints, bounded mode | ranked candidate IDs, optimizer diagnostics |
| `validate_candidate` | `run_id`, stress-case IDs | pass/fail, violated constraints, worst margins, residuals |
| `compare_candidates` | `run_ids` | cost, risk, constraints, feasible Pareto comparison |
| `submit_plan_for_approval` | validated candidate ID, rationale | approval ticket; **does not actuate** |

Use **6–8 tools** in the initial UI-integrated run; extras can be small wrappers. Every function must have a real backend implementation. No stub returning hand-authored “safe” or “optimal” verdicts.

### 11.4 Agent state machine

```text
USER INCIDENT REQUEST
    ↓
READ STATE (get_network_state, get_asset_constraints)
    ↓
BASELINE FORECAST (forecast_baseline)
    ↓
GENERATE 2–4 CANDIDATES (Nemotron)
    ↓
CALL simulate_candidate FOR EACH
    ↓
CALL validate_candidate FOR EACH
    ↓
NO VALID PLAN? ── yes → optimize_intervention → simulate → validate
    │                                                    │
    └───────────────────── compare/revise ◄──────────────┘
    ↓
RANK FEASIBLE PLANS BY MODELED COST / RISK
    ↓
RETURN EVIDENCE-BACKED RECOMMENDATION
    ↓
WAIT FOR EXPLICIT OPERATOR APPROVAL
```

**Bounded execution:** max ~10–15 tool interactions, max 2–3 refinement rounds, per-call timeouts and an overall request deadline. The app must continue to respond even if Nemotron fails; return a structured failure, retain all completed simulation results, and offer a **clearly labeled deterministic/manual workflow**. Do not pretend a scripted fallback is a live model execution.

### 11.5 Tool safety and grounding rules

- Tools accept only scenario IDs and bounded numeric controls. No direct arbitrary Python, shell, SQL, file writes, or network access from the model.
- The model can suggest a candidate; code enforces all input limits and refuses invalid units/unknown assets.
- Each tool result includes `run_id`, `scenario_hash`, `validity`, and `warnings`; the final recommendation must cite these IDs.
- On missing evidence, the model must call a tool or state uncertainty—not invent measurements.
- Validation results are final; the LLM cannot change a failing report to `pass`.
- `submit_plan_for_approval` only creates a ticket; a separate authenticated/operator click is required to approve.
- Log inputs/outputs with sensitive API keys redacted.
- If the model proposes operating outside the supported scenario domain, reject and explain unsupported action.

### 11.6 Example tool schema

```json
{
  "type": "function",
  "function": {
    "name": "simulate_candidate",
    "description": "Run a synthetic BREAKWATER what-if intervention from the immutable incident state. Does not control real assets.",
    "parameters": {
      "type": "object",
      "properties": {
        "scenario_id": {"type": "string"},
        "bypass_m3h": {"type": "number", "minimum": 0, "maximum": 25},
        "backup_treatment_m3h": {"type": "number", "minimum": 0, "maximum": 35},
        "trucking_m3h": {"type": "number", "minimum": 0, "maximum": 22},
        "curtailment_m3h": {"type": "number", "minimum": 0, "maximum": 15},
        "horizon_h": {"type": "number", "minimum": 0.25, "maximum": 6}
      },
      "required": ["scenario_id", "bypass_m3h", "backup_treatment_m3h", "trucking_m3h", "curtailment_m3h", "horizon_h"],
      "additionalProperties": false
    }
  }
}
```

Do not rely solely on JSON Schema's numeric bounds: **repeat checks on the server** with Pydantic and scenario-specific capacities. The example limits are for `pump_failure_v1`, not universal hard-coded limits in the generic tool implementation.

### 11.7 Suggested runtime code shape

```python
from openai import OpenAI
import os

client = OpenAI(
    base_url=os.environ.get("NVIDIA_BASE_URL", "https://integrate.api.nvidia.com/v1"),
    api_key=os.environ["NVIDIA_API_KEY"],
)
model = os.environ.get("NEMOTRON_MODEL", "nvidia/nemotron-3.5-lightning-30b-a3b")

# `tools` contains the real JSON schemas above.
# `messages` contains system policy + operator request + previous actual tool results.
response = client.chat.completions.create(
    model=model,
    messages=messages,
    tools=tools,
    tool_choice="auto",
    temperature=0.2,
    max_tokens=2048,
)
message = response.choices[0].message
# Process message.tool_calls with an allowlisted dispatcher, validate JSON args,
# execute the real tool, append role="tool" with matching tool_call_id,
# and continue the bounded loop. Never execute function names by eval().
```

The NVIDIA API surface may vary by model/server revision; adapt fields to the verified live endpoint. Some local Nemotron serving configurations need specific tool-call parsers, but hosted access avoids much of this infrastructure. The app should display `Model: {actual_model_id}` from live response/config, not a hardcoded logo as proof of use.

### 11.8 Prompt design for orchestration

**System/developer prompt essentials:**

> You are BREAKWATER's industrial decision-support planner. You work exclusively with synthetic scenarios. You can investigate incidents and propose interventions using registered tools, but you cannot determine physical safety or feasibility from text alone. Always get state, test candidate actions, request independent validation, compare feasible options, and cite returned run IDs. Never claim a plan is safe if validation fails. If every plan fails, report infeasibility clearly. Do not claim to actuate infrastructure; request operator approval. Optimize modeled operating cost subject to hard limits, not vice versa. Be concise and numerically grounded in final recommendations.

**Recommended test prompts:**

1. “The main transfer pump failed. Can we avoid a shutdown over the next three hours?”
2. “Why can't we just open the bypass all the way?”
3. “Find the cheapest validated combination of trucking, rerouting, and temporary production reduction.”
4. “Stress-test that plan against higher inflow and a weaker backup pump.”
5. “What evidence supports this recommendation, and which assumptions are weakest?”

### 11.9 Meaningful agents vs decorative agents

A judge should be able to see **the agent making a choice that was not a fixed UI macro**: it requests a baseline, tests an initially plausible reroute, receives a failed validation report, revises its strategy, calls optimization, and returns the feasible candidate with evidence. Show tool names, execution times, run IDs, and interpreted results in the console. Do not expose hidden reasoning traces; a concise step-level rationale and tool log is sufficient.

---

## 12. Optimizer implementation details

### 12.1 First build a trustworthy baseline

Implement `enumerate_candidate_controls()` with a deterministic, small grid or hand-labeled candidate set. For each candidate, run the reference model, validate it, compute cost, and rank **only feasible** candidates. This is useful even if autograd gets stuck.

```python
candidate_controls = [
    {"bypass_m3h": 0, "trucking_m3h": 0, "curtailment_m3h": 0, "backup_treatment_m3h": 35},
    {"bypass_m3h": 25, "trucking_m3h": 22, "curtailment_m3h": 5, "backup_treatment_m3h": 35},
    {"bypass_m3h": 20, "trucking_m3h": 22, "curtailment_m3h": 10, "backup_treatment_m3h": 35},
]
```

The first should overflow Tank A; the second should show a downstream Tank B problem around 2.67 h; the third is a candidate that is feasible for the **nominal synthetic** settings. Verify this with tests rather than leaving the expected behavior as mere prose.

### 12.2 Differentiable optimization (P1)

- Represent operating setpoints as 3–4 PyTorch scalar parameters, mapped into valid ranges.
- Initial point: a known feasible or nearly feasible plan, not arbitrary huge random values.
- Run `simulate_torch`, compute smooth operating objective + penalties, `loss.backward()`, update using Adam.
- Use a small fixed iteration budget and a tolerance-based early stop.
- Check gradients on a small analytic case before trusting optimization.
- Convert results to ordinary numeric controls, **re-run the reference simulator**, and produce a validator report.
- Rank candidates by **reference-validated** modeled cost. If none pass, return infeasible.
- Log optimization trace: objective vs iteration, worst constraint violation, gradient norm, final controls.

### 12.3 Optional specialized algorithms

- `scipy.optimize.minimize` SLSQP / trust-constr for small smooth parameterizations.
- Grid/random or Bayesian search for 3–4 continuous dimensions when gradients fail.
- Mode enumeration for the discrete “repair now / repair later” alternatives.
- Robust min–max objective across stress cases only if the model is fast enough and the specification requires robust feasibility.

No need to implement a second-order Hessian solver in this hackathon. The user's prior experience with Adam/L-BFGS can help, but good constrained optimization plus independent verification is more valuable than chasing optimizer sophistication.

### 12.4 Correctness / fairness comparison

Report for baseline, unsafe reroute, manual feasible example, and optimized candidate:

- first violation timestamp and asset;
- minimum remaining storage headroom;
- total synthetic cost units;
- total trucked volume and curtailed production;
- total modeled treatment throughput;
- wall-clock solve time and method name;
- condition (nominal / robust) and validation status.

Do not report “X% improvement” unless comparisons use the **same initial state, horizon, solver tolerance, and cost model**.

---

## 13. Backend API contract

Use FastAPI + Pydantic. The following routes are proposals that can be simplified if needed, but **agree on request/response schemas before frontend/backend work diverges**.

| Method | Route | Purpose |
|---|---|---|
| `GET` | `/api/health` | API alive, PhysicsNeMo/Nemotron availability, solver versions |
| `GET` | `/api/scenarios` | Available synthetic scenarios |
| `GET` | `/api/scenarios/{id}` | Scenario topology, capacities and initial state |
| `POST` | `/api/incidents/inject` | Create an incident-state snapshot from a selected scenario |
| `GET` | `/api/state/{scenario_id}` | Canonical state/asset telemetry |
| `POST` | `/api/simulate` | Deterministic what-if rollout; returns run ID and series |
| `POST` | `/api/optimize` | Candidate search and optional gradients |
| `POST` | `/api/validate` | Validate run against declared hard constraints |
| `POST` | `/api/agent/run` | Nemotron tool orchestration for operator request |
| `GET` | `/api/runs/{run_id}` | Full results and provenance |
| `POST` | `/api/plans/{plan_id}/request-approval` | Stage plan, never execute it |
| `POST` | `/api/plans/{plan_id}/approve` | Local simulated operator approval, not equipment command |
| `GET` | `/api/events` | Optional server-sent events for agent and simulation progress |
| `GET` | `/api/physics/transport/{run_id}` | FVM and optionally PhysicsNeMo grid/error values |

### Request example

```json
{
  "scenario_id": "pump_failure_v1",
  "incident_id": "fail_main_001",
  "horizon_h": 3.0,
  "controls": {
    "bypass_m3h": 20.0,
    "backup_treatment_m3h": 35.0,
    "trucking_m3h": 22.0,
    "curtailment_m3h": 10.0
  },
  "solver": "reference"
}
```

### Response example (schema illustration; not a claimed executed result)

```json
{
  "run_id": "run_00042",
  "scenario_id": "pump_failure_v1",
  "scenario_hash": "sha256:...",
  "synthetic": true,
  "solver": "reference_euler_v1",
  "status": "completed",
  "time_h": [0.0, 0.025, 0.05],
  "volumes_m3": {"tank_a": [80.0, 80.0, 80.0], "tank_b": [114.0, 114.025, 114.05]},
  "validity": {"pass": true, "violations": [], "max_mass_residual_m3": 0.0},
  "cost": {"value": 12780.0, "unit": "synthetic_cost_units"},
  "warnings": ["Synthetic setpoint-based flow model; not industrially calibrated."]
}
```

The illustrative cost of **12,780 synthetic units** comes from trucking (22×3×80 = 5,280) plus curtailment (10×3×250 = 7,500), with all other cost terms set to zero. The actual API must compute this from controls and scenario configuration rather than hard-coding it. The shown volume trajectories are an abbreviated three-sample excerpt, not a complete horizon. Any additional energy or repair costs must be explicitly modeled and included.

### Provenance and error handling

Every response needs `status` (`queued | running | completed | failed | cancelled` where applicable), `solver_version`, `synthetic`, `warnings`, and an error object on failure. Use HTTP 422 for invalid input, 404 for unknown IDs, and a clear service-unavailable response when a required external model/API is offline. Avoid an agent request blocking the entire UI thread.

**Streaming:** SSE is sufficient for a hackathon. Emit events like `incident_detected`, `baseline_completed`, `tool_started`, `tool_completed`, `candidate_failed`, `candidate_passed`, `recommendation_ready`, `approval_requested`. Do not require SpacetimeDB as a prerequisite for streaming the demo.

---

## 14. Frontend product requirements

### Primary visual layout

```text
┌──────────────────────────────────────────────────────────────────────────┐
│ BREAKWATER   ● Synthetic Live Twin  Incident: Pump Failure   [Reset]    │
├─────────────────────────────────────┬────────────────────────────────────┤
│ Interactive network graph            │ Key metrics / alert summary        │
│                                      │ Tank A: 80/120 m³                  │
│ Producer → Tank A ─x─→ Treatment     │ Predicted overflow: 46 min         │
│             │                        │ Tank B: 114/130 m³                 │
│             ↓ bypass                 │ Active constraints: 6/6           │
│ Producer → Tank B ─→ Treatment       │ Synthetic modeled cost             │
│            Truck route               │                                    │
├─────────────────────────────────────┴────────────────────────────────────┤
│ Time scrub + tank-volume trajectories + predicted limit crossings         │
├──────────────────────────────────────────────────────────────────────────┤
│ Candidate plans                  │ Nemotron agent trace                    │
│ [Unsafe reroute] FAILED         │ get_network_state ✓                    │
│ [Mixed controls] PASSED         │ simulate_candidate ✓                   │
│ [Optimized] ...                 │ validate_candidate ✓                   │
├──────────────────────────────────┴────────────────────────────────────────┤
│ Quality transport: distance × time heatmap / FVM vs PhysicsNeMo / error   │
└──────────────────────────────────────────────────────────────────────────┘
```

### UI components

1. **Network canvas:** use React Flow, SVG, or another lightweight graph renderer; show tanks with animated fill level, pump arrows, status colors, flow amounts, and a treatment node. Avoid spending hours on photorealistic 3D before P0.
2. **Time slider:** initial time, future forecast, and side-by-side before/after states; 3-hour default.
3. **Failure injection:** one-click main pump failure and reset; events clearly marked synthetic.
4. **Intervention sliders:** bypass, backup throughput, trucking, curtailment; bounds sourced from backend scenario.
5. **Forecast chart:** tank volumes vs time with capacity lines, first-violation flags, and hover values.
6. **Candidate cards:** feasible/infeasible, cost, first violation, objective, run ID, robustness status.
7. **Agent activity:** function call step, input summary, returned result, timing, validated reason for recommendation.
8. **Human approval button:** `Request approval` / `Approve simulated plan`; irreversible production actions do not exist.
9. **Physics panel:** concentration heatmap and model comparison; display FVM vs PhysicsNeMo error even if that exposes model limitations.
10. **Provenance ribbon:** `SIMULATED DATA`, model version, `REFERENCE` or `ML ESTIMATE`, solver health.

### Visual polish, but honest

- Color-coded flows: healthy cool tone; failed pump red; constrained route amber; approved solution green; never color a failed candidate green.
- Animated liquid motion is illustrative; numerical time series remain the source of truth.
- Use clear units in every chart and tooltip.
- Highlight the **domino effect**: the naive action may improve Tank A but make Tank B worse.
- For live demo reliability, preload scenario and numerical results; if an actual Nemotron run fails, switch to a **“Replay previous tool trace”** mode that clearly says `Recorded replay`.
- Never render a prerecorded agent transcript as if it is current live inference.

### Accessibility and performance

- Reasonable desktop viewport 1366×768 or 1440×900.
- Keep animation separate from mathematical state; interpolate visuals between solver samples.
- Do not start GPU training on the frontend request path.
- Avoid overly fine color gradients without numerical legend.

---
## 15. Suggested repository structure and file ownership

```text
breakwater/
├── README.md
├── AGENTS.md                         # shared rules for all Codex sessions
├── BREAKWATER_CODEX_TRANSFER.md      # this document, canonical product intent
├── .env.example                      # API variables, never real secrets
├── .gitignore
├── pyproject.toml                    # Python tooling/pytest/ruff; lock dependencies
├── scenarios/
│   ├── healthy_v1.json
│   ├── pump_failure_v1.json
│   ├── tracer_pulse_v1.json          # optional second PDE demonstration
│   └── stress_cases.json
├── backend/
│   ├── app/
│   │   ├── main.py                    # FastAPI startup, routers, health
│   │   ├── config.py
│   │   ├── contracts/
│   │   │   ├── scenario.py            # Pydantic models / units
│   │   │   ├── simulation.py
│   │   │   ├── agent.py
│   │   │   └── approval.py
│   │   ├── sim/
│   │   │   ├── base.py                # generic DynamicalSystem interface
│   │   │   ├── integrators.py         # reusable PyTorch Euler/RK4
│   │   │   ├── tank_graph.py          # BREAKWATER Bq flow physics
│   │   │   ├── reference.py           # separate NumPy/reference rollout
│   │   │   ├── transport_fvm.py       # 1D advection-diffusion-reaction
│   │   │   ├── coupled.py             # graph ↔ selected pipe transport
│   │   │   ├── validators.py          # HARD safety constraints, no LLM
│   │   │   ├── optimizer.py           # baseline grid + torch optimization
│   │   │   └── systems/
│   │   │       └── toy_ode.py         # demonstrates general interface
│   │   ├── ml/
│   │   │   ├── physicsnemo_transport.py
│   │   │   ├── physics_losses.py
│   │   │   └── model_registry.py
│   │   ├── agents/
│   │   │   ├── nemotron_client.py
│   │   │   ├── orchestrator.py
│   │   │   ├── tool_definitions.py
│   │   │   ├── tool_dispatch.py
│   │   │   └── prompts.py
│   │   ├── store/
│   │   │   ├── scenarios.py
│   │   │   ├── runs.py
│   │   │   └── approvals.py
│   │   └── api/
│   │       ├── scenarios.py
│   │       ├── simulation.py
│   │       ├── agents.py
│   │       └── plans.py
│   ├── tests/
│   │   ├── test_mass_balance.py
│   │   ├── test_scenario_outcomes.py
│   │   ├── test_constraints.py
│   │   ├── test_reference_vs_torch.py
│   │   ├── test_gradients.py
│   │   ├── test_transport_fvm.py
│   │   ├── test_api_contracts.py
│   │   ├── test_agent_tools_fake_client.py
│   │   └── test_approval_gate.py
│   └── requirements.txt
├── frontend/
│   ├── package.json
│   ├── src/
│   │   ├── app/                      # routing and app shell
│   │   ├── api/client.ts
│   │   ├── types/contracts.ts        # matches backend schemas
│   │   ├── components/
│   │   │   ├── NetworkCanvas.tsx
│   │   │   ├── TankTimeline.tsx
│   │   │   ├── InterventionControls.tsx
│   │   │   ├── CandidateComparison.tsx
│   │   │   ├── AgentTrace.tsx
│   │   │   ├── PhysicsField.tsx
│   │   │   └── ApprovalPanel.tsx
│   │   └── data/                     # clearly labeled temporary UI fixtures
│   └── public/
├── scripts/
│   ├── run_demo.py
│   ├── train_physicsnemo.py
│   ├── evaluate_physicsnemo.py
│   ├── benchmark_reference.py
│   └── generate_demo_trace.py
├── artifacts/
│   ├── checkpoints/                 # generated, ignored if large
│   └── demo_runs/                   # small provenance-rich fixtures
├── native/                           # optional, not needed by P0
│   ├── CMakeLists.txt
│   └── graph_flow.cpp
├── proofs/                           # optional Lean 4 project
│   ├── lakefile.toml
│   └── Breakwater/Conservation.lean
└── docs/
    ├── STATUS.md
    ├── API.md
    ├── SCIENCE_VALIDATION.md
    ├── DEMO_SCRIPT.md
    └── DECISIONS.md
```

The file structure is a recommendation, not a reason to create dozens of empty files. Implement only modules that have working functions; favor incremental packages over scaffolding a large dead codebase.

### Two-human / two-Codex work split

**Developer A: computational core, API, scientific validation**

- Own `backend/app/sim/**`, `backend/app/contracts/**`, scenario fixtures, API entrypoints, tests, and optimizer.
- Own or supervise PhysicsNeMo/optional C++/Lean after P0.
- Deliver stable `/api/simulate`, `/api/validate`, `/api/optimize` contracts early.

**Developer B: user experience, Nemotron orchestration, end-to-end demo**

- Own `frontend/**`, `backend/app/agents/**`, agent API routers, demo script and integration UI.
- Build attractive network diagram and time charts with fixtures while backend stabilizes.
- Connect real tool results and approval flow as endpoints land.

**Shared or designated integrator**

- One human is final reviewer of integration PRs and changes to cross-cutting contracts.
- Both can add tests, but neither should rewrite the other's core modules without coordination.
- Each Codex run must report touched files, new interfaces, tests executed, known limitations, and the next needed merge.

### Coordination rules for `AGENTS.md`

Create a short repository-level `AGENTS.md` mirroring these rules:

1. Read `BREAKWATER_CODEX_TRANSFER.md` before making architectural changes.
2. Preserve a working demonstration; all new work is feature-flagged when appropriate.
3. Do not modify `/backend/app/contracts` without coordinating with frontend owner.
4. Do not invent NVIDIA SDK APIs, run results, citation metrics, or passing tests.
5. Use real error handling, typed models, deterministic seeds, and precise units.
6. Every change must include tests or a manual validation recipe.
7. Do not check in API keys, field/company secrets, or large model weights.
8. Keep `docs/STATUS.md` truthful.
9. Main branch must build and pass key tests; submit small PRs or integrate frequently.
10. Prefer scope reduction over a visually impressive but scientifically broken feature.

### Concurrent Codex execution

Use one GitHub repository, two branches or worktrees, shared `AGENTS.md` and API schemas, and frequent merges. For two laptops on the same Wi-Fi, **GitHub is the coordination source of truth**, not local filesystem races or a complex new multi-agent memory platform. Separate file ownership minimizes Git conflict. Store decisions in `docs/DECISIONS.md` and use clear PR/commit messages (`core: add volume ODE`, `agent: add simulation tool call`, `ui: plot candidate constraints`). Integrate every 30–60 minutes and stop parallel development for final rehearsal.

---

## 16. Optional C++ acceleration: when, where, and what not to promise

### C++ candidate kernels

Potential future hotspots:

1. Sparse incidence-matrix flow updates on large networks.
2. Many-batch candidate rollouts.
3. Finite-volume 1D scalar transport on fine meshes.
4. Adjoint/Jacobian-vector products for parameter optimization.

However, the P0 network has **only a few state variables**. In this regime, Python orchestration, LLM latency, plotting, and API overhead may dominate. **Do not spend hours writing a C++ extension for a computation that already runs in microseconds or milliseconds.** First record a reproducible benchmark with fixed array sizes and warm-up settings.

### Appropriate usage patterns

- **Reference native solver:** pybind11 C++ kernel computes independent forward simulations and verifies Python outputs; no claim of autograd through it.
- **Autograd-aware custom operator:** implement forward and backward with a PyTorch C++/CUDA extension or explicit adjoint if native computation truly dominates; requires gradient tests and is higher risk.
- **Pure Python/Torch baseline:** good default; GPU tensor operations can already be fast for enough parallel candidates.

**Never** convert a differentiable tensor to NumPy, call native code, and then imply that `loss.backward()` still differentiates through it. The mathematical gradient path is broken unless a backward rule is supplied.

### Concrete microbenchmark

Time `100`, `1000`, and `10000` candidate rollouts on a fixed graph; compare pure NumPy vectorization, PyTorch CPU, PyTorch CUDA if present, and optional C++. Report per-candidate latency, warm-up, hardware, numerical difference, and overhead. If C++ is slower end-to-end for the demo size, leave it off the critical path and document why.

### A defensible C++ contribution even if speed is marginal

A tiny C++17/20 **sparse transfer-update kernel** with exact equivalence tests is valuable as a verified engineering extension, but **only** if the working demo and primary integrations are already complete.

---

## 17. Lean 4 formal verification: meaningful, bounded theorem

### Why Lean fits this project

A graph-flow solver should not mysteriously create or destroy fluid as it moves between internal nodes. That invariant can be proved algebraically. This is a realistic formal-methods contribution and an intellectually interesting link to faster sparse update implementations.

### Starter theorem: one transfer cancels

For initial volumes \(V_A,V_B\), external net inflows \(a,b\), internal transfer \(q\), and step \(\Delta t\):

\[
V_A'=V_A+\Delta t(a-q),\qquad
V_B'=V_B+\Delta t(b+q).
\]

Then

\[
\boxed{V_A'+V_B'=V_A+V_B+\Delta t(a+b).}
\]

A small Lean theorem over rationals can be written as:

```lean
import Mathlib

theorem two_tank_transfer_conserves
    (VA VB a b q dt : ℚ) :
    (VA + dt * (a - q)) + (VB + dt * (b + q))
      = VA + VB + dt * (a + b) := by
  ring
```

The above is a **proposed proof target**; Codex must compile it with `lake env lean` in an actual pinned Mathlib project before labeling it verified. A general graph theorem would use the zero-column-sum incidence condition \(\mathbf 1^TB=0\) to prove volume invariance under internal edge transfers.

### Useful follow-on optimization theorem

If two implementations process the same edge list:

- **Dense:** compute \(Bq\).
- **Sparse:** for each edge `(src, dst, flow)`, decrement `src` and increment `dst`.

Prove that the dense incidence update equals the sparse edge update over exact rationals/reals for a valid graph representation. This is a **semantic correctness** result, not an automatic machine-level performance optimization. Measure speed separately.

### Formal proof does not certify production safety

- It does not prove pump curves, tank capacities, treatment chemistry, or sensor calibration.
- A proof over `ℚ` or `ℝ` does not eliminate IEEE floating-point roundoff.
- If source/target IDs are invalid or an edge is inserted twice in code, you need implementation-level tests.
- The proof should use valid explicit assumptions and a compiled project, not merely a screenshot of Lean source.

**P2 cutoff:** one small compiling theorem is a success; an ambitious end-to-end verified industrial twin is not a 24-hour goal.

---

## 18. Optional SpacetimeDB / shared real-time twin

SpacetimeDB may be attractive if a relevant specialized hackathon track rewards a shared stateful app or if physical hardware and multiple browsers need synchronized telemetry. However, **do not make it a dependency of the primary simulation or agent loop**. Start with in-process or SQLite/JSON run storage plus FastAPI SSE.

If adopted, store:

- `asset_telemetry` with scenario/run/time/state provenance;
- `incident_events` with injection and source tags;
- `simulation_runs` keyed by run ID;
- `agent_tool_events` with execution status;
- `approval_tickets` with explicit operator state.

Important implementation detail: current SpacetimeDB client documentation emphasizes generated SDK bindings and clients such as TypeScript/Rust; do **not assume** a native Python SDK supports the exact workflow without checking. Prefer using a TypeScript client for the frontend and a thin server-side bridge when necessary. Official documentation: https://spacetimedb.com/docs/clients/api/

A service outage must not erase the in-memory/disk reference demo. SpacetimeDB should be a replication/presentation improvement, not the scientific source of truth.

---

## 19. Optional physical demonstration

Available 3D printers and electronics create a potential physical artifact, but software comes first.

### Minimum physical twin

- 3D-printed two-tank / pipe/valve arrangement; label Tank A, Tank B, main pump, bypass, treatment.
- LEDs show healthy/failure/approved-plan state.
- Optional ESP32 with a simple read-only serial/WebSocket/SSE feed from the simulated UI.
- Optional servo moves an illustrative bypass valve in a **dry miniature model**.

### Guardrails

- No live oil, chemicals, pressure systems, or real industrial valves.
- Keep simulated state authoritative; hardware mirrors its visual state only.
- On serial/network loss, the physical model should freeze safely while the software demo continues.
- Any motor/servo actuation must be confined to the desk demonstration and controlled by a simple, bounded, user-approved local routine.

### Decision rule

Start hardware only when the UI, pump failure, invalid reroute, feasible plan, and Nemotron trace already work. A coherent dashboard plus one functional PhysicsNeMo panel is more important than fragile hardware.

---

## 20. Verification plan: science and engineering acceptance tests

**Every scientifically meaningful claim needs a test.** These tests are not optional polish.

### 20.1 Exact synthetic scenario behavior

| Test | Action | Required behavior |
|---|---|---|
| `baseline_failure_overflows_a` | Main pump off, no mitigation | Tank A capacity crossed at approximately 0.769 h |
| `max_bypass_can_hurt_b` | Bypass 25, truck 22, curtail 5, B outlet 35 | Tank A stable; Tank B exceeds 130 m³ at ~2.667 h |
| `mixed_plan_nominal_passes` | Bypass 20, truck 22, curtail 10, B outlet 35 | Tank A = 80, Tank B = 114 + t (m³), both safe for 3 h |
| `cost_computation` | Same mixed plan, 3 h | 12,780 synthetic cost units before any extra cost terms |
| `invalid_controls_rejected` | Bypass 100 m³/h or negative trucking | HTTP 422 / validated error; no silent clipping |
| `approval_gate` | Agent recommends plan | No applied/approved status until human calls approval endpoint |
| `missing_api_key` | Nemotron key absent | Truthfully report unavailable; numerical demo stays live |

For the exact reference runs, also compare analytic constant-flow solutions to the discrete integrator at multiple time steps.

### 20.2 Conservation tests

- With internal transfer only, \(\sum_iV_i\) constant to numerical tolerance.
- With declared exogenous inflow/discharge, total storage change equals integrated net external flux.
- Swapping source/target direction changes sign correctly.
- Randomly generated small connected graphs satisfy incidence column sums \(0\).
- Check at every time step, not only at the final time.

### 20.3 Graph solver convergence and gradients

- Compare Torch rollout to an independently coded NumPy reference on fixed controls.
- If dynamics vary smoothly over time, show that reducing `dt` stabilizes predictions.
- Check `torch.autograd.grad` against central finite differences for controls away from capacity kinks.
- Run two candidates concurrently and ensure no shared mutable-state contamination.
- For `float32` vs `float64`, verify changes within reasonable tolerance and document any degradation.

### 20.4 Quality PDE validation

- With \(v=0,D=0,\kappa=0\), field remains constant absent a source.
- With \(v=0,D=0,\kappa>0\), homogeneous initial concentration decays as \(c(t)=c_0e^{-\kappa t}\) within scheme error.
- With pure advection and constant inlet, fronts travel at approximately the specified speed; investigate numerical diffusion.
- For zero decay/source and appropriate closed boundary conditions, the finite-volume *transport mass balance* matches boundary flux to numerical tolerance. Do not claim strict tracer mass conservation with a physically open inlet/outlet unless boundary flux is accounted for.
- Check discretization sensitivity and reject nonphysical large negative concentrations beyond tolerance.
- Check PhysicsNeMo inference on unseen parameter configurations, compare RMSE/MAE/max error to FVM, and show residual plots.

### 20.5 Agent contract tests with a fake client

Do not require paid NVIDIA API calls for most automated tests. Mock the model **at the model interface**, while executing real tool implementations:

- Valid tool call and result round-trip.
- Unknown tool is denied.
- Malformed arguments and over-limit controls rejected.
- Model attempts to skip validation → orchestrator forces it or refuses to finalize.
- Tool timeout → error surfaced, not silently hidden.
- Final response cites valid run IDs only.
- Infeasible scenarios → no false “safe” plan.
- Request approval does not mark plan as approved.

Then run a **live integration smoke test** with a genuine NVIDIA API key and record the actual model ID and tool call trace; label the test live only if the endpoint executed.

### 20.6 UI and end-to-end acceptance

- Reset actually restores original state and clears previous intervention selection.
- Time scrubber displays the correct sampled state at time \(t\).
- A candidate's badge uses authoritative `validity.pass`, not a local heuristic.
- Cost units and volume units are visible.
- Failure and success alerts are keyboard/accessible where practical.
- The full demonstration runs through one local start script and can be repeated multiple times without restarting services.

### 20.7 Success metrics and measured evidence

For demo/reporting, collect:

- number of distinct tools actually called by Nemotron;
- number of candidate simulations actually executed;
- count and nature of violated constraints;
- reference solver wall time and candidate throughput;
- max conservation residual;
- prediction time-to-overflow error vs analytic case;
- optimizer cost compared to the hand-crafted feasible plan;
- PhysicsNeMo field error vs reference and training time;
- whether selected plan passed nominal and stress checks.

Only report measured numbers from current runs—not numbers suggested by the LLM or invented beforehand.

---

## 21. Safety, privacy, and credibility

This prototype could be mistaken for a real industrial operations product. Avoid that confusion.

### Required product disclaimers

- **“Synthetic industrial fluid network. Educational/research decision-support prototype. Not validated for field operations or equipment control.”**
- No real telemetry connections in the demo unless the team actually implements and verifies them; if it is a replay, label it `SIMULATED` or `RECORDED REPLAY`.
- Quality transport is a scalar proxy, not regulatory water-quality certification.
- An operator “approval” in this prototype is a local simulation workflow, not authority to actuate industrial hardware.
- Project should not imply endorsement, collaboration, or a safety sign-off by EOG or NVIDIA beyond actual hackathon participation.

### Operational security

- Keep `NVIDIA_API_KEY` in environment variables or secret manager; never embed in frontend bundles.
- Backend is responsible for calling NVIDIA API.
- Sanitize tool arguments, cap compute budget and candidate count, and prevent arbitrary tool execution.
- Mask keys in logs and record only essential synthetic data.
- For cloud hosting, set CORS only for approved demo origin(s); do not expose administrative endpoints publicly without authentication.
- Control permissions for approval endpoints in any deployment beyond a local demo.

### Scientific limitations

- Pumps are setpoint/capacity abstractions, not dynamic fluid-mechanical equipment models.
- Tanks are well mixed only where explicitly modeled; P0 tracks volumes, not fluid chemistry.
- The 1D transport PDE has deliberately simple boundaries and one tracer.
- PhysicsNeMo may extrapolate poorly beyond training flow regimes.
- Projected feasibility depends on scenario assumptions, precision, and the declared horizon; it is not guaranteed real-world safety.
- Cost inputs are arbitrary demo variables, **not operator financial estimates**.

---

## 22. Twenty-four-hour execution plan and go/no-go gates

The table is a **resource allocation guideline**, not a guarantee of completion time. Two people work in parallel; timings are from project start. Spend the last several hours on testing and presentation rather than beginning brand-new subsystems.

| Window | Developer A — core/science | Developer B — agent/UI | Integration gate |
|---|---|---|---|
| 0–0.5 h | Agree graph model, units, JSON fixture, API contract | Agree demo flow, frontend shell, roles | Shared schemas + file ownership finalized |
| 0.5–3 h | Graph ODE + reference + first tests | Graph canvas + sliders + fixture charts | Can simulate pump failure from CLI |
| 3–5 h | Hard constraints + scenario outcomes + `/simulate` | Connect API to canvas, render incident | **GATE 1:** baseline failure and bad reroute verified |
| 5–8 h | Grid-search candidates + `/validate` + provenance | Nemotron client + real tools + trace UI | **GATE 2:** live Nemotron invokes real `simulate_candidate` |
| 8–11 h | Differentiable Torch optimization, gradient tests | Comparison cards, human approval, clean UX | **GATE 3:** one validated feasible intervention |
| 11–15 h | PhysicsNeMo environment, 1D FVM, PINN training | Physics field panel, graphs, demo script | PhysicsNeMo real import + working checkpoint **or** transparent block |
| 15–18 h | PINN reference comparison, stress tests | Visual polish, SSE progress, agent robustness | Full demo integrated, all P0 tests green |
| 18–21 h | Optional bounded C++/Lean or fix issues | Optional physical miniature or fix issues | No unstable feature allowed onto main |
| 21–24 h | Freeze, benchmark, docs, screenshots | Rehearsal, backup replay, final pitch | Repeat demonstration from reset ≥3 times |

### Gate 1: by ~5 h

**Must have:** correct Tank A fill timer, a downstream-failing plan, and a known feasible plan from the same incident snapshot. If not, pause all ML and agent work until this is fixed.

### Gate 2: by ~8 h

**Must have:** actual model-initiated tool use against backend (not only UI button → Python). If credentials/API are inaccessible, maintain tool registry and a mock test, but mark the NVIDIA integration not live rather than claiming it.

### Gate 3: by ~11 h

**Must have:** a candidate plan that passes independent validation, with a real cost number and human approval stage. If gradient optimization is unstable, keep grid-search/reference plan as demonstrated optimizer and record gradient work as experimental.

### Gate 4: after ~18 h

**Freeze P0/P1.** Add P2 only if it cannot endanger demonstration stability, and do not replace a working simulator with an untested C++ kernel or a PINN whose error is unknown.

---

## 23. Example acceptance-oriented work tickets for Codex

Use small, mergeable tickets rather than the vague instruction “build the whole product.”

### Ticket CORE-01 — canonical scenario and simulator

**Deliver:** `pump_failure_v1.json`, typed Pydantic scenario, `TankNetworkSystem`, reproducible reference rollout.  
**Test:** initial failure crossing for Tank A at ~0.769 h, internal transfers conserve volume, invalid units/IDs rejected.  
**Not in scope:** Nemotron, PhysicsNeMo, UI, optimizer.

### Ticket CORE-02 — validator and candidate outcomes

**Deliver:** constraint engine with violations, first-failure timestamps, margins and modeled cost.  
**Test:** specifically reject bypass 25/truck 22/curtail 5 due to Tank B while accepting bypass 20/truck 22/curtail 10 for nominal horizon.  
**Not in scope:** gradient optimization.

### Ticket API-01 — integration contract

**Deliver:** `/api/scenarios`, `/api/incidents/inject`, `/api/simulate`, `/api/validate`, `/api/runs/{id}`, `/api/health` with typed payloads and documented examples.  
**Test:** API contract tests plus one `curl` flow from scenario to validated run.

### Ticket UI-01 — interactive network console

**Deliver:** graph visualization, failure injection, time plot, control sliders and feasibility display.  
**Test:** runs through real backend endpoint, reset restores state, dimensions/units match fixture.

### Ticket AGENT-01 — genuine Nemotron tool loop

**Deliver:** hosted client, bounded dispatcher, actual tool-call events, step trace, failed candidate → revision.  
**Test:** model integration smoke when key available; fake-client tests for no key, malformed tools, skipped validation, and no silent approval.

### Ticket OPT-01 — differentiable optimizer

**Deliver:** torch continuous-control optimizer + finite-difference gradients + reference validation.  
**Test:** improves cost or feasibility over at least one relevant non-optimized candidate; does not return a failed candidate as safe.

### Ticket PHYSICS-01 — quality FVM

**Deliver:** 1D transport solver, convergence/analytic tests, time-distance grid.  
**Test:** pure decay and constant-field cases, CFL enforcement, reproducible concentration visualization.

### Ticket PHYSICS-02 — PhysicsNeMo PINN

**Deliver:** actual PhysicsNeMo model, executed training, checkpoint, held-out reference comparison, visualization.  
**Test:** import/version, finite residual, training loss trend, error metrics, readiness flag.

### Ticket FINAL-01 — demo freeze

**Deliver:** start script, event replay with truthful label, `README`, `STATUS`, `SCIENCE_VALIDATION`, short pitch, and rehearsal checklist.  
**Test:** complete 90-second flow three consecutive times from clean reset.

---
## 24. Research references and current technical guidance

These references inform the design but **do not constitute an implementation**. Links were checked for relevance as of November 7, 2026; API signatures can still change. Codex should consult the specific documentation corresponding to installed package versions.

### NVIDIA / product documentation

1. **NVIDIA PhysicsNeMo installation and v2 architecture** — https://docs.nvidia.com/physicsnemo/latest/getting-started/installation.html  
   Relevant: official install, `nvidia-physicsnemo[sym]`, consolidation of earlier PhysicsNeMo-Sym into `physicsnemo.sym`, NGC vs pip deployment options.

2. **PhysicsNeMo: defining PDE residuals** — https://docs.nvidia.com/physicsnemo/latest/user-guide/physics_addition.html  
   Relevant: define PDE terms with SymPy and evaluate with PhysicsInformer; helps avoid legacy examples and invented APIs.

3. **PhysicsNeMo example catalog** — https://docs.nvidia.com/physicsnemo/latest/physicsnemo/examples_index.html  
   Relevant: 2D flow, Darcy/FNO, ML CFD, graph models, flood forecasting, active learning. Use selected techniques, not the complexity of the largest example.

4. **Nemotron 3.5 Lightning API** — https://docs.api.nvidia.com/nim/reference/nvidia-nemotron-3-5-lightning-30b-a3b-infer  
   Relevant: OpenAI-compatible `/v1/chat/completions` on NVIDIA hosted services.

5. **Nemotron 3.5 Lightning tool calling guide** — https://docs.nvidia.com/nim/large-language-models/2.0.10/get-started/advanced/get-started-nemotron-3.5-lightning.html  
   Relevant: local NIM/vLLM parsers, tool calling, model capabilities, and runtime caveats.

6. **Nemotron 3.5 Lightning technical overview (Aug. 2026)** — https://developer.nvidia.com/blog/nvidia-nemotron-3-5-lightning-delivers-fast-accurate-specialized-task-execution-for-long-running-agents/  
   Relevant: sparse MoE with roughly 30B total / 3B active parameters, agent-oriented execution, speculative decoding.

7. **SpacetimeDB current client SDK architecture** — https://spacetimedb.com/docs/clients/api/  
   Relevant only if shared realtime state becomes P2.

### Research papers (modern methods + warnings)

**A. PINN failure modes — Krishnapriyan et al., 2021**  
*Characterizing possible failure modes in physics-informed neural networks*  
https://arxiv.org/abs/2109.01050  
Takeaway: convection/reaction/diffusion PINNs can fail because the optimization problem becomes poorly conditioned. Motivation for smooth pulses, a classical reference solver, curriculum, and honest error visualization. It directly cautions against the naive claim that lower PDE loss guarantees high physical accuracy.

**B. Fourier Neural Operator — Li et al., 2020/2021**  
*Fourier Neural Operator for Parametric Partial Differential Equations*  
https://arxiv.org/abs/2010.08895  
Takeaway: neural operators learn maps from input functions/parameters to solution fields, making repeated multi-instance inference attractive. Good P2 path if enough training data exist; not the cheapest first working solution.

**C. Physics-Informed Neural Operator — Li et al., 2021**  
*Physics-Informed Neural Operator for Learning Partial Differential Equations*  
https://arxiv.org/abs/2111.03794  
Takeaway: combine operator-learning data with governing-equation constraints at potentially different resolutions; relevant for future parameterized pipe transport and simulation surrogates. Do not describe an ordinary PINN as a PINO.

**D. Physics-informed active learning for neural operators — Polanska et al., 2026**  
*Data-Efficient Neural Operator Training via Physics-Based Active Learning*  
https://arxiv.org/abs/2605.21348  
Takeaway: use physics residuals to select informative training simulations rather than generating parameters randomly. A compelling future link between Nemotron's high-level experiment planning and numerical active learning; note the acquisition computation itself should be numerical, not LLM token-by-token.

**E. Neural ODE — Chen et al., 2018**  
*Neural Ordinary Differential Equations*  
https://arxiv.org/abs/1806.07366  
Takeaway: continuous-time differentiable dynamics; use a correction term only when there is realistic data for system identification.

### Research-informed decisions for this hackathon

- Conventional graph-flow ODE first; it supplies a trustworthy base for what-if experimentation.
- Differentiate the simulation with PyTorch; do not force a surrogate into simple, already-fast dynamics.
- Reference FVM remains authoritative for contaminant transport because unvalidated PINNs can fail, especially for transport fronts.
- PhysicsNeMo PINN supplies an authentic learned physical-field module; its errors become part of the demo's scientific rigor.
- Keep a future extension to a parameter-conditioned neural operator once dataset sizes and training/evaluation time justify it.
- For optimization, use gradient-based methods on continuous controls and enumerate discrete topologies/modes.
- Use Nemotron for tool choreography and scientific reasoning, **not** as a numerical PDE integrator or a safety oracle.

---

## 25. Nemotron architecture detail: why it is suitable, and why not to overstate it

For architecture discussions, a sparse Mixture-of-Experts layer is conceptually:

\[
\mathrm{MoE}(x)=E_{\rm shared}(x)
+\sum_{i\in\operatorname{TopK}(g(x),k)}\alpha_i(x)E_i(x).
\]

Every selected expert contributes an entire output representation, and the weighted vectors are summed; the experts do **not** each own separate fixed output dimensions. Lightning uses sparse conditional expert activation (approximately **30B total / 3B active** per token), making repeated text/tool execution more compute-efficient than a similarly sized dense model in typical settings. Its hybrid Mamba/attention backbone mixes state-space sequence processing with attention, and its multi-token prediction machinery supports faster generation when serving infrastructure enables it.

A schematic state-space recurrence is:

\[
h_t=A_t h_{t-1}+B_t x_t,\qquad y_t=C_t h_t+D x_t.
\]

This is relevant to the **agent's throughput/long-context operation**; it does **not** mean the model automatically understands hydraulics, that its individual experts correspond to engineering domains, or that its generations are mathematically verified. In BREAKWATER, **the physics is explicitly encoded in solvers**. Nemotron controls which questions are asked and which experiments are executed.

If asked by a judge, answer in three layers:

1. **Nemotron (reasoning and tool use):** incident triage, experiment selection, constraint-aware revision.
2. **PhysicsNeMo (scientific ML):** learn a transport solution field while respecting physical residuals and compare to a reference.
3. **Differentiable simulator + deterministic validator (numerical truth):** evolve network state, optimize continuous interventions, and reject invalid plans.

---

## 26. Future roadmap (not included in 24-hour commitment)

### Near-term extensions

- **Calibrate unknown pump efficiencies** from noisy telemetry with system identification and a hybrid dynamics correction:

  \[\dot V=f_{\rm nominal}(V,u;\theta)+g_\phi(V,u),\]

  with conservation constraints and uncertainty propagation. The correction must not freely invent net fluid mass.
- **Learned operator for pipe transport:** a PhysicsNeMo FNO/PINO mapping flow/control/boundary histories to downstream concentration fields, used only after good held-out evaluation.
- **Closed-loop receding-horizon control:** resolve an optimization as new telemetry arrives, with all actions still requiring authorization.
- **Uncertainty-aware planning:** carry intervals over inflow, pump availability, source concentration, and sensor errors; evaluate worst-case feasibility or chance constraints under declared assumptions.
- **Anomaly detection:** reconcile estimated fluid balance with measured inflow/outflow to flag sensor faults or possible leaks, rather than automatically declaring a leak.
- **Inverse parameter estimation:** infer line dispersion, effective pump capacity, or unknown source concentration from synthetic observations.

### More ambitious scientific extensions

- **Hydraulic physics:** pressure/head-based network equations, pump curves, valves, and fluid properties.
- **Multiphase flow:** liquid, gas, and solids; potentially much more complex and not a drop-in one-PDE upgrade.
- **PhysicsNeMo graph surrogate:** learn network dynamics over changing topology with a graph architecture, subject to conservation constraints and numerical validation.
- **Adjoint-native C++/CUDA:** implement and prove differentiable sparse operations only if profiling shows large-batch numerical computation dominates.
- **Lean-verified invariants:** graph incidence, sparse update equivalence, bounded feasible controls under explicit assumptions; acknowledge that physical model mismatch still exists.
- **Multi-agent scientific laboratory:** dedicated hypothesis-generation, simulation, critique, and system-identification agents with a shared experiment ledger.

### Vision: general scientific simulation laboratory

The enduring reusable asset is not the BREAKWATER UI, but the **model/experiment/optimization/verification loop**:

```text
DynamicalSystem / PDEComponent
         +
Differentiable rollout and gradients
         +
Reference numerical validator
         +
Model/surrogate registry
         +
Nemotron experimental planner
         +
Auditable experiment history
```

With alternate model adapters, the same architecture could support thermal diffusion, chemical reactors, ecological ODEs, energy storage, and other dynamical systems. **For this event, however, only BREAKWATER is a polished end-to-end product.**

---

## 27. Copy/paste Codex kickoff prompt

The following text is intended to be supplied to the primary Codex agent **after this file has been placed at the repository root**. It should cause implementation, not just another planning document.

```text
You are the primary Codex implementation agent for BREAKWATER.

READ BREAKWATER_CODEX_TRANSFER.md IN FULL before writing code.

MISSION
Build a reliable 24-hour hackathon demonstration of an NVIDIA Nemotron-
orchestrated industrial fluid-infrastructure digital twin. It must diagnose
synthetic main-pump failure, simulate storage tank volumes, reject naive
reroutes that violate downstream constraints, find a feasible lower-cost
combination of bypass/trucking/curtailment, and require simulated operator
approval. Include a real PhysicsNeMo-backed 1D transport PINN if the P0
system and time budget permit. Keep a reusable differentiable PyTorch ODE
system interface separate from BREAKWATER business logic.

DO NOW — in this order
1. Inspect the repository. Do not overwrite existing work without reviewing it.
2. Create/update AGENTS.md, docs/STATUS.md, docs/DECISIONS.md only as needed.
3. Implement the scenario schema, synthetic fixture, reference tank ODE,
   conservation tests, hard validator, and candidate outcome tests.
4. Run tests; verify the 0.769 h Tank A failure, 2.667 h Tank B bad-reroute
   failure, and nominal feasible mixed-plan outcome.
5. Implement the minimal backend API contract and demonstrate via HTTP.
6. Coordinate contracts with the second agent before frontend integration.
7. Add a bounded tool-calling Nemotron orchestration loop using the real
   NVIDIA-hosted API when credentials are available. Tools must perform
   real simulations, validations, optimizations, and approval-ticket creation.
8. Add differentiable PyTorch control optimization and prove gradient flow
   with numerical checks; reference-validate every recommendation.
9. Implement real PhysicsNeMo training/inference for the transport PINN,
   with an authoritative finite-volume reference and held-out error chart.
10. Stop adding features and help integrate/test the 90-second demo.

STRICT RULES
- Do not claim the system is a real industrial controls application.
- Do not invent API SDK calls: verify NVIDIA docs and installed versions.
- No fake numerical outputs, fake model execution, or fake passing tests.
- Do not make the PINN or the LLM the sole safety validator.
- Do not attempt massive 3D CFD or full hydraulic multiphase physics.
- Do not route raw shell/python/eval capabilities through agent tools.
- Do not spend P0 time on C++ or Lean; those are post-integration options.
- Do not claim differentiation through discrete failures or topology changes.
- Do not create a forest of empty placeholder modules.
- Use real units, determinism, typed contracts, and error handling.
- Every recommendation must reference actual run IDs and validation reports.
- Keep docs/STATUS.md honest: working / experimental / planned / blocked.

COLLABORATION
There are two humans with two Codex sessions. Maintain separate branches
or worktrees and respect file ownership. Never push directly to a shared
main branch without integration review. Freeze contracts early and integrate
every 30-60 minutes. Report touched files, tests run, new endpoints, and
needed merges after each meaningful increment.

FIRST RESPONSE / FIRST ACTION
Briefly state the existing repository state and the P0 implementation
sequence, then start implementing CORE-01 and CORE-02 immediately.
Do not return only a plan. Run the tests and report their actual outputs.
When an essential external dependency (e.g. NVIDIA credentials) is missing,
continue with deterministic simulator/UI work and explicitly flag the missing
live integration; do not fabricate a success.

SUCCESS
The team can click Fail Main Pump, see a quantitative overflow forecast,
watch Nemotron make genuine tool calls, see a superficially good reroute
fail at Tank B, see a better independently validated plan, and approve a
simulated intervention. PhysicsNeMo provides a genuine scientific ML field
and measured comparison with a conventional transport solver.
```

---

## 28. Final definition of done

This is the checklist used before claiming a feature is implemented. Mark each item only after it has been exercised.

### Product and demo

- [ ] Healthy graph, incident injection, time slider, and tank-level visualization work from a clean start.
- [ ] Tank A baseline fill-time forecast matches the synthetic analytic case.
- [ ] At least one seemingly reasonable reroute fails downstream, with a clear asset-specific reason and timestamp.
- [ ] At least one mixed bypass/trucking/curtailment response passes nominal constraints.
- [ ] Cost, control values, units, solver provenance, and first-violation times are visible.
- [ ] Operator approval is explicit; the recommendation alone does not change displayed operating state.
- [ ] The 90-second demonstration has been repeated end-to-end at least three times after a clean reset.

### Mathematics and software

- [ ] Internal graph flows conserve fluid volume according to incidence-matrix accounting.
- [ ] Reference and PyTorch implementations agree within documented tolerance.
- [ ] Autograd gradients were numerically checked on non-kink test cases.
- [ ] All feasibility decisions come from independent reference validation.
- [ ] 1D transport FVM passes core analytic/convergence tests and exposes its limitations.
- [ ] Stress cases are evaluated or explicitly marked incomplete.
- [ ] API schemas are typed, and invalid inputs are rejected.
- [ ] All substantive tests are green on the final integration commit.

### NVIDIA integrations

- [ ] Nemotron initiated actual function/tool calls, not only returned text about intended actions.
- [ ] Tool log includes real results and run IDs.
- [ ] Agent responds correctly to an invalid candidate and cannot overrule failed validation.
- [ ] `NVIDIA_API_KEY` is not in the repository/frontend.
- [ ] PhysicsNeMo was actually imported and executed on the chosen runtime.
- [ ] A PhysicsNeMo model was actually trained/loaded and evaluated, not merely described.
- [ ] PhysicsNeMo prediction and numerical reference comparison are displayed with honest errors.
- [ ] If a live API or ML feature is unavailable, the demo/UI says so clearly.

### Delivery

- [ ] `README.md` includes local setup, environment requirements, launch sequence, and demo commands.
- [ ] `docs/STATUS.md` differentiates shipped/experimental/planned features.
- [ ] `docs/SCIENCE_VALIDATION.md` records equations, units, assumptions, numerical tests, error metrics, and limitations.
- [ ] `docs/DEMO_SCRIPT.md` includes 90-second and 3-minute walkthroughs.
- [ ] All required assets/fixtures are locally available for live judges.
- [ ] No unsupported real-world safety, speedup, partner, or commercial claims.

---

## 29. Final message to the team

**Ship one memorable, empirically verified story:**

> “The main pump fails. A normal control rule would send the fluid to another tank—but BREAKWATER sees that this only moves the shutdown downstream. Nemotron investigates with actual simulation tools, a differentiable digital twin evaluates mixed interventions, a numerical validator blocks the dangerous ones, and the operator gets a justified plan with explicit approval. PhysicsNeMo adds a physics-informed view of how contaminants or tracer concentrations travel along a critical route.”

A narrow demo that really does all of that is much stronger than a wide diagram containing Nemotron, PINNs, Neural ODEs, FNOs, C++, Lean, and a dozen agents that never produce and verify a result.

**Core design principle:** Let the model explore and explain; let mathematics simulate; let deterministic checks validate; let the human decide.
