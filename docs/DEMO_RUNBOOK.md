# Breakwater — two-minute demo

Recording target: about 2 minutes. Software freeze: **11:30 AM Central, November 8**. Devpost/video target: **before noon Central**. This runbook does not edit or submit Devpost.

## Choose the verified release first

- Delivered baseline: PR #44 UI/EOG workflow on main `7418`; recorded/advisory console and separate editable synthetic workbench.
- PR #48 now has **one completed installed hosted search rehearsal**, attempt 10: model-issued joint search, matching continuation, separate exact independent validation and grounded final all passed. Runtime `a23fb70`, with QML-only checkout `e8210a4`; elapsed 221.98 seconds. Final release/package tests remain a separate release gate.
- Earlier archived rehearsals passed three canonical sessions and one renamed, structurally changed network using worker 0.2.1. Those are valid dated evidence; search was unavailable in that runtime.
- Final installed `28d873c` includes the six-icon equipment grid, visible Network scrollbar and polished tool panels. Actual installed normal/compact checks each passed all nine existing Qt interaction gates; untouched drawer-open PNGs show the grid and left rail without clipping. Earlier unchanged-grid `a5bbbd5` also passed a separate native Qt mouse-hover tooltip, Tab-focus tooltip, overflow-wheel scrolling and selection-without-control-mutation probe. This is native Qt-event evidence, not a manual OS-input/accessibility audit.

Release owner fills before recording:

| Item | Exact final value |
| --- | --- |
| EOG launcher | `C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-eog-validation-review\out\Breakwater-Demo-Ready\Start-EOG.cmd` |
| Sandbox launcher | `C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-eog-validation-review\out\Breakwater-Demo-Ready\Start-Sandbox.cmd` |
| Local build revision | `28d873c`; final polished release, installed checks reported separately |
| Executable SHA256 | `25b4f4bc73f183196af5c5d3a85edfe3dfa19a6b0242b8d8d7bfb8966ee4da68` |
| EOG dataset folder | `C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\EogChallenge\history\mock_test_2026-10-10T1530` |
| Current capture/cutoff or archived session | Archived session `427b4639-5bef-43d3-a642-ceb9b418f859`, cutoff `1791460800` (`2026-10-08T12:00:00Z`); disclose this when using history |
| Hosted search/final gate | **PASS — joint-search rehearsal 10**, 221.98 seconds; one pass, not a reliability study |
| One strong verified result | Modeled no-action cost **$12,120.03 → $4,171.97** for the selected mixed plan (one truck trip + three well shut-ins), independently validated; bounded 100-candidate search, `best_found`, incomplete, 26 numerically unavailable candidates |

Launch the configured local EOG build from PowerShell:

```powershell
$eogLauncher = 'C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-eog-validation-review\out\Breakwater-Demo-Ready\Start-EOG.cmd'
$eogFolder = 'C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\EogChallenge\history\mock_test_2026-10-10T1530'
if (-not (Test-Path -LiteralPath $eogLauncher -PathType Leaf)) { throw 'Final launcher missing.' }
foreach ($name in @('network.json', 'ops.json', 'telemetry.csv')) {
    if (-not (Test-Path -LiteralPath (Join-Path $eogFolder $name) -PathType Leaf)) { throw "Missing $name" }
}
& $eogLauncher
```

The launcher supplies the existing pinned Python, actual ABI 1.1 native provider and D3D11/WARP configuration; it contains no key. Use locally saved model credentials. Never paste a key into the recording, terminal output, project files or Devpost. Opening data should not trigger a hosted investigation. The sandbox can be reached from the header or its separate launcher; do not close unrelated unsaved app windows.

Final installed evidence: `out/search-installed-ui-ready28/installed-ui-results.json` and `drawer-render-results.json` in the search worktree; untouched `drawer-open-normal.png` / `drawer-open-compact.png`. Both modes passed 9/9 gates, and both render processes exited 0 with empty stderr. This is UI/package evidence. The distinct successful hosted evidence is in `docs/evidence/eog-search-integration/live/`.

Focused grid evidence: `out/grid-interaction-check/attempt3-results.json`, `compile-provenance.json` and untouched tooltip/overflow-selection PNGs in its normal/compact attempt folders. The fixture deliberately added tanks to make 24 rows; scrolling moved `contentY` from 0 to 216. Selection preserved numerical identity. Selection currently resets the list to its top; do not imply scroll position is retained.

Final panel source `28d873c` has compact Sensor/Telemetry layouts, inset titles, top-right close buttons and checked sidebar toggles. An out-only native Qt probe linked the actual fresh Main/QML/controller objects and passed 16 checks in each size: all four real sidebar buttons open and close on repeat click, popup X buttons close, navigation stays exclusive and network controls remain unchanged. Compact popups start at x=220/y=76, leaving the 208-pixel sidebar and main toolbar accessible; Sensor/Telemetry titles have a verified 14-pixel inset. Evidence and untouched panel renders: `out/panel-interaction-check/attempt2-results.json`, `compile-provenance-attempt2.json` and its normal/compact attempt2 folders. This object-linked probe supplements the separate final installed checks above.

## Before pressing Record

Use one rehearsed dataset and a known cutoff. Confirm its labels/trend render, the local provider works, and the exact release opens both workspaces. Keep the archive used for fallback available. Hide unrelated windows and secret-bearing tabs. Do not show an answer key or pass it to the application.

Hosted investigations may take substantially longer than a short clip. Either finish one before recording and show its actual capture/time, or show it running honestly. Do not present an archive as a fresh live response or edit waiting out while implying instantaneous execution.

## Time-coded screen and narration

| Time | Screen action | Say |
| --- | --- | --- |
| 0:00–0:12 | EOG console: actual imported systems/facilities | “Breakwater helps an operator understand water-handling failures and compare responses. This console is built from the supplied network, operating rules and recorded sensors.” |
| 0:12–0:30 | Select a facility/equipment/channel; show a measured trend and visible cutoff | “These are original recorded measurements. We preserve units, unusual readings and gaps. Analysis is restricted to this capture and cutoff.” |
| 0:30–0:48 | **Investigate / history → Detect events**; show a rehearsed alert/countdown | “Numerical detection distinguishes failure, wear and leak evidence. Pump failures get a shutdown forecast; inspection events remain separate.” |
| 0:48–1:12 | **Trace and explanation** of an actual completed investigation, or its real pending state | “Nemotron chooses which network and observation tools to read, then requests numerical experiments. The trace preserves its actual calls and returned evidence.” If pending: “This hosted investigation is still running; the numerical evidence already returned remains visible.” |
| 1:12–1:34 | **Plans**: baseline/candidate trajectory, cost, coverage and validation; open **Review validated advice** only if enabled | “Native calculations provide costs and constraints. A separate implementation validates the exact plan. Review records advice viewed; it dispatches no equipment.” For history: “This saved result is historical and cannot authorize a new action.” |
| 1:34–1:54 | **Simulation workbench**: verified palette/grid, select/drag existing equipment, **Run forecast** | “The separate sandbox lets us build and edit synthetic networks. Its geometry is schematic; numerical edits require a fresh forecast. It does not rewrite the EOG recording.” |
| 1:54–2:00 | Return to a clean result or network view | “Breakwater connects model-led investigation with inspectable numerical evidence and explicit human review.” |

The completed live-search evidence permits the plan segment to show **Search plans** and its actual returned `best_found` scope, then the separate exact validation. Use the exact completed session if fresh inference has not finished. Never describe a bounded search as the global cheapest continuous plan. A zero-cost result is also valid when the retained evidence supports no truck dispatch or manual well shut-in.

## Judging fallback

If hosted inference is slow or unavailable, keep the current trace/error visible, then use the tested local numerical flow: **Detect events**, and **Search plans only in the verified search release**. Show its real cost, coverage and validation. In the baseline release, use the existing evaluated candidate evidence instead of claiming search works.

To show prior genuine model use, choose **Import history** or **History → Open historical evidence** and disclose the date/capture. Historical archives remain read-only; do not try to apply their advice. If the provider is unavailable, show the imported network/trends, dated archived evidence and the workbench's actual native forecast. Explain the limitation directly.

## Stop list at freeze

Defer PR #47/#49 extended-operation/core work, new simulation contracts, stateful sensor generation, new model tools, further UI redesign and further unverified grid changes. Keep the existing sandbox intact. Do not merge a new feature merely to make the video stronger. Save the final revision/path/evidence, capture the video, make its link public, and finish the Devpost preflight in [DEVPOST_DRAFT.md](DEVPOST_DRAFT.md).
