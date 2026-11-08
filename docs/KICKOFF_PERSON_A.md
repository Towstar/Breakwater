# Person A: simulation, synthetic sensors and fault-detection evidence

Updated November 7, 2026 (America/Chicago). Person A owns the numerical/scientific
workstream. Use the active section of
[CONTINUATION_PLAN.md](CONTINUATION_PLAN.md) and
[TELEMETRY_PLAN.md](TELEMETRY_PLAN.md), not the superseded A8/A9/A10 kickoff tasks.

## Current baseline and ownership

Remote main was e057fa44ed8c7c3127466d72d816a68228e9b033 at planning. A10a/A10b,
their archived 54-proposition audit, native storage/search/transport, PyTorch worker,
deployed comparison, selected sensitivities and scientific history are delivered.
PR #26 reconciles that main through `f504310`; its fresh 28-suite normal/ASan,
127-test Python and installed sensitivity/history results are recorded in
[PR26_RECONCILIATION.md](PR26_RECONCILIATION.md). PR #26 is merged. Current main
`7f2bb7f` also includes B's PR #27 and A's PR #28; earlier passes retain their
recorded revisions and are not fresh evidence for later numerical changes.

You write engine/, science/, proofs/ and scenarios/, including their tests, local
build files and locks. B owns desktop/, contracts/, root build/scripts/solution,
README and shared docs/. Report B-owned defects with reproductions. Keep the engine
independent of Qt, Python, telemetry transports and model APIs.

Use a separate clone/worktree and a named codex/ feature branch, or continue your
existing shared A branch after inspecting status and normally merging main.
Preserve uncommitted work and other sessions' branches; no resets/force pushes.
B integrates small PRs. Generated root solutions remain B-owned.

## Read first

- AGENTS.md, original For Dev Agents.md Part A and IMPLEMENTATION_PLAN.md.
- CONTINUATION_PLAN.md, TELEMETRY_PLAN.md, PARALLEL_WORK.md and current STATUS.md.
- science/AUDIT_PERSON_A.md, science/AUDIT_POST_A10A.md and their dispositions.
- engine/TRANSPORT.md, science/TRANSPORT_QUALIFICATION.md, science/WORKER.md,
  proofs/FVM_MASS_BALANCE.md and proofs/FVM_NONNEGATIVITY.md.
- B's kickoff and the current contracts/examples once G1 is committed.

Preserve original handoff/scenario constants and failure evidence. All sensor data,
operating costs and applied controls remain synthetic. There is no physical plant
or hardware acceptance requirement for this milestone.

## Immediate work: review and adopt the shared G1 contract

The B-owned interface/validator increment is ready for exact-contract review in
[contracts/telemetry](../contracts/telemetry/README.md). Record acceptance against
the precise PR head/commit using [G1_REVIEW.md](G1_REVIEW.md); passing CI is not
numerical signoff and G2 must wait for shared adoption.

1. The numerical requirements are delivered in
   [OBSERVATION_SEMANTICS.md](../science/OBSERVATION_SEMANTICS.md) at `9941859`.
   Review B's exact G1 contract against these typed attachments, level-volume calibration,
   route position and concentration observation operator, measured versus commanded
   values, sensor uncertainty, primary/redundant policy and missing-data behavior.
   B publishes the shared contract before either workstream implements its mapping.
2. F1's 22-case study is delivered in
   [FRONT_ACCURACY.md](../science/FRONT_ACCURACY.md), including measured outlet/front
   limitations and the missed 0.9 mg/L crossing. Do not restart it as an unimplemented
   task or interpret a completed study as qualified application accuracy.
3. Any follow-up discretization change needs a separately scoped decision. Do not
   conflate mass balance/nonnegativity with peak accuracy, alter tolerances to pass,
   or start a new solver/proof family merely to enlarge scope.

Keep F1's original source/configuration hashes, commands, failures and reference
evidence intact. After G1 adoption, proceed to the A-owned G2 mapping/publisher.

## G2: observation mapping and hypothetical plant/sensor generator

After both adopt the committed G1 contract:

- Implement pure observation-to-state mapping, unit/calibration mathematics and
  analytical fixtures. Model unsupported quantities as observation-only.
- Deliver a seeded synthetic plant and sensor publisher under science/ and scenarios/.
  B owns any root launch wrapper. Emit readings through the shared input, not through
  desktop current-run internals. Keep truth and emitted observations separate.
- Cover pump delivery loss, inflow change and unmetered discharge, plus independent
  sensor noise, bias/drift, stuck readings, missing/delayed/reordered packets and
  recovery. Publish exact clock/seed/configuration for deterministic replay.
- Keep fault labels/onset/evaluator truth outside detector and Nemotron inputs.
  Include parameter mismatch and normal transients so matching generator/predictor
  equations do not give circular evidence.
- Coordinate state/actuation assumptions: measured flow is not a command; a failed
  pump must not regain intended delivery simply because a candidate changes setpoint.

Gate: known-value canonical/custom fixtures and repeatable streams, with explicit
unavailable/ambiguous observations and no clipping or invented healthy defaults.

## G3: physical residuals, rules and statistical detection

Specify and implement the numerical evidence used by B's incident layer:

- Integrated inventory-flow residuals and prediction innovations, with coherent
  observation times, declared quadrature, sensor coverage and error budgets.
- Compute a discrepancy against the prior prediction before assimilating the target
  reading. Identify measured versus assumed flow terms explicitly.
- A sensor/fault detectability matrix and counterexamples where distinct faults
  produce indistinguishable observations. Separate detection from diagnosis.
- Physical thresholds/persistence semantics, then a bounded EWMA/CUSUM comparison
  where small sustained deviations motivate it. Account for operating regimes,
  irregular clocks, noise and model uncertainty.
- Event-level evaluation: false alarms per simulated hour, recall/missed faults,
  detection delay, diagnosis confusion/abstention and runtime/memory.

B implements presentation, event lifecycle, observed snapshot identity, job retirement,
freshness and approval integration using your declared quantities. Coordinate any
new public types through B before adoption.

## D1 and subsequent science

An RBM is a legitimate experimental candidate, not a required or rejected method.
Once G2/G3 provide the dataset/evaluation harness, run a separately scoped comparison
against rules/residuals and an appropriate EWMA/CUSUM baseline. Define healthy-only
anomaly detection versus supervised classification, continuous/discrete units, causal
temporal features, missing-sensor handling, score calibration and supported topology.

Split entire runs/seeds/regimes before fitting preprocessing or creating windows.
Test healthy transients, unseen faults, domain mismatch and missing channels.
Compare detection delay/recall at declared false-alarm/latency budgets; preserve
negative/inconclusive results. Synthetic performance does not qualify real deployment.

Then support B's E1 candidate-science and E2 pipe-history gates with numerical oracles.
R1 robustness starts with declared constant-inflow/availability/horizon scenarios.
Time-dependent recovery/control schedules and M1 conditioned PhysicsNeMo require
separate numerical scope, reference gates and B-owned contracts. Maintain proof
mapping/pins when algorithms change; the 54 propositions are existing evidence.

## Verification and handoff

Run checks appropriate to changed mathematics/boundaries and report actual counts,
versions, source hashes, failures and skips. Native changes need normal and MSVC ASan
checks in separate directories; changed worker/gradient paths need actual parity and
finite-difference checks. Rerun pinned Lean/audit/rejection probes for proof changes.
Do not repeat training/performance suites without a relevant reason.

Each increment includes a runnable synthetic example, limitations, observation/detector
units and a reviewable PR. B owns shared docs/root solutions and final integration.
Do not commit the local-only science/POST_A10A_PLAN_LOCAL.md continuation file.

## Copyable starting instruction

~~~text
Continue BREAKWATER as Person A using docs/KICKOFF_PERSON_A.md.
Inspect status and read the active continuation plan, telemetry plan and ownership.
Preserve all existing work and start from current reviewed main in a separate worktree.
A10a/A10b and deployed storage science/history are already delivered.
G1 numerical guidance and the F1 accuracy study are delivered in PR28.
Review B's committed G1 contract against OBSERVATION_SEMANTICS.md and adopt the
same exact commit before implementing G2 mapping or the synthetic publisher.
Keep hypothetical sensor data synthetic. Follow G2/G3 only after the shared contract
gate, and keep D1's RBM comparison a separate evidence-based increment.
Deliver one tested A-owned increment per PR; report B-owned issues with reproductions.
~~~
