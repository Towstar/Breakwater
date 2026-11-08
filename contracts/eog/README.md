# EOG analysis contracts v1

This increment delivers **interfaces, validators and synthetic acceptance fixtures**.
It does not implement EOG detection, countdown physics, response optimization,
independent numerical validation, a process adapter or operating-control application.

Lucas / Person B owns the public boundary. Ethan / Person A owns its numerical
provider. Both adopt the **same exact reviewed commit** before dependent integration.
Numerical acceptance remains pending; the checklist below is not an approval claim.

These contracts are additive. G1 telemetry, project-v1/v2/v3, existing network hashes
and the scientific worker v1 protocol are unchanged. The proposed EOG worker has a
separate one-shot lifecycle from continuous G2 telemetry and storage science.

## Documents and operations

JSON Schema Draft 2020-12, local references only, with strict unknown-field,
kind and version rejection. The JSON schemas are the wire source of truth.
`../include/breakwater/eog.hpp` provides Qt-free C++20 DTOs; these have no parser,
numerical method, Python, model or transport dependency.

| Document | Contents |
|---|---|
| `capture-manifest-v1.schema.json` | Original identities, causal cutoff, captured-file hashes/sizes and explicit dataset clock |
| `analysis-request-v1.schema.json` | Versioned operation, request binding, absolute local capture directory, embedded manifest and typed arguments |
| `analysis-response-v1.schema.json` | Matching binding, execution status, diagnostics and operation-specific result |
| `common-v1.schema.json` | Events/evidence, proposed plans, counterfactual trajectories, costs, shared-fleet occupancy and validation records |

All envelopes contain `eog_version: 1`. Request/response `method_version: 1`
selects this operation contract, not a claim that a numerical implementation exists.
Provider method identifiers and event `threshold_version` retain implementation
provenance independently.

| Operation | Arguments | Completed result |
|---|---|---|
| `detect` | `system_ids`; empty means all systems | Events, original sample references, evidence values/units, coverage, assumptions and limitations |
| `evaluate_plan` | Exact proposed `plan` | Evaluation retaining that plan, trajectories, costs, occupancy and violations |
| `search_plans` | Systems, candidate limit and time limit | Retained evaluations, selected feasible plan ID, search coverage and optimality scope |
| `validate_plan` | Exact plan and evaluator method identifier | Separately identified validator, exact plan hash, recomputed cost/margin/violations and source evidence |

Responses echo request ID, operation, method version, dataset identity, capture ID
and cutoff. **Validate against the actual request** before publishing a response.
Standalone shape validation does not establish that binding.

Statuses are `completed`, `unavailable`, `invalid_input`, `cancelled` or `failed`.
Every non-completed response has null result and at least one bounded diagnostic.
A completed infeasible evaluation is an assessed invalid supplied action, not a
transport failure. Unknown repair horizons, unsupported modeling combinations or
insufficient evidence return top-level unavailable with null result and diagnostics;
they must not be misrepresented as demonstrated infeasibility. Missing provider
capabilities never become successful empty/healthy data.

## Allowed files, capture and clock

The application and numerical provider consume only `network.json`, `ops.json`
and `telemetry.csv`. Practice `answer_key.json` is exclusively for a separate
offline evaluator; it is not a contract field, capture input or model tool input.

The manifest has exactly those keys in `source_hashes` and `captured_files`.
Captured metadata never supplies arbitrary relative filenames. The request's
`capture_directory` is absolute and local; UNC/network paths are rejected.
The adapter supplies a private captured directory rather than the original source
directory. This is an application input discipline, not OS sandbox isolation.

The capture contains unchanged network/operations copies and the original CSV
header/rows only through `cutoff_unix`. Their raw bytes and SHA-256 hashes bind the
actual input. Network/operations hashes equal their original source hashes; when
cutoff equals source-now, the untruncated CSV hash must also equal its source hash.

- `source_first_unix` / `source_now_unix` describe the original CSV window.
- `cutoff_unix` lies inside that window.
- `snapshot_now` requires cutoff equal to source-now; `historical_cursor` permits an earlier cutoff.
- The source CSV cadence is 300 seconds. Explicit `sample_period_s` and `max_age_s` are 300.
- Dataset time governs freshness, never the laptop clock.
- A good referenced sample is at most 300 seconds old at cutoff.
- Older references can remain stale historical evidence. Missing values are null;
  finite negative or out-of-range recorded values remain evidence.
- All evidence timestamps and inferred event updates/ends are at or before cutoff.
  Recorded source/event timestamps and plan start remain exact integer Unix seconds.
  Counterfactual actions, trajectories, shutdown crossings and violations use finite
  binary64 elapsed seconds relative to plan start, without integer rounding.

The capture producer must detect source changes while copying, construct the final
manifest from completed files, and publish atomically. `verify_capture` checks only
the three fixed files, their sizes/hashes, strict JSON syntax and CSV shape,
finite values, ordering, duplicate sample identity and causal clock. It does not
perform full native network/ops validation or observation mathematics.

No CSV payload is embedded inside an NDJSON line. Limits, including newline:

| Item | Bound |
|---|---:|
| Request | 1 MiB |
| Manifest / response / other JSON document | 4 MiB |
| Network or operations capture | 4 MiB each |
| CSV capture | 128 MiB / 2,000,000 rows |
| JSON nesting | 64 |
| Imported ID | 256 UTF-16 code units; Unicode/punctuation retained |
| Retained trajectory | 8,192 samples |
| Retained search evaluations | 64 |
| Search candidates / requested time | 10,000 / 120 seconds |

The byte budget can bind before an array limit. A provider must report a bounded
failure/unavailable diagnostic if it cannot return a complete required result.
It must not silently truncate the selected plan's required evidence or trajectories.
Host cancellation/deadline enforcement belongs to the process adapter.

## Counterfactual clock, coverage and outcomes

These are failed-pump response operations. A plan declares at least one repaired
facility; inspection-only advice belongs to detect. Plan start is the captured
integer cutoff. Future fields are relative binary64 seconds:

- Trips: arrival_elapsed_s.
- Truck jobs: called_elapsed_s / available_again_elapsed_s.
- Well shut-ins and fleet occupancy: start_elapsed_s / end_elapsed_s.
- Repair claims: repair_elapsed_s.
- Trajectories: elapsed_s arrays; shutdowns and violations: elapsed_s.

This retains computed fractional crossings such as 1/7 hour = approximately
514.285714 seconds. It is binary64 arithmetic, not exact rational arithmetic.
Do not round actions, crossings or feasibility decisions to fit a display grid.
A UI may derive wall-clock labels, but must retain elapsed values for validation.

Every evaluation declares coverage (available/partial/unavailable) and outcome
(avoids_shutdown/automatic_shutdown/unavailable). Feasible means the supplied
actions/outcome are admissible; it does not mean all automatic shut-ins were avoided.
A do-nothing automatic shut-in is feasible and search-selectable when its lost
revenue is cheapest. Shutdown records identify each facility and its elapsed crossing.

A feasible evaluation requires available coverage, a computable non-null cost,
an available outcome, no violations and exact trajectory coverage of every declared
repair facility. Each trace starts at elapsed zero and includes that facility's
exact repair endpoint. Empty, short, extra-facility or missing-facility forecasts
cannot pass as a complete available result. Double timestamp pairs retain jumps.
Every declared truck arrival in an admissible plan needs the exact before/after
pair at that facility; simultaneous loads share one combined pair. Well-action
breakpoints and automatic shutdown crossings must also be retained. Endpoint-only
or decimated traces cannot silently erase interventions from a complete result.

An actually assessed invalid action can return completed feasible=false, violations,
partial/unavailable coverage, unavailable outcome and null cost. Partial assessment
must not fill absent cost with zero. Unknown horizons/evidence instead use top-level
unavailable; no numerical verdict is claimed.

Validation records carry the same assessment fields plus validated_horizons.
Bound available validation must cover the exact requested facility/horizon set.
A different or missing horizon cannot validate the original candidate. Proposal
repair claims remain subject to causal onset and captured operations verification.

Search results declare action_time_resolution_s: null means no fixed time quantum;
a positive value discloses the chosen bounded action grid. space_description states
the actual bounded candidate space. Neither null nor complete finite enumeration
claims global optimization over every possible continuous action.

## Content identity

### Existing dataset identity — unchanged

SHA-256 of these exact ASCII bytes, with lowercase raw-file hashes and LF:

```text
breakwater-eog-inputs-v1
network.json:<sha256>
ops.json:<sha256>
telemetry.csv:<sha256>
```

The filenames always occur in that order. This preserves the existing reader's
identity; file formatting changes change identity.

### Capture and plan identities

Capture ID hashes every manifest field except `capture_id`, prefixed by
`breakwater-eog-capture-v1\n`. Plan hash hashes every plan field except `plan_hash`,
prefixed by `breakwater-eog-plan-v1\n`.

The canonical byte projection follows G1's cross-language numeric rule:

1. Preserve null, booleans and strings.
2. Recursively replace every finite number by
   `{"$f64":"<16 lowercase hex digits of big-endian IEEE binary64>"}`.
3. Normalize negative zero to positive zero.
4. Sort object keys and emit compact UTF-8 JSON without ASCII escaping.

Manifest/plan keys are schema-defined ASCII keys; imported Unicode IDs remain
values. Array order remains significant. No decimal formatter or ordinary Qt/Python
JSON dump can replace this projection when computing identity.

`fixtures/hash-vectors.json` contains exact bytes/hashes. The plan's human ID is
also hashed; any action, timing, cost, context or identity edit changes `plan_hash`.
Independent validation must echo that exact hash, not only reuse a plan ID.
Hashes provide content identity, not publisher authentication.

## Numerical semantics and review boundary

These fixtures encode the following initial semantics for Ethan's review:

- Inventory = tank percentage / 100 times barrel capacity; bpd / 24 is bbl/hour.
  Percentage is not physical metres. EOG units remain bbl, bpd, mcfd, psi and %.
- Pump state is 0 stopped, 1 running, 2 fault. Stopped does not itself imply failure.
  The numerical provider must account for normal hysteresis.
- Leaks/degradation recommend inspection and have no shutdown countdown.
  Failure is evaluated against the input shutdown threshold, not overflow.
- Repair time remaining is estimated from causal onset and configured duration,
  never a future labelled endpoint. Onset bounds and limitations remain explicit.
- Each truck arrival removes one full configured truck capacity and charges one
  configured trip cost; no average continuous drainage or invented partial loads.
- First response, repeat cycle, full inventory requirement and return after the
  final trip are authoritative from captured operations.
- A truck cannot serve overlapping jobs; a subsequent job starts from the yard.
  Concurrent failures share a system's whole available fleet at snapshot-now.
- A trip must arrive strictly before repair ends. Threshold crossing is evaluated
  first at arrival/intervention/repair ties. A crossing exactly at repair is recorded
  with zero remaining automatic revenue loss; a truck arriving at repair is invalid.
  Once shut-in has occurred, a late trip cannot undo it or restart wells before repair.
- Simultaneous truck loads are checked against their combined full inventory demand.
  Occupancy is half-open [call, last arrival + cycle); the next job may start exactly
  when the preceding truck returns to the yard.
- Candidate repair deadlines are claims. The provider independently checks causal
  onset plus expected_repair_hours from captured ops; extending a candidate deadline
  cannot change the economic comparison horizon. An unknown horizon is unavailable.
- Delayed individual well shut-ins and the no-action automatic shut-in option
  retain their revenue losses through the applicable repair endpoint.
- Snapshot-now and historical-cursor contexts remain distinct from offline
  onset-reference experiments with previously occupied trucks.

A proposed plan retains declared trip volumes/costs/response/cycle fields so the
exact candidate can be evaluated and explained. **They are proposals, not trusted
replacements for captured operations.** The provider must independently recompute
and check them against network/ops. Infeasible partial/late/overbooked candidates
remain representable; `evaluate_plan` / `validate_plan` return the violations.

The interchange validator checks structure, causal evidence, exact identity,
cost-component sums, trajectory shape, occupancy-to-job correspondence and
declared result consistency. It does not simulate tank trajectories, recompute
revenue, prove no resource overlap or establish physical feasibility.

A validation record requires distinct evaluator/validator identifiers and an
`independent: true` declaration. This is necessary provenance, **not proof that
independent code ran**. Runtime integration must execute Ethan's independent
method against the exact candidate, bind its result and retain actual evidence.

Search reports `best_found` unless complete enumeration of a declared search
space justifies `proven_within_space`. The selected plan has minimum declared
total cost among retained admissible candidates, including do-nothing; ties are allowed. This does not establish global optimality
outside that declared space. The provider checks the full shared system fleet.

Raw finite negative/overflowing trajectory values remain visible. Duplicate
timestamp pairs encode the before/after step at a discrete action; three or more
samples at one timestamp are rejected. No observations or source CSV are rewritten.

## Validation and examples

The validator uses the existing pinned test environment's jsonschema/referencing
packages for schema checks and Python standard-library parsing/hash/semantic code.
It performs no hosted call, imports no Torch and installs nothing.

From repository root, using the already configured scientific interpreter:

```powershell
& $env:BREAKWATER_PYTHON -B -m unittest discover -s contracts/eog/tests -v
& $env:BREAKWATER_PYTHON -B contracts/eog/validate.py contracts/eog/examples/detect-request.json
```

The Python-only command explicitly skips the native comparison. Desktop CTest
registers `desktop.eog_contracts` and `contracts.eog`; the latter supplies
`--native <breakwater_eog_contract_tests executable>` and requires actual native
execution with no skips. The comparator implements schema and semantic checks in
C++, embeds only the local schemas, and never starts Python. Shared cases, strict
raw input, content-hash vectors and deterministic mutations are compared across
the two implementations. Native/headless builds also compile the Qt-free DTO probe.

For a bound response, pass a context JSON object of the form
`{"request": <exact request object>}` using `--context <file>`.
An optional `assets` context maps imported asset IDs to supported kinds for
asset/metric and typed pump/well-state validation.

Public functions:

- `decode_document(bytes_or_text)`: strict bounded UTF-8 / duplicate-key / finite JSON.
- `validate_document(document, kind=None, context=None)`: schema and semantic consistency.
- `dataset_identity(source_hashes)`, `capture_identity(manifest)`, `plan_identity(plan)`.
- `semantic_bytes(value)`: canonical binary64 projection used by content identities.
- `verify_capture(directory, manifest)`: actual fixed-file hash/clock verification.

`fixtures/validation-cases.json` is the shared Python/Qt comparator corpus.
Examples cover all four operations, unavailable execution, renamed Unicode IDs,
an analytical truck plan, delayed well-only plan an infeasible fleet conflict, selected do-nothing, fractional crossings and
threshold/repair ties. The shared corpus rejects incomplete facility/horizon
coverage. `REVIEW_CHANGES.md` records the draft response to Ethan's review.
`fixtures/analytical-cases.json` records the independent synthetic arithmetic.
None is copied from answer keys or claims a working EOG numerical provider.

## Exact-commit adoption checklist

- [ ] Ethan reviews units, hysteresis/state meaning and measured-versus-commanded data.
- [ ] Ethan reviews missing/stale/onset uncertainty and repair assumptions.
- [ ] Ethan reviews full-load inventory, timing ties, deadlines and return-to-yard occupancy.
- [ ] Ethan reviews shared fleet, revenue loss and snapshot planning context.
- [ ] Ethan reviews independent validation and bounded-search claims.
- [ ] Lucas verifies cross-language fixtures, binding and adapter resource/lifecycle limits.
- [ ] Record numerical acceptance against the exact contract head.
- [ ] Both implementations adopt that exact revision before integration.

Imported/reopened/replayed records remain historical evidence. These contracts
contain no operating approval, physical dispatch or sandbox-control authority.
