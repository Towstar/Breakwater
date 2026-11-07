# Person B kickoff: desktop, Nemotron and integration

Person A is the project owner and PDE/simulation developer. Person B can begin immediately
using the numerical engine already on `main`; A's next scientific work is not a prerequisite.

Copy the following prompt into Person B's coding session.

```text
You are implementing Person B's workstream for BREAKWATER: the native desktop app,
Nemotron planning, public contracts and integration. Person A owns PDEs/simulation/science.
Continue the existing application; do not rebuild it as a web app.

REPOSITORY AND BRANCH
https://github.com/Towstar/Breakwater
Use a separate clone/worktree and branch codex/person-b-desktop from current origin/main.
For a fresh directory:
  git clone https://github.com/Towstar/Breakwater.git Breakwater-B
  cd Breakwater-B
  git switch -c codex/person-b-desktop
If a checkout already exists, inspect status and preserve its work before changing branches.
If you cannot push to the upstream repo, use a fork and open a PR to Towstar/Breakwater:main.

READ FIRST
AGENTS.md
docs/PARALLEL_WORK.md
docs/STATUS.md
docs/IMPLEMENTATION_PLAN.md
For Dev Agents.md, Part A
README.md and contracts/README.md
desktop/ProjectController.hpp/.cpp and ProjectDocument.hpp/.cpp
engine/include/breakwater/engine.hpp and contracts/include/breakwater/types.hpp

OWNERSHIP
You own desktop/, contracts/, root build/configuration, scripts/, Breakwater.slnx,
README, AGENTS and shared docs/. You do not edit engine/, science/, proofs/ or scenarios/.
Each person owns the CMakeLists.txt inside their own subsystem. Request engine changes
from A instead of patching A's code. Preserve existing shared contracts for the first
increment; publish necessary additive contract changes separately with examples.

BASELINE AND SETUP
The app is C++20/MSVC x64, Qt Quick/QML and Qt Quick 3D 6.12, using Visual Studio 2026.
The existing editor already supports primitive equipment, connections, parameters,
save/load, undo/redo, background forecasts, timeline scrubbing and approval invalidation.
It is not an empty scaffold. Nemotron is not implemented yet.

Inspect the machine rather than assuming the original developer's SDKs are installed.
.tools/ and out/ are ignored and will be absent in a fresh clone. Follow README for
Qt setup; scripts/install-qt.py provides a pinned project-local SDK option. Do not
commit SDKs or generated .vcxproj files. With the SDK available, run:
  .\scripts\Generate-VisualStudio.ps1 -QtPrefix 'C:\Qt\6.12.0\msvc2022_64' -Build
Omit -QtPrefix when using the documented project-local SDK. Open Breakwater.slnx, use
x64 / RelWithDebInfo and startup project breakwater. You own any committed regeneration
of the root solution. If Qt setup is blocked, continue native tool/run-store work using
the headless engine and record the concrete setup failure.

FIRST INCREMENT: REAL RUN STORE AND TOOLS
Replace the single displayed-run limitation with a bounded immutable run store, candidate
comparison and approval staging. Preserve snapshot/revision identity and provenance.
Implement real tools against the EXISTING engine functions: inspect_network,
control_catalog, compile, simulate and validate. Start with:
  get_network_state
  get_control_catalog
  forecast_baseline
  simulate_candidates
  validate_run
  compare_runs
  request_approval
Every simulation must store actual computed results and return real run IDs. Existing
controls remain unchanged unless explicitly overridden. The planning request fixes
the horizon. Validation must use the same captured network, candidate and horizon.

Do not wait for A's search, transport, PyTorch, PhysicsNeMo or Lean work. Add a search
tool only after A's real engine implementation is integrated; never substitute a passing stub.

SECOND INCREMENT: NEMOTRON LOOP
Implement an asynchronous native HTTPS client with Qt Network using configurable
NVIDIA_API_KEY, NVIDIA_BASE_URL and model ID. Verify the current vendor API before
coding its request/response parsing. Keep credentials in process configuration, out
of QML, project files and logs. Do not ask the user to paste an API key into chat.

Nemotron should do most planning: inspect structured topology/constraints, choose
controls, propose candidates, call real simulation/validation tools, compare results
and revise failures. Use actual asset/control IDs so it works on user-created networks.
Cap sessions at 12 tool calls, three refinement rounds and two minutes. Preserve
completed runs on failure, and keep the UI responsive.

Do not let model text create a passing verdict, invent a run, shorten the horizon,
relax a bound, modify topology or approve a plan. Recommendations must cite stored,
validated evidence. Only an explicit operator action applies simulated controls.
Reject approval when numerical edits make the captured snapshot stale. Layout-only
edits preserve valid results and approvals.

TESTS AND DELIVERY
Use a fake MODEL CLIENT for most tests while executing real numerical tools. Cover
malformed/unknown tools, invented/stale run IDs, invalid controls, skipped validation,
timeouts, failed candidates, approval bypasses and renamed/custom networks. Test reset,
editing and concurrent jobs for stale-result leakage. Run the desktop project/render
tests. With credentials, separately run and record a genuine model-issued tool call;
otherwise report live integration unverified and continue the deterministic work.

Update docs/STATUS.md with actual evidence. Keep synthetic data and unavailable science
features clearly labeled. Deliver the tool/run-store increment before expanding UI polish.
Push your feature branch and open a PR. You are the integration owner: review A's small
increments, integrate agreed changes through PRs after relevant tests pass, and keep
main runnable. Do not force-push or put feature work directly onto main.

Report commit SHA, touched files, contract changes, actual tests/results, limitations
and an executable example after each increment. Start by inspecting and running the
existing baseline, then implement the first increment instead of returning only a plan.
```
