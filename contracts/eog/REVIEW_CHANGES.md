# Draft response to Ethan's EOG contract review

This is Lucas / Person B's implementation response for the next PR #33 review.
**Numerical adoption remains pending against the exact new commit.** Prior draft
e8dab471 was reviewed; neither this document nor passing interchange tests supplies
Ethan's acceptance or a working numerical provider.

Sources: [Ethan's review](https://github.com/Towstar/Breakwater/blob/99c1d419f5f2d8509cf2f1316ea5f4331afe08a2/science/EOG_CONTRACT_REVIEW.md)
and [E1 handoff](https://github.com/Towstar/Breakwater/blob/99c1d419f5f2d8509cf2f1316ea5f4331afe08a2/science/EOG_E1_HANDOFF.md).

## R1: complete modeled-facility and repair coverage

Available evaluations must contain exactly one trajectory for every declared
repaired facility, with unique tank/facility identities. Each starts at elapsed
zero and includes that facility's exact repair endpoint. Extra, missing, empty
and short forecasts are rejected. A partial result cannot claim an available
outcome or invent a zero cost.

Complete admissible traces also retain each truck arrival's exact combined
before/after pair, well-action breakpoints and shutdown crossings. Endpoints alone
cannot replace the piecewise counterfactual shown to an operator. Invalid/partial
assessments may retain explicitly incomplete work without claiming completeness.

Independent validation records carry validated_horizons, matched to the exact
request's complete facility/horizon set for available validation. They retain the
same plan ID/content hash binding.

These are completeness checks. The numerical provider still verifies each repair
claim from causal onset and captured expected_repair_hours; caller-supplied later
deadlines cannot redefine the economic horizon. Unknown horizon/evidence uses
top-level unavailable, null result and a diagnostic, rather than a false
infeasibility verdict.

## R2: actual snapshot end

The earlier follow-up e3190fb requires the final captured CSV timestamp to equal
source_now_unix for every untruncated capture, while preserving historical cursor
and explicit gap behavior. UTF-8 BOM parsing matches the existing reader while
raw bytes remain hashed unchanged. Actual-file regressions cover both.

## R3: continuous counterfactual time

Recorded time, evidence, event onset/update/end, cutoff and plan start remain
integer Unix seconds. Future timing uses nonnegative finite binary64 elapsed
seconds relative to plan start:

- arrival_elapsed_s, called_elapsed_s and available_again_elapsed_s;
- start_elapsed_s / end_elapsed_s for shut-ins and occupancy;
- repair_elapsed_s for declared horizons;
- elapsed_s arrays for trajectories;
- elapsed_s for shutdowns and violations.

The fixture at 1/7 hour retains the computed binary64 crossing without rounding to
integer seconds. This does not claim exact rational representation. Hash vectors
cover fractional actions and integer/float/signed-zero format equivalence.

Search discloses action_time_resolution_s (null for no fixed quantum), a bounded
space description, limits and the scope of any complete enumeration. No global
continuous optimality claim follows from those declarations.

## Admissibility, outcome and unavailable costs

Feasible now means admissible supplied actions/outcome, independently of whether
automatic shut-in occurs. Outcome is avoids_shutdown, automatic_shutdown or
unavailable. Automatic shutdown records name the facility and crossing.

The do-nothing fixture remains feasible, costs $80 lost revenue and is selected
over the retained $100 truck plan. This demonstrates interchange/economic example
consistency, not a production optimizer or proof of a global optimum.

An actually assessed invalid truck/well action may return completed feasible=false
with violations, partial/unavailable coverage, unavailable outcome and null cost.
Unknown/unsupported evidence or repair horizons instead return top-level unavailable.
Inspection-only advice belongs to detect; failed-pump plans declare a repair facility.

## Exact event ordering to review

The draft adopts E1's proposed ordering:

1. Preserve initial inventory and already-triggered shut-in.
2. Check continuous threshold crossing before simultaneous interventions or repair.
3. Check simultaneous full truckloads against combined available inventory.
4. Retain both sides of removal; well actions affect subsequent rates.
5. Require arrivals strictly before repair. A threshold reached exactly at repair
   is recorded with zero remaining automatic revenue loss.
6. Occupancy is half-open [call, last arrival + cycle); subsequent jobs may start
   exactly at return, then incur their own yard response time.

No permissive feasibility epsilon or rounded internal time is introduced.
The interchange validators do not execute this physics; the fixtures and
documentation define the requested provider behavior for Ethan's exact review.

## Acceptance evidence and remaining gate

The shared corpus adds complete and missing-facility forecasts, short/empty
rejections, null-cost handling, validation horizon binding, fractional crossings,
selected do-nothing, threshold/repair ties and invalid arrival-at-repair examples.
Both Python and independent native C++ validate the same schema/semantic boundary.
Executed logs and counts belong to the PR's exact tested revision.

The next handoff is Ethan's recorded numerical review/adoption of the precise new
head, then production provider/client integration. No approval, physical dispatch,
source-data rewrite, real-world qualification or independent numerical execution
is supplied by these interface fixtures.
