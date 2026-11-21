# Final Operations workspace artifact review: 1b16a88

Reviewed preserved normal and AddressSanitizer probe logs, build/probe receipts and four final condensed-dock screenshots for clean source `1b16a881fa5d5ee87269188fc4f0ef7070c5de89`. This review performed no source edits, UI input, tests or deletions. Earlier attempts, reports and snapshots remain untouched.

No new consequential issue was found in this reviewed scope. The compact-dock chart/caption reachability defect observed on the earlier `df404bb` binary is corrected in the final artifacts.

## Provenance and actual test scope

All paths below are relative to the verified repository root `C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater-next`.

| Mode | Source and build receipt | App SHA256 matching probe receipt | Actual probe result |
|---|---|---|---|
| Normal | `out/candidate-readiness/build-normal-1b16a88.json`, clean `1b16a88`, full configured build passed | `b5c8e073dbc5cfd585221a2f457dda011a43faa3718e0a8023278cbfac628f91` | 3/3 passed, 0 failed, 0 skipped, 10.91 s |
| ASan | `out/candidate-readiness/build-asan-1b16a88.json`, clean `1b16a88`, full configured build passed | `1081993d7c12dd833cb91f18cff674f8cc6b76e3de57b67f8501ff9c456ff080` | 3/3 passed, 0 failed, 0 skipped, 36.39 s |

Probe receipt files are `out/modern-operations/1b16a88-{normal,asan}-probe-first.json`. Both record the exact source above, `dirty:false`, exit 0 and filter `^desktop\.eog_workspace_`. The preserved `test-artifacts-{normal,asan}-1b16a88-probe-first/LastTest.log` commands point to this root's respective normal/ASan `breakwater.exe` and local `desktop/tests/eog_fixtures/render` source fixture. CTest summary logs confirm the totals above. These receipts associate logs and binaries with the source; the reviewer did not rebuild or independently rehash executables.

Six actual runtime records report requested sizes equal actual sizes: 1440x900, 1100x700 and 960x540 in each mode, all at device-pixel ratio 1.75. Normal/compact/minimum records contain 12/13/14 completed checks respectively. Every record reports `model_request_attempts:0`, `analysis_busy:false`, empty `analysis_session_id` and blocked verification transport. The completed preserved-state check includes sandbox controls and review authority.

## Condensed dock geometry and visual inspection

Final `09-condensed-dock-chart.png` images inspected directly:

| Mode | Actual client | Artifact directory under `desktop/test-artifacts/eog-workspace/` |
|---|---|---|
| Normal | 1100x700 | `out/portfolio-desktop/.../breakwater-eog-ui-jqLRld/` |
| Normal | 960x540 | `out/portfolio-desktop/.../breakwater-eog-ui-oOGxrj/` |
| ASan | 1100x700 | `out/portfolio-desktop-asan/.../breakwater-eog-ui-JAiIEs/` |
| ASan | 960x540 | `out/portfolio-desktop-asan/.../breakwater-eog-ui-WkYHPl/` |

The normal and ASan records agree on these logical-pixel measurements after opening the actual investigation dock and dragging the workspace scrollbar:

- At 1100x700, `panel_condensed:true`; panel height 272, clipped viewport `(10,74,1080,252)`. The chart `(633,95,445,210)` is entirely inside the viewport. The full 15-pixel caption at y311 fits through y326. Direct images show the complete chart, both UTC endpoint labels, original unit, guides and caption. The scrollbar is visible.
- At 960x540, `panel_condensed:true`; panel height 200, clipped viewport `(10,74,940,180)`. The chart is `(546,23,392,210)`, with **159 of its 210 pixels visible at once** after this scroll. The complete 15-pixel caption at y239 fits through y254. Direct images show the bottom axis, both UTC endpoint labels and full caption. The upper chart/unit/upper guides remain above the viewport and are reached by scrolling; this does not establish the whole chart as simultaneously visible at minimum size. Before opening the dock, the minimum scrollbar check reports all 210 chart pixels reachable/visible.
- Truck-yard and disposal labels are clear of the map footer in the inspected condensed images. The actual dynamic truck-yard-label/footer nonintersection assertion completed in all six records. Dock content correctly stays empty/unavailable because no analysis was requested.

## Measurement and behavior preservation observed

All six records retain the same fixture semantics: tank `104 %`, age 1500 s, sample present but stale/unavailable; historical negative `-2 %`, age 0 s, available; pump status `2 state`, displayed as `fault feedback`. The completed checks separately cover a legitimate zero, missing channel unavailable rather than zero, above-capacity retention, equipment/metric filters, actual cutoff-slider interaction, latest playback clock, explicit facility inspection and unchanged sandbox/review state. These are actual probe passes, not numerical accuracy qualification or a hosted model rehearsal.

## Review limits

This is an independent same-family artifact review of the parent/CI-agent-authored panel, probe and layout fixes. The reviewer authored `EogTrendChart.qml` styling earlier in the branch, so review of that styling is self-review. This is not formal human approval.

The scope is the three targeted Operations workspace probes per build and the four condensed screenshots listed above. The full build passing is not a claim that all configured tests ran. Broader final regressions, clean-Windows packaging, hosted Nemotron qualification, other display scales, dense arbitrary datasets, and user keyboard/accessibility acceptance remain outside this artifact report. Earlier `328f5f8`/`df404bb` test results remain historical and are not substituted for final-source results.
