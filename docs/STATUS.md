# Implementation status

## Explicit source-feed allocations — local implementation checkpoint

- Branch `codex/source-feed-connection` adds fixed percentage allocations from a
  production source to multiple distinct tanks. The editor requires an explicit
  total of 100%; curtailment scales every branch. Existing single-feed projects
  retain their format and behavior; split projects and storage-worker requests use
  version 2. See [the allocation contract](../contracts/SOURCE_FEEDS.md).
- This is an **in-progress local checkpoint**, not a delivered application update.
  The normal MSVC/Qt build succeeds. Full pinned Python discovery passes 206 tests
  with no skips. Nine native CTests and the desktop project suite pass (10/10).
  Four fixed split cases pass independent affine expectations, all three native
  solvers, standalone Torch and 24 same-process real-worker requests, including
  selected gradients and three central-difference sizes. Two verifier-harness
  failures are retained separately from the successful attempt.
  Complete desktop/worker tests, sanitizer verification and interactive acceptance
  remain outstanding. No installed or existing user application has been replaced.
- Failed attempts and raw local logs remain under `out/connection-repro/`; the
  continuation note there is intentionally excluded from version control.

## Captured-run Nemotron sensitivities and live operator-reviewed evidence

- Local source checkpoint `ca84c0c` adds stored-run scientific tools and guarded
  rehearsal exports. Latest main `de06aa1` (PR #26) was merged at local checkpoint
  `3b3127f` without conflicts;
  upstream verification/test additions preserve this increment's runtime files.
  PR #26's dated checks below apply to its own recorded source revision.
- The ten-tool production catalog now includes `get_sensitivity_catalog` and
  `analyze_sensitivities`. Actual asynchronous CPU float64 PyTorch analysis uses
  1–8 unchanged interior controls from an immutable native run, including candidate
  overrides. Bounded summaries and separate full scientific History records bind
  incident snapshot/hash, effective network hash, run, agent session and exact
  model call. Analysis remains advisory; native validation and explicit simulated
  operator approval retain authority. Manual candidate selection/general candidate
  forward comparison remain separate E1 work.
- Production startup/rehearsal options use the real investigator. Automatic export
  requires the existing review/apply action, a fresh feasible native capture matching
  the approved effective network/revision/identities, and a matching saved approval.
  Worker-thread raw checkpoint inspection preserves live History; asynchronous
  rechecks reject state drift. Bounded reason codes report failures, and the
  production coordinator is shared with its full-signal regression.
- Three canonical sessions and the renamed, changed-topology follow-up completed
  actual model tools, native validation, UI review/application and guarded export.
  The [audit summary](evidence/live-nemotron-rehearsals.json) reports all four passed;
  [reproduction and exact scope](DEMO_REHEARSAL.md#executed-live-evidence) include the
  original local JSON/SQLite sources and the public edited-project input.
- Fresh combined checks on local `3b3127f` plus the CTest-only worker suite ceiling
  adjustment: normal/MSVC **29/29** (331.29 s), full-desktop MSVC AddressSanitizer
  **29/29** (383.53 s). Each build's 12 logged Qt suites report **408 passed,
  zero failed/skipped**: gateway 27, orchestrator 23, agent science 31, storage worker
  121, manual storage science 43 and science evidence 43, plus the remaining suites.
  QML, normal/compact scientific History and pipe-evidence render/reopen checks
  passed in both builds. No sanitizer report was found; prebuilt Qt/Python/Torch
  remain outside established ASan instrumentation coverage.
- Full local Python discovery ran **127 tests** in 41.417 s: **123 passed, four
  skipped**. All skips are `ActualPhysicsNeMoTests`: PhysicsNeMo is optional and
  absent in this local CPU environment. Real CPU Torch worker/derivative exchanges
  executed successfully. Toolchain: MSVC 19.44.35219.0, Qt 6.12.0, Python 3.12.13,
  Torch 2.15.0+cpu. Normal build reported zero warnings/errors; ASan build succeeded.
- Logs: `out/agent-tools-{vs,asan}-{generate,build,tests}-combined-final.log`,
  `out/agent-tools-vs-regenerate-combined-final.log` and
  `out/agent-tools-python-combined-final.log`. The interrupted earlier normal run
  remains separate and is not counted as a complete suite. Prior failures/profiles
  remain retained. The expanded 121-case worker suite measured 170.71 s initially
  and 147.03/144.01 s in the final normal/ASan runs; its global CTest ceiling is now
  360 s, matching the multi-process manual Science suite. Worker request, application
  and hosted-model deadlines were not relaxed.
- Thirteen startup rejection cases and ten portable auditor regressions passed;
  auditor fixtures are synthetic tests, never live execution evidence. Independent
  source/caller review and documentation/evidence review found no actionable defects.

| Actual archive | Stored native runs | Finite search inputs | Selected synthetic cost | Minimum margin m³ | Tools / hosted requests |
|---|---:|---:|---:|---:|---:|
| `canonical-7.json` | 6 | 108 | 9,030 | 13 | 9 / 10 |
| `canonical-9.json` | 4 | 81 | 12,780 | 16 | 10 / 11 |
| `canonical-11.json` | 6 | 720 | 4,800 | 4 | 7 / 8 |
| `edited-2.json` | 6 | 64 | 4,800 | 9 | 8 / 9 |

- All 38 hosted completions identify
  `nvidia/nemotron-3.5-lightning-30b-a3b`, HTTP 200 and zero retries. Native runs
  retain all 121 original samples over three hours; edited runs include three tanks.
  All four SQLite `quick_check` calls returned `ok`. Different model-selected search grids
  establish neither a global optimum nor a reliability/performance comparison.
- Canonical 7's actual sensitivity call and separately retained result
  `d4c4f795-e3f7-4942-860e-d90f0f69b7cb` pass exact origin/native-capture checks.
  At curtailment 5 and bypass 16 m³/h, the Jacobian is `[[-3,-3],[0,3]]` h and cost
  gradient `[750,0]` synthetic units/(m³/h); native and worker cost are both 9,030
  with zero returned-sample difference. Science followed initial staging, so this
  trace does not demonstrate derivative-guided revision of the selected plan.
- Actual History reopening of canonical 7 showed six complete native runs, the
  overflowing baseline and candidate ending near 107 m³, with historical approval
  unavailable. The separate scientific result reopened with the same captured
  values, derivatives and zero native/worker difference. Explicit Reset clicks were
  observed after canonical 9, canonical 11 and edited 2; canonical 7 was followed by
  a fresh canonical launch rather than a recorded Reset-button click.
- Failed/no-plan/timeout/budget/unknown-finish attempts remain distinct local evidence.
  Edited 1 failed its tool limit with no operator approval while its completed
  scientific record remained valid and durable. Canonical 4's approved backend
  survived an export failure whose original cause was not reproduced by the current
  full-signal test. Neither attempt counts as a clean rehearsal. The audit checks
  retained evidence consistency, not cryptographic execution authentication.
- This closes the recorded live rehearsal slice. Broader G4 native-dialog,
  clean-Windows and relocated-release acceptance remains explicit; telemetry G1,
  automatic pipe History and the rest of E1 remain separately scoped work.

## Planned continuation: hypothetical sensors and detection — November 7, 2026

- The user requested configurable **hypothetical sensors** and synthetic live
  telemetry in addition to the remaining science/application work. The accepted
  roadmap is now in CONTINUATION_PLAN.md and TELEMETRY_PLAN.md, with current
  Person A/B kickoffs and ownership/dependency gates.
- Planned: placed/typed sensors, synthetic publisher, common local ingestion/replay,
  coherent observed snapshots, quality/freshness, physical residuals, incident
  lifecycle and observation-driven forecasts with simulated approval guards.
  Pressure/temperature/custom channels are initially observation/alarm-only.
- Planned evaluation: rules and physical residuals, statistical shift monitoring,
  and a bounded RBM comparison on held-out synthetic scenarios. No detector is
  demonstrated optimal; no learned result authorizes control.
- Retained next work: PR #26 review/merge/current-baseline checks, Windows CI,
  manual/clean-machine/operator acceptance, front/outlet/threshold accuracy,
  candidate-aware Science, automatic pipe history, scenario robustness and later
  conditioned PhysicsNeMo. A10a/A10b, deployed storage science, derivatives and
  scientific history below remain delivered rather than new assignments.
- Sensor ingestion/detection, a physical-system adapter and an RBM implementation
  are **not implemented or verified** by this documentation increment. No runtime,
  sanitizer, model, training or proof execution is claimed here. Existing evidence
  below retains its original source revision and scope.

## PR #26 reconciliation with current main — November 7, 2026

- [Reconciliation report](PR26_RECONCILIATION.md): merged `e057fa4` through
  `f504310`, preserving selected-control sensitivities, automatic scientific-history
  retention and immutable historical reopening. Application/contracts match main;
  exact-grid, disagreement, 50-cycle and post-exchange retirement regressions remain.
- Fresh complete normal/MSVC ASan builds each passed **28/28 CTests** (390.19 s /
  439.93 s), including live selected sensitivities and scientific-history rendering.
  Each build: worker **121**, controller **43**, science evidence **42**, zero
  failures/skips; QML lint passed and no sanitizer report was found. Prebuilt
  Qt/Python/Torch remain outside established ASan instrumentation coverage.
- Full Python: **127/127**, no skips, PhysicsNeMo available. Native/PyTorch/worker:
  **18 fixtures / 302 requests / 132 central differences**, unchanged tolerances.
- Corrected installed acceptance: **16 app executions** across local/relocated
  copies; actual selected derivatives, exact retained 121-sample native arrays,
  historical reopening without Python, installed module origins, explicit missing
  dependencies and SQLite integrity passed. An initial probe incorrectly expected
  historical marking in stored canonical data; the failed harness attempt is retained.
- No new interactive UI pass is claimed. Earlier native-dialog/cancellation gaps,
  coarse qualification failure and front-accuracy limitation remain explicit in the
  frozen combined report. No repeated PDE qualification/training/Lean run is claimed.
  The local continuation plan remains untracked and excluded from the PR.

## Combined verification — November 7, 2026

- [Combined report](COMBINED_VERIFICATION.md): isolated baseline `1a537ed`, corrected
  private desktop time-grid admission/validation, exact endpoint/identity regressions,
  and unchanged public schemas, dependency pins and numerical thresholds.
- Complete normal and MSVC ASan configurations each passed 24/24 CTests; Python
  passed 127/127 without skips; refined transport passed 45 cases/580 gates in both
  builds; PhysicsNeMo passed all 12 fixed-case gates after 1,800 updates; Lean audited
  54 unique propositions with both rejection probes. The final post-exchange edit,
  deletion and replacement regressions passed both affected suites in each build:
  worker 85/0/0 and controller 25/0/0; application code/deadlines unchanged.
- Local/relocated installed execution, missing dependencies, installed worker origins,
  setup idempotence and SQLite integrity were checked. Interactive save/load and
  native export dialogs remain unverified due to computer-use targeting problems;
  completion won the interactive cancellation race. The coarse scheduled-inlet
  refinement failure and 0.40191 mg/L front maximum-error limitation are preserved.
- Hosted Nemotron, clean-machine acceptance, new physics and the front study remain
  outside this run. Existing historical milestone reports below retain their scopes.

## Scientific history retention and ordered main integration — November 7, 2026

- Merged PRs **#13 → #18 → #19 → #21 → #23**, with merge commits and complete
  ancestry retained. Resolved generated-solution/shared documentation and the earlier
  science/credential bindings without discarding either feature. Each stacked PR was
  retargeted to main after its predecessor landed. The final combined desktop source
  matched tested B5c before adding retention.
- A new brother update appeared during integration: PR #24 at `2696a2b` was reviewed
  and merged at **`106a756` main**. It adds A10b's conditional exact-real FVM
  nonnegativity proofs, native regressions and post-A10a audit evidence. All A-owned
  source matches that reviewed head; no solver algorithm, dependency lock or scientific
  method was rewritten. The **54-proposition** Lean audit is A's archived execution,
  not a fresh local replay. Earlier 21/37-proposition reports below remain dated evidence.
- PA10-B01 is resolved by B5's immutable `CapturedForecast` configuration/full arrays,
  exact-horizon boundary checks and new science archive. PA10-B02 is resolved by this
  current shared status. The original frozen-source audit in
  `science/AUDIT_POST_A10A.md` remains unchanged.
- Completed forward/sensitivity evidence now queues an immutable standalone session
  on the existing SQL worker before any observer can change the capture. It retains
  the network, full original native trajectory/configuration/validation and bounded
  worker preview, all selected derivative columns and provenance. Each result has its
  own UUID, completed status, no recommendation or approval, and a versioned singleton
  science payload. Existing SQLite/session/project/worker versions remain unchanged.
- Strict 1 MiB scientific-result validation reuses the worker contract and checks
  identities/hash/revision, original grid and indices, captured controls/domain/order/
  units, finite matrix dimensions, provenance and independently recomputed diagnostic
  flags. Completed science records cannot be replaced by later checkpoints. Legacy
  investigation archives still import without scientific fields.
- History reopens scientific charts/derivatives as historical evidence. Selecting a
  result/tank does not change live controls, captures, selections or approvals. Worker
  actions remain hidden while historical evidence is displayed; Return to current
  restores the separate live view. Imported JSON is not authenticated execution.
- Capture/save failures are correlated to their result IDs, preserving unrelated
  import/export errors and subsequent successful-save acknowledgements. Interactive
  science/native forecasting remains usable after storage failures; the acceptance
  smoke requires the exact saved result ID and fails with exit 7 on persistence failure.
- Direct-root VS2022 and full-desktop MSVC ASan builds passed. **28/28 CTest suites
  passed in each** (313.16 s / 390.06 s), including live storage/selected derivatives
  and historical evidence renders at normal/compact sizes. In each build: **42 science
  evidence cases, 9 history cases, 35 science-controller cases, 80 worker cases and
  46 Nemotron-client cases** passed without failures/skips. Actual updated native
  transport passed **25/25 cases** in both. Python discovery: **127 run, 123 passed,
  four optional PhysicsNeMo skips**, 59.270 s. These timings are acceptance runs,
  not benchmarks. Normal build had five Qt/deprecation warnings and zero errors.
- Relocated installed execution with checkout science sources unavailable passed
  seven actual NDJSON requests, exact ULP tail/0.125 m3 overflow, selected derivatives,
  health and rendering. Removing the deployed grid helper explicitly disables storage
  while legacy health remains available. The installed app retained one complete
  science session with **121 original native samples** and the full two-control matrix.
  Both database integrity checks passed. A second installed process reopened/imported
  the exact evidence and rendered historical Science with an unavailable Python path;
  it needed no worker/current native capture. Checkout sources were restored unchanged.
  The bundle includes `.env.example` and no real credential file.
- Actual 1440x900 Windows UI checks verified the saved 0.95 h/0.3 h result, captured
  values 10/20, Tank A [-0.95,-0.95] h, Tank B [0,+0.95] h and cost gradients [237.5,0].
  Return to current restored no capture or selection; a new forecast used healthy
  main-pump 44 settings instead of applying the archived mixed controls. After closing
  and restarting without an import flag, History → Refresh → stored row → View
  scientific evidence reopened the same result from SQLite. Historical probes/actions
  remained unavailable. The verified normal app was left open on historical derivatives.
- A real failed-storage CLI check returned **exit 7**, `history_storage_failed`, and
  no success screenshot. Independent scoped same-family reviews found and resolved
  the storage-smoke acknowledgement and unrelated-error-correlation cases; actual
  before/after failed-import regressions pass. Original initial failures remain under
  `out/retention-vs-initial-build.log` (generated 260-character MSBuild path, fixed by
  shortening the test target) and `out/retention-asan-initial-focused-tests.log`
  (an exact decimal chart-bound assertion, corrected to compare the retained native
  terminal value while separately checking analytic agreement).
- Evidence: `out/retention-*-tests.log`, per-suite `*-results.txt`,
  `out/retention-python-tests.log`, `out/retention-source-hidden.log`,
  `out/retention-installed-reopen.log`, `out/retention-installed-*.json/.png`,
  `out/retention-negative-cli.log`, and QML lint logs. The generated solution has
  **75 projects/211 dependency references**, zero missing/dangling paths. No new hosted
  model, Lean, neural training or performance execution is claimed by this increment.
  Manual compact/file-picker and in-flight GUI-cancel gaps, plus clean canonical/custom
  operator-review rehearsals, remain separate release work. No controls were applied.

## PR #23 ordered-stack reconciliation — November 7, 2026

- Merged main after #13 → #18 → #19 → #21 into the B5c review branch. Both parents
  are preserved. Runtime source, contracts, scientific source, scripts and the
  73-project/204-dependency solution exactly match the previously tested B5c tree
  `e4d48b4`; only shared handoff/status notes required reconciliation.
- Credential source/endpoint pairing and selected-control safeguards remain intact.
  Earlier reports below retain their original scope and dates; the ordered-stack
  integration and scientific retention increment record their fresh gates separately.
## Final A10a integration check — November 7, 2026

- Merged latest main `5545417` (PR #22, underlying `9b95ff9`) into the B5c
  branch with both parents preserved and no conflicts. This adds the discrete
  finite-volume mass-balance proofs, audit/documentation and three native regression
  cases; solver algorithms, public contracts and the tested desktop source are unchanged.
- Rebuilt and reran the changed `engine.transport` suite in normal VS2022 and
  MSVC ASan builds: **21/21 cases passed in each**, including signed open-boundary
  exchange, closed internal-face cancellation and variable-step old-state decay.
  Evidence: `out/b5c-a10-normal-tests.log`, `out/b5c-a10-asan-tests.log`.
  Independent scoped review found no consequential issue. The new 37-theorem Lean
  run remains Person A's archived evidence; no fresh local Lean execution is claimed.
- The preceding complete 25-suite normal/ASan, installed worker and live hosted
  checks below remain the desktop/runtime gate. These integration changes are
  published on PR #23; the B PR stack is still awaiting its ordered main integration.

## B5c selected-control sensitivities and current-main reconciliation — November 7, 2026

- Integrated runtime main `6966707` and subsequent documentation/solution refresh
  `7785d1d`, including PR #17, PR #20's A9 qualification and
  `dc348d8` native key settings. Preserved complete ancestry and resolved the
  main/QML/status conflicts by retaining Science and credential settings together.
  The preceding audit compatibility increment is delivered separately in PR #21.
- Science now lists captured control values, units, bounds and eligibility reasons.
  Users explicitly select strictly interior controls; the worker receives only those
  IDs in native catalog order, at unchanged captured values. Fixed/boundary controls
  and source feed-capacity boundaries cannot be selected. No values are perturbed,
  implicitly selected or silently dropped; omitted controls remain captured constants.
- Typed admission/publication checks enforce the requested derivative object, exact
  ordered IDs, finite tank-by-control Jacobian/cost-gradient dimensions and no artifacts.
  Controller checks independently bind mode, selected values, capture/config and job
  generation. Selection changes retire sensitivities; independent forward results
  survive. New capture/numerical edits/reset clear selection; layout/camera/quality
  changes preserve matching evidence. No native control or approval action is invoked.
- Evidence adds mode, selected controls/units, native solver version and the complete
  derivative matrix. The UI displays one tank's rows using bounded lists, Jacobians in
  hours and cost gradients in synthetic units per (m3/h). Forward agreement covers
  returned trajectories/cost, not a runtime derivative or feasibility certification.
- Review found and fixed **B-06/P1** in the incoming configuration loader: a project
  endpoint-only `.env` could redirect a process/saved-user key. Endpoint now follows
  the chosen credential source, with explicit process URL overrides preserved. Nine
  synthetic regressions check actual mocked request destinations and bearer headers;
  no actual key or credential file was inspected by the source reviewers.
- Direct-root VS2022 and full desktop MSVC ASan builds passed. **25/25 CTest suites
  passed in each** (264.98 s / 328.82 s). Both builds: scientific worker **80 passed**,
  controller **35 passed**, Nemotron client **46 passed**, all zero failed/skipped.
  Coverage includes native central differences at three scales, opaque/Unicode IDs,
  subsets/order, signed/zero derivatives, malformed results, stale jobs, cancellation
  and synchronous observer deletion. Normal build: four Qt-header warnings, zero errors.
- Fresh Python discovery: **127 run, 123 passed, four optional PhysicsNeMo tests
  skipped** (56.602 s). A9's refined matrix passed **45 cases/580 gates** with both
  normal and ASan native exporters. The original coarse study retained its one failed
  convergence gate and remains unqualified; local peak/threshold accuracy is not
  established. The first ASan exporter launch outside the developer shell failed with
  loader exit `0xC0000135`; the correctly initialized developer-shell run passed and
  both attempt reports remain retained. No Lean/neural-training/performance run is claimed.
- Actual 1440x900/1100x700 GUI checks showed captured curtailment 10/bypass 20,
  Tank A [-3,-3] h, Tank B [0,+3] h and cost gradients [750,0], eligibility reasons,
  scroll access and selection order. Layout preservation, sensitivity invalidation,
  forward preservation and numerical/new-capture/reset clearing passed. Both private
  QA apps closed cleanly; stderr logs were empty. GUI in-flight cancel still raced
  completion; its automated checks passed. Earlier native save/reopen dialog gaps remain.
- Relocated installed execution with science sources temporarily unavailable passed
  seven real worker requests, exact-tail/overflow checks, selected derivatives and
  deployed module origins. Missing grid helper still produces explicit unavailable
  storage with valid legacy health. The installed desktop completed actual native/
  Torch comparison plus selected derivatives, health and rendering. Source was restored
  unchanged. Five invalid CLI selections returned exit 6 and saved no screenshot.
- The replacement User-scope API key passed the rebuilt client's genuine hosted
  tool-call/execution/result-continuation smoke against the configured Lightning model.
  Only fixed sanitized outcome metadata was retained; no key/raw response was logged.
- Root solution: **73 projects/204 dependency references**, all valid, including A9's
  new exporter. Evidence: `out/b5c-*-tests.log`, per-suite `*-results.txt`,
  `out/b5c-python-tests.log`, `out/b5c-qualification-*/report.json`,
  `out/b5c-installed-science.json`, `out/b5c-source-hidden.log`, retained GUI reports
  and `out/b5c-live-key-verification.json`. Independent scoped same-family reviews
  covered protocol/controller/credential/A9 callers; remaining manual/release gates
  are explicit. Next: bounded scientific retention/restoration and clean rehearsals.

## PR #21 current-main reconciliation — November 7, 2026

- Integrated #19's merged baseline while preserving current key settings, all A-owned
  sources and the complete deployed worker/grid package. The independent desktop
  exact-horizon checks and native solver provenance 0.1.1 remain in this increment.
- Retained the regenerated 73-project VS2022 solution and checked its complete graph.
  Earlier test reports below keep their original scope; final combined-stack checks
  remain the gate before further release claims.

## Person A audit-fix integration before B5c — November 7, 2026

- Checked GitHub and merged PR #17 head `367dcf1` into B5b `9f8f550`, preserving both
  parents without conflicts. The A-owned source matches that head; B changes only
  contracts, desktop integration/tests and shared handoff/evidence documentation.
- Aggregated [A's audit](../science/AUDIT_PERSON_A.md) in [AUDIT_REPORT.md](AUDIT_REPORT.md).
  Its P1 shortened-horizon counterexample supersedes the earlier bounded B audit's
  no-P0/P1 conclusion. A's exact-horizon, finite-cost, residual and documentation fixes
  are included. Scientific worker methods remain advisory; native validation and
  explicit simulated operator approval remain separate.
- The desktop independently reconstructs rounded interior multiples and the exact
  requested endpoint instead of deriving work/indices with rounded division. Added
  exact-tail, duplicate, underflow, work-boundary and malformed-endpoint checks;
  actual desktop/worker tests retain a 0.125 m3 capacity violation and deny approval.
  Planning-tool regression checks reject its validation/staging/search recommendation.
- Deployed the newly required `storage_grid.py`. Native solver provenance advances
  to `constant-flow-0.1.1`; current wire provenance is
  `torch-constant-flow-0.1.2+worker-v1`. Project/protocol/method versions stay 1.
  Historical records do not become newly validated or actionable after this update.
- Fresh direct-root VS2022 normal and full desktop MSVC ASan builds passed;
  **24/24 CTest suites passed in each** (171.37 s / 247.48 s). Normal compilation
  reported five warnings in Qt headers/A's benchmark and zero errors. The unchanged
  solution has 72 projects/201 dependency references; all paths resolve.
- Fresh full Python discovery: **94 run, 90 passed, four optional PhysicsNeMo tests
  skipped**, 46.145 s. The 19 analytical residual/mathematics tests executed; genuine
  PhysicsNeMo neural training/gradients, Lean and performance measurements remain
  attributed upstream evidence, not new local executions.
- Fresh actual-worker/native verification passed **302 NDJSON requests**, 18 fixtures,
  three solvers, 44 interior controls and 132 finite-difference checks. Exact grids and
  offline schemas passed; maximum volume difference `7.67386154621e-13 m3`.
  An initial probe ran before the new exporter finished building and rejected its old
  12-case document; the completed-build run above used the actual updated executable.
- With original science sources temporarily unavailable and Windows-only PATH, the
  relocated installed worker passed six real requests, exact ULP-tail timestamps,
  the 0.125 m3 overflow and deployed module-origin checks. Removing its grid helper
  produced `runtime_unavailable` while legacy health remained completed. The installed
  desktop then completed native/PyTorch comparison (both costs 12,780), health, visible
  rendering and screenshot export. Source was restored unchanged. Its deployed-qsqlite
  database passed read-only integrity checks. The initial local probe's version assertion
  omitted the documented `+worker-v1` suffix; corrected fixture expectations passed.
- Evidence: `out/b-audit-*-build.log`, `out/b-audit-*-tests.log`,
  `out/b-audit-python-tests.log`, `out/b-audit-worker-verification.json`,
  `out/b-audit-installed-worker.json`, `out/b-audit-installed-science.json`, and
  `out/b-audit-source-hidden.log`. Independent scoped source/caller review found no
  consequential regression. Existing manual dialog/cancel/release gaps remain open.
- Next implementation: B5c explicit selected-control derivatives. Automatic scientific
  retention and clean hosted rehearsals remain later work. No controls were applied.

## PR #19 current-main reconciliation — November 7, 2026

- Combined B5b's forward-only desktop storage path with merged #13/#18 and current
  native key settings. The entry point and AgentPanel receive both science and
  credential controllers; the installed worker retains its grid helper and package.
- Regenerated the combined VS2022 solution. Final ordered-stack runtime checks remain
  pending; no selected-control derivatives or scientific history are added here.
## PR #18 current-main reconciliation — November 7, 2026

- Integrated the merged historical-evidence baseline while preserving native key
  settings and A's current sources. Added the newly required storage_grid.py helper
  to deployment alongside worker_storage.py and the complete scientific package.
- Regenerated the combined VS2022 solution. Runtime and installer verification of
  the final ordered stack remains the integration gate; earlier reports below
  retain their original scope and do not establish a fresh combined run.
## B5a/B5b local runtime and desktop storage integration — November 7, 2026


- Merged the audit-first handoff at `6fb8c29` and froze `10cb8f7` for
  [AUDIT_PERSON_B.md](AUDIT_PERSON_B.md). Direct root MSBuild and normal/full-desktop
  MSVC ASan each passed 20/20 baseline suites. No P0/P1 blocker was found in scope.
  [AUDIT_REPORT.md](AUDIT_REPORT.md) leaves the missing comprehensive A audit explicit.
- Installed/checksummed local uv 0.11.9, CPython 3.12.13 AMD64, Torch 2.15.0+cpu,
  NumPy 2.5.4 and test-only schema dependencies from the existing locks. An actual CPU
  float64 product returned 11.0; an idempotent setup rerun passed. Existing Python,
  global PATH/registry/user settings, A-owned source and locks were preserved.
- Fresh scientific execution: **32 worker/mapper/protocol tests passed**, zero failures/
  skips. Native verification passed **290 NDJSON requests**, 12 cases, three solvers,
  44 interior controls and 132 finite-difference checks; maximum volume difference
  `7.67386154621e-13 m3`. No PhysicsNeMo or Lean replay is claimed.
- The copied installed app ran with Windows-only PATH while the original science
  directory was temporarily unavailable. Actual health/capabilities/storage/verification,
  deployed module origins, native comparison and desktop rendering passed; both costs
  were 12,780. Source was restored unchanged. Its deployed-qsqlite database passed
  read-only integrity checking. Missing mapper/Torch fail explicitly while health stays
  valid. Missing installed worker.py yields worker_unavailable, with no source fallback,
  while the visible native desktop still renders. Combined health/science flags preserve
  health evidence; report-write failure returns exit 7 without producing a report.
- The client adds bounded same-process capabilities/storage/capabilities and clean-exit
  publication while preserving legacy health. Retiring processes cannot overlap new
  jobs. Typed validation, byte limits, fixed errors and credential redaction remain.
  Native forecasts now retain original configuration and immutable reference arrays;
  science binds the complete capture/generation and original indices, including the
  inherited one-ULP endpoint. Numerical edits, reset/deletion/new forecasts and cancel
  retire results; layout/camera/selection and independent pipe edits preserve them.
  Publication rechecks synchronous observer changes. Science grants no approval.
- Final direct-root VS2022 normal and full desktop MSVC AddressSanitizer checks:
  **24/24 suites passed** (150.05 s / 189.00 s). Worker tests: **42 passed, 0 failed/
  skipped**; controller tests: **14 passed, 0 failed/skipped**. After the final context
  badge adjustment, three affected QML/real Science render checks passed again in both
  builds. QML lint passed. Solution: 72 projects/201 dependencies, no missing paths.
- Actual 1440x900/1100x700 checks verified real comparisons, both tanks, provenance,
  capacity boundaries, capability reset, layout preservation, numerical/reset invalidation
  and current-native labeling with a selected historical investigation. On the bad
  network Tank B reaches 132 m3 over its 130 m3 boundary; native validation/approval
  remain failed/disabled despite agreement. The original user app was preserved.
- Manual gaps: GUI in-flight cancellation raced completion; filename-entry tooling
  prevented native dialog save/reopen completion. Backend cancel/persistence/export
  checks passed; those GUI gaps remain explicit. PowerShell 7 launch/configuration tests
  passed; PowerShell 5 execution was blocked by its unchanged policy. Qt translation
  catalogs were unavailable; the English UI rendered. Fresh-machine VC runtime is untested.
- Setup/use: [DESKTOP_SCIENCE.md](DESKTOP_SCIENCE.md). B5c gradients UI, candidate/history
  science, automatic retention and clean hosted rehearsals remain separate. No new
  hosted calls or operating-control application ran in this increment.

## PR #13 current-main reconciliation — November 7, 2026

- Merged main `5545417` into the historical native-pipe evidence branch, preserving
  both parents, Person A's audit/A9/A10a work and native credential settings.
  Shared entry-point/QML changes retain both credential setup and read-only evidence
  reopening; no B5 desktop storage feature is introduced in this increment.
- Regenerated the VS2022 x64 solution against the combined source, retaining the B7
  evidence test/render targets and A9 qualification exporter. Original status reports
  below remain historical evidence. The final ordered-stack build and verification
  are the integration gate; a new full suite or Lean run is not claimed here.
## PR #14 pull/review and next B5 plan — November 7, 2026

- Pulled main `794c7cd`, including PR #14's `d4cae03` worker mapping. Reconciled the
  status-document conflict, preserving B7 evidence and labeling the upstream reports.
  No desktop runtime, engine or public schema implementation changed in this merge.
- Scoped independent protocol/mapping/integration reviews found no new consequential
  blocker. The repo worker now implements capabilities and CPU storage execution;
  the desktop still exposes health only and installation still copies only `worker.py`.
- Local checks against the new worker: **20/20 combined normal CTest suites passed**,
  the targeted MSVC ASan desktop-worker suite passed, and **11 standard-library Python
  checks passed** (two legacy-worker plus nine framing/availability checks). A new full
  ASan build/run, the 69-test scientific suite, 290-request native/Torch verification
  and Lean execution were not repeated in this review.
- The available bundled Python is 3.12.14 without Torch/PhysicsNeMo, and no process/user
  `BREAKWATER_PYTHON` is configured. Actual PyTorch execution therefore remains an
  acceptance gate for the next increment, not a result claimed by this pull.
- [The next B5 task](CONTINUATION_PLAN.md#next-person-b-task-b5-desktop-storage-cross-check)
  deploys the real runtime/modules and executes a forward-only cross-check of a current
  retained native forecast. It explicitly handles process-local verification, per-frame
  output bounds, retained grids, stale results and approval separation. Selected-control
  derivatives follow after this path passes; no production B5 implementation is included.

## B7 native evidence reopen — November 7, 2026

- Reconciled current main `a707b32`, including A's `f469df8` control-influence
  optimization, without conflicts after the B7 implementation. The native worker and
  public desktop/science contracts are unchanged. Scoped independent kernel and proof
  reviews found no blocker. A's recorded proof/source hashes match its committed
  evidence (21 audited theorems); Lean and performance benchmarks were not rerun here.
- The formal control-column equivalence concerns exact-real folds, not floating-point
  guards, complete feasibility/ranking behavior, memory safety, speed or optimality.
  Native differential/oracle checks run as part of the combined suite below.
- Also merged main's subsequent `dc75ef9` documentation/solution refresh. Reconciled the
  generated solution and status entry, preserving B7's complete graph and distinguishing
  the upstream report below. Runtime source is identical to the final 20/20 tested tree.
- Native transport JSON exports can be reopened after restart through the sidebar's
  Pipe transport viewer, without a selected route or current forecast. Charts, signed
  mass ledger, recorded parameters and provenance reuse the existing native display.
- Imported files always remain historical, unverified evidence with no approval, even
  when IDs/hashes match the current project. Their recorded settings are read-only and
  never enter the authoritative run store. Current calculation is an explicit separate
  view; numerical edits/reset invalidate that view without recategorizing the import.
- Reading/parsing/validation run off the UI thread with a 1 MiB file cap, strict native
  v1 fields/method, matching parameter fingerprint, finite data, ordered native sample
  identities, bounded dimensions/ledgers and supported native diagnostics. Signed values
  are preserved; chart arithmetic handles extreme finite values without data clipping.
  Rejected or cancelled imports retain prior evidence and cannot publish late or overlap
  another job. Re-export preserves the supported payload, without authenticating it.
- Added a production `--import-pipe-evidence` path and a CTest render check that first
  produces an actual native export through the controller, then reopens it in the app.
  This check requires successful import and the visible evidence modal, independent of
  scene labels behind that modal. Usage is in [PIPE_TRANSPORT.md](PIPE_TRANSPORT.md).
- Final direct VS2022 normal and full desktop MSVC ASan checks: **20/20 CTest suites
  passed** in each build. The controller suite reports **14 passed, 0 failed/skipped**;
  structural evidence validation reports **13 passed, 0 failed/skipped**. Coverage
  includes actual native export/reopen after object restart, signed inlet exchange,
  mismatched/current identities, numerical edits/reset, malformed/oversized inputs,
  cancelled/destroyed importers, preservation of prior evidence and approval separation.
- Actual GUI checks at 1440×900 and 1100×700 verified charts/ledger, the persistent
  historical banner, disabled recorded parameters/boundary, readable provenance,
  compact scrolling to diagnostics, sidebar access without a forecast, and explicit
  return to an empty current context without restoring approval. The original user
  app remained untouched. Native file pickers opened, but computer-use tooling could
  not target the filename field; picker-based import remains unverified. The production
  CLI successfully reopened an actual export and rendered screenshots in both builds.
- A separately crafted finite-extreme fixture (`±1e308`, not solver evidence) rendered
  successfully without clipping or arithmetic overflow. The root solution retains
  68 projects and 187 dependency references with no missing paths. Independent scoped
  code review found no remaining actionable issue after the CLI/render fixes.
- Final QML lint passed. A malformed file returned a failing CLI smoke exit and no
  screenshot. The updated normal build used by the existing local launcher also passed
  actual native-evidence reopen/render smoke; the original open app was preserved.
- At this B7 checkpoint, scientific worker mapping was pending; PR #14 above now
  supplies it while the desktop remains health-only. This
  B7 subincrement does not automatically store native quality results in SQLite, complete
  clean live-model rehearsals or qualify learned predictions. No new hosted calls ran.

Scope: shared base, native search, durable desktop investigations, science contracts,
native selected-pipe viewing, and Person A's A3–A9 science/benchmark/proof increments.
PRs #7, #8, #10, #11, #14, #17 and #20 are merged; their complete histories are retained.
The accepted plan and original handoff are preserved. The execution sequence remains
in [CONTINUATION_PLAN.md](CONTINUATION_PLAN.md); its A3–A6 items are now delivered.

## Audit/A9 main integration — November 7, 2026 (upstream report)


- Merged #17 at `0d78ffa` and retargeted/merged #20 at `6966707`, with no conflicts.
  Local main was fast-forwarded. The four A-owned audit findings are resolved;
  A9 supplies bounded synthetic transport qualification. See the
  [integration record and next-change plan](POST_A9_PLAN.md) for ownership,
  acceptance criteria, commands and evidence paths.
- Preserved dotenv/credential-support commit `dc348d8` and its application,
  scripts, README and public template unchanged. Real `.env` files remain ignored
  and untracked. No real key was read, changed or published by this integration.
- Regenerated the root Visual Studio 2026 x64 solution: 67 projects, 183 build
  dependencies, no missing/dangling references. The direct `Breakwater.slnx`
  MSBuild passed. Combined normal CTest passed **18/18** (26.14 s); separate
  full-desktop MSVC ASan build/CTest passed **18/18** (88.20 s), no sanitizer report.
- Full integrated-source Python suite passed **127/127**, no skips (81.358 s).
  Native/PyTorch/worker checks passed 18 cases across three native solvers,
  **302 actual NDJSON requests**, 44 interior controls and 132 central differences;
  maximum volume difference `7.67386154621e-13 m3`.
- Actual fixed-case PhysicsNeMo model/residual/training/checkpoint smoke passed
  **12/12 gates**, including 1,800 updates and zero checkpoint inference difference.
  Held-out field RMSE `0.00336886618 mg/L`; this is experimental fixed-case evidence.
- Refined transport qualification passed **45 cases / 580 gates** in normal and
  MSVC ASan execution. The original coarse study reproduced its expected single
  failed refinement gate; that configuration remains unqualified. Mean-error gates
  do not qualify local peaks: refined scheduled-inlet maximum cell-average error
  remains about `0.40191 mg/L`. See [A9 scope](../science/TRANSPORT_QUALIFICATION.md).
- Pinned `lake build` passed (1,061 jobs), all **21 theorem assumption audits**
  passed, and the deliberate unapproved-assumption probe was rejected. These are
  exact-real finite-graph storage/control-influence algorithm proofs, not PDE,
  IEEE, memory-safety or global-optimality guarantees.
- Evidence is local under `out/post-a9/` and `out/a6-proofs/`. No new hosted-model
  call, performance benchmark or installed-app acceptance run is claimed here.
  Desktop scientific execution remains health-only on this main baseline; the
  installed-worker/forward-comparison work is in the still-unmerged B stack
  #13 → #18 → #19. Review that stack next rather than reimplementing it.

## Local Nemotron key setup — November 7, 2026 (upstream report)

- Implemented automatic `.env` loading for the desktop and hosted smoke executable.
  The key-free `.env.example` is versioned and installed; real `.env` files remain
  ignored and are excluded from installation. Existing ignore rules already cover
  this distinction. No scientific or numerical contract changed.
- Installed users can choose **Nemotron investigator → Set API key** to save,
  replace or remove a key in their per-user settings directory. Entry is masked
  and cleared on close; there is no saved-key getter exposed to QML. The local
  settings file is plaintext, not an encrypted credential vault.
- Native configuration reloads between investigations, preserves active requests,
  records the refreshed model identity and keeps file credentials out of child
  environments. Process settings take precedence over launch/checkout `.env`,
  followed by saved user settings. Details: [configuration](AGENT_INTEGRATION.md).
- Normal MSVC desktop build and **18/18 CTest suites passed** (32.20 s), including
  **37 Nemotron-client cases** and **24 worker cases**, with no failures or skips.
  New checks exercise parsing, precedence, worktree boundaries, malformed/oversized
  files, atomic save/reopen/replacement/removal, invalid input, write failures and
  idle-only configuration reload, using synthetic keys.
- `cmake --install out/desktop --prefix out/installed` succeeded. The staged package
  contains `.env.example` and zero real `.env` files. Launched the packaged executable
  through the Windows UI and inspected the setup dialog, empty masked input and
  disabled empty-input save action. Persistence was tested with temporary files;
  no real key was entered and no live hosted request or new sanitizer run is claimed.
  Build/test/package output is local to `.worktrees/local-env/out/`.

## A8 scientific-worker integration — November 7, 2026 (upstream report)


- PR #14 merged at `5c773f7` without conflicts. The repo worker implements
  `capabilities` and CPU float64 `storage_rollout`, with selected-interior-control
  derivatives and bounded indexed previews. Legacy health remains compatible.
- Native C++ retains feasibility/approval authority. Worker identity is echoed
  provenance, not authentication of a native run. Python `pipe_transport` remains
  explicitly unimplemented; the native viewer continues to use authoritative FVM.
- The desktop adapter still uses health only, and the installer still copies only
  `worker.py`. B's next step is deploying the mapper/package/runtime and extending
  capability negotiation/result handling. A copied worker alone keeps health and
  capabilities working and reports storage unavailable.
- Fresh merged-main direct `Breakwater.slnx` MSVC x64 RelWithDebInfo build passed,
  followed by **18/18 combined CTest suites** (17.26 s). The strict desktop worker
  suite passed 24 cases with zero failures/skips against the merged worker. The
  regenerated solution retains 66 projects/180 dependencies with no missing paths.
- The complete merged-source Python suite passed **69/69 tests** (64.571 s) using
  the pinned CPython 3.12.13 science environment. Pinned `lake build` completed
  1,061 jobs; all **21 theorem audits** and the expected rejection of an unapproved
  assumption passed. No new sanitizer run is implied by these normal-build checks.
- Re-ran worker/native verification against the just-built Visual Studio exporter:
  **290 real NDJSON requests**, 12 actual native fixtures, three solvers, 44 interior
  controls and 132 finite-difference comparisons passed with offline schema checks.
  Maximum volume difference remained `7.67386154621e-13 m3`.
- Local logs: `out/pr14-visual-studio-build.log`, `out/pr14-ctest.log`,
  `out/pr14-python-tests.log`, `out/pr14-lean-verify.log` and
  `out/pr14-worker-verification.json`. The versioned component report and complete
  scope are linked from [science/WORKER.md](../science/WORKER.md).

## A7 and desktop-stack integration — November 7, 2026 (upstream report)

- PR #11 merged at `e7569d9` without conflicts. Its request-local control influence
  path selects workloads with at least 32 routes and 2,048 candidates, preserves
  canonical survivor-cost ordering and independently validates finalists. Recorded
  component benchmark ratios are 2.178x for screening and 1.094x for complete search
  on the custom 10,000-candidate case; these are not new integration timings.
- Before the desktop-stack merge, direct Visual Studio x64 RelWithDebInfo build and
  **13/13 combined suites passed**; a separate native MSVC ASan build passed **8/8**.
  Pinned Lean `lake build`, all **21 theorem audits**, and the unapproved-assumption
  rejection passed in this main checkout. The B stack does not change proof sources.
- Reviewed PRs #7/#8/#10 and integrated their complete stack at `a707b32` without
  conflicts. GitHub records all three PRs as merged. SQLite history, bounded worker
  health, project pipe profiles and the captured native transport viewer are now
  on main, alongside A7. No scientific worker execution is advertised by that stack.
- Regenerated the root solution with **66 projects and 180 build dependencies**;
  every path/reference resolves, including the new control-influence test target.
- Rebuilt the combined root `Breakwater.slnx` directly with MSVC x64 RelWithDebInfo:
  **18/18 engine/desktop CTest suites passed** (18.63 s). A separate full-desktop
  MSVC AddressSanitizer build also passed **18/18** (79.65 s), with no reported
  sanitizer findings. The Python-worker suite ran 24 cases and pipe-viewer suite
  ran 10 cases, both with zero failures/skips. Qt Sql/qsqlite and the actual pinned
  Python interpreter were available; worker-health tests were not skipped.
  Local logs: `out/stack-visual-studio-build.log`, `out/stack-ctest.log` and
  `out/stack-asan.log`. The proof-source hashes remain identical to the fresh
  21-theorem verification above; the B stack changes no Lean sources or pins.
- At this checkpoint, the next science increment was implementing `capabilities` and
  `storage_rollout` methods in Person A's `science/` workstream, preserving legacy
  health and the published contract. Native volume approval remains authoritative;
  fixed-case PhysicsNeMo evidence does not supply arbitrary-route inference.

## Earlier B-workstream reconciliation — November 7, 2026

- Merged main `5c47d4f`, including Person A's latest `7c99c68` Lean increment, into the
  pipe-viewer workstream. Resolved README/status and generated-solution conflicts while
  preserving B3/B4, history/approval behavior and the full 65-project/177-dependency graph.
  No engine/science/desktop/runtime-contract code changed in this reconciliation.
- Rechecked the regenerated direct Visual Studio normal build: **17/17 CTest suites
  passed**. The full desktop ASan **17/17** result below covers identical runtime source.
- Scoped proof review found no blocker: five owned source hashes and the manifest match
  committed evidence; the 15-theorem audit permits only `propext`, `Classical.choice`
  and `Quot.sound`, and the negative-assumption probe is recorded as rejected.
- At this earlier B-workstream checkpoint, the following PR #6 proof/science record
  was upstream evidence. Lean/Lake and scientific packages were unavailable in that
  process; it did not claim a local proof/training replay. The upstream main-checkout
  report above records its own Lean run; this B7 chat did not repeat it. Proofs concern exact
  real finite-network algorithms, not C++ floating point, PDEs, ML quality or optimality.

## PR #6 integration — November 7, 2026 (upstream report)

- Merged A3–A6 at `fe40c20` without conflicts. Native reference exporters and benchmark
  targets are included in the regenerated root Visual Studio solution.
- Direct Visual Studio 2026/MSVC x64 RelWithDebInfo build of `Breakwater.slnx` passed,
  followed by **12/12 combined CTest suites passed**, including benchmark correctness
  and desktop/QML checks. This normal-build result does not imply a new combined
  sanitizer run.
- The regenerated solution includes 55 projects with valid paths. The merged-main
  Python suite also passed **39/39 tests** in the pinned science environment.
- A3 delivers CPU double-precision PyTorch storage rollout, actual dense/sparse/affine
  native comparisons and finite-difference/autograd checks. Recorded comparison:
  12 cases, 44 varying controls and 132 central-difference checks; maximum all-sample
  volume difference `7.67386154621e-13 m3`. See [science evidence](../science/PROGRESS.md).
- A4 executed an actual PhysicsNeMo 2.2.2 model, PDE residual, bounded 1,800-step
  training, checkpoint round-trip and held-out native transport comparison. The full
  component Python suite passed 39 tests. This is an experimental fixed-case smoke,
  not a qualified chemical-quality predictor. See [PHYSICSNEMO.md](../science/PHYSICSNEMO.md).
- A5 records 100/1,000/10,000-candidate optimized MSVC benchmarks for two network
  shapes, separating dense/sparse sampled kernels, affine screening, retained-run
  validation and full search. Normal and MSVC ASan component checks passed. Timing
  scopes, raw evidence and limitations are in [BENCHMARKS.md](../engine/BENCHMARKS.md).
- A6 delivers Lean 4.35.0/Mathlib-pinned exact-real algorithm proofs for valid finite
  graphs. Repeated in the merged-main checkout: `lake build` completed all 1,060 jobs,
  the 15-theorem assumption audit passed, and the deliberate unapproved-assumption
  probe was rejected as required. Local verification logs are under ignored
  `out/a6-proofs/`. See [component proof evidence](../proofs/PROGRESS.md) and
  [proof scope](SCIENCE_VALIDATION.md#checked-lean-algorithms).
- The health-only worker is unchanged. Desktop science contracts, process adapter
  and selected-pipe transport UI remain separate integration work.

## B4 native selected-pipe viewer — November 7, 2026

- Select a route and captured manual/investigation run, edit physical pipe inputs,
  and execute Person A's native scalar transport outside the UI thread. Flow resolves
  captured controls, source curtailment and pump availability; layout supplies no geometry.
- Added actual outlet traces, sampled time-distance fields, signed mass ledger, CFL,
  diagnostic extrema/residuals and run/snapshot/route/parameter provenance. Bounded
  previews retain native sample indices and endpoints; diagnostics cover the full run.
- Quality edits, numerical changes, reset, route deletion and changed source identity
  retire stale work. Layout moves preserve valid evidence. One native job may finish
  after cancellation, but cannot publish late or overlap a new job. Quality operations
  never apply controls or confer volume approval. Atomic JSON export is capped at 1 MiB.
- Final C++ checks: **17/17 combined suites passed** in direct VS2022 normal and full
  desktop MSVC ASan builds. The new pipe suite reports **10 passed, 0 failed/skipped**,
  covering direct native field/ledger parity, captured flows, candidate overrides,
  invalidation, lifetime/cancellation, bounds and atomic export. Independent scoped
  review found no actionable issue. Final QML render checks passed in both builds.
- Interactive native-only QA passed at 1440×900 and 1100×700: actual plots/ledger,
  physical length 100→120 m invalidating/rerunning quality while preserving volume
  approval, resizing and layout dragging preserving both forecast and native result.
  Only pre-existing Shortcut warnings appeared. The user's open app was preserved.
- UI export pickers opened, but computer-use tooling rejected filename input in owned
  pickers; no UI file write or UI saved-project round trip was verified. The native
  export, parameter persistence and cancellation paths passed automated checks. A GUI
  in-flight cancel was too fast to exercise manually. No additional hosted calls ran.
- The root solution now has 65 projects, 177 dependency references and no missing paths.
  Usage and scientific limits are in `docs/PIPE_TRANSPORT.md`. Worker methods beyond
  health remain reserved pending A's mapping; generic PhysicsNeMo route inference
  and clean live-model rehearsals are not claimed. A6's standalone proofs are included
  by the latest-main reconciliation, with the formal scope stated above.

## Person A merge and B3 science boundary — November 7, 2026

- Merged `codex/person-a-engine` at `eca890d` into this workstream with both parents
  preserved and no conflicts. Public numerical interfaces and worker health-v1 remain
  unchanged. A's source adds standalone CPU-double rollout/gradient checks, the fixed-case
  PhysicsNeMo smoke, native reference exporters and measured benchmark evidence.
- Independent scoped reviews found no integration blocker. The merged baseline passed
  **7/7 native suites** and **15/15 combined desktop ASan suites**, including the new
  benchmark correctness check. No performance measurements were repeated here.
- A's scientific training/parity results remain A-reported in `science/PROGRESS.md`.
  This laptop's bundled interpreter has no Torch/PhysicsNeMo; only the two dependency-
  free worker tests were rerun locally. The worker still implements health only.
- B3 publishes explicit units, identity, array ordering, signed mass ledger, bounded
  previews and safe reserved artifact rules. All three Draft 2020-12 schemas and all
  seven illustrative examples passed `jsonschema` 4.25.1 validation. These are proposed
  scientific methods for A's review/adoption, not advertised worker capabilities.
- Added strict native pipe-parameter parsing/hashing and optional project-v1 route
  profiles. Geometry is independent of layout; quality edits preserve otherwise eligible
  volume runs/approval. Route deletion/undo, malformed profiles and old-project round trips
  are covered. Projects without profiles keep their previous serialized shape.
- B3 combined checks: **16/16 suites passed** in both the direct VS2022 normal build
  and full desktop MSVC ASan build. Regenerated the root solution: 63 projects,
  170 dependency references and no missing project paths. Native pipe UI integration
  is the next B4 increment; A4's fixed-case model is not generic route inference.

## Live Nemotron verification follow-up — November 7, 2026

- A locally configured Windows user-environment key authenticated successfully. The
  existing protocol smoke verified the exact configured model identity, a genuine
  `get_network_state` call, native execution, matching result continuation and completion.
- A production CLI investigation selected a native-validated 4,800-unit run and staged
  it for operator review. No simulated controls were applied by verification tools.
- The already-running desktop investigation exposed repeated calls after its candidate
  quota was exhausted, followed by an invalid final response. Its evidence was retained
  and approval stayed disabled. The orchestrator now supplies remaining budgets on each
  request, removes exhausted candidate tools and offers no tools after the final call.
  Backend limits and strict validation/final-output checks remain authoritative.
- Three subsequent canonical production-controller sessions archived full trajectories
  and request metadata: one returned `no_validated_plan`; one selected a validated
  5,280-unit plan and staged it; one selected a validated 16,530-unit advisory plan
  without staging. A fourth session on a seven-asset, three-tank network with renamed
  IDs and a bypass redirected to a new buffer selected and staged a validated
  5,280-unit plan. These are selected evaluated plans, not global optima.
- All archived requests in those four sessions returned HTTP 200 with matching model
  identity and zero retries. Archives exclude credential/header/reasoning fields.
  A verification-only harness under ignored `.tools/live-check/` reused the actual
  AgentController, history recorder, native tools and repository; it did not mock the
  model or apply controls. It reloaded stored terminal evidence before JSON export.
- New budget regressions and existing orchestration checks: **17 passed**; normal
  history/controller checks also passed. Full desktop ASan: **14/14 suites passed**.
  Direct Visual Studio `/m:1` build and **14/14 suites passed**. The attempted parallel
  rebuild stalled and was cancelled before the serial verification; no parallel-build
  success is claimed for this follow-up.
  An updated normal desktop was built separately, preserving the user's open app.
- Hosted access is now verified. Three clean canonical operator-review rehearsals
  are **not complete**: bounded no-plan and unstaged advisory results remain explicit.
  Planning quality is variable; the demonstration still needs rehearsal. Scientific
  PyTorch/PhysicsNeMo execution remains unavailable.

## Person A transport integration — November 7, 2026

- PR #4 merged the pure C++20 finite-volume scalar transport API, example/CSV export,
  numerical documentation and 18 transport case groups. See engine/TRANSPORT.md and
  engine/PROGRESS.md for equations, limitations and original executed evidence.
- Headless normal and MSVC ASan builds passed six CTest programs: 47 baseline/search/
  transport case groups and three executable examples. The post-PR #2 integration
  headless run also passed. Transport remains separate from volume feasibility.
- Example: 64 cells, 41 frames, 136 substeps, maximum CFL 0.9 and maximum signed mass
  residual 1.36887055523e-12 mg. No model training or quality qualification is claimed.
- Desktop selected-pipe UI, physical-parameter persistence and worker integration are
  planned next. The Python worker still supports health only.
- Regenerated the full Visual Studio 2026 solution after transport integration:
  52 projects and 134 dependency references, with no missing project paths. A fresh
  parallel build exposed Qt 6.12's QML statistics aggregation preceding its input
  generation; a deferred executable dependency now orders that target correctly.
- After MSBuild Clean (with generated QML statistics confirmed absent), direct parallel
  root `.slnx` MSVC 19.51 / Qt 6.12.0 build passed, followed by **11/11 combined CTest
  suites passed**. This is a new normal-build result; prior component ASan evidence
  above does not imply a new combined 11-suite sanitizer run.

## Person B desktop foundation — November 7, 2026

PRs #1 and #2 were reviewed and merged with merge commits. The new foundation work
starts from `main` commit `630e33c` and merges the later changes through `bf8beb1`,
preserving transport, client safeguards, long control IDs and Qt build ordering.
This increment adds no edits to Person A's native/scientific files.

Implemented:

- Automatic SQLite investigation history with a dedicated worker-owned connection,
  transactional complete checkpoints, out-of-order checkpoint rejection, interrupted
  session recovery, atomic JSON export and validated historical-only import.
- Saved immutable snapshots, complete native trajectories/reports, objectives, events,
  sanitized model request metadata, recommendations and operator approval audit.
- History browsing, baseline/candidate selection, cost/control/margin/violation
  comparison, tank selection and actual trajectory charts including capacity boundaries
  and unclipped violations. Selection does not change simulated operating controls.
- Explicit staged-plan review showing IDs, native evidence and current→proposed
  controls. Historical evidence or a different selected/staged/recommended run cannot
  be applied; existing revision/reset/validation/double-application gates remain.
- An asynchronous Python-worker health adapter for the existing NDJSON-v1 protocol,
  with identity/schema checks, bounded output, timeout/cancel/crash handling, local
  interpreter configuration and accurate unavailable/not-implemented states.
- Sanitized request completion metadata before terminal events, including cancellation
  and deadlines. Final checkpoint capture/serialization is queued off the UI thread.
- The project-v1 and numerical engine interfaces remain unchanged. Imported evidence
  is never inserted into an authoritative live model gateway.

Executed evidence:

- Current combined desktop/Ninja build: **14/14 CTest suites passed**.
- Regenerated the root Visual Studio solution with 58 projects and all 155 existing
  dependency edges preserved. Direct VS2022 solution build and **14/14 suites passed**.
- Full combined desktop MSVC AddressSanitizer build: **14/14 suites passed**, no sanitizer
  failures reported. This includes persistence, controller/history, worker, agent,
  numerical search and QML startup/render checks.
- The configured real Python 3.12 worker suite reports **24 passed, 0 failed, 0 skipped**.
  It executes the existing worker's actual health operation as well as bounded failure
  fixtures; no PyTorch/PhysicsNeMo model execution is claimed.
- SQLite checks report **14 passed, 0 failed, 0 skipped**, including worker-thread
  capture, immediate shutdown draining, restart recovery, rollback and long control IDs.
- Combined transport-client checks report **30 passed, 0 failed** with fake HTTP responses.
- Interactive checks at 1440×900 and 1100×700 exercised all four tabs, actual tank A/B
  trajectories, control comparisons, historical provenance and disabled approval.
  QML lint passed for the investigator and chart. Qt's pre-existing Shortcut warnings
  remain; no QML runtime errors were observed.
- The CMake-installed application passed render/import/health smoke with Windows-only
  PATH and explicit Python. It loaded the deployed Qt SQL/SQLite plugins and the
  installed worker, returning real Python 3.12.14 health with scientific execution
  `not_implemented`. This checks the local install, not a fresh-machine VC runtime.
- Restarted that installed application with its existing database and no import
  argument, reopened the saved session through History, and recovered both actual
  runs, trajectories, cost and headroom. Historical operator approval stayed disabled.
- Comparison tests can export `offline-verified-session.json` when
  `BREAKWATER_TEST_ARTIFACT_DIR` is configured. Its numbers are actual native baseline
  and mixed-candidate results; the archive explicitly states no live model ran.

At original foundation delivery, the live milestone was **pending credentials**.
`NVIDIA_API_KEY` was absent from the process,
user and machine environments. No authenticated hosted request or live model rehearsal
was performed. Protocol smoke, three canonical rehearsals and edited/custom-network
live investigations remain required before claiming the live demonstration complete.
See [DESKTOP_FOUNDATION.md](DESKTOP_FOUNDATION.md) for workflow, storage, worker setup,
archive semantics and verification commands.

## Person B agent increment — November 7, 2026

Implemented:

- Asynchronous Qt Network Nemotron client with process-environment configuration,
  non-streaming tool calls, matching tool-result continuation, retries, deadlines,
  cancellation, bounds, redirect rejection and redacted errors.
- Eight allowlisted tools backed by the existing engine, immutable snapshot/run IDs,
  a 128-run session store, full retained trajectories, explicit independent validation,
  candidate comparison and approval staging. Existing native contracts are unchanged.
- Bounded adaptive orchestration: 12 calls, three candidate-refinement/search rounds,
  two minutes. Model-generated numbers cannot establish feasibility or create runs.
- A Qt investigator panel with activity, historical run summaries, grounded result
  fields and an explicit operator action to apply a staged simulated plan.
- Numerical edits/reset invalidate sessions and approvals; layout-only moves preserve
  them. Completed evidence survives cancellation/failure within the current session.
- Protocol/full-investigation live smoke executable and integration guide with schemas:
  [AGENT_INTEGRATION.md](AGENT_INTEGRATION.md).

Actual checks on Person B's laptop:

- Qt 6.12.0 pinned SDK, MSVC 19.44.35219.0 / VS2022 17.14.19, CMake 3.31.6 / Ninja.
- Native baseline: engine acceptance passed; baseline/bad/mixed/healthy CLI cases
  reproduce 0.769230769231 h, 2.66666666667 h, mixed 80/117 m3 and cost 12,780.
- Desktop RelWithDebInfo build and all six CTest suites passed: engine, project,
  fake-HTTP transport, real-engine gateway, fake-model orchestration and QML render.
- Full desktop AddressSanitizer build and the same six CTest suites passed with no
  reported sanitizer failures. The render smoke now waits for an actual rendered
  frame within 20 seconds instead of assuming readiness after a fixed delay.
- Agent-panel screenshot captured and visually inspected. Interactive model execution
  was not exercised in the screenshot; automated controller tests use a fake model.
- `breakwater_nemotron_smoke.exe`: exit 2, credentials unavailable, no live request.
- Independent same-family subagent code review identified cancellation/restart signal
  reentrancy; guards and regression cases now cover that issue and custom graph IDs.

Reproduce with `scripts/build.ps1 -Preset desktop -EmbeddedDebugInfo` after setting up
the Qt SDK. This machine also needed a short process-local PATH before importing the
VS developer shell. `/Z7` avoids this installation's C1902 PDB-helper mismatch. Checked
desktop configuration used `BREAKWATER_ENABLE_ASAN=ON`, `BREAKWATER_BUILD_DESKTOP=ON`
and `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` in a separate build directory.

Unavailable/limited:

- Live NVIDIA model access/tool execution is **unverified**; no API key is configured.
  Fake responses test software behavior, not live adaptive model performance.
- Person A's native bounded search (`4eea5a7`) is merged and exposed through
  `search_interventions`; combined post-merge verification is recorded below.
- Streaming, HTTP 202 polling, robust optimization and animated replay remain later
  work. The foundation above now provides durable evidence history and trajectory charts.
- Scientific calculations and proofs are available as standalone components; desktop
  science integration and qualified quality predictions remain unavailable.

## Implemented

### Person A search integrated into Person B

- Merged `codex/person-a-engine` commit `4eea5a7` without conflicts.
- Added the real `search_interventions` tool without changing the existing engine
  interfaces. Search axes, candidate counts, fixed horizon, run storage, native
  reference validation and operator approval gates are enforced in C++.
- Post-merge desktop/Ninja build: **9/9 CTest suites passed**. Full desktop MSVC
  AddressSanitizer build: **9/9 passed**, including the 16-case engine search suite,
  both search examples, gateway search and model/tool orchestration tests.
- The canonical tool test actually evaluates 432 combinations, finds 39 screening
  survivors and stores five independently validated finalists. Lowest evaluated cost
  is 4,800 synthetic units; final A/B volumes 116/117 m3, minimum headroom 4 m3.
  This uses storage headroom and establishes neither global optimality nor indefinite
  sustainability. The restricted search reports no feasible plan among its inputs.
- The fake-model integration test chooses search, receives real stored run IDs and
  native validation, stages the selected result and produces a grounded recommendation.
  This remains distinct from an unexecuted live NVIDIA investigation.
- Regenerated `Breakwater.slnx` with the optional VS2022 generator and existing .NET
  10 migration. The complete generated graph (50 projects, 127 dependency edges)
  is preserved, including all new agent/search targets. Direct single-process MSBuild
  of the root solution passed, followed by **9/9 generated-tree CTest suites passed**.
  VS2026 remains the generation-script default. No generated `.vcxproj` or SDK files
  are committed.

### Shared base

- C++20 / MSVC numerical library, canonical scenario, native CLI and 13 acceptance cases.
- Generic supported topology, control catalog, immutable candidate compilation, dense
  reference, sparse rollout, allocation-free affine endpoints, independent validator.
- Versioned C++/JSON contracts and scientific-worker protocol/health boundary.
- Native Visual Studio project generation and root `Breakwater.slnx`.
- Qt Quick 3D editor: primitives, placement, connections, pump attachment,
  property editing, save/load, undo/redo, background forecasts and approval invalidation.

## Original baseline evidence recorded before this increment

- `scripts/build.ps1 -Preset headless`: native compilation and all 13 engine cases passed.
- `scripts/Generate-VisualStudio.ps1 -Headless -Build`: Visual Studio/MSBuild x64 build
  and engine acceptance suite passed using MSVC 19.51.36260.0 / CMake 4.3.1.
- `scripts/build.ps1 -Preset asan`: MSVC AddressSanitizer build and engine suite passed.
- Python 3.12.13: `python -m unittest discover -s science/tests -v`: 2 tests passed.
- `scripts/Generate-VisualStudio.ps1 -Build`: full native Visual Studio/MSBuild x64
  RelWithDebInfo solution passed all three CTest suites: engine (13 cases), desktop
  project/controller (7 cases), and QML startup/render smoke (including asset labels).
- Direct `MSBuild.exe Breakwater.slnx /p:Configuration=RelWithDebInfo /p:Platform=x64`:
  root solution built successfully. Generated projects explicitly use v145 / stdcpp20.
- The Qt 6.12.0 scene was captured to `out/desktop-smoke.png` and visually inspected.
  Desktop controller behavior is tested; full mouse-gesture end-to-end automation is later work.

Tested runtime: Qt SDK revision `6.12.0-0-202609280346`, MSVC 19.51.36260.0,
CMake 4.3.1, Visual Studio 2026. Dependency downloads/build products are ignored in `.tools/` and `out/`.

## Planned, not implemented in this base

- Animated replay, robust optimization and additional chart types.
- Deployment and desktop integration of the implemented scientific worker methods;
  Python pipe transport remains reserved/unimplemented.
- Qualified/generalized ML quality predictions and transport differentiation.
- Custom SIMD/GPU kernels and sophisticated routing handles.

The UI's unavailable science integrations remain planned. Actual standalone PhysicsNeMo
execution, checked algorithm proofs and measured native timings are recorded above;
live hosted Nemotron execution is verified as recorded above. Validated chemical
quality and production control remain outside scope.

## PR #2 integration review (2026-11-07)

- Fixed a final-response reentrancy edge in `NemotronClient::finish`: consuming an
  oversized body may synchronously trigger client deletion or a replacement request.
  Lifetime, generation and reply-identity guards now protect the continuation. Both
  paths have regression tests, including a reply that emits only `finished`.
- Control IDs now have a separate 512-character parser/schema limit, so control names
  derived from valid 256-character imported asset/route IDs work through simulation
  and search. Snapshot/run/candidate ID limits remain 256. A regression round-trips
  the imported project and executes the discovered controls through real tools.
- Independently rebuilt the reviewed PR using MSVC 19.51.36260.0 / Qt 6.12.0:
  desktop RelWithDebInfo **9/9 CTest suites passed**, and the full desktop ASan
  configuration **9/9 passed**, with no reported sanitizer errors.
- Captured and visually inspected the investigator panel. Missing-key smoke returned
  the expected exit code 2 and explicit unavailable message; no live request was made.
  Genuine hosted NVIDIA access/tool calling remains unverified.
