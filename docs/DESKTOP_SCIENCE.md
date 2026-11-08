# B5: desktop CPU storage comparison

The Science tab executes the actual Python worker's CPU float64 PyTorch storage
rollout for a retained native forecast. It compares returned samples at their original
native indices, reports differences and preserves finite negative/over-capacity values.
Native C++ remains the feasibility/approval authority. This is forward-only analysis;
selected derivatives, candidate/history science and automatic science retention are
separate increments. The native pipe viewer remains independent of Python.

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

Agreement uses `1e-9 + 1e-10 * max(abs(native), abs(worker))`, separately in m3 and
synthetic cost units. Scaled error at or below one is within that tolerance. The metric
covers returned preview samples, not undisplayed worker samples or every feasibility
constraint. Agreement is advisory and never stages/applies controls or grants approval.

## Process and identity boundary

Each job uses one fresh process for capabilities, forward storage, and capabilities
again. All IDs/versions, complete captured identity, tank ordering, original time indices,
finite dimensions and provenance are checked. A successful post-execution capability
flag belongs to that completed process; a fresh process starts unverified. Results are
published only after stdin closes and the process exits successfully. Unexpected output,
crashes, cancellation, deadlines or stale generations cannot publish late.

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

CTest registers real Science render checks when the configured interpreter detects
Torch; real tests still have to execute successfully. Missing-runtime skips are not
counted as scientific success. [STATUS.md](STATUS.md) records actual counts, installed
copy checks, UI coverage and limitations for this increment.
