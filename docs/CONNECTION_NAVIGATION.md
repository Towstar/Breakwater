# Connection navigation and endpoint focus

Lucas / Person B's next workspace increment follows PR82. It reuses the existing
network, selection, inspector and view-only camera; public contracts and saved
projects are unchanged.

## Workspace behavior

The Network heading has Equipment and Connections icon buttons, with active
outlines and named hover/keyboard tooltips. They share the existing filter and
virtualized list rather than consuming another row of the compact sidebar.
Connection rows show source → destination, configured rate and separate
availability. Their full names and stable route IDs are accessible and available
in bounded, plain-text tooltips. Filter by route ID, either endpoint name/ID,
pipe/pump type or availability text. Filtering and switching lists preserve the
current selection and numerical evidence.

For source-fed routes, the displayed rate comes from the controller's production
minus reduction setting. Other routes show their configured setpoint. An
unavailable route may retain a positive setting; availability is displayed
separately. These are simulation settings, not measured delivery.

Selecting a connection selects its exact route ID. The equipment multi-selection
and Ctrl/Shift paths remain separate. Connection creation returns the navigator
to Equipment and prevents route-row selection from becoming an endpoint choice.

Focus, F and the inspector's Focus action now frame both endpoints of a selected
route. Previously the camera helper only considered selected equipment, so
route Focus returned without moving the view. The two endpoint labels receive
contextual priority even with More equipment labels off; they remain ordinary,
unselected equipment. Existing clipping, spacing, toolbar exclusions and label
culling still apply. The connection inspector puts direction, settings and pump
availability above the editable parameters.

## Verification

PR82's unchanged head `f9e47e8` passed all six Windows CI jobs: native 17/17 and
desktop 87/87 in both normal and ASan, CPU science 464 passed with four expected
optional skips, and 54 Lean theorem audits. Its compact WARP reports have six
picks, six labels and zero overlaps. The
[source-qualified CI record](evidence/connections-navigation/pr82-ci.json) binds
the head, identical merge tree, toolchains, artifact hashes and raw reports.
This establishes the preceding baseline, not this new UI increment.

The new native interaction harness exercises connection modes, case-insensitive
ID/endpoint filters, hidden selection, empty results and recovery, configured
source-rate wording, exact route selection, all three Focus paths and contextual
labels. It checks the complete editor document, editor/numerical revisions, run
ID, network hash and preview-approval eligibility throughout. It restores the
previous view preferences before continuing the existing editor, history and
scientific workflows.

Application source `2b5480d` passed the six focused CTest suites below in normal
and ASan builds, with no failures or skips. Each mode includes all four UI sizes,
compact WARP picking and eight Qt label cases. The full UI reports retain the 19
prior groups and add three connection groups. On the identified AMD Radeon 880M,
default and low-detail dense checks picked 6/6 and 128/128 assets, respectively,
with no label overlaps; observed frame rates were 62.50 and 61.29 FPS. Dense
labels remain intentionally culled. These results do not claim a full 87-test
run on the later source. See the [local checks and unchanged raw reports](
evidence/connections-navigation/local-checks.json).

```powershell
. ./scripts/ci/Initialize-Compiler.ps1
cmake --build out/portfolio-desktop --target breakwater
ctest --test-dir out/portfolio-desktop -R '^desktop\.(ui_overhaul_(normal|compact|short|minimum)|scene_picking_warp|label_placement)$' --output-on-failure
```

Repeat using `out/portfolio-desktop-asan` for the configured ASan build. Keep
hosted credentials out of these offline checks.

The latest normal application was shown maximized with Connections selected,
the backup route selected and its endpoint Focus exercised. The user's actual
saved project was preserved byte-for-byte and restored; its contents remain
local. Only this application instance was open at handoff. The [handoff record](
evidence/connections-navigation/handoff.json) identifies the compiled source and
binary separately from the documentation commit. Human owner, licensing, main
integration and actual clean-Windows release gates remain open.
