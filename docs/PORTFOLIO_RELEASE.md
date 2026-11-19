# Breakwater v0.2.0 portfolio release

Accepted direction: 17 November 2026. This is an implementation ledger, not a
claim that the release gates have passed. Devpost and the frozen hackathon build
are outside this work.

## Active release-readiness pass

The implemented application work currently ends at PR66 (`0cfee72`). The readiness
branch prepares the owner review packet, final qualification, current README/
screenshots and a clean-Windows VM handoff. Its exact candidate and outcomes are
recorded separately from the earlier package reports below. No new simulation
features, video or resume-writing deliverables are included in this pass.

Main integration remains gated on Ethan's formal review of PR58. Public binary
publication additionally requires his recorded GPLv3 agreement, completed
dependency/source-delivery review and actual clean-Windows acceptance. The handoff
helper checks archive/source consistency but always leaves those external gates
pending. See [RELEASE_REVIEW](RELEASE_REVIEW.md),
[DEPENDENCY_DISTRIBUTION](DEPENDENCY_DISTRIBUTION.md) and
[CLEAN_WINDOWS_CHECK](CLEAN_WINDOWS_CHECK.md).

## Product

Sandbox is the primary editable, synthetic constant-flow workbench. Operations
is the recorded-data/advisory workspace; EOG is an import adapter. Both share a
graphite UI, daylight industrial scene, consistent controls and docked evidence.
Layout decoration never supplies hydraulic geometry or changes numerical identity.

The first release consolidates existing capabilities. Generic sensors through
detection and observed-state forecasting, evolving fleet dispatch and multifeed
projects remain subsequent increments. Existing project-v1/v2 and archives stay
compatible. Historical/imported/recorded evidence never restores operating approval.

## Integration and owners

The starting main is `3116b3d8349beab729f8a1413cbbd9ff160e67ac`.
The preview candidate is PR56 at `0314abaa7dab195267887efe8e06dd6938f4e8cc`.
Integrate its ancestry on an isolated branch; do not merge already-present PRs
again. Preserve the unpublished observed-capture work separately. The bounded
test-dataset metadata compatibility patch is adopted without modifying its source
worktree; Ethan's numerical review remains required before main integration.

Lucas owns desktop, contracts, root integration, packaging and shared docs. Ethan
owns engine/science/proofs, numerical fixtures, qualification and numerical review.
The combined persistent-plant/private committed evaluator is not a released
end-to-end dispatch UI. Detector research limitations remain explicit.

## Delivery ledger

| Increment | Acceptance | State |
|---|---|---|
| Baseline | Ancestry merge; normal/ASan native and desktop; science; Lean | Integrated on isolated branches; local results retained; owner review pending |
| Shell | Sandbox/Operations; responsive docks; common controls and dialogs | Implemented; normal/compact/minimum native UI checks pass |
| Scene | Industrial ground/pads/supports/materials; camera and labels | Implemented; default and 128-asset overview measured locally |
| Editing | Atomic group selection/move/duplicate/align/delete; previews | Implemented; backend and native-event checks pass |
| Evidence | Native trajectories; compatible comparisons; history/review clarity | Implemented; historical-only reopening and exact validation verified |
| Offline | Self-authored examples; local search/validation; genuine recorded AI | Implemented; real no-model walkthrough and four final hosted rehearsals pass |
| Science | Existing optional tracer fit with real-runtime qualification | Real CPU environment, 23 worker checks, 3 qualification checks and desktop explicit-apply case pass |
| Delivery | Portable Windows build; clean-machine verification; docs/media | Packaging/relocation implemented and tested locally; final clean Windows and public release gates pending |

Every UI increment is built and the exact executable is shown locally. Retain
test counts, skips, source revision, machine/toolchain metadata and failed attempts.
No previous component report becomes a rerun on the new combined source.

Review is split into draft PR58 (baseline), PR59 (workspace), PR60 (large-scene
performance), and the stacked delivery branch (portable runtime, optional setup,
documentation and final integration fixes). Main is unchanged. The first desktop
CI attempts completed their CTests but failed the final reporter on the explicitly
optional tracer-fit skip. The corrected reporter retains that exact case/reason
and still rejects unexpected skips, missing totals, failed and blacklisted cases.
Its 21 local regression checks pass; fresh hosted CI outcomes remain separate.

Executed counts, failures and source/runtime scope are in
[`docs/evidence/portfolio/qualification.json`](evidence/portfolio/qualification.json).
The initial normal suite's stale assertion and the minimum-window ASan failure
remain recorded alongside their passing fixes. The final hosted round passed
three canonical and one renamed/topology-changed investigation; all 16 attempts,
including the six earlier incomplete investigations, remain archived. This is
rehearsal evidence, not an LLM reliability or comparative superiority benchmark.

On one Radeon 880M laptop at 1440×900 client size and 1.75 device pixel ratio,
the default scene measured 62.5 FPS. The 128-asset/256-route overview improved
from 13.9 to 62.5 FPS after stopping hidden-label projection/repeated asset-list
materialization and bounding overview detail. All 128 actual bodies remained
pickable. Large overviews use the low rendering profile; close/default views
retain industrial detail. These are local measurements, not hardware guarantees.

The clean-source `97be3c1` portable candidate is 104,109,086 bytes compressed and
passes six sanitized local checks with its isolated Python and no supplied key.
Its source and bundle hashes are retained with the package metadata. Manual
review then found the recorded-example menu could lose import intent during
dataset retirement. Import now follows the explicit load-completed signal;
normal/compact/minimum regression checks pass in normal and ASan builds. Final
bundle verification also exercises this real historical recording path.

The earlier application-code candidate, clean commit `6cb2587`, passes all seven
isolated-process checks, including the full recorded-archive UI regression. Its
Windows ZIP is 104,110,029 bytes. The [candidate manifest](evidence/portfolio/candidate-6cb2587.json)
links exact source/bundle hashes, build metadata and verification results. Later
documentation commits do not relabel this binary's source revision.

The [actual application gallery](images/portfolio/README.md) shows the expanded
evidence dock, native comparison, explicit review and self-authored Operations
dataset. A fresh local Operations search and separate exact-plan validation ran
through the packaged desktop; controls were left unapplied.

## UI and editor acceptance

Default 1440x900; minimum client 960x540. Below 1280 wide, one auxiliary side panel
is visible at a time. Results resize to at most 55% of available height. Preserve
active white outlines, repeat-click closing, equipment tooltips and network scrollbar.
Dialogs have a fixed header/X and footer with a scrolling body and focus restoration.
Shared user preferences are local/versioned and separate from projects and keys.

Sandbox results: Forecast, Compare, Investigate, History, Science. Operations:
Observations, Events, Plans, Trace, History. Selecting evidence never runs tools.
Keep exact evidence, original units, out-of-range predictions, compatibility checks
and explicit review. Costs are modeled, finite searches are best-found, not optimal.

Asset selection supports Ctrl toggling, Shift addition/list ranges and marquee.
Group transforms are single undo transactions with complete preflight/rollback.
Duplicate internal connections with new identities, +80 X/Z offset, no external
edges/sensors/evidence. Layout transforms retain numerical identity; topology
changes retire approval. Scene shortcuts do not steal text-editing input.

Keep legacy camera serialization. Orthographic/fit overrides are view-only and
reset on project load/reset or return to saved orbit. Decorative parts are not
pickable; overview geometry and labels are bounded.

## Release gates

- Required normal/ASan native and desktop checks, CPU science and pinned Lean.
- Investigate the preview's reported startup access violation; repeated launch,
  rendering, shutdown and restart recovery without a fatal diagnostic.
- Real optional PhysicsNeMo qualification separate from default CPU availability.
- Three canonical hosted sessions and a renamed/topology-changed session on final
  source, including retained failures and sanitized request metadata.
- Independent synthetic numerical oracles and exact capture-bound comparisons.
- Clean Windows/relocation/offline/no-key checks; no development PATH or SDK needed.
- Owner agreement on GPLv3, complete source/build instructions and dependency/data
  notices before publishing. Public GitHub release remains a reviewed final step.

Package a small isolated Operations Python runtime; Torch/PhysicsNeMo stay optional.
Honor explicit BREAKWATER_PYTHON and visibly reject incompatible overrides. Do not
copy a development virtualenv or expose credentials in logs, QML or archives.
Only self-authored/redistributable fixtures and genuine matching AI traces are
bundled. Missing credentials leave hosted gates pending; no fabricated replay.
