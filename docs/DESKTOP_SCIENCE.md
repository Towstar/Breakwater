# B5: desktop CPU storage comparison and sensitivities

The Science tab executes the actual Python worker's CPU float64 PyTorch storage
rollout for a retained native forecast. It compares returned samples at their original
native indices, reports differences and preserves finite negative/over-capacity values.
Native C++ remains the feasibility/approval authority. B5c adds explicitly selected
continuous-control derivatives at unchanged captured values. Completed forward and
sensitivity results are now retained automatically in local History, including results
that disagree with native. Agent-candidate science uses a separate capture and worker
lane with the same comparison and retention checks. The native pipe
viewer is independent of Python.

## Nemotron-selected scientific analysis

The production investigator exposes `get_sensitivity_catalog(snapshot_id, run_id)`
and `analyze_sensitivities(snapshot_id, run_id, control_ids)`. Nemotron can inspect a
stored native baseline/candidate, select 1–8 eligible interior controls in native
catalog order, execute actual CPU PyTorch derivatives at unchanged captured values,
and use their directions when choosing later numerical experiments. Tool order and
the final selected run remain model choices. Native simulation, independent validation,
staging and explicit operator review retain their existing authority.

Each analysis freezes the stored run's effective network including its overrides.
Its capture uses that run ID as snapshot ID and revision zero; it never reads a
later mutable editor state. The returned model summary (at most 256 KiB) contains
native/worker sampled comparison, selected-control values and units, derivatives,
provenance and limitations, with `advisory: true` and
`validation_status: "not_performed"`. It contains no full rollout or credentials.
The complete scientific DTO and original native trajectory are saved as a separate
scientific History session. Matching `origin`/`agent_origin` fields bind the original
incident snapshot/hash, effective network hash, native run, agent session and tool-call
ID. Opening that evidence cannot stage or authorize the candidate.

This increment adds stored-run agent sensitivities. Manual Science still uses the
current `ProjectController` capture; a general investigation-candidate picker and
candidate forward-comparison workflow remain separately scoped E1 work.

The dedicated scientific worker keeps model requests and UI updates nonblocking;
missing methods/runtime and rejected or timed-out responses become matched tool
errors. Cancelling an investigation retires its scientific work and stops model
continuation. Completed native runs remain available as evidence. Manual Science-tab
selection and captures are independent. [DEMO_REHEARSAL.md](DEMO_REHEARSAL.md) describes
operator-reviewed live runs and a read-only audit of their retained cross-references.

## Automatic scientific history

1. Run a native forecast, open Science, then compare with PyTorch or run selected
   sensitivities. The local history status reports saving, success or a storage error.
2. Open History and select the saved comparison or sensitivity session. Choose
   **View scientific evidence** to inspect its tank charts, errors, derivatives,
   captured values, versions, warnings and original identities.
3. Restart the application and reopen the same session. **Export JSON** and
   **Import JSON** use the existing versioned session workflow. **Return to current**
   returns to the separate live capture; it does not restore controls or selections.

Each completed result is an immutable completed session with a distinct result UUID.
It retains the frozen network, full original native trajectory/configuration and
reported native validation, plus the bounded worker preview (at most 256 original
sample indices) and full selected-control derivative matrix. Omitted controls remain
in the captured network. Worker previews are not represented as full worker rollouts.

SQLite and session archive versions remain 1. Science sessions add
`session_kind: "scientific"`, `scientific_evidence_version: 1` and a singleton
`scientific_evidence` array. Each scientific result is capped at 1 MiB; full native
data retains the existing session/network/sample/archive limits. The validator checks
field/version allowlists, identities/hash/revision, exact original grid, captured
controls/domain/order/units, preview/native binding, finite dimensions, provenance
and recomputed comparison diagnostics before saving or importing. Existing investigation
archives need no science fields. Project-v1 files and worker NDJSON are unchanged.

Serialization, validation and transactional writes run on the existing repository
thread, which drains accepted checkpoints on shutdown. A storage failure remains
visible while native forecasting and completed scientific results stay usable.
Reopening is historical and non-actionable. Imported JSON is internally checked
evidence, not authenticated worker execution, fresh feasibility validation or approval.
It never restores a live capture, launches a worker or authorizes a simulated plan.

For a reproducible historical render of an exported science session:

```powershell
.\scripts\Start-Desktop.ps1 -Wait -AppArguments @('--import-session','C:\path\science.json','--open-scientific-history','--smoke')
```

The science-check smoke requires successful retention of its exact result ID. A
storage failure returns exit 7 and does not produce a success screenshot.

## Local setup and launch

From the repository root in PowerShell 7:

```powershell
.\scripts\Setup-ScientificWorker.ps1
$scientificPython = (Resolve-Path '.tools/science-cpu/Scripts/python.exe').Path
.\scripts\Generate-VisualStudio.ps1 -Generator 'Visual Studio 17 2022' -EmbeddedDebugInfo -PythonExecutable $scientificPython
# Run in an x64 VS developer shell, with the documented local Qt SDK available.
MSBuild.exe Breakwater.slnx /p:Configuration=RelWithDebInfo /p:Platform=x64 /m:1
ctest --test-dir out/visual-studio -C RelWithDebInfo --output-on-failure
.\scripts\Start-Desktop.ps1 -Wait -AppArguments @('--agent-panel')
```

VS2026 remains supported; select the generator installed on the machine. See README
for Qt setup. The setup script verifies pinned uv 0.11.9's official archive checksum,
installs isolated CPython 3.12.13 AMD64, synchronizes A's CPU hash lock (Torch
2.15.0+cpu/NumPy 2.5.4), then installs the worker-test hash lock additively. It verifies
an actual CPU float64 calculation. Tools, caches and the environment remain under
ignored `.tools`; scientific source/locks, global PATH and existing Python are preserved.
An optional Windows minor-version shortcut failure is narrowly handled by using the
verified concrete interpreter path. Other failures stop setup.

`Start-Desktop.ps1` selects Python in this order: explicit `-PythonExecutable`, existing
process `BREAKWATER_PYTHON`, then the local environment. It configures only the child
process. `-Executable` supports an installed application. Missing science configuration
does not prevent native application launch. Hosted credentials are optional and never
printed; science requires no API key. The scripts were exercised in PowerShell 7;
this machine's PowerShell 5 execution policy blocked its launch probe and was preserved.

## Operator workflow

1. Choose a preset or edit/load a valid network, then **Run forecast**.
2. Open the investigator panel and select **Science**.
3. **Check health** preserves the compatibility operation. **Check capabilities**
   discovers deployed methods; neither operation proves that Torch has executed.
4. Choose **Compare with PyTorch**. The first attempt does not require a previously
   verified execution flag. Inspect both tank curves, capacity boundaries, per-tank
   returned-sample errors, cost difference, provenance and warnings.
5. **Cancel** retires the job. Numerical edits, reset, deletion or a new native forecast
   retire prior science. Camera, selection, layout and independent pipe-parameter edits
   preserve a still-matching captured forecast. Selecting/charting tanks changes no controls.
6. Scroll to **Selected-control sensitivities** and select the captured controls to
   differentiate. Fixed/unavailable controls, values on a bound and source flows on
   their feed-capacity boundary are disabled with a reason. No controls are preselected.
7. Choose **Run selected sensitivities**. Inspect the selected tank's terminal-volume
   Jacobian in **hours** and cost gradient in **synthetic units per (m3/h)**. Select
   another tank to view its rows. Columns follow the displayed native catalog order.

Only the selected controls are sent as overrides, at exactly their captured values;
omitted controls remain captured constants. Changing a value requires editing the
native network and running a fresh forecast. The UI never perturbs a boundary input,
changes operating controls, or silently drops a requested derivative. For example,
over a three-hour horizon a one-unit increase in an interior transfer from tank A
to B has terminal-volume derivatives -3 h / +3 h with other controls fixed.

Changing the selection retires pending/completed sensitivity evidence; an independent
forward result remains viewable. A new capture clears selection. The complete validated
matrix is retained, while the bounded list displays one tank's rows. Oversized results
fail explicitly rather than truncating columns. Sensitivities do not differentiate
through rejection, discrete search or operator approval.

Agreement uses `1e-9 + 1e-10 * max(abs(native), abs(worker))`, separately in m3 and
synthetic cost units. Scaled error at or below one is within that tolerance. The metric
covers returned preview samples and cost, not undisplayed samples, derivatives or every
feasibility constraint. Derivative checks enforce finite shapes, exact IDs/order and
captured provenance; numerical derivative/native finite differences are regression
checks, not a runtime feasibility verdict. Agreement is advisory and never stages/applies
controls or grants approval.

## Process and identity boundary

Each job uses one fresh process for capabilities, forward storage, and capabilities
again. All IDs/versions, complete captured identity, tank ordering, original time indices,
finite dimensions and provenance are checked. A successful post-execution capability
flag belongs to that completed process; a fresh process starts unverified. Results are
published only after stdin closes and the process exits successfully. Unexpected output,
crashes, cancellation, deadlines or stale generations cannot publish late.

Sensitivity evidence also binds analysis mode, selected IDs/values and job generation.
Results include `analysis_mode`, `selected_controls`, `native_solver_version`, and
the requested `derivatives` object. Forward results reject unrequested derivatives.

Limits are 1 MiB per UTF-8 frame including newline, three MiB total for the storage
exchange, and 64 KiB counted/discarded stderr. Health defaults to ten seconds; science
has a sixty-second whole-job deadline. Replacement work waits for the previous child
to exit. Child-provided error text and model credentials do not enter evidence.

The captured native run retains its original requested configuration and immutable full
reference arrays. The worker's bounded preview is never used to infer a new step. The
grid retains every rounded `k * step` strictly before the requested horizon, then
appends that exact horizon once. This includes a one-ULP terminal interval when
needed. The desktop independently checks that grid and its original indices instead
of deriving its length with `ceil(horizon / step)`. More than 10,000 updates or an
unrepresentable grid fails explicitly. The original request is never shortened.

## Installed and automated checks

CMake installs `worker.py`, `worker_storage.py`, `storage_grid.py` and the Python package beside one
another. Development source fallback is limited to the canonical development binary
directory. A copied installed app must use its deployed files and external configured
runtime; an absent mapper or Torch produces an unavailable state.

For real desktop/render evidence, with the environment configured:

```powershell
$report = Join-Path (Get-Location) 'out/storage-science.json'
$image = Join-Path (Get-Location) 'out/storage-science.png'
$database = Join-Path (Get-Location) 'out/storage-smoke.sqlite'
.\scripts\Start-Desktop.ps1 -Wait -AppArguments @('--science-check','--smoke','--science-report',$report,'--screenshot',$image,'--history-db',$database)
```

This launches a separate synthetic mixed-plan demonstration, computes a native forecast,
executes the real worker and requires matching comparison evidence plus the visible
Science page and a rendered frame. `--compact-ui --science-example bad` exercises the
compact view and infeasible native predictions without treating agreement as approval.
Report-write failure returns exit 7; execution/identity/comparison failure returns a
nonzero exit. Test reports and screenshots are generated evidence, not project files.

For the same actual-app check with explicit selected captured controls:

```powershell
.\scripts\Start-Desktop.ps1 -Wait -AppArguments @('--science-check','--science-derivatives','--science-control','source:source_a:curtail','--science-control','route:bypass:flow','--smoke','--science-report',(Join-Path $PWD 'out/storage-sensitivities.json'))
```

Missing, duplicate, unknown or ineligible selections fail explicitly. This mode supplies
no alternative values: the displayed native mixed-plan capture remains the reference.

CTest registers real Science render checks when the configured interpreter detects
Torch; real tests still have to execute successfully. Missing-runtime skips are not
counted as scientific success. [STATUS.md](STATUS.md) records actual counts, installed
copy checks, UI coverage and limitations for this increment.
