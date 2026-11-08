# Independently coded EOG plan validation

`science/eog_plan_validator.py` implements the private numerical validator and
`eog_worker.py` worker 0.2.0 exposes `validate_plan` through adopted public contract
**72cadd1de7eed6666a60faff1d296e29c8f026ee**. No public schema/DTO, desktop, root
build or native evaluator source is changed by this increment. Search remains
explicitly unavailable until the next tested increment.

## Independence and model scope

The validator receives the immutable input `Model`, causal `View` and exact proposed
plan. It does **not** import/call the detector, response compiler, native bridge,
native evaluator or a model API. It has no file access. Actual worker tests run
validation without a configured native DLL. These are implementation-independence
checks, not a claim that shared input parsing or physical assumptions are independent.

It reconstructs each current failure from its own observation queries. A reverse
selection of the last coherent recovery and first subsequent fault establishes the
current point-onset assumption. Earlier known well states establish productive
eligibility. It independently checks dataset freshness/alignment, fault delivery,
well/field meter consistency, current inventory, pre-trip economics and repair
seconds from configured duration and causal onset. Scheduled duration never proves
observed recovery. Unknown/expired onset, economics or state remains unavailable.

Candidate claims are checked directly using original opaque asset IDs. Shared truck
conflicts use **all pairwise job intervals**, separate from the native sorted-slot
algorithm. Full loads, arrival recurrence, final return, exact repair claims,
per-well stop ownership and combined simultaneous inventory are recomputed.

The numerical reference uses Python's standard-library `Fraction` to evaluate exact
rational primitives, with an explicit nearest/even binary64 conversion after each
documented primitive. This independently checks the declared **binary64** event
policy; it is not an exact-real ODE proof. It uses no feasibility tolerance or action
time grid. Affine segments preserve zero-effect breakpoint invariance; threshold
wins ties, a pickup at repair is invalid, and an automatic trip never restarts wells
before repair. Unrepresentable/ambiguous arithmetic is unavailable, not certified.

Each productive well's stopped-time union is costed directly from its input oil,
gas and prices. Private audits retain individual well losses, exact-derived volume
residuals and resource calendars. A valid do-nothing automatic trip remains costed
and admissible. Invalid actions retain violations and null cost; unknown physics
never becomes a completed infeasible verdict or zero. Validation IDs include exact
plan hash, model economics, causal evidence and method; analysis IDs also include
request arguments to prevent reused request IDs aliasing different candidate results.

The supported evaluator identifier is `eog-native-seconds-0.1.0`, checked by
`eog-rational-validation-0.1.0`. An unknown identifier is unavailable, not silently
relabeled. The worker verifies the actual capture and validates the response against
its original request. It preserves plan ID/hash and full requested horizon coverage.

## Bounds and limitations

Bounds include 256 jobs/stops, 16,384 trips, 1,000,000 exact arithmetic primitives,
256 retained violations and 4,096 required evidence references. The public framing,
input and result limits can bind sooner. Host deadline/cancellation is B-owned.

The complete current failure set is required. Prior evolving dispatch commitments,
subset response contexts, post-repair evolution and hydraulic behavior remain outside
this model. Point-onset, proportional meter reconciliation and fixed-rate assumptions
are shared declared physics. Inspection delay/recovery and physical onset uncertainty
remain unqualified. Agreement of independent implementations is evidence of their
tested consistency, not universal correctness, external ground truth or optimality.

## Reproduction and retained evidence

Set `BREAKWATER_EOG_NATIVE` to the explicitly identified native DLL for cross-checks
and `BREAKWATER_EOG_CONTRACTS` to the adopted `contracts/eog` checkout. Execute:

```text
python -m unittest discover -s science/tests -p test_eog_plan_validator.py -v
python -m unittest discover -s science/tests -p test_eog_worker.py -v
python science/qualify_eog_validation.py --evaluator-records E3_PROVIDER_EVIDENCE --output-dir NEW_OUTPUT --contracts-root ADOPTED_CONTRACT_ROOT --cutoff-unix 1791460800 --cutoff-unix 1791516600 --cutoff-unix 1791622800 --cutoff-unix 1791646200
```

The qualification client sends **only the retained plan/capture** to actual worker
processes, without a native DLL, native costs or evaluator trajectories. It compares
the independent result with frozen E3 records afterward, checks one-ULP forged
deadline rejection and retains itemized audits. No answer key is read. These four
official practice prefixes are correlated. Whole-application, installed and final UI
acceptance still require Lucas's integration and a joined final revision.
