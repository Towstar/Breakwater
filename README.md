# BREAKWATER

Person B's native Nemotron tools and investigation panel are documented in
[docs/AGENT_INTEGRATION.md](docs/AGENT_INTEGRATION.md), including local `.env`
configuration, actual tool-call smoke commands, snapshot rules and approval staging.

The [G1 telemetry contracts](contracts/telemetry/README.md) define sensor configuration,
stream lifecycle, observed snapshots and incident evidence for the planned synthetic
monitoring extension. They provide offline validation and test fixtures; shared
[Person A/B adoption](docs/G1_REVIEW.md) precedes sensor/runtime development.

Nemotron can inspect stored native runs and request local PyTorch control
sensitivities, with complete scientific evidence retained in History. Native
validation and explicit operator review govern simulated control changes.
[Four actual live rehearsals](docs/DEMO_REHEARSAL.md#executed-live-evidence) record
the canonical and edited-network outcomes, including retained unsuccessful attempts.
Automatic history, run comparison/charts, operator review and worker health are
described in [docs/DESKTOP_FOUNDATION.md](docs/DESKTOP_FOUNDATION.md).
The native pipe viewer, physical inputs, concentration field and mass ledger are
described in [docs/PIPE_TRANSPORT.md](docs/PIPE_TRANSPORT.md).

C++20 / MSVC Windows base for an editable synthetic fluid-network workbench.
The original handoff is preserved in [For Dev Agents.md](For%20Dev%20Agents.md).
The complete accepted roadmap is [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md).
The next increments and parallel ownership are in
[docs/CONTINUATION_PLAN.md](docs/CONTINUATION_PLAN.md).
The accepted next sensor milestone uses **hypothetical sensors and synthetic live
telemetry**, with seeded streaming/replay, observed-state forecasts and evaluated
fault detection. It is planned, not implemented; see
[docs/TELEMETRY_PLAN.md](docs/TELEMETRY_PLAN.md).
The base now includes native search, Nemotron tools, single-pipe transport,
standalone differentiable storage and experimental PhysicsNeMo calculations,
measured benchmarks, and checked Lean algorithm proofs.

For Nemotron access, open **Nemotron investigator → Set API key** and save your key
locally. Installed users need no source checkout or launch script. Developers can
also copy `.env.example` to `.env` and fill it in; `.env` stays out of Git and is
read automatically. Existing process variables take precedence over local files.

## Open in Visual Studio

Use Visual Studio 2026 with the **Desktop development with C++** workload and Windows SDK.
Qt 6.12 MSVC x64 needs Core, Quick/Quick Controls, Quick 3D and Shader Tools.

The current workspace is already configured: open **`Breakwater.slnx`** directly.
On a fresh checkout, generate the native `.vcxproj` files using CMake 4.3 or newer:

```powershell
.\scripts\Generate-VisualStudio.ps1 -QtPrefix 'C:\Qt\6.12.0\msvc2022_64' -Build
```

Then open **`Breakwater.slnx`** at the repository root. Select **x64** and
**RelWithDebInfo**, and set **breakwater** as the startup project. All native projects
use C++20. Qt code generation and source references come from CMake; regenerate after
changing the build structure. Generated `.vcxproj` files live in ignored `out/visual-studio`.
The root `.slnx` retains the generated dependency graph and includes the documentation.

Visual Studio 2022 17.14 with the existing .NET 10 SDK is also supported locally:

```powershell
.\scripts\Generate-VisualStudio.ps1 -Generator 'Visual Studio 17 2022' -EmbeddedDebugInfo -Build
```

That option migrates CMake's generated `.sln` to `.slnx` with `dotnet sln migrate`.
The default generator remains Visual Studio 2026. Regenerate after pulling engine or
desktop target changes; SDKs and generated project files are not delivered by Git.
Visual Studio tracking files use short, project/configuration-specific paths below
the build directory's `.t` folder to support nested checkouts. Tracking stays
enabled; an explicitly supplied `CMAKE_VS_GLOBALS` `TLogLocation` takes precedence.

If using the project-local SDK downloaded during development, omit `-QtPrefix`.
The script detects `.tools/Qt/6.12.0/msvc2022_64` automatically.

For Person A's engine alone, Qt is unnecessary:

```powershell
.\scripts\Generate-VisualStudio.ps1 -Headless -Build
```

This regenerates the root solution with just the engine projects. Run the script
without `-Headless` to restore desktop projects.

## Command-line build

```powershell
.\scripts\build.ps1 -Preset headless
.\out\headless\engine\breakwater_cli.exe --case baseline
.\out\headless\engine\breakwater_cli.exe --case bad
.\out\headless\engine\breakwater_cli.exe --case mixed
.\scripts\build.ps1 -Preset asan
```

`scripts/build.ps1` initializes the MSVC developer environment. Its `desktop` preset
also accepts `-QtPrefix`. In restricted agent environments MSVC's PDB helper may need
an unrestricted build process; this is separate from ordinary Visual Studio usage.
If the local PDB helper reports compiler error C1902, use `-EmbeddedDebugInfo` to
select MSVC `/Z7` through CMake, including checked builds. This avoids the PDB helper.

## Workbench controls

- Drag equipment from the palette into the scene, or click to add at the origin.
- Drag existing equipment to move it on a 20-unit ground grid.
- Right-drag to orbit, middle-drag to pan, and scroll to zoom.
- Choose **Connect equipment**, then source and destination equipment.
- Drop a pump onto a tank-origin route, or select a route and **Attach pump**.
- Select equipment/routes to edit numerical parameters. New source assets need a route.
- Save/Open use local JSON files. Undo/Redo cover topology, parameters and object placement.
- Run a forecast, then scrub its three-hour trajectory. Numeric volumes are never clipped.
- **Pump failure**, **Bad reroute**, and **Mixed plan** load real synthetic configurations;
  press **Run forecast** to compute them. They are not model-generated recommendations.
- Approval records approval of the current simulated preview only.

The native engine uses fixed flow setpoints and volumes. Placement and pipe geometry
do not imply pressure/head calculations. Quality checks are not part of base feasibility.

## Science and proofs

Worker health and capability negotiation require only the Python 3.12 standard library:

```powershell
python science/worker.py
python -m unittest discover -s science/tests -p test_worker.py -v
```

Send a line such as:

```json
{"protocol_version":1,"request_id":"check","method":"health","payload":{}}
```

The repo worker now implements `capabilities` and `storage_rollout`, including
selected-control derivatives, through the agreed science contract. See
[science/WORKER.md](science/WORKER.md) for requests, limits and executed checks.
The asynchronous desktop adapter deploys the mapper/package and exposes forward
storage comparison and explicitly selected captured-control sensitivities, with
process-local capability verification and clean-exit checks.
Standalone calculations also include [PyTorch rollout and gradient checks](science/README.md),
[experimental PhysicsNeMo smoke](science/PHYSICSNEMO.md),
[native benchmarks](engine/BENCHMARKS.md), and [Lean proofs](proofs/README.md).
Install the documented pinned science environment before running the full Python
test suite; it includes Torch and optional PhysicsNeMo tests.

Native pipe transport is connected to the desktop, alongside investigation history,
comparison and guarded operator review. Its bounded JSON exports can be reopened as
read-only historical evidence; see [the pipe viewer guide](docs/PIPE_TRANSPORT.md). See
[docs/SCIENCE_VALIDATION.md](docs/SCIENCE_VALIDATION.md) for proof scope and numerical
limits, and [docs/STATUS.md](docs/STATUS.md) for executed evidence.

The [desktop Science workflow](docs/DESKTOP_SCIENCE.md) now deploys a pinned local
CPU Python/Torch runtime and compares an actual retained native forecast with the
real worker. Run `scripts/Setup-ScientificWorker.ps1`, then `scripts/Start-Desktop.ps1`.
Science comparison and derivatives are advisory; native feasibility and operator approval stay separate.

## Optional project-local Qt setup

`scripts/install-qt.py` fetches pinned Qt 6.12.0 MSVC archives, verifies their published
checksums, and extracts them into ignored `.tools`. It requires `requests==2.32.5`
and `py7zr==1.0.0`. Qt's installer is also supported. This does not change system Qt.

## Two-person ownership

Person A is the project owner/user and PDE/simulation developer. Person A owns
`engine/`, `science/`, `proofs/`, and `scenarios/`. Person B owns
`desktop/`, `contracts/`, root setup/scripts, and integration documentation. Before
concurrent implementation, make a common baseline commit and use separate worktrees.
Do not switch branches underneath the other developer's session.

The shared baseline is now committed. Continue with the [parallel workflow](docs/PARALLEL_WORK.md)
and separate copy/paste kickoffs for [Person A](docs/KICKOFF_PERSON_A.md) and
[Person B](docs/KICKOFF_PERSON_B.md). A3–A10b, deployed storage comparison,
selected-control sensitivities and immutable scientific history are delivered.
Next: current integration/release gates, front accuracy, and the agreed sensor/
synthetic-telemetry contract followed by monitoring and detection. Both workstreams
adopt the same committed contract before dependent implementation.
