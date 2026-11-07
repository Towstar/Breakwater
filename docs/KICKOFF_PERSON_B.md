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
docs/CONTINUATION_PLAN.md
docs/IMPLEMENTATION_PLAN.md
For Dev Agents.md, Part A
README.md and contracts/README.md
desktop/ProjectController.hpp/.cpp and ProjectDocument.hpp/.cpp
engine/include/breakwater/engine.hpp and contracts/include/breakwater/types.hpp
engine/TRANSPORT.md and engine/include/breakwater/transport.hpp

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
Native Nemotron orchestration, real tools, bounded search, immutable agent runs and
approval staging are integrated, including review fixes. Hosted model calling is
verified; clean operator-review rehearsals remain incomplete. Native single-pipe
transport and its selected-pipe desktop viewer are implemented. Scientific worker
methods beyond health remain pending.

Follow-up: the approved desktop-first increment implements investigation history,
comparisons/charts, operator review and a health-only QProcess adapter; see
docs/DESKTOP_FOUNDATION.md and the latest docs/STATUS.md. Scientific worker methods and
generalized learned pipe inference remain pending. B3 contracts/persistence and B4
native viewing are now implemented; follow the updated next increment below.

Inspect the machine rather than assuming the original developer's SDKs are installed.
.tools/ and out/ are ignored and will be absent in a fresh clone. Follow README for
Qt setup; scripts/install-qt.py provides a pinned project-local SDK option. Do not
commit SDKs or generated .vcxproj files. With the SDK available, run:
  .\scripts\Generate-VisualStudio.ps1 -QtPrefix 'C:\Qt\6.12.0\msvc2022_64' -Build
Omit -QtPrefix when using the documented project-local SDK. Open Breakwater.slnx, use
x64 / RelWithDebInfo and startup project breakwater. You own any committed regeneration
of the root solution. If Qt setup is blocked, continue B3 contract/schema examples and
adapter protocol test design, and record the concrete setup failure.

NEXT INCREMENT: B5 SCIENTIFIC WORKER INTEGRATION
Review docs/SCIENCE_CONTRACTS.md and contracts/science/ with A. The proposed method
examples/schemas and native route-parameter persistence are committed; scientific wire
methods remain reserved. Coordinate units, provenance and ordering before A implements
the worker mapping. The current desktop process adapter implements health only.

Physical pipe length/area/dispersion/decay are independent of layout. Flow comes from
the captured numerical run, including availability. Worker JSON lines are limited to
1 MiB but native fields may contain four million values; define bounded display output
and any size/hash-checked application-owned artifact references. Keep health v1 working.

PRESERVE B4 AND EXTEND B5
The selected-pipe native viewer is implemented; read docs/PIPE_TRANSPORT.md. Keep its
native parity, physical/layout separation, explicit provenance and stale-result guards.
Do not replace it with A4's fixed-case PhysicsNeMo model or claim arbitrary-route inference.

Extend the existing QProcess adapter only for actually implemented scientific methods:
capability negotiation, exact matching identity, bounded output and separate diagnostics.
Keep framing/IDs/timeouts/cancellation/restart guards. Test fake workers, then A's actual
methods when available. A owns science/ implementations and dependency pins; do not edit them.

Retain the integrated Nemotron limits: 12 tool calls, three refinement rounds and two
minutes. Keep credentials in process configuration, out of QML, project files and logs.
With credentials, run the existing genuine hosted smoke and record actual tool/run IDs.
Do not ask the user to paste a key into chat. Missing access does not block native work.

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

Also cover selected-pipe parity, maximum output, wrong request IDs, missing Python,
worker crashes/restarts and quality-result invalidation. Keep quality separate from
volume approval. Update docs/STATUS.md with actual evidence. Follow B7 for durable
evidence export/reopen after the native viewer and worker boundary; defer UI polish.
Push your feature branch and open a PR. You are the integration owner: review A's small
increments, integrate agreed changes through PRs after relevant tests pass, and keep
main runnable. Do not force-push or put feature work directly onto main.

Report commit SHA, touched files, contract changes, actual tests/results, limitations
and an executable example after each increment. Start by inspecting and running the
existing baseline, then implement the first increment instead of returning only a plan.
```
