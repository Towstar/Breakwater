# Modern instrument workspace

Lucas / Person B's visual increment is source `99aff9851544d2cba00b463b2c617b37a97a4a43`, based on `4559bf1`. It makes the existing instrument workspace quieter and more coherent through native QML controls. The industrial scene remains central.

## Implemented

- `ActionButton` now honors its existing `flat` property. Navigation and utility actions have transparent resting chrome, hover feedback, white checked outlines and a separate keyboard focus ring. White filled actions retain their primary role.
- Sandbox and Operations form a grouped mode switch. Existing accessible names, icon tooltips and handlers remain available.
- A translucent graphite plate groups the existing scene tools. Its sibling anchor preserves toolbar, navigation-caption, framing and label-safe-area coordinates.
- The network list uses a restrained blue-gray selection surface and inset accent rail. Equipment silhouettes, drag/click tiles and the visible scrollbar remain available.
- The inspector separates identity, editable inputs and retained inventory through typography, spacing and quieter surfaces. Quantities and units remain readable, with wrapping for long finite inventory values.
- Shared charcoal surfaces and a neutral blue-gray scene background tie the workspace together. Status colors keep their existing meanings. Lighting, models, physics and approval behavior are unchanged.

The 64px header, 220px navigator, 286px inspector and 42px collapsed dock retain their dimensions. Numerical editing/rejection, stale-state checks, repeated tool toggling and camera handlers are unchanged. No new fonts, graphics effects or runtime dependency were added.

## Executed checks

Before this increment, PRs [#90](https://github.com/Towstar/Breakwater/pull/90)
and [#91](https://github.com/Towstar/Breakwater/pull/91) each passed all six
original CI jobs. No fix or rerun was necessary. PR91's normal and ASan desktop
jobs each passed 88/88 CTests and 1,181 Qt cases, with one expected optional
PhysicsNeMo skip. All six artifacts matched its exact head tree. These
[retained CI results](evidence/modern-workspace/99aff98/ci91/READOUT.md) qualify
their earlier source identities; they are separate from the UI checks below.

Both full configured builds passed on the development Windows machine. The focused regression included 32 CTests per configuration, including fixture dependencies:

| Build | Passed | Failures / skips | Duration |
| --- | ---: | ---: | ---: |
| Normal | 32/32 | 0 / 0 | 183.25s |
| AddressSanitizer | 32/32 | 0 / 0 | 509.93s |

Coverage includes actual UI interactions, numeric rejection and invalidation, tool toggles, camera framing, WARP body/label picking, telemetry replay, observed advisory review, Science rendering, retained pipe evidence and Operations views. All four workbench cases report actual logical clients of 1440×900, 1100×700, 1008×672 and 960×540 at DPR 1.75 in both modes. These are 32-test focused matrices, not a new 88-test full qualification. ASan covers owned C++ targets; prebuilt Qt/Python/Torch libraries are outside that instrumentation.

The normal executable SHA256 is `b78ad3fbdf3e8b0bb26281f860fda134f03114db21abee914d01f1cc89edb002`; ASan is `251fb7ec0e110abcb8be038086101a29c3531a8ef1ad43e7166690dc73be0fad`. The saved user project remains private and was preserved byte-for-byte before rebuilding. Manual keyboard review verified Tab focus, its visible ring and Space activation of Fit.

[Original build/test logs, JUnit, actual UI records and review](evidence/modern-workspace/99aff98/) retain this scope. Normal automated snapshots were not enabled. ASan retained screenshots across all four sizes; the [gallery](images/portfolio/README.md) contains three unaltered synthetic-fixture captures with [byte hashes and source/app metadata](evidence/modern-workspace/99aff98/gallery.json). Private user-workspace captures are excluded.

## Reproduce

Use the repository's existing pinned Windows Qt/compiler/CPU-science setup. Build the normal or ASan desktop graph as documented by the Windows checks, then run the focused matrix from that configured checkout:

```powershell
. ./scripts/ci/Initialize-Compiler.ps1
$env:NVIDIA_API_KEY = ''
$env:QSG_RHI_BACKEND = 'd3d11'
$env:QSG_RHI_PREFER_SOFTWARE_RENDERER = '1'
ctest --test-dir out/portfolio-desktop --output-on-failure -R '^desktop\.(ui_overhaul_.*|observed_advisory_.*|sensor_editor_.*|telemetry_replay_.*|eog_(analysis|overview|facility)_.*|qml_smoke|scene_picking_warp|scene_framing|label_placement|workspace_settings|editor_tools|project|pipe_evidence_smoke|storage_science_(normal|compact))$'
```

Use `out/portfolio-desktop-asan` for the ASan build. To retain workbench PNGs, set `BREAKWATER_UI_EVIDENCE_DIR` to an existing absolute local directory before CTest. Keep desktop input quiet during GUI checks.

## Attention

Independent same-family source/artifact review found no introduced issue in the five-file QML diff and inspected actual screenshots at all four sizes. This is not formal human GitHub approval or a cross-family review. The bounded local decision-trail audit covered its cited source/workspace/build/normal-test checkpoints; full transcript-dependent checks are incomplete across compaction. Clean-Windows VM acceptance, other real display scaling, owner/licensing release gates and new hosted rehearsals remain separate. Agent and numerical implementations did not change in this increment.
