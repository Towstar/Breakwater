# Person B: desktop, hypothetical sensors, telemetry and integration

Updated November 7, 2026 (America/Chicago). The active assignments are in
[CONTINUATION_PLAN.md](CONTINUATION_PLAN.md) and
[TELEMETRY_PLAN.md](TELEMETRY_PLAN.md). Older instructions to implement B5 or merge
the #13/#18/#19 stack are historical; those features are already delivered.

## Current baseline and ownership

Remote main was e057fa44ed8c7c3127466d72d816a68228e9b033 at planning. It includes
deployed storage comparison, selected-control sensitivities, immutable scientific
history, A10b and existing native transport/Nemotron/simulated review. Historical
counts and archived proof/training results do not establish fresh combined acceptance.

You write desktop/, contracts/, root CMake/config/scripts/solution, README and shared
docs/. A owns engine/, science/, proofs/, scenarios/ and their tests/build/locks.
Review any source, but request A-owned changes with a minimal reproduction.
Publish shared contracts as a separate reviewed commit; both workstreams adopt that
commit before dependent mapping/transport code.

Use a separate clone/worktree and named codex/ branch after inspecting status.
Preserve other sessions' uncommitted work and branches; no resets or force pushes.
B is the sole main integrator. Do not implement new features directly on main.

## Read first

- AGENTS.md, For Dev Agents.md Part A, IMPLEMENTATION_PLAN.md and STATUS.md.
- Active CONTINUATION_PLAN.md, TELEMETRY_PLAN.md and PARALLEL_WORK.md.
- Current audit dispositions and Person A's kickoff.
- DESKTOP_SCIENCE.md, PIPE_TRANSPORT.md, DESKTOP_FOUNDATION.md, AGENT_INTEGRATION.md
  and existing project/science/history contracts.

Keep the numerical engine independent and approval simulated-only. Sensor/stream/
detector functionality in the new plan is not yet implemented or verified.

## G0: current integration and baseline acceptance

PR #26 now reconciles main `e057fa4` through `f504310` and preserves sensitivities,
scientific history and the additional exact-grid/lifecycle regressions. See
[PR26_RECONCILIATION.md](PR26_RECONCILIATION.md) for fresh 28/28 normal and ASan
CTests, 127 Python tests without skips and installed sensitivity/history checks.
Review and merge the existing PR after inspecting current remote status; do not
restart its reconciliation, change its branch or overwrite that worktree. Manual
workflow, clean-machine and release-rehearsal gaps remain explicit.

Main already has the core rounded-multiple/exact-endpoint policy independently.
Retain useful additional regressions/evidence while preserving current sensitivities,
scientific history and credential binding. Validate the actual resulting revision;
older 24-suite and newer 28-suite reports apply only to their recorded revisions.

Refresh current summaries without rewriting historical evidence. Establish automated
Windows PR checks: native correctness first, desktop/science/Lean in separate supported
jobs with pinned setup. Expose unavailable dependencies/skips rather than passing
an unexecuted integration. Exercise existing operator workflow/manual acceptance gaps
and preserve deterministic/manual use when model credentials are unavailable.

## G1: publish the shared sensor/telemetry contract

With A reviewing physical semantics, define additive versioned schemas/types/examples:

- Sensor definitions: stable ID, type/unit, source/channel, asset/route attachment,
  physical measurement location, independent layout anchor, calibration, uncertainty,
  expected update rate, age limits and primary/redundant policy.
- Reading/batch: stream/epoch/sequence, event versus receive time, finite values,
  canonical/original units, quality, bounded sizes/rates and explicit errors.
- Observed snapshot: measurement/configuration identity, coherent time/skew, quality,
  commands versus measured values, derived state, assumptions and provenance.
- Detection event: evidence/threshold versions, onset/update/clear/acknowledgement,
  severity, affected sensors/assets and unresolved diagnosis.
- Compatibility: old sensor-free projects/archives remain readable; define the
  extension/version policy explicitly rather than changing old v1 semantics.

Use the proposed concepts in TELEMETRY_PLAN.md; its example is not an existing schema.
Define identity, freshness and retirement policy before parallel implementation.

## G2: sensor editor, synthetic streaming and replay

Sensors are hypothetical. Provide clearly identified Manual, Synthetic live, Replay
and Historical modes. No physical system, industrial protocol, hardware procurement
or remote listener is needed for the initial acceptance.

- Place/select/configure/move/delete sensors attached to assets/routes. Save/load and
  undo/redo preserve IDs/mappings. Marker layout is independent of physical location.
- Support typed volume/level, flow, pump-status and compatible pipe concentration.
  Pressure/temperature/custom channels are initially display/history/alarm-only.
- Integrate A's seeded publisher through bounded local NDJSON and the same typed
  ingestion used by replay/tests. B owns root launch wrappers and process lifecycle;
  A owns generator physics/scientific fixtures.
- Keep observed values separate from commands/design. Define duplicate, reordered,
  delayed/future and restarted input handling. Show timestamps, age, quality and
  unavailable/stale/inconsistent states. Preserve finite out-of-model-range evidence.
- Keep input/recording/parsing off the UI thread and bound buffers/traces/storage.
  Provide pause/resume, replay speed/epochs, backpressure counters and error handling.

Gate: canonical/custom placed sensors, save/load and repeatable replay from actual
emitted messages. No direct writes from hidden plant truth into the forecast state.

## G3: incidents, frozen observed state and forecasts

Integrate A's residual/rule quantities and coverage limits:

- Event lifecycle with persistence/hysteresis, evidence plots and unresolved-cause
  display. Acknowledgement does not clear the physical condition.
- Capture coherent observed state at a bounded cadence or incident/manual trigger.
  Retain sample IDs, event times, quality, calibration/sensor config and assumptions.
- Keep telemetry identity distinct from network hash/revision. New accepted state,
  sensor mapping changes, stream changes and expiry retire dependent live eligibility;
  incoming packets do not mutate an existing run or flood undo/job queues.
- Native forecasts/candidates use the captured original horizon/grid. Preserve
  measured delivery versus intended intervention and declared constant-rate assumptions.
- Optional bounded Nemotron investigations consume captured evidence, not true fault
  labels or a new request per packet. Native validation and freshness guards precede
  explicit human application to the synthetic plant only.
- Archive bounded incident/snapshot evidence. Historical/replayed imports cannot
  restore live approval or write controls.

Gate: noisy healthy operation, pump delivery loss, persistent loss/drift, sensor faults,
missing coverage and recovery; responsive cancellation, no stale publication and
honest detection versus diagnosis. Model access is not required to exercise detectors.

## G4: release acceptance and follow-on features

Complete three clean canonical review/approval/reset rehearsals and a custom-network
follow-up, retaining actual model tool/run/validation evidence and no-plan attempts.
Close save/open/export, compact layout and in-flight cancellation gaps. Verify a
relocated installation without checkout sources and clean Windows setup, including
synthetic publisher/replay and unavailable worker/model paths.

Then deliver separately:

- E1: Science for a selected immutable investigation candidate without applying
  controls; reuse the deployed forward/selected-derivative worker and exact capture.
- E2: automatic native pipe-result History, bounded evidence and detached reopening;
  preserve existing JSON import/export and visible storage errors.
- D1 support: A compares RBM/statistical/physical baselines. Publish a distinct
  advisory detector-result contract only before a separately selected runtime method.
- R1: UI for independently validated constant-inflow/availability/horizon scenario
  comparisons, worst margin/failure time and cost; no global robustness claim.
- M1: distinct conditioned learned-quality contract only after A's reference/error/
  held-out-domain gates. The fixed-case PhysicsNeMo smoke is already delivered.

A can perform F1 while G0/G1 proceed, then G2/G3 and D1 while you complete application
and release work. Preserve this split rather than implementing A's numerical logic.

## Verification and handoff

Run relevant real/fault-fixture input, identity, clock, malformed/oversized,
cancellation, persistence, stale-result and approval tests. Integration checks include
normal and full-desktop MSVC ASan builds, current QML/compact renders and actual
installed runtime behavior. Coordinate A's numerical checks and record inherited
versus freshly executed science/Lean evidence. Do not claim skipped model or tests ran.

Deliver one tested increment per PR, with commit/paths, interface changes, exact
commands/counts/skips, limits and a runnable synthetic example. Update shared STATUS
and solutions when appropriate; preserve credentials, pins and original handoff.

## Copyable starting instruction

~~~text
Continue BREAKWATER as Person B using docs/KICKOFF_PERSON_B.md.
Inspect status, current remote/main and the active integration worktree before G0;
review and merge the verified PR #26 without overwriting its worktree/evidence.
Read the active continuation and hypothetical-telemetry plans. B5 comparison,
selected sensitivities and scientific history are delivered; do not repeat them.
After the integrated gate, publish G1 sensor/reading/snapshot/event contracts with A's
physical review. Both adopt that committed baseline before G2 editor/ingestion/replay.
Use hypothetical sensors and synthetic live data only. Follow G3 detection/forecast
and G4 acceptance, then E1/E2 in small PRs. Keep A-owned files and original handoff
untouched. Report actual checks, unavailable paths and inherited evidence separately.
~~~
