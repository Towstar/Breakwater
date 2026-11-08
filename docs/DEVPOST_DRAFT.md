# Breakwater — Devpost draft

Editorial draft only; no Devpost fields have been changed or submitted by this document.

## Readiness before pasting

Delivered baseline: PR #44 UI/EOG workflow on main `7418`. PR #48 has **one completed installed hosted search rehearsal**: attempt 10, compiled runtime `a23fb70`, QML-only checkout `e8210a4`, 221.98 seconds. It includes actual search, separate independent validation and a grounded final; final package gates remain separate. Earlier worker-0.2.1 hosted archives demonstrate genuine tools and independent validation, with search unavailable.

**Verified example:** the retained synthetic no-action cost was **$12,120.03**; the selected plan used one truck trip and three well shut-ins at **$4,171.97**, with an independently validated modeled outcome `avoids_shutdown`. The search stopped at 100 candidates, `best_found`, incomplete, with 26 numerically unavailable candidates; this is one recorded result, not a global optimum or physical accuracy claim.

Final installed `28d873c` includes the six-icon equipment grid and visible Network scrollbar. Actual installed normal/compact runs each passed all nine Qt gates, and untouched drawer-open renders show the left rail without clipping. Earlier unchanged-grid `a5bbbd5` also passed a separate out-only native probe for mouse-hover and Tab-focus labels, overflow-wheel scrolling and selection without numerical changes. This is native Qt-event coverage, not a manual OS-input/accessibility audit. Local evidence is in `out/search-installed-ui-ready28/` and `out/grid-interaction-check/`. The separate hosted joint-search rehearsal **10 passed**; that claim follows from its retained model/numerical evidence, not the UI checks.

Final panel polish at `28d873c` passed an out-only fresh-object Qt interaction probe: 16 checks each in normal/compact sizes, including real sidebar repeat-click toggles, exclusive navigation, popup close buttons, inset titles, main-toolbar clearance and unchanged network controls. The sidebar remains usable while popups are open. Evidence: `out/panel-interaction-check/attempt2-results.json`. The final installed package is at the release path below.

The local EOG launcher is `C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-eog-validation-review\out\Breakwater-Demo-Ready\Start-EOG.cmd`; the adjacent `Start-Sandbox.cmd` opens the synthetic workbench. These local paths belong in the recording/runbook, not the public Devpost description. Do not describe extended stateful EOG operation, implemented PhysicsNeMo modeling, global optimization, physical accuracy or superiority over other LLMs.

## Project overview

**Title:** Breakwater

**Elevator pitch:** A native 3D operator console that turns recorded water-network data into evidence-based alerts, inspectable response plans and human-reviewed numerical evidence.

**Public code:** https://github.com/Towstar/Breakwater

**Demo video:** **[PUBLIC_VIDEO_URL — REQUIRED BEFORE FINAL SUBMISSION]**

**Technologies:** C++20, Qt Quick/QML, Qt Quick 3D, SQLite, Python, PyTorch, NVIDIA hosted API, NVIDIA Nemotron 3.5 Lightning 30B-A3B. PyTorch supports the separate scientific comparison/sensitivity workflow; the EOG response calculations use the native numerical provider.

## Paste-ready project description

### Inspiration

A failed water-handling pump can turn a manageable incident into an expensive production shutdown. An operator needs more than a warning: which equipment is affected, how long remains, what a response costs, and what evidence supports it. We built Breakwater to make those decisions inspectable.

### What it does

Breakwater has two connected but distinct workspaces. The EOG console imports the challenge's three files—`network.json`, `ops.json` and `telemetry.csv`—and builds a dataset-driven 3D network with selectable equipment and measurement trends. Its analysis uses a fixed causal cutoff and preserves recorded readings. It detects incident evidence, estimates shutdown exposure, evaluates candidate responses and displays trajectories, costs, constraints and limitations.

The modular simulation workbench lets users create and edit synthetic networks of sources, tanks, routes, pumps, treatment and trucking capacity. Native forecasts show the consequences of accepted numerical edits. The workbench remains separate from the recorded EOG data.

### How we use Nemotron

NVIDIA Nemotron 3.5 Lightning coordinates an on-demand investigation through bounded tools. It navigates the imported network structure, requests relevant observations and chooses numerical detector, evaluation and validation calls. Its returned plan and event references must match retained evidence.

Native calculations supply costs and constraints. A separately implemented validator recomputes the exact plan before advisory review becomes available. The desktop retains tool order, arguments, numerical results and sanitized request metadata in local history. Imported or reopened evidence is clearly historical and cannot restore current approval. EOG review records advice viewed; it never dispatches trucks or controls wells.

### How we built it

The application is a native C++/Qt Quick desktop with a schematic 3D view, strict shared contracts, an asynchronous Python process boundary and SQLite evidence retention. Lucas focused on the desktop, model orchestration, contracts, history and integration. Ethan focused on the numerical models, scientific methods and independently computed validation.

We connected the actual hosted model to executable tools rather than treating model prose as a numerical result. Our earlier live rehearsals retained genuine tool calls and independently validated recommendations, including a renamed network with a facility branch removed. Those were derived practice inputs, not independent held-out accuracy evidence.

### Challenges we ran into

Correctness spans several boundaries: historical cutoffs must exclude future readings, noisy or missing evidence must remain explicit, plan IDs and hashes must stay bound to one capture, and shared truck schedules must respect inventory and repair deadlines. We also had to make model output inspectable and reject malformed or ungrounded final responses without granting authority.

### What we are proud of

The desktop brings the recorded network, original measurements, model-selected investigation steps and numerical evidence into one workflow. It preserves the editable sandbox while making stale state, rejected edits and historical evidence visible. Human review remains explicit.

### What we learned and what is next

We learned that useful agentic software needs precise evidence boundaries and clear failure states. Next we want broader unseen-network evaluation, better incident timing/uncertainty assessment and a separately qualified stateful simulation workflow. Current models are simplified; the project does not claim universal physical accuracy or global optimality.

## Search paragraph — include in the verified search release

In one completed installed hosted investigation, Nemotron requested bounded joint search and then separate exact validation. The retained modeled cost changed from $12,120.03 for no action to $4,171.97 for a mixed one-trip/three-well plan. This is one recorded synthetic example. The search compares retained no-action, truck-only, well-only and mixed candidates under declared limits. We display its scope and `best_found` status. This is not a guarantee of the globally cheapest continuous plan.

## Final submission preflight

- Team: **Lucas and Ethan**. Both are at **the University of Texas at Dallas**, and both confirmed this is their **first hackathon**. Check both appear on the same Devpost project; do not create duplicate submissions.
- Beginner/university answers should reflect those confirmed facts. Additional track-specific eligibility remains subject to the actual organizer form; none is invented here.
- Sponsor/company tracks: **[EXACT_SELECTED_TRACKS — CONFIRM]**, at most **three** under the supplied guide. Sponsor choices are still unspecified in this draft.
- Verify the public repository link opens without authentication.
- Replace the public video placeholder; aim for about two minutes and check playback without signing in. Use screenshots from the actual final application.
- Verify the recorded metric/cutoff against the final evidence and remove internal readiness notes. Keep final package verification separate from the hosted result.
- Check all editable project fields and complete the final **Submit** flow, not just Save draft, before **noon Central**.

Existing project: https://devpost.com/software/breakwater-5bpyd8
