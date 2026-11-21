# Operations workspace source review

Scope: read-only review of the dirty `desktop/main.cpp`, `desktop/CMakeLists.txt` and new `desktop/verification/EogWorkspaceCheck.h` against baseline `ce256dd`. Reviewer provenance: independent same-family agent for these verification changes. This reviewer authored the separate trend-chart styling file, so this is not an independent review of that file.

## Isolation findings resolved at the updated source checkpoint

**P2, corrected: an empty hosted configuration could rediscover real credentials if EOG investigation was accidentally invoked.** The first correction isolated normal startup, but `EogAnalysisController::investigate()` at its existing line780 casts a supplied transport to `NemotronClient` and calls `reloadConfiguration()`. The updated new-flag path instead supplies three verification-only `RejectingModelTransport` objects. They are not NemotronClient instances, contain no configuration discovery or network code, reject ordinary/final requests and count every attempt. The probe asserts all three counters remain zero each tick and retains their aggregate count in the result. Their owning objects outlive all controllers that borrow them. The EOG model is constructed normally only outside the new flag. Static inspection confirms this closes the reload path; no runtime pass is claimed. No reviewer source edits were made.

## Verified source corrections

- The original new-flag `qputenv(NVIDIA_API_KEY, empty)` was insufficient: `NemotronConfig::fromEnvironment()` falls back to local/saved files when the process key is empty. The rejecting transports plus the explicit temporary `CredentialSettings` path now prevent credential discovery. A guard rejects configured/saved test credential state. No real key file or credential value was inspected by this review.
- Verification preference storage now fails closed when an allocated `QTemporaryDir` is invalid. Previously an empty `filePath()` could cause `WorkspaceSettings` to fall back to the real user's `workspace-v1.ini`. The guard applies only where temporary verification preferences were requested; ordinary startup retains its previous path.

## Other inspected boundaries

- New behavior is gated by one explicit `--eog-workspace-check`, EOG workspace and source dataset. Existing live/import/science/report/benchmark combinations are rejected through the explicit conflict lists and earlier startup checks.
- The fixture is copied to a private temporary directory; only the copied CSV is extended. Its added timestamp is1800seconds after the source's first timestamp and1500seconds after the last tank reading, matching the intended stale condition. The source already contains the negative `-2`, above-capacity `104`, pump fault feedback `2`, and legitimate flow `0` observations.
- Workbench and EOG evidence databases use the already validated temporary history directory. The isolated example URL/path points to the copied fixture; production example discovery remains unchanged outside the flag.
- Filter selections use closed ComboBox keyboard actions and later verify controller-selected identities. Scene/cutoff actions use mouse events and verify the resulting state. The probe checks unchanged sandbox document/hash/run/revision/approval plus empty analysis session/events/plans/trace/review records.
- Input events are delivered only to the bound `QQuickWindow` with `QCoreApplication::sendEvent`; they are not global OS input and cannot operate another application. Click coordinates are checked against the client. Minimum-size thumb dragging checks scrollbar interactivity and resulting visible chart geometry. Actual clipping/reachability still needs rendering validation.
- The probe has a45second internal deadline and75second CTest timeout. These are bounded checks, not measured timing results; cold software-renderer/ASan adequacy remains unverified.

## Attention

No unresolved authority/isolation source finding remains after these corrections. A nonblocking failure-evidence wording issue was reported to the parent: `finish(false)` currently labels analysis as `not_requested` even if unexpected analysis caused the probe to fail. Qualifying that field as probe intent or marking failed-result state explicitly avoids overstating failed evidence. The zero-activity claims of a passing result are enforced by the source assertions.

No compilation or runtime probe was executed by this reviewer. Builds remain pending the resource/cleanup gate at this checkpoint. Proposed assertions and screenshot filenames are not passing test evidence. This is a bounded same-family source review, not human GitHub approval or a full transcript audit. Runtime layout, all three actual client sizes, real control activation, source-input immutability and normal/ASan timing still require execution.
