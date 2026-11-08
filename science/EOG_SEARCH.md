# Bounded joint response search

Private numerical method **eog-joint-search-0.1.2**, worker **eog-worker-0.3.2**,
native **eog-native-seconds-0.1.1** and independent validator
**eog-rational-validation-0.1.0**. Runtime source is `34fdf5d`; accepted public
contract remains exact `72cadd1de7eed6666a60faff1d296e29c8f026ee`.

`search_plans` now executes native candidate evaluations and independently
recomputes the exact selected plan. All current failed facilities and their full
causally derived repair horizons remain in every plan. Snapshot search assumes
no previous dispatch; evolving synthetic commitments require a separate stateful
simulation. No physical controls or operating approval result from selection.

Only empty/all `system_ids` are supported. An unknown ID is invalid input; a
known proper subset explicitly returns `system_scope_unavailable`. The meaning
of subset search is coordinated with Lucas on PR #41; no new wire scope field
or silent failure omission is introduced.

## Declared finite candidate family

The no-action baseline is evaluated first. Well seeds use exact-rational
terminal and pickup-prefix stopped-volume relaxations, with revenue/water,
revenue and water ordering. Prefix policies are combined, and representable time
neighbors are tested. Inventory targets at the input threshold or one/four
representable inventory steps below it change proposed controls only. The actual
evaluator always uses the original threshold, strict event ordering and full
loads, without clipping or a feasibility tolerance.

Per-facility well menus are limited to 24; joint policies to 64 per schedule,
with six pickup prefix pivots. The generator includes optimized backgrounds,
uniform menu indices and individual deviations rather than a full Cartesian
product. Calls include current yard availability, full-load inventory readiness,
the no-action crossing neighborhood and quarter-horizon times. Trip counts
include 1–8, needed-load and well-economic breakpoints with neighbors, and the
maximum supported count. Repeated loads, shared physical truck slots and
sequential yard jobs include occupancy through the final return.

Best-first scheduling uses a relaxed economic estimate only for ordering; it
does not prune on a claimed cost lower bound. Symmetric unused trucks are
canonicalized. Already-shut/zero-baseline-loss fields remain fully modeled but
receive no cost-increasing controls under this independent-facility model.

Default limits: 256 jobs total/per truck, 4,095 trips/job, 16,384 trips total,
2,048 queued states, 20,000 retained state keys, eight complete evaluations and
3.5 MB retained JSON. The public request bounds evaluations to 10,000 and the
planning budget to 120 seconds. The baseline and selected full traces take
precedence; when the budget permits, the best available truck-only, well-only
and mixed results are also retained for comparison. Traces are never decimated.
Only the selected plan is independently recomputed; alternatives need their own
`validate_plan` request. Output limits never replace a cheaper unrepresentable
selection with a claimed best result.

The cooperative planning deadline includes evaluation and selected validation,
reserving up to two seconds / 20% for validation. It excludes capture verification,
loading and both causal snapshot preparations. Individual bounded native calls
are not preempted; desktop process timeout/cancellation remains an outer bound.
If independent validation times out, native alternatives may be returned but no
recommendation is selected. Numerical unavailability is separate from infeasibility.

`complete` means this precisely declared family was exhausted without a time,
candidate, queue, numerical or output limitation. `proven_within_space` never
means a continuous/global optimum. Bounded results say `best_found`; an exhausted
zero-cost lower bound can terminate without claiming family enumeration.

## Executed qualification and remaining scope

`science/evidence/eog-search/` retains exact source/binary/artifact identities,
actual worker requests/responses, generated fixtures, itemized independent
ledgers, test logs and earlier failures. Seven unfamiliar synthetic known-value
cases cover a single truck (3), shared fleet (13), two sequential yard jobs (2),
mixed truck/delayed cheap well (2), nine economic repeated loads (55), a well-only
fractional-knapsack bound (24), and a late first pickup (2.8 infimum). The late
pickup's strict threshold is approached with actual representable controls;
it is not an attained real-number optimum.

Four official practice prefixes run search, then fresh native evaluation and
separate no-DLL independent validation of each exact selected plan. Baseline
costs/trajectories match frozen E3 records; selection checks include exact cost,
outcome, shutdowns, plan hash and complete horizon coverage. These correlated
practice cuts are not held-out qualification. Inspection latency/physical onset
uncertainty, arbitrary continuous optimization, stateful simulation and final
application/render/installation acceptance remain open.

To reproduce with the real schema dependency environment and built native DLL:

```text
python science/qualify_eog_search.py --evaluator-records science/evidence/eog-response/provider --output-dir NEW_OUTPUT --contracts-root contracts/eog --native-library BUILT_DLL --cutoff-unix 1791460800 --cutoff-unix 1791516600 --cutoff-unix 1791622800 --cutoff-unix 1791646200
```

The earlier E3 capture recipe reconstructs the protected prefixes from the pinned
official three-file dataset. No answer key is loaded by this qualifier or runtime.
Lucas owns packaging `eog_search.py`, client/model/history adoption and shared
STATUS, including the revised worker method and retained selection limitations.
