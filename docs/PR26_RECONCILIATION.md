# PR #26 reconciliation and affected verification

PR #26 is reconciled with main **`e057fa44ed8c7c3127466d72d816a68228e9b033`**.
Fresh normal and MSVC ASan builds each passed **28/28 CTests**, with zero Qt/Python
skips and zero sanitizer reports. All **127 Python tests**, including PhysicsNeMo,
passed. Selected sensitivities, automatic scientific-history retention and historical
reopening were retained and exercised with actual workers and installed applications.

## Revisions and resolution

- Previous PR head: `e07bdf8961f64c16e466cca3fe13513ae76849ee`.
- Main merged: `e057fa44ed8c7c3127466d72d816a68228e9b033` (PR #25, following #23/#24).
- Merge/tested source: **`f50431088806927abaa823858ab5377276223972`**, preserving
  both parents. Verification records are a separate subsequent commit.
- Worktree/branch: `C:/Projects/Breakwater/.worktrees/combined-verification`,
  `codex/combined-verification`. The root checkout, its untracked local continuation
  plan, other worktrees, existing installation and running applications were preserved.
  `git archive HEAD` supplied the disposable package source; no local plan was copied.

Five files conflicted. `WorkerProtocol.cpp` retains main's complete sensitivity-aware
`storagePayload`/response validation, bounded rounded-multiple grid and exact endpoint.
The worker and controller suites combine main's derivative, malformed-result,
immutable-history and lifecycle tests with PR #26's fixed grid expectations, two/256
sample previews, numerical disagreement, 50 cancel/retry cycles and retirement after
a complete exchange awaiting clean process exit. Redundant forward rows are expressed
through the shared fixed-case table; the large-step underflow and renamed-budget rows
remain. The fixture retains both derivative/error modes and PR #26's lifecycle markers.
`STATUS.md` preserves main's complete new history and sensitivity entries plus the
dated original combined verification; the new reconciliation entry gives current scope.
The [original conflict diff](../science/evidence/pr26-reconciliation/merge/conflicts.patch)
and exact resolver/execution scripts are retained separately from source corrections.

The merged application, science protocol/controller/archive, history, QML, public
contracts and build configuration match accepted main. No numerical algorithm,
project/worker schema, dependency pin, numerical threshold or application deadline was
changed by resolution. Main's native solver provenance **`constant-flow-0.1.1`** and
worker provenance **`torch-constant-flow-0.1.2+worker-v1`** are retained. The
[manifest](../science/evidence/pr26-reconciliation/manifest.json) records the tested
tree, parents, source/executable SHA-256 values, equality check and individual Qt counts.

## Fresh automated results

| Configuration | Registered / passed | CTest elapsed |
|---|---:|---:|
| Normal | 28 / 28 | 390.19 s |
| MSVC ASan | 28 / 28 | 439.93 s |

Both configurations include all eight headless/native suites, real forward and selected
sensitivity worker/controller checks, scientific archive persistence/export/reimport,
normal/compact historical rendering, live forward/derivative rendering and native
pipe evidence reopening. Registration and individual Qt results were inspected;
there were no omitted science suites or unexpected skips. QML lint returned zero in
both builds; its existing advisory diagnostics and compiler warnings remain in logs.
All 25 native transport cases ran in each complete configuration.

| Qt suite | Passed in each build | Failed | Skipped |
|---|---:|---:|---:|
| `agent_gateway` | 22 | 0 | 0 |
| `agent_orchestrator` | 17 | 0 | 0 |
| `history_controller` | 9 | 0 | 0 |
| `nemotron_client` | 46 | 0 | 0 |
| `pipe_transport` | 14 | 0 | 0 |
| `pipe_transport_evidence` | 13 | 0 | 0 |
| `python_worker` | 24 | 0 | 0 |
| `science_evidence` | 42 | 0 | 0 |
| `session_repository` | 14 | 0 | 0 |
| `storage_science` | 43 | 0 | 0 |
| `storage_worker` | 121 | 0 | 0 |

The worker suite includes **50 controlled cancel/retry cycles in each build**; original
process handles remain retained through survivor checks. Both builds rejected malformed
forward/derivative frames, identities, ordering, provenance, endpoints and indices,
and preserved clean-exit publication, captured arrays, selected values and native
approval authority. Controller checks include edit/delete/replacement while a full
exchange awaits exit, synchronous changes and numerical disagreement.

Python discovery ran **127 tests, zero skips**, with CPython 3.12.13, Torch 2.15.0+cpu,
NumPy 2.5.4, PhysicsNeMo 2.2.2 and jsonschema 4.25.1. Native dense/sparse/affine versus
PyTorch and actual-worker verification passed all **18 fixtures**, **302 real NDJSON
requests**, **44 selected controls** and **132 central differences** at the unchanged
three step sizes. Maximum forward volume difference: **7.67386154621e-13 m³**.
Forward tolerance remains `1e-9 + 1e-10*max(abs(a),abs(b))`; grid/identity comparisons
remain exact. Reports, runtime versions, commands and elapsed observations are in
[science evidence](../science/evidence/pr26-reconciliation/science/executions.json).

Fresh separate Ninja `RelWithDebInfo` trees use C++20, MSVC 19.51.36260.0, VC tools
14.51.36231, Windows SDK 10.0.26100.0, Qt 6.12.0, CMake 4.3.1-msvc1 and Ninja 1.13.2.
`/fp:precise`, `/Z7`, `/O2 /Ob1 /MD` and non-incremental linking remain enabled;
ASan adds `/fsanitize=address`. Configure/build/lint/CTest logs, flags, CMake caches
and registered tests are retained for both builds. Zero sanitizer reports were found.
Prebuilt Qt/Python/Torch and other vendor libraries remain outside established ASan
instrumentation coverage. Builds and other verification overlapped; these timings
are observations on this machine, not isolated benchmarks.

## Installed sensitivity and historical acceptance

A fresh disposable source archive of the merge commit was built, packaged and installed
outside the export. A second installed copy was relocated. Only that disposable source
export was renamed unavailable; the pinned interpreter stayed outside it. The installed
process environment had Windows-only PATH and no Python/source path overrides.
Deployment logged an absent optional Qt translations catalog; the default-language
application and all required deployed modules passed the acceptance checks below.

**16 installed application executions passed in the corrected attempt** across local
and relocated copies.
Actual mixed-case native/Torch sensitivities retained terminal volumes **80/117 m³**,
cost **12,780**, captured controls **10/20 m³/h**, selected-column order, terminal
Jacobian `[[-3,-3],[0,3]] h` and cost gradient `[750,0]`. Each copy saved its exact
completed result ID with the original **121 native samples** and full selected matrix.
SQLite integrity returned `ok`. Independent restarted installed processes imported the
exact persisted archive and rendered historical Science at normal and compact sizes
with a deliberately missing Python executable, retaining identical arrays/derivatives,
historical labeling and non-actionable state. These are automated rendering checks,
not an interactive workflow test. The database archive was serialized directly for
this installed restart check; actual repository export/reimport is covered separately
by the `science_evidence` Qt suite, using real worker forward and derivative results.
SQLite retains the original canonical completed archive unchanged; historical marking
and the non-actionable recommendation view are applied by the repository on loading.

Both installed module-origin probes resolved only deployed files and executed
capabilities → selected-derivative storage → capabilities (**six NDJSON requests**),
with verification false before execution and true afterward. Missing `worker.py`,
`worker_storage.py`, `storage_grid.py`, interpreter and Torch returned the expected
explicit error and exit 6 without development-source fallback. No credentials were
copied into the export, child environment or evidence. Installed file hashes,
application commands/reports, SQLite/history checks, module origins and automated
screenshots are in [installed evidence](../science/evidence/pr26-reconciliation/installed/summary.json).

## Reproduction and preserved scope

Exact top-level commands used from the isolated worktree (scripts are retained in
the committed `commands` evidence directory):

```powershell
# run-build.ps1 initializes the x64 VS developer shell and Qt/Python paths.
& ./out/verification/reconciliation/run-build.ps1 -Variant normal
& ./out/verification/reconciliation/run-build.ps1 -Variant asan
& 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe' -X utf8 ./out/verification/reconciliation/run-science.py
& ./out/verification/reconciliation/run-package.ps1
& 'C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe' -X utf8 ./out/verification/reconciliation/installed_checks.py
```

The scripts' original location was ignored `out/verification/reconciliation/`; their
root calculation is relative to that location. For replay, copy them back there first
and use fresh attempt/build/install names. Exact expanded commands/configure arguments
are recorded in each execution JSON and package/source hashes in package evidence.
Merge conflict evidence is attempt 01, complete builds attempt 02, science attempt 03,
packaging attempt 04 and corrected installed checks attempt 06. Installed attempt 05
stopped after two successful app executions: the probe incorrectly expected a stored
`historical` key and raised `KeyError`. Source inspection and the rendered historical
state established that the repository applies that marking on load while retaining
the canonical archive. The probe now asserts exact stored archive equality and relies
on the actual-app historical smoke/Qt checks for the loaded view. No product code or
acceptance tolerance was changed. The failed script, assertion record, app reports and
screenshots are retained in `failed-installed-attempt-05`; all full attempt directories
remain. The original failed baseline and prior attempts remain unchanged.

The [original combined report](COMBINED_VERIFICATION.md) and its evidence retain their
frozen baseline scope. No engine/science/proof source changed between its final head
and this merge; this reconciliation did not repeat the separate 45-case/580-gate PDE
qualification, 1,800-update neural smoke or Lean audit. Their earlier executed results
remain attributed there, including the **known coarse scheduled-inlet refinement
failure**, **0.40191 mg/L front maximum-error limitation**, 12 neural gates and
54-proposition audit. The original checksum index describes files at `e07bdf8`, including
that revision's `STATUS.md`; it is not a checksum claim about the updated status file.

No new interactive UI pass is claimed. The earlier native-dialog save/load/export
interactions remain unverified and completion won the interactive cancellation race;
deterministic automated cancellation evidence remains passing. Hosted Nemotron,
clean-machine acceptance, new physics and the separate front-accuracy study remain
excluded. The local continuation plan is excluded from this branch and PR.
