# Native desktop UI overhaul

Lucas / Person B owns this native presentation increment. It follows the supplied
`BREAKWATER_UI_OVERHAUL_SPEC.md` and the later authorization for richer 3D geometry.
The simulation workbench and recorded-data EOG console remain separate.

## Architecture, assets and 3D

- Qt Quick Controls Basic + Qt Quick 3D; no web runtime or renderer replacement.
- `Theme.qml`: graphite surfaces, white primary actions, amber cautions/rose errors,
  Segoe UI text and Consolas numerical details.
- `ActionButton.qml`: native input, enabled/checked appearance, local SVG icon,
  accessible name, hover/focus tooltip and keyboard ring.
- `ActionFeedbackCard.qml`: one nonmodal, stable scene-corner notice; no focus steal
  or full-window input blocker. `WorkbenchUiCheck` exercises the compiled Main.
- The exact supplied logo and 36 local Lucide icons are packaged under
  `qrc:/breakwater/assets/`. Preserve [asset provenance](../desktop/assets/ASSET_SOURCES.md)
  and [license/notices](../desktop/assets/LUCIDE-LICENSE.txt). No platform icon theme is needed.
- `IndustrialEquipment.qml` selects bounded assemblies: tank shell/roof/bands/ladder/
  sight gauge; pump skid/motor/fins/coupling/flanges; wellhead/valves; paired treatment
  vessels/cabinet; tanker cab/chassis/wheels; pipeline valve; lighter facility overview.
- `IndustrialPipe.qml` adds flange/gasket/bolt detail to existing route transforms.
  Decorative parts are unpickable; original `equipment:`, `asset:`, `route:` and
  `eog-asset:` identities remain. Water connections and truck access are distinct.
- Geometry is schematic, supplies no physics/travel calculation, and has no cosmetic
  flow animation. Bounded tank-gauge geometry does not clip raw negative/overflow
  numbers or charts. Layout, snapping, connected routes and camera gestures retain
  their existing numerical independence and scene lifetime.

## Operator action inventory

| Capability | Current location / meaning |
|---|---|
| EOG / sandbox | Header navigation; existing controllers/evidence contexts retained |
| Open / Save / Undo / Redo | Header icons with names/tooltips and original enabled guards |
| Pump failure / Bad reroute / Mixed plan | **Demo scenarios** menu; whole example configurations |
| Reset | Separate labeled action, distinct from Undo |
| Create / connect | Left equipment palette and explicit **Connect equipment** mode |
| Select / edit / delete / fail pump | Scene or network list, then right inspector |
| Forecast / time scrub / preview approval | **Run forecast** and lower dock; actual returned horizon |
| Investigate / compare / review / history | **Nemotron investigator** dock and existing tabs |
| Sensors / local synthetic monitoring / pipe transport | Left navigation and existing dialogs |
| Storage science / sensitivities | Science tab; captured-control ordering/restrictions retained |
| EOG detect / evaluate / review / archives | **Investigate / history**; advisory only |

Candidate selection/inspection does not apply controls. **Apply simulated controls**
remains a separate explicit guarded staged-plan action; manual preview approval keeps
its separate meaning. No EOG action dispatches trucks, changes wells or applies sandbox controls.

## Feedback mapping and persistence

All rows below use actual `ProjectController::reportAction` outcomes: unique ID,
code, title/message, affected IDs/field, `model_changed:false`, and the allowlisted
`view_issues` recovery hint. Recovery below describes operator steps, not automatic fixes.

| Backend condition → code | Visible explanation / recovery |
|---|---|
| Unsupported direction → `connection.unsupported_types` | **Connection not supported**; source → tank or tank → tank/treatment/trucking |
| Same target → `connection.self`; deleted target → `connection.missing_target` | Choose a different/existing destination; no route added |
| Second source route → `connection.source_already_connected` | **Source already connected**; delete its existing route explicitly; nothing replaced |
| Ground/unrelated pump drop → `pump.choose_route` | **Choose a pipe route** leaving a storage tank |
| Source-feed pump → `pump.source_feed` | **Pump cannot attach here**; choose a tank-origin route |
| Existing pump → `pump.already_attached` | Informational **Pump already attached**; no second pump |
| Invalid numeric text → `property.invalid_number` | **Enter a valid number** in displayed units; inline attempted text + accepted value |
| Backend edit rejection → `edit.rejected` | **Property needs attention**; actual validator explanation, no competing UI validator |
| Invalid position → `placement.out_of_range` | Finite position within existing layout bounds; no added/moved equipment |
| Limits → `equipment.limit` / `connection.limit` | Actual limits: 128 assets / 256 routes; remove unused entries explicitly |
| Attached sensor → `delete.sensor_attached` | Remove/reattach the named sensor before deleting equipment/route |
| Ineligible fault injection → `pump.not_available` | **Select an available pump**; no different scenario loaded |
| Incomplete forecast → `forecast.incomplete_draft` | **Network needs attention**; keep editing/draft saving and resolve actual issues |

Repeated deliberate failures reopen the notice; ordinary refreshes do not. Dismissal
hides only the card. Rejected numeric entries remain inline until a successful same-field
commit or selection change. Return/Enter and edited focus loss commit once; unedited
focus loss does not resubmit an accepted value. These presentation records are not a
new persistent history format. Actual `draftIssues` remain in **Network issues · N**.

Completed unfavorable forecasts retain values/run context rather than becoming software
errors. Numerical edits/reset retire affected runs/approval; permitted layout moves retain
them. Imported/reopened history stays nonactionable. Service/storage panels retain their
actual unavailable/failure/cancelled states; installed packages do not imply executed science.

## Trace and keyboard

The EOG trace is a virtualized feed of actual retained model exchanges, tool requests,
results/failures, cancellation and final references. Durations/HTTP/retries are shown only
when recorded. Full explanation, trace and operator JSON are lazy explicit disclosures.
No raw model reasoning, fake progress or eligibility calculation is introduced.

Tab/focus tooltips identify native actions; Space/Return activates palette entries.
Existing platform Undo/Redo bindings remain. Focused-notice Escape dismisses only the
notice, using actual focus ancestry even when the global shortcut wins routing.
Escape elsewhere retains connection cancellation. No popup acquires focus automatically.

## Recorded verification and limits

| Revision / scope | Actual result |
|---|---|
| `6cfa3ee` normal / ASan combined | **64/64**, 512.02 s / **64/64**, 934.32 s |
| `6cfa3ee` Visual Studio | Full compile + **23/23** EOG/UI tests, 89.60 s |
| VS tracking audit | 133 paths, 0 duplicates, maximum 167 characters, 0 MSB8028 warnings |
| `3b79f3e` label/test-cache follow-up | **9/9** normal, 16.14 s; **9/9** ASan, 67.68 s |
| Relocated installed UI | Nine actual Qt-input gates pass at observed DPR 1, 1.25, 1.5 and 1.75 |
| Installed scientific worker | Real health, Torch two-control sensitivities/history; agreement within 1e-9 |
| Historical science | Real saved SessionRepository result `c79d291d-cebf-470e-bde2-2a1366224d3b`; installed import exits 0, read-only/approval disabled |
| Hosted EOG rehearsals | Canonical 06/13/15 and renamed 02 passed their gates; earlier failures retained |
| `758b92c` optional screenshot-helper | Normal 2/2 (5.71 s), ASan 2/2 (21.00 s); positive PNG capture and invalid-parent rejection pass in both modes; final VS 10/10 (48.92 s) |
| Final relocated installed executable | Nine Qt-input gates pass; SDK/plugin/import paths cleared, Windows-only PATH, no requested model calls |
| `81dfeb6` dense official overview | **8/8** normal (16.63 s), ASan (77.66 s) and VS (22.95 s); actual official overview/facility renders retain 25/8 nodes, 30/6 routes and all source hashes |

ASan instruments owned native code, not prebuilt Qt/Python. The full-suite results above
belong to their stated revisions; later helper-only changes do not inherit a new full-suite claim.
Hosted traces prove those executions, not universal reliability; joint search remains
unavailable, so no global-optimality or independent held-out accuracy claim is made.

Independent source reviews covered picking, bindings, feedback and the helper. Actual
Qt-input tests caught/regressed Return→blur resubmission and global Escape cancellation.
Source review, rendered frames and in-process Qt mouse/key events are distinct evidence.
OS computer-helper input was denied (`GetCursorPos: E_ACCESSDENIED`); no human-driven
end-to-end OS workflow or accessibility conformance was established. 1366×768 remains
unrecorded. Installed DPI/resource checks do not imply every dialog/state was manually tested.

## Reproduce

From the worktree root, initialize x64 MSVC and use pinned Qt/Python paths (adjust the
two local tool paths for another machine). Keep hosted credentials out of these checks.

```powershell
. .\scripts\ci\Initialize-Compiler.ps1
$qt = (Resolve-Path '..\Breakwater\.tools\Qt\6.12.0\msvc2022_64').Path
$python = (Resolve-Path '..\Breakwater\.tools\science-cpu\Scripts\python.exe').Path
$env:PATH = "$qt\bin;$env:PATH"
$env:QSG_RHI_BACKEND = 'd3d11'
$env:QSG_RHI_PREFER_SOFTWARE_RENDERER = '1'
cmake -S . -B out/desk-normal -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_PREFIX_PATH=$qt" "-DPython3_EXECUTABLE=$python" -DBREAKWATER_BUILD_DESKTOP=ON -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded
cmake --build out/desk-normal
ctest --test-dir out/desk-normal --output-on-failure
cmake -S . -B out/ui-vs -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$qt" "-DPython3_EXECUTABLE=$python" -DBREAKWATER_BUILD_DESKTOP=ON -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded
cmake --build out/ui-vs --config RelWithDebInfo
ctest --test-dir out/ui-vs -C RelWithDebInfo -R 'desktop\.(ui_overhaul|eog_)|contracts\.eog' --output-on-failure
cmake -S . -B out/ui-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_PREFIX_PATH=$qt" "-DPython3_EXECUTABLE=$python" -DBREAKWATER_BUILD_DESKTOP=ON -DBREAKWATER_ENABLE_ASAN=ON -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded
cmake --build out/ui-asan
ctest --test-dir out/ui-asan --output-on-failure
```

Embedded debug information uses MSVC `/Z7`. The actual ASan evidence was produced in
sibling `Breakwater-eog-provider-review/out/provider-asan`; `out/ui-asan` above is a clean
reproduction directory. Existing logs are under `out/`, including `ui-vs-tracking-audit.json`.
Run the built/installed app with `--workspace workbench --ui-overhaul-check` (add
`--compact-ui` for 1100×700). `QT_SCALE_FACTOR` can exercise process scaling; JSON records
actual DPR and logical dimensions. Optional `BREAKWATER_UI_EVIDENCE_DIR` must name an
absolute existing parent; a unique new child retains real PNGs, errors are explicit,
and unset mode writes no screenshot artifacts. The helper reports hosted requests as
`not_requested`, not an instrumented transport count.

## Screenshot index and rollback

These are unchanged PNGs from the actual application. Hashes and source revisions are
recorded in [the release evidence](evidence/ui-release.json). The before image is
the earlier console; the after image includes the overview-label correction.

| Actual state | Evidence |
|---|---|
| Ready / repeated rejected number / retired run | [Ready](evidence/ui/workbench-ready.png), [invalid entry](evidence/ui/invalid-number.png), [numerical edit](evidence/ui/retired-run.png) |
| Restored real scientific evidence | [Historical science](evidence/ui/historical-science.png) |
| EOG overview label follow-up | [Before](evidence/ui/eog-overview-before.png), [after](evidence/ui/eog-overview-after.png) |
| Dataset-specific industrial facility | [Facility](evidence/ui/eog-facility-after.png) |
| Actual official EOG dataset | [All systems](evidence/ui/official-overview.png), [S1 facility A](evidence/ui/official-facility.png); source hashes and unchanged-workbench checks in the paired render reports |

For rollback, revert isolated UI integration/follow-up commits. Preserve subsequent
scientific/provider/contracts/evidence work; do not reset shared branches or delete sessions.
