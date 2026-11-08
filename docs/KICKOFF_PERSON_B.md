# Person B: Nemotron, APIs, desktop and integration

Updated November 7, 2026. This is your active audit-first work plan. Coordinate with
[Person A's plan](KICKOFF_PERSON_A.md). The steps below are planned work, not a claim
that the comprehensive audit or next features have already been completed.

Current workstream update: B5a/B5b runtime deployment and forward desktop comparison
are implemented. Person A's audit/fixes through PR #17 (`367dcf1`) are integrated;
see [AUDIT_REPORT.md](AUDIT_REPORT.md) and the latest [STATUS.md](STATUS.md) evidence
before starting B5c. The original audit and staged roadmap below remain intact.
Manual release-workflow gaps and scientific retention remain open.

## Role, baseline and ownership

You own the native C++20/MSVC/Qt Quick application, NVIDIA HTTPS client, Nemotron
tools, public contracts, packaging and integration. API work means the existing
native tool/NDJSON boundaries; retain the accepted Windows architecture.

At preparation, main was 794c7cd. A3–A8 are merged: the repo worker implements
health, capabilities and storage_rollout. The desktop client still calls health
only, and its installer copies only worker.py. Native pipe transport/viewing,
investigation history, planning tools and simulated approval already exist.

Recorded baseline evidence is 18 combined CTest suites, 69 Python tests, 21 audited
Lean theorems, and a 66-project/180-dependency solution. Reproduce relevant checks
on your machine and record actual counts; these records are not a new audit.

| You may edit | Request changes from A |
|---|---|
| desktop/, contracts/, root CMake/presets, scripts/, Breakwater.slnx, README and shared docs/ | engine/, science/, proofs/, scenarios/, their tests, local build files and dependency locks |

Read any subsystem for review. Make fixes within your ownership. Publish necessary
additive contract changes with examples, then obtain A's adoption before implementation.

Use a separate clone/worktree and codex/person-b-desktop or a named codex/ subbranch
from latest origin/main. Inspect status first; preserve existing work. Do not switch
another session's branch, reset its work, force-push, or put feature work directly on
main. B integrates small reviewed PRs. Preserve complete ancestry and original plans.

Read first: [AGENTS](../AGENTS.md), [original handoff, Part A](../For%20Dev%20Agents.md),
[accepted plan](IMPLEMENTATION_PLAN.md), [status](STATUS.md),
[parallel ownership](PARALLEL_WORK.md), [science contract](SCIENCE_CONTRACTS.md),
[schemas](../contracts/science/README.md), [A8 worker](../science/WORKER.md),
[desktop foundation](DESKTOP_FOUNDATION.md) and [pipe viewer](PIPE_TRANSPORT.md).

## 1. Audit the entire foundation with A before new features

Freeze the reviewed commit and create a requirements-to-implementation/evidence matrix.
Distinguish working source, installed functionality, recorded upstream evidence,
unverified behavior and planned features. Review A's boundary conclusions while A
reviews your scientific mappings.

| Area | Required review and evidence |
|---|---|
| Editor/project | Stable IDs, topology edits, route attachments, drafts, save/load, undo/redo, atomic writes, physical settings, layout versus numerical revisions |
| State/approval | Immutable snapshots/runs, effective controls, validation before staging, user-only application, reset/deletion/cancellation, late completion and historical-import rejection |
| Worker | Legacy health, version/method negotiation, selected controls, request IDs, identity, dimensions, exact horizon, original indices, byte/work limits and missing runtimes |
| Pipe viewer | Captured flow including failed pumps/curtailment, explicit geometry, signed ledger, cell averages, stale binding and volume/quality separation |
| Persistence | SQLite thread/lifetime behavior, immutable records, corruption/size failures, export/import and historical-only restoration |
| Nemotron | Real numerical tools, 12 calls/three refinement rounds/two minutes, fixed constraints/horizon, fabricated IDs, skipped validation, malformed calls, timeout/no-plan behavior and credential redaction |
| Delivery/docs | Qt/QML/Sql/qsqlite, scientific modules, Python pins, source-path fallback leakage, actual dialogs and current versus historical claims |

Rebuild the root solution directly and run normal plus full-desktop ASan tests.
Configure an actual Python interpreter so worker tests run; record skips as unavailable.
Coordinate A's fresh numerical/science/Lean checks instead of attributing old evidence
to this audit. Manually check save/export/reopen dialogs, in-flight cancellation,
restart, 1440×900 and 1100×700 layouts, and a renamed/custom network.

Use x64 Visual Studio Developer PowerShell with installed MSVC, CMake/Ninja and Qt:

~~~powershell
$qtPrefix = 'C:\path\to\Qt\6.12.0\msvc2022_64'
$env:BREAKWATER_PYTHON = 'C:\path\to\science\.venv\Scripts\python.exe'
.\scripts\Generate-VisualStudio.ps1 -QtPrefix $qtPrefix -EmbeddedDebugInfo
MSBuild.exe Breakwater.slnx /p:Configuration=RelWithDebInfo /p:Platform=x64 /m:1
ctest --test-dir out/visual-studio -C RelWithDebInfo --output-on-failure
cmake -S . -B out/audit-desktop-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBREAKWATER_BUILD_DESKTOP=ON -DBREAKWATER_ENABLE_ASAN=ON -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded "-DCMAKE_PREFIX_PATH=$qtPrefix" "-DPython3_EXECUTABLE=$env:BREAKWATER_PYTHON"
cmake --build out/audit-desktop-asan --parallel 2
ctest --test-dir out/audit-desktop-asan --output-on-failure
~~~

Check each exit status. VS2026 is the default; explicitly select the supported VS2022
generator when needed. .tools/ and out/ are local/ignored, not supplied by cloning.
Record setup failures; do not assume another developer's paths/packages. Sanitizer
timings are not benchmarks.

Write docs/AUDIT_PERSON_B.md, then aggregate A's science/AUDIT_PERSON_A.md into
docs/AUDIT_REPORT.md. Each finding needs severity, owner, file/evidence reference,
reproduction, impact, proposed fix and acceptance test. Record versions, commands,
commit, failures/skips and requirement gaps. Passing tests alone does not close
review. Resolve P0/P1 correctness, approval, persistence and deployment blockers before
dependent features; explicitly triage lower-priority findings.

## 2. Fix findings and reconcile existing work

Deliver owner-specific fixes in small PRs. At preparation,
[PR #13](https://github.com/Towstar/Breakwater/pull/13) was open: historical native-pipe
evidence reopening plus a B5 plan, not production science-client integration. Recheck
its current head/status against main/A8 and integrate after relevant checks pass.
Preserve historical/unverified labels and rejection of approval restoration, including
matching IDs/hashes. It does not supply automatic scientific-result retention.

Refresh STATUS, continuation instructions and kickoffs from the findings. Preserve
IMPLEMENTATION_PLAN.md and For Dev Agents.md as the full roadmap/handoff.

## 3. B5a: deploy the scientific worker

Install worker_storage.py and the required breakwater_science package beside
worker.py; configure a compatible Python environment using A's pinned dependencies.
Keep SDKs, environments, credentials, caches and checkpoints out of Git. Test an
installed/staged application from a new location with the source checkout unavailable,
so a development fallback cannot hide missing modules. Execute actual storage requests.

**Exit gate:** installed health/capabilities/storage work. Missing Python/Torch produces
an explicit unavailable state while native simulation stays usable. A worker.py-only
deployment still preserves legacy health truthfully.

## 4. B5b: typed asynchronous capabilities and forward analysis

Extend desktop/science/ with a bounded session lifecycle. Use the same worker process
for capabilities → storage_rollout → capabilities: execution verification is process-local.
Allow the first calculation before execution_verified becomes true. Code/package
availability is not scientific execution or qualification.
After the final response, close stdin and require a clean process exit before publishing
the scientific result; a response followed by a crash must fail the job.

Test fake-worker faults, then A's real worker: malformed/oversized output, unsupported
versions, wrong IDs, bad shapes/nonfinite values, stream limits, crashes, deadlines,
cancel/restart and late completions. Enforce identity/control bindings independently
of JSON Schema. Artifacts remain unsupported under current capabilities.

Start with a forward-only UI comparison using an immutable native run and effective
controls. Keep its original horizon/time grid; **never infer step_h from a decimated
display preview**. Preserve time indices and tank IDs. Show native/PyTorch values,
differences, units, versions and warnings without granting feasibility, staging plans
or applying controls.

Audit the current captured-run metadata: add the original requested simulation config
and actual native grid/reference arrays where absent. Coordinate representable-grid
and rounding conventions with A before strict comparisons. Investigate the reported
one-ULP short-endpoint case and A8's rejection of duplicate-time grids. Do not
silently repair samples, shorten the requested horizon or infer config from a last point.

**Exit gate:** actual development and installed requests agree with native references
on canonical and renamed/custom networks; UI stays responsive. Numerical edits, reset,
run changes, deletion and cancellation retire stale results. Layout-only movement
preserves eligible results.

## 5. B5c: selected-control derivatives

Add gradients separately after the forward path works. The request controls list
supplies both overrides and derivative columns in that exact order; omitted controls
remain captured constants. Selected fixed/boundary controls fail explicitly. Explain
this domain and show terminal Jacobians in hours and cost gradients in synthetic units
per m3/h. Never silently perturb inputs or drop requested derivatives.

Changed analysis settings require a corresponding native captured run/identity; do
not reuse an unrelated run ID. Check response identity, ordering, dimensions and horizon.
Scientific results remain advisory and cannot restore or bypass native approval.

## 6. Evidence, Nemotron rehearsals and release

Persist bounded, versioned scientific evidence with captured identity/controls, units,
versions and warnings. Reopened results are historical and non-actionable. A fresh
editable incident requires explicit creation, native validation and fresh approval.

Complete actual save/export/reopen and cancellation workflows, then the outstanding
three canonical operator-review rehearsals and a custom-network rehearsal. Use existing
hosted smokes with configured credentials; never request keys in chat or expose them
in QML/logs. Retain failed/no-plan attempts and actual model-issued tool/run evidence.
A valid plan need not match a prescribed cost; finite search is not global optimality.

**Release gate:** reproducible installed Windows build, current normal/ASan/science/Lean
evidence, declared limitations and a working operator workflow. Keep the fixed-case
PhysicsNeMo smoke separate from arbitrary-route inference and authoritative native FVM.

## Delivery and copy/paste starting instruction

A can qualify transport in parallel with B5 without changing this contract. Request
numerical fixes from A with reproductions; B owns shared contracts and integration.
Each PR reports commit, paths, interface changes, actual tests, limitations and a runnable
example. Regenerate solutions when targets change; merge reviewed PRs, update STATUS
and merge main into shared branches without force pushes.

~~~text
Continue BREAKWATER as Person B using docs/KICKOFF_PERSON_B.md. First inspect the
current branch/status and perform step 1 with Person A. Produce the finding ledger
and acceptance gates before new features. Preserve A-owned files, existing work,
legacy health and native approval authority. Resolve blockers, then deliver B5a,
B5b and B5c as separate tested PRs. Begin with actual inspection/evidence; do not
claim the audit or planned features are already complete.
~~~
