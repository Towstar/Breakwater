# Operations workspace source checkpoint: 328f5f8

Reviewed frozen commit `328f5f8` against `ce256dd`. This is an independent same-family review of the new C++/CMake verification path and the parent-authored `EogDatasetPanel.qml`/`Main.qml` changes. The reviewer authored the separate `EogTrendChart.qml` styling, so preservation checks of that file are self-review, not independent review.

No unresolved consequential source finding was found after the corrections below. This is not a compilation or runtime pass.

## Corrected findings verified in the frozen source

1. Empty process credentials originally allowed checkout/saved-file fallback. The new flag now injects three `RejectingModelTransport` instances, with no hosted client construction or credential discovery for those actors. They are not NemotronClient instances, so EOG's configuration-reload cast cannot match them. Ordinary/final requests are rejected and counted; every probe tick requires all counters to remain zero. Their owners outlive all borrowing controllers. `CredentialSettings` receives a path in the validated temporary history directory and rejects configured/saved state.
2. Invalid allocated temporary preference storage now returns failure before `WorkspaceSettings` can fall back to the real user's preference file. Ordinary startup retains its existing default path.
3. Failed-result metadata now separates probe intent from observed state: `analysis_requested_by_probe:false`, actual busy/session fields, explicit blocked hosted transport and actual request-attempt count. It no longer unconditionally labels failed evidence as no analysis requested.

## Reviewed boundaries

- Production startup outside `--eog-workspace-check` retains normal model construction, examples, history and preference paths. The EOG model moved to an owning pointer without changing its configuration call or lifetime relative to its borrower.
- One explicit test flag requires EOG workspace and a local source directory. Incompatible investigation, import, history, report, benchmark, Science and legacy smoke modes fail preflight. Normal EOG startup auto-loading is suppressed only for the new onboarding probe.
- The source fixture is copied into a temporary directory before its CSV is extended. The added30-minute timestamp is consistent with its Unix timestamp and yields the intended1500-second tank age. The original source already supplies negative `-2`, above-capacity `104`, status `2` and legitimate numeric `0` cases. Copy/write paths do not modify the repository fixture.
- The probe uses temporary workbench/EOG histories and preferences, and overrides only its synthetic-example URL/path. Preserved assertions compare the entire sandbox document, hash, run, revision and preview-approval state, plus empty analysis/review evidence.
- ComboBox selections use keyboard events and verify resulting selected identities; scene/cutoff operations use mouse events and verify state changes. Direct dataset cursor setters are preparation for specific observation cases, not evidence that the slider worked; the separate slider step verifies its actual interaction.
- Events are delivered to the bound QQuickWindow using Qt event dispatch, without global OS input. Client-coordinate guards and the minimum-size actual thumb-drag assertions are present. Actual clipping/reachability and render stability remain to be executed.
- The probe has a45-second internal deadline and75-second CTest timeout. These are bounded source rules, not measured timing results. Requested/actual client dimensions and model-request attempts are recorded.
- The panel keeps existing selection/cutoff handlers and original-unit observations. Missing data remains explicit, numeric0 stays distinct, and stale/negative/above-capacity values are retained. Loading the example remains an explicit action. The new mode hints and scroll affordance do not add numerical or approval authority.

## Attention

No compiler, CTest, live model request or UI input was executed by this reviewer. Parent-managed builds/tests are a later evidence boundary. Proposed assertions and screenshots are not passing checks. This review will be supplemented by an artifact review when actual reports/screenshots are available, especially minimum client overflow, condensed Operations views and observation semantics. No real user credential file or key value was read. This is a bounded same-family review, not formal human approval, clean-Windows acceptance, a complete transcript audit or independent review of the reviewer's own chart styling.
