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
prefixes. These are process/model checks, not independent plan validation.

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
| D06 | S Repairs / Horizon | Rule | A | Remaining repair from onset plus captured duration | E3 direct-seconds deadline derivation; forged and one-ULP horizon rejection | Scoped pass | Independent E4 validator still required |
| D07 | S Pressure formulas | Rule | A/B interpretation | Treat pressure offsets as placeholders, no hydraulic claim | Source review only | Documented | Preserve this limitation in tools/UI |
| T01 | S Plans / Trucks; X section 4 | Rule | A | Full loads, response/cycle, cost and sufficient inventory | E3 native full-load/cost/timing/inventory tests plus real worker truck plan | Scoped pass | Independent plan validator and search remain |
| T02 | S Plans / Trucks | Rule | A | Arrival strictly before repair; late pickup cannot undo trip | CR 72cadd1 exact elapsed clock; E3 strict repair/threshold/one-ULP ordering | Scoped pass | Late full pickup is costed without undoing shutdown; independent validator remains |
| T03 | S Shared trucks | Rule | A | Shared fleet, final return occupancy, sequential yard jobs | E3 native shared slots, overlapping jobs, final return and sequential yard tests | Scoped pass | Joint search and evolving prior commitments remain |
| T04 | S Shared trucks; H reference actions | Rule | A/B | Now shares entire fleet; evolving simulation carries commitments | E1 explicit initial fixture fleet | Partial | Distinct context/stateful simulation pending |
| C01 | S Plans / Shut-in | Rule | A | Immediate/delayed per-well action lasts through repair | E3 native per-well economics, delayed stops, automatic union and 600 independent random well-stop oracles | Scoped pass | Application and independent validator remain |
| C02 | S Doing nothing | Rule | A/B semantics | Do-nothing can be cheapest valid outcome despite shutdown | Accepted 72cadd1 semantics; E3 actual native no-action worker costs | Scoped pass | Costed automatic outcome is supported; minimum-cost search remains |
| C03 | R The decision; S Plans | Rule | A | Compare baseline, truck, well, mixed at same snapshot/horizon | E1 public illustration 4.48h / $5016 / $905 | Partial | No production joint optimizer delivered |
| C04 | S Plans; G independent checks | Chosen | A | Recompute exact plan physics/cost/calendar independently | None; CR only verifies declarations | Planned | Separate algorithm, adversarial plans, known optima |
| C05 | G bounds; S Plans | Chosen | A | Bounded search, explicit best-found/enumerated scope | CR result consistency only | Planned | Runtime/memory/coverage evidence pending |
| C06 | G coverage; CR R1/R3 | Chosen | B boundary; A recomputes | Full facility/repair horizon, exact fractional events | CR completeness; E3 complete current-failure compilation and native exact horizons/breakpoints | Scoped pass | E4 independently coded verification and integrated application remain |
| U01 | S Alerts view | Chosen | A ranking; B UI | All systems: failures by countdown, then revenue; inspections follow | D pure urgency tests and E3 all-system detect output | Partial | B alert-view ranking/render qualification remains |
| U02 | R Suggestions / network, trends; G UI | Chosen | B | Dataset-driven 3D and channels; readable gaps/extrema/units | Handoff reports reader only, not fresh EOG render | Planned | Lucas implementation/render acceptance |
| U03 | G whole-application regression | Chosen | B | Async cutoff/dataset binding, cancellation, restart, bounded history | No EOG integrated evidence | Planned | Reproduce real process failures and stale completions |
| U04 | G model tools; P Nemotron | Chosen | B adapter, A methods | On-demand genuine tools with exact returned IDs; no load/seek call | Prior workbench evidence only | Planned | EOG live rehearsal after provider integration |
| U05 | G persistence/approval; A | Chosen | B | Evidence import/reopen has no operating authority; review != dispatch | Prior workbench evidence only | Planned | EOG history plus retained-workbench regression |
| S01 | R record immutability; G Recorded replay | Chosen | B playback; A causal evidence | Play/pause/step/speed/seek; meaningful staleness preserved | No complete EOG UI evidence | Planned | CLI replay alone insufficient |
| S02 | R Suggestions / what-if; G Counterfactual | Chosen | A trajectories; B application | Branch exact snapshot, animate inventories/events/trucks/costs | E3 exact snapshot/repair trajectories with full discrete breakpoints | Partial | Evolving/animated application path and independent checks remain |
| S03 | G Extended synthetic simulation | Chosen | A core; B application | Seeded bounded cycles/failures/repairs/inflows; hidden truth isolated | No implementation | Planned | Agree additive simulation contract first |
| S04 | G long-run acceptance | Chosen | A/B | Mass/revenue/load ledgers; resource conservation; event refinement | No long-run evidence | Planned | Include resets/cancellation/responsive application playback |
| W01 | P editable foundation; G preserve workbench | Chosen | B; A numerical regressions | Save/load/undo/redo, layout vs physics, retired approvals | Inherited status only | Planned | Fresh final normal/ASan/science/render/install checks |
| V01 | G whole-app / completion | Chosen | Both; Lucas integrates | Joined exact contract/revision, Windows installed/rehearsal | No integrated EOG revision | Planned | Small PRs, owner reviews, final acceptance manifest |
| V02 | H folders; G evaluation | Chosen | A separate offline evaluator | False alarms/misses/delay, countdown/cost/feasibility/runtime | D retained 23-label evaluation and failed causal cuts; E3 four real causal worker prefixes | Partial | Inspection timeliness and independent plan cost/feasibility qualification remain |
| V03 | G final UI priority | Chosen | B; A numerical/UI audit | UI overhaul after correctness/simulation stable | No final UI evidence | Planned | Verify actual rendered frames, compact/keyboard use |
| P01 | R Suggestions; S events.json | Suggestion | B if selected | Export/mobile/handoff/confidence bands | Not selected as core requirement | Optional | No fabricated probabilities or compulsory mobile scope |
| P02 | R Terms presentation clauses | Source anomaly | Lucas decision | Explicit disposition of Comic Sans and Avon Corps copyright text | B main 1713cc4, docs/EOG_TRACK_ALIGNMENT.md rejects embedded agent-addressed branding/style authority | Disposition recorded | Preserve actual ownership; final UI remains Lucas-owned |

## Next useful work

Science capture/provider binding and native response evaluation now execute against
exact 72cadd1. Next: separately coded independent plan validation and bounded joint
search, then stateful simulation and B-owned application/installed integration.
Inspection latency/current uncertainty remain explicit A-owned qualification gaps.
Each implemented increment updates the rows with actual
source hashes, commands and scope. Lucas aggregates this into shared status.
