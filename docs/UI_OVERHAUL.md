# Native desktop UI overhaul

This Person B increment follows Lucas's supplied `BREAKWATER_UI_OVERHAUL_SPEC.md`.
His subsequent instruction also authorizes richer 3D equipment geometry. It retains
the simulation workbench and the separate recorded-data EOG console.

Working branch: `codex/person-b-eog-handoff`, base `5c22cdb`; root records final commit/checks below. Source review is not a tested release.

## Architecture and assets

- Native Qt Quick Controls Basic and Qt Quick 3D; no web layer or new renderer.
- `Theme.qml` supplies graphite surfaces, white primary emphasis, restrained amber
  caution and rose error accents, Segoe UI text and Consolas numerical details.
- `ActionButton.qml` retains native Button input and adds local SVG icons, text,
  enabled/checked states, keyboard focus rings and hover/focus tooltips.
- `ActionFeedbackCard.qml` is one compact, nonmodal scene-corner notice. It never
  requests focus or creates a full-window input surface.
- The exact supplied logo is `desktop/assets/breakwater-logo.svg`; it is not
  redrawn, distorted or tinted as an action icon. CMake bundles it and the icons
  under `qrc:/breakwater/assets/`.
- The 36 local action icons use one Lucide family. Exact revision, substitutions,
  logo hash and license provenance are in [ASSET_SOURCES.md](../desktop/assets/ASSET_SOURCES.md).
  Preserve [LUCIDE-LICENSE.txt](../desktop/assets/LUCIDE-LICENSE.txt) when distributing.
  No system icon theme, downloaded font or network image is required.

## Action map

| Capability | Location and unchanged meaning |
|---|---|
| EOG / sandbox navigation | Header; each retains its existing scene/controller context |
| Open, Save, Undo, Redo | Compact header icons, names/tooltips and existing enabled guards |
| Pump failure, Bad reroute, Mixed plan | Header **Demo scenarios** menu; loads whole example configurations |
| Reset | Separate labeled header action; remains distinct from Undo |
| Create / connect equipment | Left palette and **Connect equipment** active-mode button |
| Select / edit / delete / fail pump | Scene or network list, then right inspector; existing commit rules |
| Forecast / scrub / approve preview | **Run forecast** and lower dock; actual run horizon is displayed |
| Investigate / compare / review / history | **Nemotron investigator** dock and its existing tabs |
| Sensors / synthetic telemetry / pipe science | Left navigation; existing editors and evidence dialogs remain reachable |
| Storage science / sensitivities | Investigator's existing Science tab and captured-control restrictions |
| EOG analysis / advisory review / archives | **Investigate / history**; no dispatch or sandbox-control authority |

Candidate selection and plan inspection remain view operations. **Apply simulated
controls** remains a separate, explicit, guarded action in the staged-plan review.
The manual simulated-preview approval keeps its separate existing meaning.

## Industrial 3D presentation

`IndustrialEquipment.qml` selects only the assembly needed for each asset:

- Tanks: foundation, cylindrical shell, roof, bands, access ladder and sight gauge.
- Pumps: skid, motor body/fins, coupling, casing, inlet/outlet flanges and gauge.
- Sources/wells/disposal: wellhead tree, valves, cross-pipes and fittings.
- Treatment: paired process vessels, connecting pipe and control cabinet.
- Trucking: tanker body, chassis, cab, wheels and tank bands.
- Pipeline equipment: short supported pipe with valve/gauge details.
- Facility overview: lighter grouped vessels and cabinet for topology navigation.

`IndustrialPipe.qml` adds bounded flange/gasket/bolt detail to the existing route
transform. EOG water links and dashed truck-access links remain distinguishable.
Geometry is schematic: it supplies no hydraulic constraint, measured flow,
surveyed position or calculated travel time. There is no cosmetic flow animation.

The original outer `equipment:<id>` node and `asset:` / `route:` / `eog-asset:`
picking identities remain intact. Decorative parts are unpickable. Existing
dragging, snapping, connected-route updates, orbit/pan/zoom, stored camera state
and scene lifetime are retained. Main's floor/background/sensor recoloring is
presentation-only. Invalid pump drops now reach the existing controller rejection
path, including empty ground; they do not create a free-standing pump.

Tank gauge geometry is bounded, while actual negative/overflow numbers and chart
values remain available. Layout and mesh dimensions never change numerical inputs.

## Status, feedback and evidence

- **Rejected action:** a new structured controller outcome opens the notice, even
  for identical repeated attempts. No rejection creates a route, undo entry or
  numerical revision. Dismissal hides the notice, not the underlying condition.
- **Incomplete draft:** actual `draftIssues` remain in **Network issues · N**;
  construction remains editable and forecast gating still uses backend checks.
- **Rejected numeric entry:** the affected inspector row shows the attempted text,
  authoritative explanation and accepted value. The presentation does not parse,
  clamp or silently substitute a number; the controller remains authoritative.
- **Completed unfavorable forecast:** capacity violations remain numerical results,
  with raw values and run context; they are not reclassified as application crashes.
- **Service/storage failure:** existing local status and diagnostics remain visible;
  an unavailable model or science worker does not manufacture successful evidence.
- **Historical/stale:** existing retirement and approval guards remain authoritative.
  Reopened/imported evidence cannot regain current approval or EOG review eligibility.

The EOG trace tab presents retained model exchanges, requested tools, actual
completed/failed results, cancellation and final references in a virtualized feed.
Durations/HTTP status/retries appear only when retained. Full explanation, exact
trace and operator JSON are created only after explicit disclosure. No raw model
reasoning, fabricated progress, provider invocation or approval is added by the view.

## Keyboard and verification handoff

Tab reaches native actions; focus rings and tooltips identify icon-only controls.
The equipment palette supports Space/Return; existing platform Undo/Redo bindings remain.
Escape dismisses feedback when a card descendant owns keyboard focus;
otherwise the existing scene connection-cancel behavior remains. Notices do not steal focus.
These interaction paths require the runtime checks below.

Final evidence to fill in by the integration owner:

| Gate | Status / evidence |
|---|---|
| Final normal/ASan builds and relevant regressions | Pending final-source verification |
| Actual load/edit/forecast/investigate/compare/review workflow | Pending |
| Invalid/repeated connections, pump drops, numeric rejection and unchanged model | Pending |
| Keyboard focus, Return/focus-loss, Escape and input outside the notice | Pending |
| Stale/historical and current science/evidence screenshots | Pending |
| 1440×900, compact 1100×700; 1366×768 where practical | Pending |
| Windows 100%, 125%, 150% scaling where available | Pending; no accessibility conformance claim |
| Installed/relocated app logo/icons without checkout or network | Pending |

Source lint/diff checks and review are separate from these runtime gates. Some
descendant styling/polish may remain; no new numerical model or physical monitoring is claimed.
Rollback: revert isolated UI integration commit(s), preserving later scientific,
provider, contract and evidence work. Do not reset shared branches or delete sessions.
