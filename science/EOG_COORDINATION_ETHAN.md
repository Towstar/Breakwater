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

## Active execution checkpoint: causal known truck commitments

Current branch/worktree: `codex/person-a-eog-commitments`,
`C:\Projects\Breakwater\.worktrees\person-a-eog-commitments`, stacked on #51.
Frozen runtime **621c28de7e67af89ab6474f558763da3523a17b5**. New private
commitment method **0.1.0**; existing observation/noise and native plant methods
remain **0.1.0**. Public EOG stays exact **72cadd1**; no worker operation/wire or
native algorithm is changed. Details: `science/EOG_COMMITMENTS.md`.

`SyntheticStream.control_history()` emits immutable accepted facts only, excluding
seed, future schedules, full scenario identity and rejected diagnostics. Pure
`reconstruct(Model, View, History)` independently rebuilds pickups/final-return
reservations, with exact model/catalog/cutoff binding and fractional clocks. No
native/file/label access, repair-based release or silently free unknown fleet.
Scheduled pickups do not establish actual removal, cost, feasibility or authority.
The caller must authenticate a complete live prefix; reopened numerical evidence
cannot grant operating authority. Verified strictly older prefixes survive later
cancellation, while unsupported current history remains unavailable.

Actual full science **439/439**, no skips/failures; focused **14/14** normal and
ASan. Thirty-two varied scenarios compare 64 accepted dispatches / 128 snapshots /
256 truck states, and two real systems keep equal truck ordinals independent.
Ten-stage actual normal/checked runs produce 12 byte-identical complete payloads,
including controls, observations, reconstructed/native calendars and host receipt.
Initial normal-off fixture and real historical-cancellation failures are retained.
Evidence: `science/evidence/eog-commitments/` with pinned source/binary/payloads.

B staging needs `eog_commitments.py` and changed `eog_synthetic.py` plus existing
plant bridge/runtime. The shared additive simulation boundary remains unadopted;
it must separate fractional decision clock from integer observation cutoff and
authenticate live accepted history. Do not send the full host receipt to models.
The existing plant CI target/env request on #49 remains needed; no B-owned path
was edited. Next A step is occupied-slot/committed-pickup evaluator context and
independent validation, then stateful planning. Application replay/what-if/mode
controls and final installed/model/workbench/UI checks remain in the full goal.

## Published checkpoint: search arithmetic robustness

Current worktree/branch:
`C:\Projects\Breakwater\.worktrees\person-a-eog-search-robustness`,
`codex/person-a-eog-search-robustness`, stacked on #50. Frozen runtime
**343d7e93d9aafae329ab3815dccf4ed275832d95**, search **0.1.3**, worker **0.3.4**;
detector remains **0.3.0**, native **0.1.1** and exact public **72cadd1** unchanged.

Reproduced finite `1e308` truck-capacity overflow in three seed helpers, direct
native search and a real protected worker. Integer-count load products now stay
rational before the existing rational seed relaxation. Native inventory/cost
checks are unchanged: impossible full loads still reject, and unrepresentable
candidate costs remain unavailable and prevent a complete enumeration claim.

Actual full CPU science **425/425**, no skips/failures; seven new focused methods
pass normal **3.026s** and with the inherited instrumented DLL **4.777s**. Seven
known-value cases and twelve real search/fresh-native/no-DLL processes pass.
All four official selected exact costs and plan hashes match the earlier 0.1.2
record, covering nine failures and retaining best_found. Evidence is pinned in
`science/evidence/eog-search-robustness/`; no installed/UI/hosted result is implied.

B adoption needs changed `eog_search.py` / `eog_worker.py`, with no new module name
or wire field. #48's documented finite-extreme limitation now has a reproduced
A-owned fix. Adopt its exact source before updating packaged runtime evidence;
do not relabel version-1 operation fields as worker implementation attestation.

Next A groundwork: project only causally accepted operator controls, excluding
seed/future forcing, and independently reconstruct per-truck pickups/return
commitments with fractional elapsed clocks. Bind this to imported Model and
causal View; do not pass the full synthetic provenance receipt to a planner or
reuse recorded-now search as if busy trucks were free. Stateful planning and the
shared B-owned simulation boundary, replay/what-if controls and final joined
application/model/installed checks remain required. The full goal stays active.

## Published checkpoint: inspection timeliness

Current worktree/branch: `C:\Projects\Breakwater\.worktrees\person-a-eog-inspection`,
`codex/person-a-eog-inspection`, stacked on #49. Frozen runtime **95a8bf1**,
detector **0.3.0**, worker **0.3.3**, unchanged public **72cadd1**. No B-owned file
or native numerical algorithm is edited. Exact evidence is in
`science/evidence/eog-inspection/`, with frozen declarations/failed attempts in
`EOG_INSPECTION_STUDY.md` and measured limits in `EOG_DETECTION_RESULTS.md`.

The retained 5% path is supplemented by a bounded running-observation change
score. Coherent observed off intervals preserve continuity without supplying
discrepancy/recovery evidence; missing/fault/contradictory frames reset it. Old
score expires after six hours. Cumulative recovery requires four actual healthy
running samples and discards old positive evidence. Sensor-bias/physical-loss
indistinguishability and unbounded physical onset are explicit.

Actual final science **418/418**, zero skips/failures; focused **18/18**. Fresh
generated cases and retained development families pass the declared four-hour
3% loss bound, with no alarms in 9,856 healthy-only synthetic hours. Official
practice keeps 23/23 matches/no unmatched/early alarms: leak delay improves to
1h25m-3h10m, wear to 5h30m-6h05m. The first historical gate now passes; two later
gates still fail for one ambiguous unrecovered inspection each. Recovery is not
uniformly improved; the paired report preserves one additional 15-minute leak
lag. Nine independent raw countdown/repair/revenue checks pass, plus four actual
detect/four native no-action worker processes covering nine current failures.
This is not global, held-out, physical or final installed/UI qualification.

B integration: adopt the exact new A source before staging worker 0.3.3; existing
module names/public schemas remain unchanged. PR #48 is still draft, latest
inspected head **05f906f** (Windows fixture line-ending normalization only).
PR #49's exact-head science job **128780171556**, run **42496462808**, fails on
two unexpected class skips because `BREAKWATER_EOG_PLANT` is absent; the concrete
build-target/environment request is recorded on #49. Native normal/ASan and Lean
passed; desktop jobs were live at the preceding observation. Do not reclassify
that hosted run as passing or retry unchanged prerequisites.

Next: finite-extreme search arithmetic robustness, the exact B-owned simulation
boundary/adoption, commitment-aware planning and final joined application checks.
Inspection timeliness/physical onset remain limited; the full goal stays active.

## Published checkpoint: imported-model causal synthetic observations

Current branch/worktree: `codex/person-a-eog-synthetic-observations` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-synthetic-observations`. Runtime
source **78338aa**, initial ABI/mapping d0c15f9; complete offline driver **1947638**.
Latest accepted UI main **7418a2d** is merged as **be31979**. A-owned tested source
is unchanged by that merge; the only root numerical-build-area change is B's
Visual Studio tracking location, outside these Ninja checks. Public gate remains
exact 72cadd1; native plant remains 0.1.0, private computational BWES ABI 1.

The imported Model maps to a real persistent native plant through a checked C ABI,
with explicit initialization, seeded schedules and 300-second causal projection.
Future truth/seed/full receipt never enter `view()` or detector execution. Raw
negative/noisy values and dropped-frame age/gaps are preserved. Bounded fractional
control journals reproduce physical state/cost/fleet and observations; reset clears
them. Unsupported future oil/gas changes reject rather than becoming unobserved
economic assumptions. Details: `science/EOG_SYNTHETIC_OBSERVATIONS.md`.

Final full science: **399/399**, zero failures/skips. Native normal/ASan: **13/13**
each, 28 ABI and 57,498 synthetic checks. Checked Python: bridge 8/8 + final
synthetic/control 14/14. Actual one-day official-model/unfamiliar-model runs emit
56,355 / 11,271 samples, round-trip every sample/model/detector result through the
fixed loader and match normal/checked bytes. All six injected failures are found
within one acquisition interval; no extra inspection events. Original source
hashes remain unchanged. Controlled fractional replay retains busy occupancy after
repair and reproduces cost 9390.1683984375. This is explicit-control evidence, not
an optimized plan, independent physical proof or application acceptance.

B needs the new `breakwater_eog_plant` target/DLL, two Python modules and absolute
`BREAKWATER_EOG_PLANT` test configuration; keep zero-unexpected-skip policy. A has
not edited root CI, public contracts or desktop files. A-owned private numerical
input/results are ready for exact-source mapping review; a shared simulation wire
and operator controls remain B-owned and unadopted.

B's new `codex/person-b-eog-search-integration` branch includes exact a420deb A
source unchanged and now implements search client/model/history/packaging changes.
Its source-only provenance note is `docs/EOG_SEARCH_INTEGRATION.md`; its final joined
tests/installation still need evidence/review. Earlier #46 hosted stale assertion
failure is preserved below and is not treated as fixed merely from a new branch.

Next A correctness priority: inspection observability/latency and causal lifecycle
qualification, then commitment-aware planning. Keep the replay/counterfactual/
extended application and final installed/rendered/workbench acceptance in scope.
The goal remains active; do not mark complete on these native/science checks.

## Published checkpoint: native stateful synthetic plant

Current branch/worktree: `codex/person-a-eog-simulation` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-simulation`. Corrected native
source **af73a42**; latest accepted main **c1622b9** merged as **7323968** without
changing tested engine source. This adopts Lucas's merge of #45. No B-owned
desktop/contracts/root file is edited.

Private `engine/include/breakwater/eog_simulation.hpp` / `src/eog_simulation.cpp`
now implement bounded persistent pump cycling, repeated failures/repairs, explicit
rate forcing, per-well through-repair shut-ins and full-load jobs on physical shared
trucks. Final-return commitments survive repairs and later incidents. Input is
copied; snapshots contain only current truth. No seeded observation projector or
production simulation protocol is claimed. Details/limits: `engine/EOG_SIMULATION.md`.

Actual final normal/ASan: **12/12 native CTests each**, **57,498 synthetic checks**
per mode, including 100 observation-partition comparisons and 100 exact actual
E3 cost/inventory comparisons. The original 31-day cycle error (2.58757e-5 bbl)
failed and was corrected by algebraic event roots, without changing tolerance:
final error 4.98283e-7 bbl, raw mass residual -8.54726e-7 bbl. Signed event-boundary
rounding stays separately visible. Retained initial source/failures and pinned
normal/checked evidence are in `science/evidence/eog-simulation/`.

Search PR **#46** now targets main after #45 merged. Exact a420deb hosted run
42491491720 passes native normal/ASan, Lean and science. Desktop normal passes
61/62 CTests; actual Qt artifact 13622352092 proves its one failure is a stale
search-unavailable expectation at eog_provider_integration_tests.cpp:293. B's
concrete requested genuine search/review/history acceptance is recorded at:
https://github.com/Towstar/Breakwater/pull/46#issuecomment-6723113570 .
The B-owned source/assertions and installed eog_search.py staging remain B's work.

Next: map real imported Model/IDs into this private native plant, add a seeded
causal observation projector and qualify detector/planner isolation. Snapshot
search must not ignore persistent commitments. Lucas must publish the additive
application/simulation boundary before binding UI controls. B's #44 at e0b3da6
publishes new installed/render/live evidence; it requires exact-source review and
does not complete final joined search/simulation/UI acceptance. Goal stays active.

## Published checkpoint: bounded joint search

Current A branch/worktree: `codex/person-a-eog-search` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-search`. Runtime freeze **34fdf5d**,
search **eog-joint-search-0.1.2**, worker **eog-worker-0.3.2**; exact public gate
72cadd1/native method 0.1.1 stay unchanged. Source, actual process exchanges and
retained failures are in `science/evidence/eog-search/`; supported family and
limits are in `science/EOG_SEARCH.md`.

Final full science: **377/377**, zero failures/skips. Native normal/ASan: **11/11**
each, 6,768 assertions per mode; checked Python base 63/63, final affected
search/worker 38/38. Seven known-value synthetic families pass. Four actual
official-prefix searches and eight fresh native/no-DLL independent worker checks
all agree exactly on selected cost/outcome/shutdowns/hash/full horizons over nine
failures. Selected totals are 3106.4603465511495 / 1810 / 3021.9269698545995 / 1810,
all explicitly best_found. No continuous/global/physical/held-out claim.

Every candidate retains global current-failure coverage. Empty/all-system search
works; proper known subsets are explicitly unavailable pending Lucas's existing
#41 scope disposition. Default retention preserves available baseline/truck-only/
well-only/mixed leaders; only the exact selected plan is independently recomputed.
Inspection latency/onset uncertainty and evolving prior commitments remain open.

Lucas must stage new `science/eog_search.py` alongside the eight provider modules,
update worker-method identity/history handling and execute search in actual
client/model/history/installed paths. The search response includes an independent
validation ID in limitations; callers still invoke validate_plan to obtain the
complete bound review record. Public schemas are not extended or weakened.
Concrete request and current B status pointer:
https://github.com/Towstar/Breakwater/pull/44#issuecomment-6722804883 .

Next A work: stateful synthetic simulation core and long-run ledgers, keeping truth
separate from causal observation streams and persistent truck commitments.
Coordinate the additive shared simulation boundary with Lucas before application
binding; recorded replay/counterfactual UI, detector qualification, retained
workbench and final integrated/render/installed acceptance remain required.
Goal mode remains active; Lucas remains main's integrator.

## Input preservation correction on latest main

User's latest-first instruction is fulfilled: `origin/main` at `387b583` was
merged without conflicts into the ongoing search branch (`23b0251`). Latest main
includes the joined real provider and independent validation; Lucas's #44 remains
separate draft UI/model work. Search remains A's active next numerical increment.

An isolated `codex/person-a-eog-report-guard` branch from that main resolves Lucas's
offline CLI overwrite request in `docs/EOG_COORDINATION.md`. Resolved-path and
hard-link report aliases reject before any success/failure report write. Actual
14/14 input methods pass without skips, including source-byte preservation and
three real hard links; initial sandbox link denial and successful permitted rerun
are recorded in `science/EOG_REPORT_GUARD.md`. No official input was modified.
Lucas owns main integration and shared STATUS; this correction does not transfer
offline CLI packaging ownership or complete search/simulation/application goals.

## Prior execution checkpoint: independent response validation

Current branch/worktree: `codex/person-a-eog-validation` in
`C:\Projects\Breakwater\.worktrees\person-a-eog-validation`, based on PR #40
head `6581259f46edec8b8f9099c5fb815222cb625a53`. Initial source is 20489a6;
corrected runtime/native source **a404df7**, qualification driver **7dceb86**.
Published PR: https://github.com/Towstar/Breakwater/pull/41 ; measured evidence
head `a7fd503bd66906b2c48f1382073bd3da1370b8bd` before this pointer update.
Concrete B method/runtime/history requests:
https://github.com/Towstar/Breakwater/pull/41#issuecomment-6721559452 .
Provider cross-reference:
https://github.com/Towstar/Breakwater/pull/40#issuecomment-6721560056 .

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

## Historical hosted integration checkpoint (superseded above)

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

## Historical preparation snapshot

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
