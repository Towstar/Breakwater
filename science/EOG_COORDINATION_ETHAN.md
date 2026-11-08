# EOG coordination — Ethan / Person A

Prepared November 8, 2026, America/Chicago. This is a recoverable handoff, not a
live status service. Recheck GitHub and exact commits before acting.

## Objective and durable pointers

Ethan requested a Goal-mode continuation that establishes HackUTD conformance
across the complete application, adds recorded/counterfactual/extended simulation,
services Lucas's GitHub requests, verifies other features whenever work is waiting,
and performs the UI overhaul last.

- Goal instructions: `science/GOAL_HACKUTD_ALIGNMENT.md`.
- A branch/worktree: `codex/person-a-eog-analysis`,
  `C:\Projects\Breakwater\.worktrees\person-a-eog-analysis`.
- A increment: https://github.com/Towstar/Breakwater/pull/34
- Current causal observation increment: https://github.com/Towstar/Breakwater/pull/36
- Current detection worktree: `C:\Projects\Breakwater\.worktrees\person-a-eog-detection`
  on `codex/person-a-eog-detection`; its separate PR is published with this checkpoint.
- Contract coordination: https://github.com/Towstar/Breakwater/pull/33
- Numerical handoff: `science/EOG_E1_HANDOFF.md`.
- Numerical contract review: `science/EOG_CONTRACT_REVIEW.md`.
- Sources/evidence: `science/evidence/eog-e1/` and
  `science/evidence/eog-contract-review/`.

The preparation commit did not activate a goal. Ethan explicitly invoked this file
on November 8; Goal mode is now active. This is not an executed detector, optimizer,
simulation UI or integrated acceptance report.

## Active execution checkpoint: causal detector and real-input evidence

Current A branch/worktree: `codex/person-a-eog-detection` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-detection`. Base is PR #36's
`de001c4`, with merged G2 main `1713cc4` incorporated by normal merge `4cb16bb`.
Current detector source is `78f1afe` after rule freezes at 37d8e77 / 2ba9b06.
Only A-owned science files are authored; incoming desktop/root changes are the
already-merged G2 baseline, not an ownership transfer.

Implemented strict fixed-file loading, immutable EOG configuration, causal event
lifecycles/countdowns/exposure, an offline CLI, and a separately invoked offline
label evaluator. No public contracts/desktop files are authored. Public wire
binding, intervention evaluation/search/independent plan validation and application
simulation remain subsequent work.

**Shared interface gate is now satisfied:** both adopt exact
`72cadd1de7eed6666a60faff1d296e29c8f026ee`. Lucas records this at f4e4c0f in
`docs/EOG_COORDINATION.md`; verified wire/numerical files are unchanged. A's
acknowledgement: https://github.com/Towstar/Breakwater/pull/33#issuecomment-6720661497 .
Use that exact interface; do not reopen historical R1/R2/R3 absent new evidence.

Actual evidence: **113/113** stdlib EOG methods; **299/299** full science tests;
393,120 official rows/195 channels loaded; independent raw-input oracle **9/9**
current-failure countdown/repair/exposure cases. Full-week event presence matches
all 23 labels with no unmatched/early alarms. **Timeliness remains unqualified:**
inspection emission takes 5-16 hours, and three of four correlated historical
presence/lifecycle cuts fail. Missing leak/uncertain recovery cases, initial v0.1
misses and generated-adversary failures remain retained. Do not report these as
full EOG accuracy, timely inspection or installed application acceptance.

Details: `science/EOG_DETECTION.md`, `science/EOG_DETECTION_RESULTS.md` and
`science/evidence/eog-detection/`. Raw labels are outside the runtime input folder
and are read only by the separately invoked evaluator. Inspection physical-onset
bounds are unavailable; threshold-time estimates are not confidence intervals.

Next: inspect Lucas's latest integration state and requests; bind protected captures
to the accepted provider protocol, implement response evaluation in elapsed seconds,
then joint search/independent validation. Continue inspection latency/uncertainty
qualification without hidden-label/endpoint leakage. Lucas integrates PR #34/#36
and the new small numerical increment, and owns desktop/process/model/history/UI
and shared status. Whole-application/replay/extended simulation/UI gates stay open.

## Historical observation increment checkpoint

Current A branch: `codex/person-a-eog-observations`, worktree
`C:\Projects\Breakwater\.worktrees\person-a-eog-observations`, based on the
published Person A review checkpoint `c526da1`. The EOG contract review remains
in PR #34; this separate increment changes only `science/`.

Implemented `science/eog_observations.py`, a bounded immutable index with causal
latest/series/cohort/window queries, explicit freshness/skew/gaps, preserved raw
finite evidence and a private causal fingerprint. No file parser, detector,
provider wire mapping, planner or application simulation is claimed. The exact
public numerical contract acceptance remains 72cadd1 as recorded below.

Current method is `eog-observations-0.1.1`. The PR #36 follow-up separates actual
first/last acquisitions from an explicit verified capture cutoff, so a cursor at
450 s with last captured acquisition at 300 s is supported with the true 150 s age.
It rejects supplied rows beyond that cutoff. Original source dataset-now still
belongs to the separately verified manifest; no reading or clock is fabricated.

Actual final checks: **36/36** focused tests on Python 3.12.13 with `-S` (site packages
disabled); **222/222** complete science tests in the pinned environment, no skips.
The initial missing-API capture-gap failure and earlier 33/219 passes are retained.
Four existing actual PhysicsNeMo CPU tests executed; the CUDA-driver warning is
retained, not represented as GPU execution or a new learned-quality qualification.
Evidence, source identities, commands and limits: `science/evidence/eog-observations/`.
The initial post-test base fast-forward added only the published review/docs/evidence
and standalone review script; the final capture-gap checks ran after that update.

Next: inspect Lucas's latest PR #33/main/coordination state, obtain reciprocal exact
72cadd1 adoption/integration, then implement causal event detection/lifecycle and
file-to-provider binding in small tested increments. Keep immutable observations
separate from model settings and future synthetic truth. Do not repeat E1 or the
closed numerical review. Full application/replay/planning/simulation/UI acceptance
remains open in the requirements matrix; Goal mode stays active.

## Numerical contract acceptance checkpoint

Ethan accepts exact PR #33 contract `72cadd1de7eed6666a60faff1d296e29c8f026ee` after
independent normal/ASan **2/2** CTests per mode, **70 Qt + 48 Python** cases per
mode without skips, eleven review probes and independent arithmetic. R1/R2/R3
and the recorded outcome/cost/repair semantics are closed for this exact revision.
See the latest `science/EOG_CONTRACT_REVIEW.md` and source/binary-bound evidence in
`science/evidence/eog-contract-review-72cadd1/`. Await Lucas's reciprocal exact
adoption/integration record before binding the provider/client. No production
detector, plan validator or EOG application acceptance is implied.

The next A increment was isolated on `codex/person-a-eog-observations` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-observations`, based on `94060df`.
It adds a private pure observation index, not an unreviewed public protocol.
Continue there after inspecting its status and latest GitHub coordination. Lucas
owns the reciprocal note and main integration. A's published review checkpoint
was fast-forwarded into that branch before this delivery.

## Historical first execution checkpoint

- A execution base: `c194bd59a20a1f50008236a54f72bc6f3e975884`, clean at startup.
- Remote main rechecked: `2bd7bb9e13f1f6788a57c4c49e6d7570293ad0eb`.
- Upstream official main rechecked unchanged at `135ba4c8348357457420a1111df80ec1906bcbdb`;
  README, SPEC, examples and history guide reviewed. No published judging folder.
- Exact PR #33 `e3190fbfa73d5321adb8e1180a783a881001a919`: **40/40** contract
  tests passed in a clean isolated detached checkout; R2 endpoint fix verified.
  R1 empty/short forecast and R3 fractional-event representation remain open.
- Numerical adoption remains **none**. Outcome/repair/cost semantics need Lucas's
  explicit examples/decision. No provider or desktop binding was added.
- New requirements matrix: `science/EOG_REQUIREMENTS_MATRIX.md`; scoped evidence
  and command details: `science/evidence/eog-contract-review-e3190fb/`.
- Startup coordination posted on PR #33:
  https://github.com/Towstar/Breakwater/pull/33#issuecomment-6720226295 .
- Next independent increment: pure causal observation/detection core and generated
  fixtures, without consuming an unadopted wire representation. Lucas owns the
  contract fixes, reciprocal coordination note, application path and shared status.

## Current preparation snapshot

| Item | Observed state / next action |
|---|---|
| Official challenge | Last checked main `135ba4c8348357457420a1111df80ec1906bcbdb`; pin and recheck |
| Person A PR #34 | Head before this documentation increment: `99c1d419f5f2d8509cf2f1316ea5f4331afe08a2`; analytical foundation and review evidence |
| Local numerical evidence | 42/42 EOG cases, 550 randomized oracle checks; normal and native ASan 10/10 CTest suites; joint DTO/header compile passed |
| Contract PR #33 | Draft; last inspected head `e3190fbfa73d5321adb8e1180a783a881001a919`, advanced from reviewed `e8dab471` |
| Contract adoption | Pending a current exact-revision numerical review; the historical 37/37 tests are evidence for reviewed `e8dab471` only |
| PR #32 | Separate draft source-allocation compatibility work; do not bundle or downgrade project-v3 allocations into the v2 telemetry wrapper |
| PR #30 | Separate experimental transport study, with its existing accuracy limitations |

## Request ledger

| Request / source | Owner | Disposition / next action |
|---|---|---|
| Review current EOG boundary, PR #33 | Ethan | Closed: both adopt exact 72cadd1; Lucas records it at f4e4c0f, A acknowledged on PR #33 |
| Complete forecast coverage / cutoff-only result rejection, PR #33 review R1 | Lucas, Ethan verifies | Closed at 72cadd1: empty/short/missing-facility and one-ULP-short probes reject |
| Actual CSV end must agree with untruncated snapshot-now, PR #33 review R2 | Lucas, Ethan verifies | Fixed at e3190fb; independently reverified at 72cadd1 |
| Fractional counterfactual events, do-nothing feasibility, cost availability, repair ordering/deadlines | Joint, Lucas publishes | Closed for exact 72cadd1 representation and declared semantics; provider still must execute/check them |
| Causal provider, independent validator and joint search | Ethan | Planned after exact contract adoption; existing E1 functions are private analytical models |
| Recorded replay and longer scenario simulation | Ethan numerical core; Lucas application | New user requirement in goal prompt; coordinate versioned inputs/results before UI binding |
| UI overhaul | Lucas; Ethan audits numerical presentation | Last product priority after correctness, simulation and integrated workflows |

For a new request, record its URL, author/owner, requested revision, acceptance
condition, status, implementation/evidence commit and next dependency. Reply in
GitHub when fulfilled; do not let stale "pending" entries override newer evidence.

## Reciprocal coordination request for Lucas's Codex

Please maintain a B-owned counterpart (for example `docs/EOG_COORDINATION_LUCAS.md`)
and post its stable link in the relevant PR conversation. Include the current
branch/head, adopted contract, active changes/owned paths, concrete requests for A,
test evidence, blockers and next integration action. Inspect this A note and PR #34
before assigning overlapping work. Ask Ethan's Codex to do the same as new requests
arrive. Both agents should leave recoverable GitHub pointers after meaningful
increments and should read each other's latest notes after compaction/restart.

Ownership remains unchanged. Request and acknowledge a specific handoff before
editing another track's files. Coordinate through small PRs; Lucas integrates main.
