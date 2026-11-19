# v0.3.0 implementation ledger

Approved scope, November 18, 2026: one expanded release, delivered in small tested
increments. Lucas's desktop/integration work and Ethan's numerical work are both
authorized in this development session. This does not substitute for either
author's review or licensing agreement.

## Baseline and preservation

- Implementation branch: `codex/breakwater-next-foundation`, based on `0162656`.
- Main was `3116b3d` at the initial GitHub check; PR58 had no formal review.
- Keep the existing PR58–PR67 ancestry and dated evidence. Do not merge main,
  add a root license, tag or publish binaries before the release gates below.
- The four-file observed-capture prototype from
  `codex/person-b-observed-capture` was copied to an ignored patch with source-file
  checksums. Its original worktree is untouched. Porting requires new tests.
- Storage cleanup has an exact inventory and byte-checked archive references in
  `out/v03/`. The user approved that inventory. Record actual cleanup results;
  require 8 GiB free before full builds, qualification or packaging.

## Ordered increments

1. **Nemotron completion reliability.** Reuse only exact, current, completed
   independent validation results. Retain the original request reference, count
   reused calls separately, preserve model-selected order and strict grounding.
   Reserve one bounded format-only final correction, with no extra tool allowance.
2. **Native observation mapping.** Convert/calibrate supported measurements,
   propagate declared absolute uncertainty, require coherent coverage and freeze
   immutable state. Preserve raw negatives/overrange values. Unsupported or stale
   mappings are unavailable. Measurements never become commands.
3. **Local acquisition and replay.** Add a seeded stdlib Python publisher, generic
   renamed topology, independent fault truth and versioned receiver-timed local
   recording. Keep G1 frames/commands and old NDJSON replay readable. Enforce
   existing ingress, reorder, trace and recording bounds. No publisher actuation.
4. **Detection and observed forecasts.** Compute pre-assimilation residuals over
   a declared 30-second window with measurement, calibration, timing, quadrature
   and arithmetic bounds. Use three exceedances to activate, five below half-bound
   to clear. Keep acknowledgement separate. Freeze native forecasts and review
   advisory plans without changing Sandbox controls. Add origin-aware model tools.
5. **Scientific evidence.** Run Science manually on a selected immutable stored
   candidate. Automatically retain native pipe results with source run, parameters,
   method and diagnostics. Add bounded version-2 history envelopes and migration,
   while preserving project-v1/v2, G1, worker, pipe-result-v1 and EOG formats.
6. **Targeted utilitarian polish.** Add origin/as-of/expiry cues, readable charts,
   dense-network label priorities, selection/status cues, onboarding and keyboard
   access. Retain the current industrial scene. Show each verified UI increment
   and finish with only the latest verified app instance open.
7. **Qualification and delivery.** Expand independent calibration, observation,
   detector, uncertainty and transport reference cases. Preserve the production
   `fvm-upwind-euler-0.1.0` method and its documented accuracy limits. Compare
   EWMA/CUSUM offline; do not promote the private research comparator.

## Acceptance evidence

- Run meaningful focused checks for each increment, then combined normal and
  ASan desktop/native suites, CPU science and pinned Lean audits on the final
  source. Report actual skips and optional scientific checks separately.
- Cover malformed values, missing/conflicting coverage, negative/overflow
  evidence, source replacement, generation retirement, cancellation, replay,
  layout-only edits, immutable candidate selection and historical approval denial.
- Detector qualification: two topologies, ten held-out seeds and one healthy hour
  per seed/topology. Report false physical events and coverage failures separately.
  Assess supported pump loss within ten seconds and observable residual faults
  within sixty seconds, recording both physical onset and observable onset.
- Resource budgets remain explicit: 256 sensors/batch readings, 256 KiB frames,
  16-batch/4 MiB reorder queue, 32 MiB/32,768-reading recent trace. Pipe retention
  stops visibly at 256 records or 256 MiB instead of deleting earlier evidence.
- Final hosted evidence: protocol smoke, three canonical EOG sessions, one renamed
  network, one observed pump fault and one missing-coverage no-plan case. Preserve
  every attempt. The previous candidate's 3/4 rehearsal result remains unchanged.
- Verify actual native rendering, keyboard navigation and real Windows display
  scaling. Measure the default and 128-asset/256-route scenes; target 60/30 FPS on
  this laptop, with all bodies pickable. Do not apply this FPS target to VM WARP.

## External release gates

- Ethan's formal PR58 review and explicit GPLv3 agreement, plus final notices and
  corresponding-source delivery review. Software tests cannot supply consent.
- Actual clean Windows VM/account acceptance without SDK, Python or a model key,
  including offline startup, Unicode paths, restart/cancel/review boundaries and
  100/125/150/200 percent display scaling. A staged local bundle is not VM evidence.
- Final successful hosted qualification and current-source CI.

Keep the public release pending until these gates have evidence. Existing EOG
recorded analysis and Sandbox simulation remain separate. All monitoring is
synthetic/local; no field accuracy, automatic actuation or whole-app proof claim
is authorized by this work.

## Execution status

- Preservation and read-only storage audit completed.
- Exact cleanup approved and completed: 54,599 listed files, 23.225 GiB logical
  bytes removed, zero errors and 23.958 GiB free immediately afterward. The full
  inventory, original review and result are retained under `out/v03/`.
- Nemotron reliability implementation passes the focused normal and ASan checks:
  68 investigator, 54 controller and 62 transport Qt cases in each mode, plus the
  seven CLI cases. The normal CLI hard-link case needed an unrestricted rerun.
  No new hosted result is claimed. See `evidence/v03/nemotron-reliability.json`.
- Native observation mathematics and receiver-thread immutable captures pass six
  focused CTest suites in normal and ASan builds. The final capture suite contains
  nine Qt cases. The numerical mapper is connected to the capture seam, while
  forecast/review UI and ongoing detection are still pending. See
  `evidence/v03/observed-mapping.json` and `OBSERVED_STATE.md`.
- Local acquisition/replay and the capture UI pass normal and ASan checks. Actual
  Python subprocess integration exercises pump loss, lifecycle acknowledgements,
  restart retirement and timed export/replay. Four window-size UI checks pass in
  both modes. Live inspection captured ten samples with zero mapping issues and
  left only the newest app open. A reproduced CPython stdin-lock shutdown failure
  was corrected and retained as regression evidence. PR68 and PR69 Windows CI
  passed. See `LOCAL_MONITORING.md` and `evidence/v03/local-acquisition.json`.
- Detection, forecasts/review, history/scientific workflow and final qualification
  remain pending implementation and verification.
