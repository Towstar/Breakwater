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
- Contract coordination: https://github.com/Towstar/Breakwater/pull/33
- Numerical handoff: `science/EOG_E1_HANDOFF.md`.
- Numerical contract review: `science/EOG_CONTRACT_REVIEW.md`.
- Sources/evidence: `science/evidence/eog-e1/` and
  `science/evidence/eog-contract-review/`.

The preparation commit did not activate a goal. Ethan explicitly invoked this file
on November 8; Goal mode is now active. This is not an executed detector, optimizer,
simulation UI or integrated acceptance report.

## Active execution checkpoint: exact-contract re-review

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
| Review current EOG boundary, PR #33 | Ethan | Re-reviewed exact e3190fb; 40/40 passed; adoption pending the remaining findings |
| Complete forecast coverage / cutoff-only result rejection, PR #33 review R1 | Lucas, Ethan verifies | Reproduced again on e3190fb; still accepts both incomplete results |
| Actual CSV end must agree with untruncated snapshot-now, PR #33 review R2 | Lucas, Ethan verifies | Fixed at e3190fb; independent false-endpoint probe rejects with invalid_time |
| Fractional counterfactual events, do-nothing feasibility, cost availability, repair ordering/deadlines | Joint, Lucas publishes | e3190fb fractional event rejected with invalid_shape; explicit semantics/examples still required |
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
