# Ethan's numerical review of EOG contract PR #33

Reviewed exact revision: `e8dab4711a47ec8012fd206bb86d6c753d38254e`.
Date: November 8, 2026, America/Chicago.

**Disposition: numerical adoption pending.** No shared wire revision has been
adopted and no provider has been bound to this draft. The following two
reproduced completeness/clock findings should be closed before integration;
the fractional-time and outcome semantics below also need an explicit decision.
Lucas owns all requested contract changes. No B-owned source was edited here.

## Reproduced findings

### R1 — P1: completed evaluations can omit the required forecast

`contracts/eog/validate.py:303` validates each trajectory that happens to be
present, without requiring coverage of the plan's repaired facilities or their
remaining horizons. Starting with the valid bound evaluate-plan example:

- Replace `evaluation.trajectories` with `[]`: validation accepts a completed,
  feasible result against the exact request.
- Keep the trajectory but retain only its cutoff sample: validation again accepts
  the completed, feasible result, although two hours of forecast are missing.

This is an interchange-completeness issue, not a request to calculate physics in
the contract validator. A process client cannot distinguish a successful full
forecast from an incomplete one using the current binding/status alone.

Requested change: define required modeled-facility/repair coverage and enforce it
for completed supported evaluations, including final horizon coverage. If some
facilities or requested combinations are unsupported, return explicit unavailable
coverage/status rather than a completed feasible empty/short result. Add shared
valid/invalid corpus cases for empty, missing-facility and shortened trajectories.
The independent numerical validator must still recompute the physics separately.

### R2 — P1: snapshot-now can disagree with the actual captured CSV end

`contracts/eog/validate.py:470` checks the actual first timestamp but does not bind
the actual last timestamp to `source_now_unix` for an untruncated snapshot-now
capture. A synthetic two-row CSV ends at Unix 300; set both declared source-now
and cutoff to 600, and recompute every raw-file hash, dataset identity and capture
identity correctly. `verify_capture` accepts and returns `last_unix: 300` while
the accepted manifest says dataset now is 600.

The contract already defines dataset now from the last CSV timestamp. This mismatch
changes freshness and remaining-repair arithmetic even when all bytes/hashes match.

Requested change: require the verified last row to equal declared source-now for
snapshot-now/untruncated captures; retain explicit gap/cursor behavior for genuinely
truncated historical captures. Add both rejecting and valid snapshot tests. The
probe uses only tiny self-created allowed files, not an official answer key.

### R3 — P2: exact counterfactual crossing times need a representation

`common-v1.schema.json:580` permits only integer Unix seconds in trajectories, as
does violation timing. A continuous crossing such as `1/7 h = 514.285714... s`
cannot be represented exactly on that grid. The probe confirms that an otherwise
ordered trajectory containing fractional seconds is rejected as `invalid_shape`.
An ongoing event's floating-point `countdown_hours` does not identify each
counterfactual candidate's future shutdown transition.

Requested decision: add a relative floating-point elapsed-time/crossing field for
counterfactual results (keeping recorded timestamps exact integer seconds), or
explicitly define display sampling and provide a separate exact numerical event
record. Do not round candidate actions or crossing decisions to make them fit.
Declare integer-second action-search resolution if that is the intended bounded
space. Threshold-first comparison remains an internal exact-time decision.

## Semantics to settle in an example before adoption

- Define whether `feasible` means admissible actions/outcomes or avoidance of all
  automatic trips. The challenge permits choosing a do-nothing automatic shut-in
  when its lost revenue is cheapest. Search currently requires a selected plan to
  have `feasible: true`. Include a valid selected do-nothing example with automatic
  shutdown time and revenue loss; distinguish outcome from invalid truck/well
  actions. Otherwise clients can accidentally exclude the cheapest allowed choice.
- Define unavailable cost behavior for unsupported/invalid actions. Mandatory
  numeric cost fields must never be filled with zero merely because the provider
  cannot compute them. An explicit non-completed response is suitable if no
  completed numerical assessment exists; a completed infeasible counterfactual
  needs a declared cost interpretation and coverage.
- Confirm threshold versus **repair** ordering in addition to the already stated
  threshold/arrival tie. E1 proposes threshold first, reporting a trip exactly at
  repair with zero remaining automatic revenue loss. This is not a claimed rule
  inferred from the official data.
- Treat proposed `repair_deadlines` as checked claims against causal onset and
  captured `expected_repair_hours`, just as proposed trip loads/prices/timing must
  be checked against captured operations. Candidate-supplied later repairs must
  not change the economic comparison horizon.

## Numerically consistent parts

The reviewed draft correctly separates observed state from commands; preserves
bbl, bpd, psi, percent and mcf/day; targets shutdown below overflow; uses explicit
five-minute data-clock freshness; forbids future evidence; distinguishes leak and
degradation inspection from pump-failure planning; and retains onset uncertainty.
Full-load arrivals, response/cycle rules, final-return occupancy, shared fleets,
delayed well revenue, exact candidate hashes and separate validator provenance
agree with the handoff's intended numerical boundary. A different method ID alone
does not demonstrate independent computation, as the draft correctly explains.

Independent Decimal calculations confirm the three supplied analytical fixtures:
850 bbl before the example truck, 700 afterward, 850 at repair, $100 trip cost;
915 bbl and $425 for the delayed well case; and two overlapping uses exceeding a
one-truck fleet. This verifies those arithmetic examples, not an executed provider.

## Executed evidence and reproduction

In a detached checkout at the reviewed hash:

```powershell
& <review-python> -B -m unittest discover -s contracts/eog/tests -v
```

All **37/37 tests passed** with Python 3.12.13 and the existing hash-pinned
`science/requirements-worker-tests-win-py312.txt` dependencies. The first attempt
used a base interpreter without jsonschema; its failure is retained separately.
An offline cache attempt also lacked that package; installation then used the
existing hash-verified lock into an ignored isolated environment. No manifest or
dependency version was changed.

From Ethan's EOG numerical worktree:

```powershell
& <review-python> -B science/review_eog_contract.py `
  --checkout <detached-e8dab471-checkout> `
  --revision e8dab4711a47ec8012fd206bb86d6c753d38254e `
  --report out/eog-contract-probes.json
```

`science/evidence/eog-contract-review/e8dab471-probes.json` retains the exact
observations. The script's successful exit means the independent arithmetic
matches; it does **not** mean the reported review findings passed acceptance.

A joint C++20 `/permissive- /W4 /WX /Zs` header probe initially found a collision
between Ethan's new private types and Lucas's DTO names. Ethan fixed his own types
by moving them to `breakwater::eog::analytical`. The before/final probe logs and
final numerical rerun evidence are retained. This does not change Lucas's DTOs or
approve their JSON mapping. Provider integration remains a subsequent increment.
