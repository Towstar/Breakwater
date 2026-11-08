# BREAKWATER continuation plan

**Current B handoff:** B5a local runtime/deployment and B5b forward-only desktop storage
comparison are implemented in this workstream. Follow [DESKTOP_SCIENCE.md](DESKTOP_SCIENCE.md)
for setup/use and [STATUS.md](STATUS.md) for evidence and limitations. Next is B5c
explicit selected-interior derivatives, using the controls/column semantics below.
Automatic retention and release rehearsals remain. The earlier B5 plan is retained.

**Earlier handoff after PR #14:** A3–A8 are merged, including `capabilities` and
`storage_rollout` in the repo worker. The earlier execution sequence/checkpoints below
are retained as history; do not restart A's worker mapping. B's next increment is
deployment and desktop-adapter integration. See [STATUS.md](STATUS.md) and
[science/WORKER.md](../science/WORKER.md) for current scope and evidence.

## Next Person B task: B5 desktop storage cross-check

Plan after reviewing PR #14 (`d4cae03`, merged through `5c773f7`) and pulling main
`794c7cd`. The worker mapping is delivered; the next task is a desktop-to-PyTorch
calculation matched to an actual retained native forecast. Keep this first increment
focused on the current manual forecast and forward execution (`derivatives: false`).

### 1. Deploy and configure the real runtime

- Extend `desktop/CMakeLists.txt` installation to include `worker_storage.py` and the
  `breakwater_science` Python sources beside `worker.py`, excluding caches/environments.
- Provide reproducible local setup using A's existing hash-locked CPU environment and
  pinned Python 3.12.13; configure its executable through `BREAKWATER_PYTHON`. Keep
  schema-validation dependencies test-only and PhysicsNeMo outside this runtime gate.
- Test a clean install prefix with Windows-only PATH plus the explicitly configured
  interpreter. Preserve explicit missing-runtime/module errors and legacy health.

### 2. Extend the asynchronous process adapter

Extend `desktop/science/PythonWorkerClient` using its existing retirement, generation,
deadline and credential-redaction safeguards. Preserve `checkHealth()` and its tests.
Use a bounded single-job process for `capabilities -> storage_rollout -> capabilities`,
then close stdin and publish only after clean process exit. These exchanges must use
the same process: `execution_verified` starts false on every restart and becomes true
only after checked storage publication. `implemented: true` means the mapper is
deployed; it does not prove Torch can execute or block the first unverified attempt.

Match every request ID and method/version/state, handle partial and multiple frames,
and apply the 1 MiB **including newline** limit per UTF-8 frame. Bound total output to
the finite expected exchange and stderr separately. Retire on timeout, cancellation,
unexpected output or crash; retain no raw child diagnostics or model credentials.
Do not start a replacement process until the retiring job has exited.

### 3. Capture the actual native reference

Add a `StorageScienceController` and a bounded read-only `ProjectController` accessor
for the retained run's original configuration, tank order, times, volumes and cost.
`capturedRun()` currently lacks the step/reference arrays and derives horizon from
the last sample; store/capture the original horizon and step explicitly. Do not
reconstruct a reference from edited controls or hardcode a fresh grid.

Capture network, overrides, snapshot/run IDs, physical hash, numerical revision and
grid before launch. Bind completion to the complete payload and job generation.
Numerical edits, reset and a new forecast invalidate publication; layout/camera and
ordinary selection changes preserve the same captured evidence. Keep imported
history detached and defer candidate/history science to a later increment.

### 4. Validate, compare and display advisory evidence

Validate response envelope/provenance, exact captured identity, tank order, finite
arrays/cost, ordered sample indices and the requested grid. Reject missing, unknown,
wrong-sized or inconsistent data before display. Compare each returned worker sample
with the corresponding retained native index using the existing forward tolerance
`1e-9 + 1e-10 * scale`, reporting per-tank maximum error and cost difference. Preserve
negative and over-capacity predictions; numerical agreement is separate from feasibility.

One inherited grid edge can leave `min(k * step, horizon)` one ULP below the requested
horizon. Coordinate that native/A3 endpoint convention before adding strict floating
equality checks; retain the original requested configuration and actual indexed grid
without shortening, shifting or silently repairing either result.

Update the Science tab with separate unavailable/deployed/unverified/executed/error
states, worker provenance, tank trajectories and comparison diagnostics. Reuse existing
chart facilities where suitable. No result creates a native run, changes operating
controls, grants approval or triggers optimization/Nemotron tools.

### 5. Verify and deliver

- Fake-worker coverage: fragmented/coalesced frames, duplicate/extra responses, wrong
  IDs, malformed capabilities/results, per-frame/aggregate limits, timeout/cancel,
  crash after response, clean-exit gating and restart verification reset.
- Real configured-worker coverage: same-process verification false-to-true, canonical
  and renamed/custom networks, shortened final step and the endpoint edge, unclipped
  violations, strict parity, stale completions and unchanged controls/approval.
- Run installed-prefix smoke, combined normal/MSVC ASan checks and QML/render checks,
  then interactively verify responsive execution/cancel and readable comparison.
- Deliver a small PR with executable setup/demo instructions and actual local evidence.
  Completion requires a real desktop -> deployed worker -> PyTorch -> checked native
  comparison; upstream scientific results alone do not satisfy this gate.

### Following increment

Add explicit selected-interior-control sensitivities after the forward path passes.
The current `controls` array supplies both overrides and derivative columns. Sending
all stored boundary controls with `derivatives: true` correctly fails; dropping them
would change the forecast. Use captured effective settings and an explicit selection,
preserve full IDs/order, and never nudge values to create derivatives. Report Jacobian
units h and cost-gradient units synthetic units per (m3/h). Automatic science-history
retention, generic PhysicsNeMo/pipe inference and clean live-model rehearsals remain
separate work.

**Status after PR #6:** A3, A4, A5 and A6 below are implemented and merged.
This document retains their original execution sequence and acceptance criteria;
use [STATUS.md](STATUS.md) for current evidence. B3/B4 are implemented in this workstream;
the next integration is the agreed scientific worker mapping and adapter.
The subsequent A7 control-influence optimization is also integrated through main
`a707b32`; it does not add scientific worker methods or alter the next ownership boundary.

Updated November 7, 2026. This is the execution order for the next increments, based
on merged PRs [#1](https://github.com/Towstar/Breakwater/pull/1),
[#2](https://github.com/Towstar/Breakwater/pull/2),
[#3](https://github.com/Towstar/Breakwater/pull/3), and
[#4](https://github.com/Towstar/Breakwater/pull/4).
The [accepted implementation plan](IMPLEMENTATION_PLAN.md) remains the full scope;
the original handoff is preserved. There is no fixed event deadline.

Desktop foundation follow-up: the subsequently approved desktop-first increment now
implements automatic SQLite investigation history, comparison/charts, explicit plan
review and the health-only Python adapter. See [DESKTOP_FOUNDATION.md](DESKTOP_FOUNDATION.md)
and [STATUS.md](STATUS.md) for scope and executed checks. This completes the health
portion of B5 and investigation-retention portion of B7 below. A's A3–A6 source through
`7c99c68` is merged into this workstream, and B3 publishes the selected-pipe/science
boundary plus independent physical-parameter persistence. A8 subsequently added worker
capabilities/storage; B5a/B5b now deploy and integrate forward storage comparison.
The native pipe viewer is implemented. Selected-control derivative UI follows as B5c.
Hosted Nemotron access is verified; clean operator-review rehearsals remain incomplete.
The B7 native-evidence subincrement now reopens bounded transport exports as historical,
read-only evidence in the existing charts. Automatic quality-result retention is still
separate unfinished work; investigation sessions already have SQLite history.

The desktop now selects a pipe and captured forecast, executes authoritative native
tracer transport, and displays its outlet trace, field and mass ledger. The desktop
also includes deployed scientific storage comparison; the standalone
learned smoke does not supply arbitrary-route inference.

## Starting point

- Working: C++20/MSVC storage engine, dense/sparse/affine forecasts, independent
  validation, bounded candidate search, editable Qt Quick 3D application, native
  Nemotron tools/run store, and explicit simulated operator approval.
- Integrated: authoritative single-pipe finite-volume scalar transport, native
  example/CSV output and 18 transport case groups, now exposed by the B4 native viewer.
- Integrated A3–A6: standalone differentiable storage, A-reported fixed-case
  PhysicsNeMo execution, measured native benchmarks and checked exact-real proofs.
  A8 worker methods and B5 forward storage comparison are now integrated.
- Unfinished: desktop selected-control derivatives, scientific retention and generic
  pipe-quality integration/qualification. Native volume approval remains separate.
- Live hosted Nemotron calling is verified with archived traces; bounded no-plan and
  unstaged advisory results mean the three clean review rehearsals remain incomplete.

Volume feasibility stays authoritative in native C++. Quality/learned results remain
separate and experimental. Layout does not set physical length, pressure or flow.
The 4,800-unit search result is best among evaluated candidates over three hours;
it uses storage headroom and establishes neither global optimality nor sustainability.

## Start these two increments next

**Person A: scientific worker mapping and proof maintenance.** Standalone A3/A4,
A5 benchmarks and A6 proofs are integrated. Review B3's units/array semantics before mapping
scientific methods into the worker, keeping native feasibility authoritative. Continue
checked finite-graph algorithm proofs as the corresponding algorithms evolve. Their
exact-real scope does not certify compiled floating-point implementations or quality models.

**Person B: B5 scientific worker integration.** B3 contracts/persistence and B4 native
pipe viewing are implemented. Extend capability negotiation and adapters only for
actual worker methods supplied by A, preserving health-v1 and unavailable states.

Use the refreshed [A kickoff](KICKOFF_PERSON_A.md) and [B kickoff](KICKOFF_PERSON_B.md).
Deliver each numbered increment as its own tested PR rather than one combined change.

## Ordered work and acceptance gates

| Increment | Owner / paths | Deliverable and completion gate | Depends on |
|---|---|---|---|
| B3: science contracts | B: `contracts/`, shared docs | Publish request/result examples, errors, provenance, parameter persistence and bounded output rules described below. Existing projects and health requests remain readable. A reviews units and array semantics before adoption. | Current main |
| A3: differentiable storage | A: `science/` | Pin a working Python 3.12/Torch environment; generic `torch.float64` rollout, native comparison fixtures and gradient-check command. Canonical and renamed/custom graphs agree with dense/sparse/affine native results; finite differences agree with autograd. | Current main; B3 for worker exposure |
| B4: selected-pipe viewer | B: `desktop/`, integration tests | Select a route and captured volume run; enter physical pipe parameters, run native FVM asynchronously, show outlet trace/field preview, signed mass ledger and CFL. Direct native example and UI adapter agree. Edits/deletion/cancellation cannot attach stale results to another route/run. | B3; merged native transport |
| B5: Python adapter | B: `desktop/`, root setup/tests | Async `QProcess` health/capability negotiation, framed requests, cancellation/restart, deadlines and bounded output. Test missing runtime, malformed/oversized output, mismatched IDs, crashes and stale completions with fake workers; integrate actual A methods once available. | B3; A3 for real science execution |
| A4: genuine PhysicsNeMo smoke | A: `science/` | Execute a real model and PDE residual, bounded training, checkpoint round-trip and held-out inference/reference comparison. Record versions/device/seed/runtime/errors; report pass or failure against thresholds chosen before evaluation. | Native transport; resolved compatible environment |
| A5: measured performance | A: `engine/` | Benchmark 100, 1,000 and 10,000 candidates on canonical/custom graphs; separate sampled dense/sparse kernels, affine screening, reference validation and total search. Record repeated timings, hardware and candidate counts. | Current engine; schedule after A3 |
| A6: Lean algorithm proofs | A: `proofs/` | Pin Lean/Mathlib; first prove sparse/incidence equivalence and internal conservation, then recurrence/affine equivalence and interval endpoint bounds for finite graphs. `lake build`, no `sorry`, audited axioms and C++ mapping required. | Current engine equations; separate PRs |
| B6: live planning evidence | B: existing smoke tools/docs | When environment credentials exist, execute protocol and full investigation smoke on canonical and renamed/custom networks; retain actual model-issued calls and computed run IDs. Missing access remains explicit and does not block other increments. | Available hosted access |
| B7: durable evidence/release | B: serialization, desktop, delivery docs | Bounded versioned export/reopen of immutable runs and provenance with atomic writes; historical evidence requires fresh approval against current settings. Complete comparison views and reproducible setup/release evidence. | B4/B5 and relevant science methods |

A5 and A6 are delivered; future benchmark/proof changes remain separate reviewed increments.
Do not parallelize edits inside one owner's subtree without assigning exclusive files.
B6 has verified hosted access; clean operator-review rehearsals remain incomplete.

## B3 contract decisions to publish before integration

1. Capture route ID, volume-run/snapshot IDs, numerical revision/hash, control values,
   physical-parameter hash, units, method/version and warnings. Derive flow from the
   captured simulation, including pump availability; never use rendered pipe length.
2. Define physical length, cross-section, dispersion, decay, initial concentration,
   inlet schedule and supported source terms explicitly. Store them with backward
   compatibility; transport-parameter edits invalidate quality results without
   changing the fixed-flow storage equations. Record separate result identity.
3. Specify time/space ordering, cell-average concentrations, signed inlet/outlet/source/
   decay ledger, work limits and error states. Preserve the native boundary semantics:
   zero flow with a Dirichlet inlet can still exchange tracer by diffusion. Unsupported
   inputs fail explicitly; no clipping or silent fallback to a different boundary.
4. Keep worker protocol v1 health compatible. Publish additive capability/method
   payloads and request/response examples before either lane implements their mapping.
   Runtime package detection must remain distinct from successful scientific execution.
5. Respect the existing **1 MiB NDJSON line limit**. Native transport can produce up
   to four million field values, so return bounded display samples/diagnostics. If
   full artifacts are needed, define a bounded application-owned output directory,
   schema, size/hash checks and safe relative artifact references. Test size limits;
   do not put the full allowed field into an unbounded JSON line.

## Scientific checks for A3 and A4

- A3 includes time zero, exact requested horizon, stable IDs and unchanged unspecified
  controls. Use the existing forward tolerance `1e-9 + 1e-10 * scale`, with units and
  comparison scale recorded. Invalid controls remain explicit errors.
- Check autograd with central differences at several perturbation sizes and interior
  controls. Initial target: `1e-6` absolute plus `1e-4` relative, with per-objective
  units/scaling documented. Treat bounds, max/min operations and discrete search
  separately; do not imply gradients through rejected controls or search decisions.
- A4 uses the merged FVM solution and consistent cell-average comparisons, including
  boundary fluxes, decay and mesh/time refinement. Define model conditioning before
  training: the minimum fixed-case smoke can hold out space/time samples; changed
  inlet/flow settings require explicit conditioning or separately reported retraining.
  Use smooth activations for second spatial derivatives and evaluate residuals within
  smooth intervals between schedule events. Choose quality error thresholds before
  evaluating held-out data. Low training loss and checkpoint reload alone do not
  qualify a learned predictor or establish cross-setting generalization.
- Resolve actual compatible packages instead of inventing pins. Follow NVIDIA's
  [installation workflow](https://docs.nvidia.com/physicsnemo/latest/getting-started/installation.html)
  and [system requirements](https://docs.nvidia.com/physicsnemo/latest/getting-started/system_requirements.html).
  Verify native Windows on the actual machine; record any required alternative runtime
  and its reason. CPU reference work continues if training dependencies are blocked.
- Keep learned output outside volume approval. No tank chemistry, network-wide pipe
  storage, full hydraulics or automatic topology redesign is introduced by this plan.

## Parallel workflow and integration

Keep [exclusive ownership](PARALLEL_WORK.md): A writes `engine/`, `science/`, `proofs/`
and `scenarios/`; B writes desktop, shared contracts, root setup and shared docs.
B owns the desktop process adapter; A owns Python implementations and science locks.

Start each increment from latest `origin/main` in a separate clean checkout/worktree.
Continue a shared branch by merging main after inspecting status; never reset someone
else's work or force-push it. Use `codex/` feature branches and PRs. B integrates agreed
contracts before dependent implementation and regenerates the full root solution when
targets change. Do not cherry-pick an increment and later merge the same branch.

Each handoff states commit, touched paths, interfaces, executable example, actual
commands/results and limitations. A records evidence under its owned subtree; B updates
shared status. Keep SDKs, environments, keys, generated projects and checkpoints ignored.

Integration gates:

1. After B3: both lanes use the same committed examples/units and agree on output limits.
2. After A3/B4/B5: native reference and Torch agree; desktop executes real selected-pipe
   results and actual worker capabilities without UI blocking or stale-result leakage.
3. After A4/A6/B6: actual ML smoke, checked proofs and live model evidence each have
   separate reproducible records. Any unavailable item remains explicitly incomplete.
4. Before foundation release: MSVC solution and combined CTest/ASan checks pass; Python
   science tests and Lean build run where applicable; benchmarks, setup pins, proof
   assumptions and all known limitations are recorded. No fixed calendar deadline is set.

For every native merge run the relevant engine/desktop tests; for combined integration
run the full desktop suite and checked build. Keep optimized benchmark timings separate
from sanitizer runs. The canonical 0.76923 h / 2.66667 h limits and 80/117 m3 mixed plan
at 12,780 synthetic units remain regression anchors.
