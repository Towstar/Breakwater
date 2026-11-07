# BREAKWATER

Person B's native Nemotron tools and investigation panel are documented in
[docs/AGENT_INTEGRATION.md](docs/AGENT_INTEGRATION.md), including environment
configuration, actual tool-call smoke commands, snapshot rules and approval staging.
Automatic history, run comparison/charts, operator review and worker health are
described in [docs/DESKTOP_FOUNDATION.md](docs/DESKTOP_FOUNDATION.md).

C++20 / MSVC Windows base for an editable synthetic fluid-network workbench.
The original handoff is preserved in [For Dev Agents.md](For%20Dev%20Agents.md).
The complete accepted roadmap is [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md).
This implementation is the requested **minimum base for Person A and Person B**.

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

## Scientific worker base

Python 3.12, standard library only:

```powershell
python science/worker.py
python -m unittest discover -s science/tests -v
```

Send a line such as:

```json
{"protocol_version":1,"request_id":"check","method":"health","payload":{}}
```

Only protocol/health behavior exists at this milestone. Torch, PhysicsNeMo, the native
process adapter, Nemotron orchestration, transport, search, and Lean proofs remain
explicitly planned. See [docs/STATUS.md](docs/STATUS.md) for actual verification evidence.

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
[Person B](docs/KICKOFF_PERSON_B.md). Person B can start the app/Nemotron work against
the existing engine immediately.
