# Workspace framing and scene focus

This increment follows PR84's selected-route inspection and projection refresh.
It keeps camera state separate from numerical network inputs and approval.

## Behavior

- Top uses both dimensions of the usable viewport, rather than only its width.
  It reserves room for visible navigation and persistent scene overlays, with
  90 schematic units of body padding on each side. Resizing panels recomputes
  the fit; Fit resets manual pan and zoom. Focus uses selected equipment or the
  two endpoints of a selected connection.
- The footer identifies Plan or Perspective and distinguishes network fit,
  focused or previous focus, saved view and manual adjustment. Fit/Focus buttons
  reflect the current framing target rather than a later selection.
- Middle drag and wheel in Top change the plan camera. Projected equipment
  labels refresh after plan translation as well as zoom and camera switches.
- View > Scene focus mode, or Ctrl+Shift+F outside text editors and dialogs,
  temporarily hides navigator, inspector and results. Toggle again or press
  Escape to return. Opening panels, results or resetting workspace layout exits
  this mode. Saved panel preferences are preserved by the temporary mode itself.
- A short scene hides the general gesture hint, while connection instructions
  remain available. If overlays consume the entire scene, the footer explicitly
  asks for more space; it does not claim a successful fit.

Perspective framing retains the existing orbit behavior. These changes do not
perform a forecast, change operating controls, apply a plan or renew approval.
They do not change the scientific methods, network hash or evidence formats.

## Verification

Application source `cd4dc5f` passes seven focused suites in each of normal and
ASan. The helper has 41 passing Qt cases and label placement has eight. Each of
four native window sizes passes 25 check groups, including actual Qt projected
body corners, tall/wide layouts, reactive dock resizing, route focus, wheel,
middle-button pan and label anchors, reversible panel preferences, text-editor
shortcut guards and Escape. WARP retains six actual picks, six labels and no
overlap in each mode. The owned helper test driver is ASan-instrumented; prebuilt
Qt/Python/Torch remain outside that instrumentation scope.

The initial `3e8a051` probe passed the helper but rejected a valid camera because
the assertion compared Qt float properties with JS doubles too strictly. Its 24
rendered corners were all inside. `25c539a` corrected property comparison precision
and passed both matrices; the independent rendered one-pixel gate was unchanged.
Manual Windows input then exposed a global shortcut conflict, leading to
Ctrl+Shift+F and complete reruns on `cd4dc5f`. Those earlier records are preserved.

Both final hardware scenes meet local targets on the identified AMD Radeon 880M:
six and 128 actual body picks, zero overlaps. FPS is recorded with its conditions;
no controlled speedup comparison is claimed. Actual Windows input verified the
new shortcut and Escape in the restored, maximized application, with one latest
instance left open. See [final checks](evidence/workspace-framing/local-final-checks.json),
[intermediate checks](evidence/workspace-framing/local-checks.json) and the
[handoff](evidence/workspace-framing/handoff.json).

Reuse the pinned installed Qt 6.12 SDK and CPU Python. After configuring each
normal/ASan build, initialize the existing compiler/runtime environment and run:

```powershell
. ./scripts/ci/Initialize-Compiler.ps1
cmake --build out/portfolio-desktop --target breakwater breakwater_scene_framing_tests breakwater_label_placement_tests
ctest --test-dir out/portfolio-desktop -R '^desktop\.(ui_overhaul_(normal|compact|short|minimum)|scene_picking_warp|label_placement|scene_framing)$' --output-on-failure
```

Repeat with the ASan directory after both builds complete. Keep build processes
and computer-use input separate from native UI checks. Hosted Nemotron rehearsal,
owner/licensing review and actual clean-Windows release qualification are separate
gates; this view-only increment does not establish those results.

Camera API reference: [Qt 6.12 OrthographicCamera](https://doc.qt.io/qt-6/qml-qtquick3d-orthographiccamera.html).
