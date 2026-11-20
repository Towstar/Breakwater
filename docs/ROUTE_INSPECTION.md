# Selected connection inspection

The Connections navigator can select and focus a route, but an unavailable route
keeps its rose availability colour and a zero-setting route has no ordinary
configured-direction cone. This increment adds a separate selected-route cue so
selection remains readable in those cases.

The cue is a static neutral source-to-destination arrow above the existing pipe.
Its geometry loads only for the selected finite, nonzero-length route and scales
down for short spans. Its shaft and head do not intercept picking. The original
pipe and pump retain their identity and availability colour. No animated flow,
measured delivery or additional hydraulic interpretation is introduced.

A bounded scene card reuses the navigator's current route metadata: exact ID,
endpoints, configured/source setting and separate availability. Labels and the
hover text preserve literal imported names. Short viewports condense the visual
card; its full accessible description and hover text retain the setting. The
card explicitly says that the arrow shows direction only. Its screen rectangle
is reserved by the existing label-placement helper, and connection creation hides
it while presenting endpoint instructions.

These are read-only view cues. Selection, navigation and camera actions use the
existing paths; numerical edits, forecasts, approval and imported data formats
retain their existing behaviour.

## Rendered label positions

Top view exposed a separate projection problem: label bindings could cache the
preceding camera matrix, so names stayed at perspective positions after the
equipment had moved to the plan view. Camera changes now request a bounded
two-frame projection refresh. The first swap may belong to an already rendering
frame; the next refresh uses the updated camera. Continuous camera movement
still refreshes once per rendered frame, and the tail drains when movement stops.
This does not add a continuously running idle timer or change label priorities.

Native interaction checks switch through Top, Focus in Top, perspective return
and Home, then compare retained label anchors with fresh projections within one
logical pixel. They preserve the captured project, run, hashes and approval eligibility.
Route and projection observations are retained separately from the later editor
workflow's observations.

## Verification record

Application source `87021c2` builds in normal and MSVC ASan. Each passed all six
focused desktop CTest suites on its first attempt at that source: four native UI
sizes, label placement and compact WARP picking. All eight complete UI reports
contain the same 24 action/check groups, with retained route-picking and rendered
projection observations. Both modes retain six compact WARP picks, six labels
and zero overlaps. Builds completed before the sequential tests; no root computer
input ran during them. See the [final local checks](
evidence/next-revamp/local-final-checks.json) and their unchanged raw reports.

The identified Radeon 880M hardware sweeps picked 6/6 default and 128/128 dense
bodies, with zero label overlaps. Observed rates were 104.99 and 67.82 FPS;
the local 60/30 targets passed. The dense overview intentionally uses low detail,
with 27 visible labels and 101 culled labels. These are individual local probes,
not a comparative speed benchmark or a guarantee for every mesh/camera angle.
Short-span cue scaling is source-reviewed; the interaction fixtures use longer
routes. No hosted model request or full 87-suite run is claimed for this later
source.

The [earlier local checkpoint](evidence/next-revamp/local-checks.json) retains the
initial startup failure, separate runtime/PATH probe, mixed ASan attempt and its
configured idle retry. It is not rewritten into a clean first attempt. PR83's
preceding full CI is [recorded separately](evidence/next-revamp/pr83-ci.json).
Earlier binary hashes fingerprint those checkpoints; reusable local build paths
now contain the later executable, and historical binaries are not published here.

The updated app was shown with the preserved user workspace, selected route card
and corrected Top/perspective labels. Only the latest maximized instance remained
open; the raw saved project stays local. See the [handoff](
evidence/next-revamp/handoff.json). Human review, licensing/notices, main integration,
new-increment full CI and actual clean-Windows release acceptance remain separate
gates.

With the repository's existing configured Qt and CPU-Python environment, use:

```powershell
. ./scripts/ci/Initialize-Compiler.ps1
$env:NVIDIA_API_KEY = ''
cmake --build out/portfolio-desktop --target breakwater
ctest --test-dir out/portfolio-desktop -R '^desktop\.(ui_overhaul_(normal|compact|short|minimum)|scene_picking_warp|label_placement)$' --output-on-failure
```

Set `BREAKWATER_PYTHON` to the configured CPU interpreter for the existing Science
steps. Repeat with `out/portfolio-desktop-asan` for ASan; initialize the compiler
environment in that shell too, since it supplies the sanitizer runtime path.

## Follow-up framing

Top's existing automatic framing can crop equipment on tall layouts in wide
viewports. Zoom and selected-endpoint Focus remain available. Aspect-aware plan
view fitting is a separate follow-up; this pass changes projection freshness,
not the camera's fit-span calculation.
