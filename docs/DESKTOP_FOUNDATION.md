# Desktop investigation foundation

This increment implements the desktop-first milestone after PRs #1 and #2 were
reviewed and merged into `main`. The engine/science/proof owners and existing
project-v1 and worker-v1 contracts are unchanged.

## History, comparison and operator review

Open **Nemotron investigator**. Its tabs separate the current investigation,
numerical comparison, saved history and worker readiness.

- **Investigation:** start/cancel the current model session and inspect actual activity.
  Storage failures are shown separately; manual forecasts remain available.
- **Comparison:** select baseline and candidate runs, select a tank, and inspect costs,
  controls, headroom, first violations and complete stored volume trajectories. Capacity
  boundaries and out-of-bounds predictions remain visible. Selecting a run never
  changes operating controls.
- **History:** sessions save automatically. Reopen a session, export it as portable
  JSON, or import an existing JSON session. Restored evidence is always historical.
- **Worker:** run a real health check of the configured Python process. Package
  detection is reported separately from scientific execution, which is not implemented.

**Review staged plan** shows the exact run and validation IDs, fixed horizon, modeled
cost, headroom and current-to-proposed controls. Only the final **Apply simulated
controls** action applies that eligible plan. Changing the selected candidate,
viewing historical evidence, numerical edits, reset, failed validation, mismatched
staging or repeated application cannot bypass the C++ approval gate. Layout-only
moves preserve an otherwise eligible current plan.

## Automatic local storage

The default database is `history.sqlite` under Qt's application-local data location.
`--history-db <path>` overrides it for local verification or a separate history catalog.
SQLite connections, serialization for queued final checkpoints and all database work
belong to one dedicated worker thread. Normal captures also serialize away from the UI.

A transaction stores an entire session checkpoint with indexed summary fields. Records
retain immutable physical snapshots, objectives, actual native runs/full trajectories,
validation reports, tool events, sanitized request metadata and operator approval audit.
Monotonic checkpoint sequences reject delayed older writes. Existing numerical
trajectories and append-only audit records cannot be replaced by later checkpoints.
Unfinished saved sessions reopen as interrupted after restart. No automatic history
deletion is performed, and storage failures never disable the native simulator.

Session import/export uses `archive_version: 1`. Exports are atomic JSON files, bounded
to 512 MiB. Imports validate versions, IDs, physical hashes, horizons, native network
bounds, finite numbers, trajectory dimensions/endpoints, controls and audit structure.
An imported session cannot overwrite different evidence with the same session ID.
Unknown database versions or corrupt records are reported without resetting the data.

Loaded/imported recommendations are marked historical and non-actionable. Original
validation/approval records remain audit evidence; no archived flag grants operating
authority and imported records are never inserted into a live model tool gateway.
The native project-v1 file remains the editable network/layout format.

## Scientific worker

Set `BREAKWATER_PYTHON` in the launching process to your Python 3.12 executable, then
choose **Check health** on the Worker tab. No interpreter or scientific packages are
installed automatically. An unset executable produces an explicit unavailable state.

The asynchronous QProcess adapter sends one fresh UUID-tagged NDJSON-v1 health request
to the existing worker. It checks protocol/request identity and the health schema,
handles fragmented output, and enforces a ten-second timeout, 1 MiB stdout line and
64 KiB stderr bounds. Cancellation and process failures retire the child safely;
a new health check starts a fresh process. The NVIDIA API key is removed from the
worker's inherited environment. Neither a detected package nor a successful health
exchange is presented as executed PyTorch/PhysicsNeMo science.

Development builds locate the existing worker through CMake. Installed builds prefer
`../share/breakwater/science/worker.py` relative to the executable directory. This
matches the application install layout and does not require the source checkout.

## Model metadata and live verification

Model completion events contain only request ID, sanitized configured/response model
identifiers, HTTP status, elapsed milliseconds, retry count, outcome and error code.
Metadata precedes terminal session events, including cancellation/deadline failures.
Credentials, authorization headers, raw response bodies and reasoning are excluded.

The live milestone remains pending until `NVIDIA_API_KEY` is configured locally.
Then run the existing protocol smoke, followed by the production investigation loop
and the UI against the canonical and edited/custom networks. Save the actual session
archives and verify model identity, real tool/result continuation, native validation,
and human-only application. Three clean canonical rehearsals and a custom-network
follow-up are still required; deterministic tests do not count as live execution.

## Verification

Configure the test interpreter explicitly when available:

```powershell
$env:BREAKWATER_PYTHON = 'C:\path\to\python.exe'
.\scripts\build.ps1 -Preset desktop -EmbeddedDebugInfo
.\scripts\Generate-VisualStudio.ps1 -Generator 'Visual Studio 17 2022' -EmbeddedDebugInfo -Build
```

On a machine without Python, the desktop still builds and the worker suite explicitly
reports a skip. With the configured interpreter, it executes the real existing worker
and bounded failure fixtures. CTest writes each Qt suite's detailed result file into
its build directory. STATUS.md records the checks actually executed on this laptop.

For UI verification, `--import-session <file>` opens an archive as historical evidence;
`--agent-panel` opens the investigator and `--worker-health` starts the health check.
Setting `BREAKWATER_TEST_ARTIFACT_DIR` for the comparison test optionally exports
`offline-verified-session.json` to that directory. It contains actual native baseline
and candidate evidence, clearly labeled as offline, with no claim of live NVIDIA execution.
