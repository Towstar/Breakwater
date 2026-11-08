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
Evidence CR: `science/evidence/eog-contract-review-e3190fb/`, reviewed exact contract
`e3190fbfa73d5321adb8e1180a783a881001a919`. These historical/scoped results do not
qualify a subsequent integrated build. No EOG contract is adopted at this checkpoint.

| ID | Exact source / section | Class | Implementation / owner | Acceptance case | Actual evidence / revision | Status | Limitation / next action |
|---|---|---|---|---|---|---|---|
| I01 | S Files; R What you get | Rule | B capture, A provider | Read only the three allowed names; deny answer-key access | E1 has no file access; CR tests fixed capture names | Partial | Production provider/input-access test pending |
| I02 | S Files / Time range | Rule | B reader/capture; A verifier | First/last CSV timestamps define window/now | CR 40 tests and endpoint probe at e3190fb | Scoped pass | Verify actual importer and changing-source capture |
| I03 | R How judging works; H Every folder | Rule | Both | Renamed/custom systems, prices, thresholds, fleets | E1 uses arbitrary constructed IDs | Partial | Unfamiliar file networks pending; no published judging set |
| I04 | S telemetry; G Input acceptance | Rule + Chosen | B parser; A analysis | Reject malformed, duplicate, nonfinite, unknown inputs | CR structural cases only | Partial | Provider and desktop agreement pending |
| I05 | S telemetry; G causal acceptance | Rule + Chosen | Both | Historical cutoff excludes all later readings/tools | CR manifest/evidence causality | Partial | End-to-end future-data and changing-cursor checks pending |
| I06 | G Input acceptance | Chosen | B capture; A verifies | Mutation/corruption invalidates capture; exact hashes | CR raw-hash/endpoint cases | Partial | Atomic producer and race tests pending |
| O01 | S network/telemetry; R Terms | Rule | A | bbl, bpd/24, mcfd revenue, psi, percent | E1 arithmetic and CR Decimal oracles | Scoped pass | Preserve units through actual wire/UI |
| O02 | S telemetry noise; G observations | Chosen | Both | Preserve finite unusual readings; no hidden clipping | E1 conversion/availability fixtures | Partial | Actual parser/charts and unsupported states pending |
| O03 | S five-minute telemetry; G policy | Chosen | A evidence; B displays | Explicit freshness, gap and cross-channel coherence | E1 caller-supplied observation bounds | Partial | Versioned production 300-second policy pending |
| D01 | S Normal behavior; X section 2 | Rule | A | Normal stopped/running hysteresis creates no fault | E1 hysteresis fixtures | Partial | Causal detector on sawtooth histories pending |
| D02 | S Three problems; R What goes wrong | Chosen | A | Failure, degradation, leak from causal sensors | None for production detector | Planned | Define/evaluate versioned rules and abstentions |
| D03 | S Three problems; H Reading key | Rule | A | Onset uncertainty, causal resolution; degradation then failure separate | CR event shape only | Planned | Lifecycle implementation and delay/confusion metrics pending |
| D04 | S Countdown; S Automatic shut-in | Rule | A | Failure predicts configured shutdown, not physical overflow | E1 constant-inflow countdown | Partial | Bind actual causal state and inspection nulls |
| D05 | S Automatic shut-in / Repairs | Rule | A | Already-tripped fault retains revenue until causal repair | E1 already-shut fixture | Partial | Recover exposure from pre-trip evidence; never future endpoints |
| D06 | S Repairs / Horizon | Rule | A | Remaining repair from onset plus captured duration | E1 remaining-duration fixture | Partial | Review deadline checks/assumptions in contract |
| D07 | S Pressure formulas | Rule | A/B interpretation | Treat pressure offsets as placeholders, no hydraulic claim | Source review only | Documented | Preserve this limitation in tools/UI |
| T01 | S Plans / Trucks; X section 4 | Rule | A | Full loads, response/cycle, cost and sufficient inventory | E1 analytical scheduled cases | Scoped pass | Production mapping and independent validator pending |
| T02 | S Plans / Trucks | Rule | A | Arrival strictly before repair; late pickup cannot undo trip | E1 deadline/tie and one-ULP cases | Scoped pass | Exact wire transition timing blocked by CR R3 |
| T03 | S Shared trucks | Rule | A | Shared fleet, final return occupancy, sequential yard jobs | E1 multi-facility/resource cases | Scoped pass | Joint production search and retained calendars pending |
| T04 | S Shared trucks; H reference actions | Rule | A/B | Now shares entire fleet; evolving simulation carries commitments | E1 explicit initial fixture fleet | Partial | Distinct context/stateful simulation pending |
| C01 | S Plans / Shut-in | Rule | A | Immediate/delayed per-well action lasts through repair | E1 delayed independent wells | Scoped pass | Captured economics and production plans pending |
| C02 | S Doing nothing | Rule | A/B semantics | Do-nothing can be cheapest valid outcome despite shutdown | E1 no-action costs; CR semantics pending | Partial | Contract selected-plan example and outcome definition needed |
| C03 | R The decision; S Plans | Rule | A | Compare baseline, truck, well, mixed at same snapshot/horizon | E1 public illustration 4.48h / $5016 / $905 | Partial | No production joint optimizer delivered |
| C04 | S Plans; G independent checks | Chosen | A | Recompute exact plan physics/cost/calendar independently | None; CR only verifies declarations | Planned | Separate algorithm, adversarial plans, known optima |
| C05 | G bounds; S Plans | Chosen | A | Bounded search, explicit best-found/enumerated scope | CR result consistency only | Planned | Runtime/memory/coverage evidence pending |
| C06 | G coverage; CR R1/R3 | Chosen | B boundary; A recomputes | Full facility/repair horizon, exact fractional events | CR empty/short forecasts accepted; fractional rejected | Blocked | Lucas owns contract fixes; no adoption |
| U01 | S Alerts view | Chosen | A ranking; B UI | All systems: failures by countdown, then revenue; inspections follow | No integrated evidence | Planned | Unknown countdown remains explicit |
| U02 | R Suggestions / network, trends; G UI | Chosen | B | Dataset-driven 3D and channels; readable gaps/extrema/units | Handoff reports reader only, not fresh EOG render | Planned | Lucas implementation/render acceptance |
| U03 | G whole-application regression | Chosen | B | Async cutoff/dataset binding, cancellation, restart, bounded history | No EOG integrated evidence | Planned | Reproduce real process failures and stale completions |
| U04 | G model tools; P Nemotron | Chosen | B adapter, A methods | On-demand genuine tools with exact returned IDs; no load/seek call | Prior workbench evidence only | Planned | EOG live rehearsal after provider integration |
| U05 | G persistence/approval; A | Chosen | B | Evidence import/reopen has no operating authority; review != dispatch | Prior workbench evidence only | Planned | EOG history plus retained-workbench regression |
| S01 | R record immutability; G Recorded replay | Chosen | B playback; A causal evidence | Play/pause/step/speed/seek; meaningful staleness preserved | No complete EOG UI evidence | Planned | CLI replay alone insufficient |
| S02 | R Suggestions / what-if; G Counterfactual | Chosen | A trajectories; B application | Branch exact snapshot, animate inventories/events/trucks/costs | E1 analytical response fixtures only | Partial | Application path and full independent checks pending |
| S03 | G Extended synthetic simulation | Chosen | A core; B application | Seeded bounded cycles/failures/repairs/inflows; hidden truth isolated | No implementation | Planned | Agree additive simulation contract first |
| S04 | G long-run acceptance | Chosen | A/B | Mass/revenue/load ledgers; resource conservation; event refinement | No long-run evidence | Planned | Include resets/cancellation/responsive application playback |
| W01 | P editable foundation; G preserve workbench | Chosen | B; A numerical regressions | Save/load/undo/redo, layout vs physics, retired approvals | Inherited status only | Planned | Fresh final normal/ASan/science/render/install checks |
| V01 | G whole-app / completion | Chosen | Both; Lucas integrates | Joined exact contract/revision, Windows installed/rehearsal | No integrated EOG revision | Planned | Small PRs, owner reviews, final acceptance manifest |
| V02 | H folders; G evaluation | Chosen | A separate offline evaluator | False alarms/misses/delay, countdown/cost/feasibility/runtime | No official-data qualification | Planned | Generated unfamiliar cases first; labels never production input |
| V03 | G final UI priority | Chosen | B; A numerical/UI audit | UI overhaul after correctness/simulation stable | No final UI evidence | Planned | Verify actual rendered frames, compact/keyboard use |
| P01 | R Suggestions; S events.json | Suggestion | B if selected | Export/mobile/handoff/confidence bands | Not selected as core requirement | Optional | No fabricated probabilities or compulsory mobile scope |
| P02 | R Terms presentation clauses | Source anomaly | Lucas decision | Explicit disposition of Comic Sans and Avon Corps copyright text | Source reviewed at pinned revision | Pending decision | Do not invent ownership/affiliation; task authorization governs |

## Next useful work

Finish the exact contract review with Lucas; meanwhile build a pure causal
observation/detection model and generated unfamiliar fixtures without binding to
an unadopted wire contract. Each implemented increment updates the rows with actual
source hashes, commands and scope. Lucas aggregates this into shared status.
