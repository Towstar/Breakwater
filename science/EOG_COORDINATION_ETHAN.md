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
  on `codex/person-a-eog-detection`; PR https://github.com/Towstar/Breakwater/pull/38 .
- Contract coordination: https://github.com/Towstar/Breakwater/pull/33
- Numerical handoff: `science/EOG_E1_HANDOFF.md`.
- Numerical contract review: `science/EOG_CONTRACT_REVIEW.md`.
- Sources/evidence: `science/evidence/eog-e1/` and
  `science/evidence/eog-contract-review/`.

The preparation commit did not activate a goal. Ethan explicitly invoked this file
on November 8; Goal mode is now active. This is not an executed detector, optimizer,
simulation UI or integrated acceptance report.

## Active execution checkpoint: independent response validation

Current branch/worktree: `codex/person-a-eog-validation` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-validation`, based on PR #40
head `6581259f46edec8b8f9099c5fb815222cb625a53`. Initial source is 20489a6;
corrected runtime/native source **a404df7**, qualification driver **7dceb86**.

`science/eog_plan_validator.py` independently reconstructs causal failures/onsets,
productive wells, horizons, inventory and economics from Model/View. It does not
import/call the detector, response compiler, native bridge/evaluator or files.
It checks truck calendars pairwise and recomputes binary64 primitives using exact
Fraction arithmetic followed by explicit rounding. Actual `validate_plan` workers
run without a configured DLL. Worker is now **0.2.1**.

An exact-rational boundary probe found positive loss underflowing to zero in native
0.1.0 and the initial reference. Both are corrected to return unavailable; native
method is now **eog-native-seconds-0.1.1**, same private C ABI. Method-based numerical
results/approvals must be retired. Old ordinary plans may be independently
recomputed, but the zero-cost corner is not certified. Counterexample and earlier
logs remain retained; see `science/EOG_VALIDATION.md` and `science/evidence/eog-validation/`.

Corrected evidence: **354/354** full CPU science tests, zero skips; normal and ASan
native **11/11** CTests each; **6,768** native assertions per mode. Checked native
cross-check/validator **20/20**, worker **18/18**, bridge **7/7**, zero skips.
360 generated candidates plus joint/between-frame cases match exact native results.
Four fresh native evaluations, four independent validations and four one-ULP forged
deadline rejections execute through actual processes. Nine current official-prefix
failures match exact costs/outcomes/crossings; fresh native 0.1.1 also matches frozen
0.1.0 complete ordinary trajectories. Private per-well/mass/resource audits retained.

Next: bounded joint search with known optima, shared fleet and sequential jobs;
then evolving simulation/replay/long-run ledgers and joined application acceptance.
Search remains explicitly unavailable. Inspection timing and physical-onset
uncertainty remain unqualified. No public contract, desktop or root CI changes.

B-owned CI/runtime request:
https://github.com/Towstar/Breakwater/pull/40#issuecomment-6721439899 .
Native method correction handoff:
https://github.com/Towstar/Breakwater/pull/40#issuecomment-6721477295 .
PR #40 exact 6581259 run **42478872117** ended with native normal/ASan, desktop
normal and Lean passing; science failed on three unexpected EOG dependency skips
(0 test failures/errors; four additional permitted PhysicsNeMo skips). Desktop
ASan job **128726634345** failed stream timeout180.56 s / lifecycle77.28 s and
skipped both dependent replay smokes (37/41 executed successfully). These are
integration failures, not a passing matrix. Lucas owns the prerequisite/build/env
and telemetry follow-up integration. Keep the skip gate; configure real execution.

PR #37 exact 59ff937 run **42477239668** completed all six jobs successfully.
A notified Lucas at
https://github.com/Towstar/Breakwater/pull/37#issuecomment-6721326452 .
That closes its watched run, not whole EOG acceptance or automatic branch adoption.

## Previous execution checkpoint: native response provider

Current A branch/worktree is `codex/person-a-eog-response` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-response`, stacked on PR #38
head 70f7c8e. Published PR: https://github.com/Towstar/Breakwater/pull/40 ; measured
delivery head `51430b6e9aa4772ea784d547dace6be2a6bbcab2` before this pointer update.
Native source freeze is `fe0da88`; final provider/reference mapping
and qualification harness are `a41f1226fb2329ae14e5de5626ddc33488fc6294`.
Native/science implementation only; no B-owned contract/desktop/root edits.

The Qt/Python-independent native DLL evaluates complete failed-pump responses in
elapsed seconds, checking exact deadline/trip/travel/load/cost claims, shared fleet
occupancy and full inventory. It retains automatic shut-in as an admissible costed
option and never restarts wells after a late pickup. The separate one-shot science
provider implements protected `detect` / `evaluate_plan` against adopted 72cadd1.
It verifies captured bytes twice across the boundary, uses dataset time, and
validates responses against the actual request. See `science/EOG_RESPONSE.md`.

The real official-prefix exercise reproduced duplicate private evidence references
that made public detect responses unavailable. Worker 0.1.1 maps references uniquely;
the initial failed responses and explicit ASan DLL dependency-load failure are
retained under `science/evidence/eog-response/`. MSVC ASan runtime must be staged
beside the checked DLL for Python's restricted dependency search.

Final evidence: **330/330** full CPU science tests, zero skips; normal and native
ASan **11/11** CTests each; **6,766** native response assertions per mode including
1,200 independent randomized no-action/well-stop oracles. The initial checked DLL
passed 143 EOG Python methods; the final corrected worker passed 14/14 with that
checked DLL. Four official causal prefixes completed all eight actual worker
requests (four detect/four no-action evaluation), covering nine active failures.
Computed plan costs are not independently validated or minimum-cost claims.
Inspection rule timing is unchanged and remains unqualified.

B handoff and concrete runtime/CI/packaging requests are recorded at
https://github.com/Towstar/Breakwater/pull/40#issuecomment-6721291263 .
The integration/Lean update is
https://github.com/Towstar/Breakwater/pull/33#issuecomment-6721292002 .
Newly discovered B console PR #39 is open at
`989f83024a04065f680c197b9853f367374c0f54`, stacked on #33. It implements a
read-only dataset overview/trends alongside the retained editor; its process,
capture/history/model integration is separate and no fresh full acceptance is
implied. There were no PR #39 comments/requests at the latest inspection.

Next A work: separately coded numerical/resource validator and bounded joint search,
then evolving simulation/replay and extended ledgers. Search/validation explicitly
return unavailable in this revision. The initial provider requires the complete
current failure set; subset contexts/prior commitments remain unavailable. Existing
inspection timing/recovery gaps remain unqualified. Lucas owns application process,
DTO/parser/model/history/UI/packaging integration, atomic desktop capture producer,
CI runtime configuration and shared `docs/STATUS.md`; A owns the science provider.

## Previous execution checkpoint: causal detector and real-input evidence

Current A branch/worktree: `codex/person-a-eog-detection` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-detection`. Base is PR #36's
`de001c4`, with merged G2 main `1713cc4` incorporated by normal merge `4cb16bb`.
Current detector source is `78f1afe` after rule freezes at 37d8e77 / 2ba9b06.
Only A-owned science files are authored; incoming desktop/root changes are the
already-merged G2 baseline, not an ownership transfer.
Publication head: `391290bb7ff14d245551ad469600a980e1b2d051` before this recovery
pointer update. A34/A36 were updated with G2 main and the child merged their
published ancestry, so PR #38's diff stays exclusively within science/.

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

## Hosted integration check checkpoint

The bounded Lean retry **128717105524 completed successfully** in run 42472091130.
The original setup failure remains retained; do not repeat the earlier "live" state.
PR #33 remains open at f4e4c0f; exact numerical interface adoption remains 72cadd1.
PR #37 advanced to **59ff937e110467367f106ec88636ab095da89a4b**. Lucas documents
a finite 360 s ASan stream CTest allowance after a 37/38 hosted desktop result,
with unchanged operational deadlines/stress counts. A has not adopted/reviewed
that source or claimed its new hosted run green. Recheck the current head/run.

At the latest inspection, PR #37 run **42477239668** had five successful jobs and
desktop ASan **128721529622 still running**. PR #38 exact 70f7c8e run
**42476021181** completed with five successful jobs and desktop ASan failure:
stream timeout, lifecycle 73.64 s failure and both dependent replay smokes not run.
Do not call either complete matrix green from their partial passing jobs.

PR #33 exact integration head f4e4c0f run
https://github.com/Towstar/Breakwater/actions/runs/42472091130 is not green.
Original Lean job 128709714117 failed at root CI line 196 downloading Lean from
GitHub's error page, before proofs ran. A requested one bounded retry through the
job connector; latest Lean job **128717105524 is live in build/audit** at the last
inspection. Poll this specific run/job; do not restart because a first job-list
lookup temporarily returned empty. No proof pass has been claimed.

Original desktop ASan job 128709714451 timed out telemetry stream at 180.06 seconds,
failed lifecycle at 58.17 seconds and skipped its two dependent replay smokes.
EOG native/Python comparisons passed in that same job. The latest visible desktop
ASan job 128717106747 also has a failure conclusion. B owns its root/desktop fix;
PR #37 is a separate stream-validation optimization, not yet silently adopted.
Detailed request: https://github.com/Towstar/Breakwater/pull/33#issuecomment-6720915421 .
Native normal/ASan, desktop normal and CPU science jobs have passing conclusions
for the inspected integration run; this does not make its failed matrix a pass.
PR #38 head a157fb6 had Windows run **42475771332 in progress** at inspection.
Recheck current heads/runs instead of treating this checkpoint as live state.

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
