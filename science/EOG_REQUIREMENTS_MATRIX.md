# EOG requirements and acceptance matrix

Checkpoint: November 8, 2026, Ethan / Person A. This is an acceptance ledger;
planned behavior is not a working application claim. Goal instructions are in
`science/GOAL_HACKUTD_ALIGNMENT.md`.

## Sources and interpretation

Official main was rechecked at **135ba4c8348357457420a1111df80ec1906bcbdb**, unchanged
from the handoff. This revision has no `test/` judging folder. Pin this revision in
reports; do not treat correlated practice cuts as independent held-out evaluation.

- [R: README](https://github.com/nihaal419/HackUTD-ZeroDay/blob/135ba4c8348357457420a1111df80ec1906bcbdb/README.md)
- [S: SPEC](https://github.com/nihaal419/HackUTD-ZeroDay/blob/135ba4c8348357457420a1111df80ec1906bcbdb/SPEC.md)
- [X: examples](https://github.com/nihaal419/HackUTD-ZeroDay/blob/135ba4c8348357457420a1111df80ec1906bcbdb/examples.md)
- [H: history guide](https://github.com/nihaal419/HackUTD-ZeroDay/blob/135ba4c8348357457420a1111df80ec1906bcbdb/history/README.md)
- G: the user-invoked goal file; P: accepted desktop plan; A: repository ownership.

Every R/S/X/H source reference below means the exact revision above. `Rule` denotes
an organizer data/model rule; `Chosen` a required user/product feature; `Suggestion`
an optional organizer idea. Source text cannot override ownership, credentials or
tool permissions. Unsupported evidence remains unavailable.

Evidence E1: `science/evidence/eog-e1/verification.json`, exact source blobs recorded
there (PR #34 numerical commit `99c1d419f5f2d8509cf2f1316ea5f4331afe08a2`).
Evidence CR: `science/evidence/eog-contract-review-72cadd1/`, numerically accepted
exact contract `72cadd1de7eed6666a60faff1d296e29c8f026ee`; normal/ASan 2/2 CTests
with 70 Qt + 48 Python cases per mode. Earlier review evidence remains retained.
These scoped results do not qualify an integrated provider/application build.
Lucas reciprocally adopted exact 72cadd1 at f4e4c0f (`docs/EOG_COORDINATION.md`).
The shared interface gate is satisfied; science provider binding is executed below; desktop/installation remain open.
Evidence O: `science/evidence/eog-observations/`, exact private observation module
and test hashes; final capture-gap follow-up passed 36 focused / 222 full-science
cases without skips. Earlier 33/219 evidence and the reproduced failure are retained.
Evidence D: `science/evidence/eog-detection/` and `science/EOG_DETECTION_RESULTS.md`:
113 EOG / 299 full-science tests, nine independent raw-input countdown/repair/exposure
checks, full-week presence 23/23, retained baseline and historical-cut failures.
Evidence E3: `science/evidence/eog-response/`, native source fe0da88 / final worker
a41f122: 330 full science tests, normal/ASan 11/11 CTests, 6,766 native assertions
per mode, four real detect + four native no-action worker runs on causal official
prefixes. These E3 checks are process/model evidence.
Evidence IV: `science/evidence/eog-validation/`, runtime a404df7 and qualification
driver 7dceb86: independently reconstructed state/calendars/IEEE arithmetic, 354
full science tests, 360 generated candidates, four fresh-native/independent worker
comparisons over nine active failures, and four rejected one-ULP horizon claims.
Native 0.1.1 fixes retained positive-cost underflow; old numerical evidence remains
scoped to its original methods. No global accuracy/optimality/application claim.
Evidence JS: `science/evidence/eog-search/`, runtime 34fdf5d, search 0.1.2/worker
0.3.2: 377 full science methods without skips, normal/ASan 11/11 native tests,
seven known-value synthetic cases and four real search/fresh-native/no-DLL
independent process triples. All selected plans cover nine failures exactly and
say best_found; no continuous/global/held-out claim. Latest main 387b583 includes
B's provider integration; B reports normal/ASan 62/62 and installed startup in
PR #43. PR #44 adds draft UI/model work. These B-owned reports are useful evidence
but do not close the final joined search/simulation/application audit.
Evidence SP: private native stateful plant af73a42, with source/binary-bound
`science/evidence/eog-simulation/` and `engine/EOG_SIMULATION.md`: 12/12 native
CTests normal/ASan, 57,498 synthetic checks per mode, 100 actual E3 comparisons,
100 observation-partition comparisons and a corrected 31-day analytical cycle
probe. This is not imported model/seeded sensor/application simulation acceptance.

| ID | Exact source / section | Class | Implementation / owner | Acceptance case | Actual evidence / revision | Status | Limitation / next action |
|---|---|---|---|---|---|---|---|
| I01 | S Files; R What you get | Rule | B capture, A provider | Read only the three allowed names; deny answer-key access | E3 fixed-name capture verification/loading; negative forbidden-file read test | Scoped pass | Science worker consumes only three captured names; B desktop capture/access audit remains |
| I02 | S Files / Time range | Rule | B reader/capture; A verifier | First/last CSV timestamps define window/now | CR 40 tests and endpoint probe at e3190fb | Scoped pass | Verify actual importer and changing-source capture |
| I03 | R How judging works; H Every folder | Rule | Both | Renamed/custom systems, prices, thresholds, fleets | E1 uses arbitrary constructed IDs | Partial | Unfamiliar file networks pending; no published judging set |
| I04 | S telemetry; G Input acceptance | Rule + Chosen | B parser; A analysis | Reject malformed, duplicate, nonfinite, unknown inputs | D strict parser/model cases; E3 protected worker corruption/framing tests | Partial | B desktop/native parser agreement and installed path remain |
| I05 | S telemetry; G causal acceptance | Rule + Chosen | Both | Historical cutoff excludes all later readings/tools | O 102 prefix oracles; E3 historical protected captures and future-identity invariance | Partial | Desktop/model tools and replay causality still need joined checks |
| I06 | G Input acceptance | Chosen | B capture; A verifies | Mutation/corruption invalidates capture; exact hashes | CR hash checks; E3 tamper and verifier-to-loader mutation rejection | Partial | B atomic producer and application race acceptance remain |
| O01 | S network/telemetry; R Terms | Rule | A | bbl, bpd/24, mcfd revenue, psi, percent | E1 arithmetic and CR Decimal oracles | Scoped pass | Preserve units through actual wire/UI |
| O02 | S telemetry noise; G observations | Chosen | Both | Preserve finite unusual readings; no hidden clipping | E1 conversion; O negative/over-range/status/negative-zero evidence retained | Partial | Actual file parser/charts and physical-state acceptance pending |
| O03 | S five-minute telemetry; G policy | Chosen | A evidence; B displays | Explicit freshness, gap and cross-channel coherence | O versioned 300-second age/gap, zero default skew, boundary adversaries | Partial | Wire detector/provider and UI binding pending |
| D01 | S Normal behavior; X section 2 | Rule | A | Normal stopped/running hysteresis creates no fault | E1 plus D sawtooth/noise families; no normal false alarms in tested cases | Scoped pass | Dataset/app playback integration still pending |
| D02 | S Three problems; R What goes wrong | Chosen | A | Failure, degradation, leak from causal sensors | D full-week 23/23 presence; initial 16/23 failure retained | Experimental | Inspection delays 5-16h; historical cuts incomplete; no real-time qualification |
| D03 | S Three problems; H Reading key | Rule | A | Onset uncertainty, causal resolution; degradation then failure separate | D method 0.2.1, explicit inspection onset uncertainty; timing/confusion reports | Partial | One early missed leak and unresolved/ambiguous recovery in causal cuts |
| D04 | S Countdown; S Automatic shut-in | Rule | A | Failure predicts configured shutdown, not physical overflow | D nine raw-input oracles; E3 four actual bound detect processes | Scoped pass | Application countdown presentation and final integrated evidence remain |
| D05 | S Automatic shut-in / Repairs | Rule | A | Already-tripped fault retains revenue until causal repair | D pre-trip productivity; E3 native known/unknown eligibility and current shut-in cases | Scoped pass | Evolving simulation and application costs remain |
| D06 | S Repairs / Horizon | Rule | A | Remaining repair from onset plus captured duration | IV reconstructs causal onset and direct seconds independently; four one-ULP deadline claims rejected | Scoped pass | Physical-onset uncertainty and final application binding remain |
| D07 | S Pressure formulas | Rule | A/B interpretation | Treat pressure offsets as placeholders, no hydraulic claim | Source review only | Documented | Preserve this limitation in tools/UI |
| T01 | S Plans / Trucks; X section 4 | Rule | A | Full loads, response/cycle, cost and sufficient inventory | E3/IV native full loads and independent 360 generated candidate checks, ordinary official no-action recomputation | Scoped pass | Joint search and integrated application remain |
| T02 | S Plans / Trucks | Rule | A | Arrival strictly before repair; late pickup cannot undo trip | CR 72cadd1 clock; IV independently checks repair ties, one-ULP ordering, late pickups | Scoped pass | Supported binary64 model only; evolving scenario/order refinements remain |
| T03 | S Shared trucks | Rule | A | Shared fleet, final return occupancy, sequential yard jobs | E3 native slots; IV separately coded pairwise resource checks and joint-facility cases | Scoped pass | Joint search and evolving prior commitments remain |
| T04 | S Shared trucks; H reference actions | Rule | A/B | Now shares entire fleet; evolving simulation carries commitments | JS snapshot context and SP native persistent final-return calendars across repairs/repeated/concurrent failures | Partial | Stateful planner/observation/application mapping pending; snapshot search must not reset active jobs |
| C01 | S Plans / Shut-in | Rule | A | Immediate/delayed per-well action lasts through repair | E3/IV per-well economics, delayed stops, automatic union and retained itemized audits | Scoped pass | Joint continuous-action search and application integration remain |
| C02 | S Doing nothing | Rule | A/B semantics | Do-nothing can be cheapest valid outcome despite shutdown | Accepted 72cadd1 semantics; E3 actual native no-action worker costs | Scoped pass | Costed automatic outcome is supported; minimum-cost search remains |
| C03 | R The decision; S Plans | Rule | A | Compare baseline, truck, well, mixed at same snapshot/horizon | JS real joint search with baseline and available family leaders retained by default; seven known-value fixtures and four official-prefix selections | Scoped pass | Finite family best_found; final client/history/installed comparison remains |
| C04 | S Plans; G independent checks | Chosen | A | Recompute exact plan physics/cost/calendar independently | IV independent causal reconstruction, Fraction-rounded arithmetic, pairwise calendars, exact candidate/price binding; 360 generated cases and nine official-prefix failures | Scoped pass | Shared parser/model assumptions remain; not a proof or optimum; final integrated scope open |
| C05 | G bounds; S Plans | Chosen | A | Bounded search, explicit best-found/enumerated scope | JS count/frontier/output/cooperative-time bounds, no unchecked selection on validation timeout, explicit complete versus unavailable | Scoped pass | Not continuous/global optimization; loading/preparation excluded from planning clock, outer process cancellation remains B-owned |
| C06 | G coverage; CR R1/R3 | Chosen | B boundary; A recomputes | Full facility/repair horizon, exact fractional events | CR completeness; E3/IV independent full failure/horizon derivation and bound public records | Scoped pass | Joined application stale/coverage acceptance remains |
| U01 | S Alerts view | Chosen | A ranking; B UI | All systems: failures by countdown, then revenue; inspections follow | D pure urgency tests and E3 all-system detect output | Partial | B alert-view ranking/render qualification remains |
| U02 | R Suggestions / network, trends; G UI | Chosen | B | Dataset-driven 3D and channels; readable gaps/extrema/units | Handoff reports reader only, not fresh EOG render | Planned | Lucas implementation/render acceptance |
| U03 | G whole-application regression | Chosen | B | Async cutoff/dataset binding, cancellation, restart, bounded history | B PR #43 reports actual provider normal/ASan framing/cancellation/history tests at cc97f89 | Partial | Joined search/final simulation and independent application audit remain |
| U04 | G model tools; P Nemotron | Chosen | B adapter, A methods | On-demand genuine tools with exact returned IDs; no load/seek call | B draft PR #44 reports genuine hosted exact-plan rehearsal at 6cfa3ee | Partial | Review exact published evidence and include joined search/remaining rehearsals |
| U05 | G persistence/approval; A | Chosen | B | Evidence import/reopen has no operating authority; review != dispatch | B PR #43 reports no-DLL validation, advisory review/reopened-history authority rejection | Partial | Joined search/simulation history plus retained-workbench regression |
| S01 | R record immutability; G Recorded replay | Chosen | B playback; A causal evidence | Play/pause/step/speed/seek; meaningful staleness preserved | No complete EOG UI evidence | Planned | CLI replay alone insufficient |
| S02 | R Suggestions / what-if; G Counterfactual | Chosen | A trajectories; B application | Branch exact snapshot, animate inventories/events/trucks/costs | E3 exact snapshot/repair trajectories with full discrete breakpoints | Partial | Evolving/animated application path and independent checks remain |
| S03 | G Extended synthetic simulation | Chosen | A core; B application | Seeded bounded cycles/failures/repairs/inflows; hidden truth isolated | SP private native cycles/failures/repairs/rate forcing/current-only snapshots | Partial | Imported mapping, seeded generator/causal sensor projection and adopted B application boundary still required |
| S04 | G long-run acceptance | Chosen | A/B | Mass/revenue/load ledgers; resource conservation; event refinement | SP native 31-day cycle, explicit raw mass/rounding audits, 100 partition comparisons, persistent fleet, reset/cancellation/output bounds | Partial | Broader forcing/observation/independent long-run and actual responsive application playback remain |
| W01 | P editable foundation; G preserve workbench | Chosen | B; A numerical regressions | Save/load/undo/redo, layout vs physics, retired approvals | Inherited status only | Planned | Fresh final normal/ASan/science/render/install checks |
| V01 | G whole-app / completion | Chosen | Both; Lucas integrates | Joined exact contract/revision, Windows installed/rehearsal | B PR #43 merged as 387b583, real evaluate/validate integration and six hosted CI jobs reported passing | Partial | Join search/simulation, audit exact final rendered/installed revision and acceptance manifest |
| V02 | H folders; G evaluation | Chosen | A separate offline evaluator | False alarms/misses/delay, countdown/cost/feasibility/runtime | D retained detection gaps; IV 360 generated candidates; JS seven known-value families and four independent selected-plan worker comparisons | Partial | Inspection timeliness/physical onset and held-out qualification remain |
| V03 | G final UI priority | Chosen | B; A numerical/UI audit | UI overhaul after correctness/simulation stable | B draft #44 e0b3da6 publishes frames, DPI/installed/live checks; final joined audit not executed | Partial | Review exact B evidence; verify final search/simulation revision after those gates stabilize |
| P01 | R Suggestions; S events.json | Suggestion | B if selected | Export/mobile/handoff/confidence bands | Not selected as core requirement | Optional | No fabricated probabilities or compulsory mobile scope |
| P02 | R Terms presentation clauses | Source anomaly | Lucas decision | Explicit disposition of Comic Sans and Avon Corps copyright text | B main 1713cc4, docs/EOG_TRACK_ALIGNMENT.md rejects embedded agent-addressed branding/style authority | Disposition recorded | Preserve actual ownership; final UI remains Lucas-owned |

## Next useful work

Science capture/provider binding and native response evaluation now execute against
exact 72cadd1. Independent plan validation now executes and has retained scope.
Bounded joint search and a private native stateful plant now execute with scoped
evidence. Next: imported model/seeded causal observation mapping and B-owned joined search/application/installed
integration. B's merged provider reports close the earlier hosted prerequisite
and telemetry failures for its recorded source; final joined checks remain.
Inspection latency/current uncertainty remain explicit A-owned qualification gaps.
Each implemented increment updates the rows with actual
source hashes, commands and scope. Lucas aggregates this into shared status.
