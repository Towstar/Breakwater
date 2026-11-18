# BREAKWATER

Breakwater is a Windows desktop workbench for investigating synthetic fluid
networks and recorded operating data. It combines an editable industrial 3D
workspace, native numerical forecasts, bounded response search, independent
validation, and persistent evidence. A human reviews an exact validated plan
before any simulated controls change.

The v0.2.0 portfolio candidate leads with **Sandbox**, the editable network
workbench. **Operations** presents recorded observations and advisory analysis;
EOG remains a supported dataset adapter. Neither workspace controls a plant.

![Sandbox with a real baseline/candidate comparison](docs/images/portfolio/sandbox-comparison.jpg)

![Operations displaying the self-authored recorded dataset](docs/images/portfolio/operations-overview.jpg)

These are actual candidate screenshots. See the [capture notes and gallery](docs/images/portfolio/README.md).

| Area | Available behavior | Boundary |
|---|---|---|
| Sandbox | Equipment/pipe editing, group transforms, undo/redo, native trajectories | Fixed flow/volume model; layout is schematic |
| Local investigation | Actual bounded native search and independent validation | No API key; best found on the declared finite grid |
| Nemotron | Real hosted tool calling, evidence-bound recommendations, retained request metadata | Optional; model errors and ungrounded finals are rejected |
| Operations | Read-only import, original-unit plots, detection, response search and exact validation | Synthetic/recorded advisory analysis, no dispatch |
| History | SQLite persistence, filtered browsing, versioned import/export, compatible comparison | Imported/replayed evidence never restores approval |
| Optional Science | Torch storage comparison/sensitivity and bounded PhysicsNeMo tracer fitting | Separate pinned CPU installation, explicit parameter application |
| Proofs | Standalone Lean/Mathlib checks of specified algorithms and assumptions | Not a proof of the desktop, a field model or every recommendation |

See [the offline walkthrough](docs/PORTFOLIO_WALKTHROUGH.md),
[architecture and ownership](docs/ARCHITECTURE.md), and
[the release ledger](docs/PORTFOLIO_RELEASE.md). Actual results and remaining gates
are recorded in [STATUS](docs/STATUS.md); historical hackathon reports retain
their original source/runtime scope.

## Build and run on Windows

Use Visual Studio 2022 with Desktop C++, CMake/Ninja, PowerShell 7 and the pinned
Qt 6.12.0 MSVC 2022 x64 SDK. Qt Quick, Quick Controls, Quick 3D, SVG, SQL, Network
and Qt Test are used. The repository's installer records the exact Qt archives.

```powershell
# Project-local setup. Does not change registry/PATH permanently.
python -m pip install --require-hashes -r scripts/ci/requirements-win-py312.txt
python scripts/install-qt.py
./scripts/Setup-ScientificWorker.ps1

# In a VS developer PowerShell, or after the compiler bootstrap:
./scripts/ci/Initialize-Compiler.ps1
cmake --preset portfolio -DCMAKE_PREFIX_PATH="C:/PATH/TO/Qt/6.12.0/msvc2022_64" -DPython3_EXECUTABLE="C:/PATH/TO/Breakwater/.tools/science-cpu/Scripts/python.exe" -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded
cmake --build --preset portfolio --parallel 3
ctest --preset portfolio
./scripts/Start-Portfolio.ps1 -QtPrefix "C:/PATH/TO/Qt/6.12.0/msvc2022_64"
```

Use absolute paths for external SDKs/interpreters. `BREAKWATER_PYTHON` explicitly
overrides the worker interpreter. An incompatible explicit override fails visibly;
it is not silently replaced. The isolated portable runtime is used automatically
only when no override is supplied. Missing optional science never blocks native
forecasts or recorded 3D viewing.

For Visual Studio, use Open Folder and the existing checked Windows build script
with the pinned Qt prefix. [Windows check instructions](docs/WINDOWS_CHECKS.md)
cover normal/AddressSanitizer configurations and retained job artifacts. No hosted
Nemotron request runs in credential-free CI. The manual Optional CPU Science
workflow installs and qualifies the real pinned PhysicsNeMo environment separately.

## Try the offline examples

Sandbox starts with a healthy synthetic network. Project contains healthy,
failed-pump, bad-reroute and mixed-response examples. Run a forecast, select a
tank, and inspect the full trajectory including negative/overflow values. Choose
Investigate and Local search for real numerical investigation without a model.

Operations → Examples loads the self-authored datasets under
[examples/portfolio](examples/portfolio/README.md). Importing a dataset never
starts an investigation. Select a cutoff, run Search plans and Validate exact
plan explicitly. History can export and reopen the complete captured evidence.
A bundled recorded Nemotron session is clearly historical, and makes no new
hosted request. The renamed/topology-changed fixture requires fresh analysis.

The source verification helper exercises import → actual search → independent
validation → SQLite completion → archive export → historical reopening:

```powershell
out/portfolio-desktop/desktop/breakwater_offline_check.exe "C:/ABSOLUTE/Breakwater/examples/portfolio/operations" "C:/ABSOLUTE/EXISTING/OUTPUT"
```

## Interaction

- Click or drag equipment into the scene. Ctrl-click toggles selection; Shift adds
  assets or extends a navigator range; a background marquee selects a group.
- Group moves snap a common delta to a 20-unit grid and commit as one undo action.
  Ctrl+D duplicates selected assets and internal routes with new identities.
- Edit contains align/distribute/delete actions. Incident routes are removed with
  deleted equipment; attached sensor guards prevent silently deleting evidence.
- Home, Fit, Focus and Top change the view. Right-drag orbits, middle-drag pans,
  and the wheel zooms. Camera/pad/marker placement supplies no hydraulic geometry.
- Forecast, Compare, Investigate, History and Science share a resizable results
  dock. Drag its top edge upward to give charts and evidence more room. Repeated
  utility-button clicks close the corresponding tool.
- View contains panel visibility, low/medium/high quality, label and density
  preferences. These are local/versioned and separate from project files.

## Optional Science and hosted Nemotron

```powershell
./scripts/Setup-OptionalScience.ps1
./scripts/Start-Portfolio.ps1 -Science -QtPrefix "C:/PATH/TO/Qt/6.12.0/msvc2022_64"
```

The pinned optional CPU environment uses Python 3.12.13, Torch 2.15.0+cpu and
PhysicsNeMo 2.2.2. It fits a bounded scalar tracer-decay parameter to entered
synthetic observations. The fitted value stays staged until explicit application;
applying it retires stale numerical approval. This does not establish field
chemistry, pressure hydraulics or calibrated predictive accuracy.

Use API key settings in the app or a local `NVIDIA_API_KEY`. Never commit keys or
paste them into issues/screenshots. The current hosted model is
`nvidia/nemotron-3.5-lightning-30b-a3b`. It selects tools and interprets evidence;
the numerical engine computes outcomes and an independent validator checks exact
plans. No comparative model benchmark establishes superiority over other LLMs.
See [agent integration](docs/AGENT_INTEGRATION.md) and
[the scientific worker contract](science/WORKER.md).

## Local portable candidate

```powershell
./scripts/Package-Portfolio.ps1 -Destination "C:/ABSOLUTE/NEW/CANDIDATE"
./scripts/Verify-PortfolioBundle.ps1 -Bundle "C:/ABSOLUTE/NEW/CANDIDATE/Breakwater" -EvidenceDirectory "C:/ABSOLUTE/NEW/VERIFICATION"
```

Packaging requires committed source by default and refuses to overwrite a bundle.
It stages Qt, both native providers, an isolated minimal Python runtime, local
schemas/examples, an exact source snapshot and file hashes. Torch/PhysicsNeMo
stay outside the basic bundle. Verification clears development PATH/worker
variables and runs real local tools. It is a sanitized local process check;
a clean Windows account or VM remains a distinct release gate.

Public binary distribution is pending both owners' GPLv3 agreement, completed
notices/source delivery and clean Windows verification. See
[LICENSING](docs/LICENSING.md). No GitHub release is published by these scripts.

## Ownership and further work

Lucas owns desktop/UI, 3D interaction, asynchronous adapters, shared contracts,
Nemotron integration, persistence, packaging and integration verification. Ethan
owns the numerical engine, scientific methods, sensor-generation mathematics,
response evaluator/validator/search and Lean proofs. Shared numerical changes
require Ethan's review.

Generic sensor observations through detection and observed-state forecasting,
evolving commitment-aware fleet dispatch, broader numerical accuracy studies and
remaining research methods are subsequent increments. Existing project-v1/v2
formats and evidence contracts remain compatible. See [G1](contracts/telemetry/README.md),
[G2](docs/TELEMETRY_DESKTOP.md), [pipe transport](docs/PIPE_TRANSPORT.md),
[science](science/README.md), and [proofs](proofs/README.md).
