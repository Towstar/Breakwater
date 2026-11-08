# Private committed-state response evaluation

Frozen runtime: **2448fa0b7bc92bc5c8a408872b497c70c559b744**. This increment
adds native `eog-native-committed-seconds-0.1.0` and separately implemented
`eog-rational-committed-validation-0.1.0`. It is stacked on the causal commitment
projection in PR #52. Existing native ABI1 layouts/entry points, method
`eog-native-seconds-0.1.1`, public EOG contract72cadd1 and worker0.3.4 are retained.

The new private ABI takes an explicit post-event decision snapshot, proposed
future response and three collections of known commitments: occupied physical
system/truck slots through final return, strictly future full-load pickups, and
previously stopped productive wells. All decision-time events have already
been incorporated into the initial state. Busy intervals are half-open, so a new
call exactly at return is allowed and one binary64 ULP before return is rejected.
Reservations survive a facility repair; pending pickups must precede repair.

Known and proposed pickups share inventory and event ordering. Simultaneous
loads require their full combined inventory; threshold crossing retains the
existing precedence over a tied pickup. Previously stopped wells retain future
opportunity cost through repair, with voluntary/automatic loss counted once.
Costs cover future committed and proposed pickups plus future revenue loss.
Already charged loads and previously accrued loss are sunk costs and excluded.
Invalid inventory/controls remain invalid; unrepresentable arithmetic or bounded
output/work exhaustion remains unavailable, with no successful partial outcome.

`native_eog_committed.py` copies and normalizes computational inputs before
invocation and binds the result to exact snapshot/plan/commitment hashes. An older
DLL lacking the extension is unavailable; it cannot silently substitute a free
fleet. `eog_committed_validation.py` independently validates topology, resource
calendars, events, conservation and costs with rational primitives rounded to
binary64. It does not call the native evaluator, simulator or files. It reuses
only the existing bounded arithmetic primitives, and can reject altered native
method, context, cost or inventory results.

Actual checks and source/binary identities are retained in
`evidence/eog-committed-response/`. Fourteen focused methods include 400 generated
native/reference comparisons, 60 empty-context comparisons with the unchanged
legacy entry point, and two actual persistent-plant suffix comparisons. The
latter branch after a pickup, optionally after a well stop, then compare final
inventory and incremental future cost, explicitly subtracting the actual sunk
prefix. Separate native tests execute 43 checks per normal/ASan build.

This remains caller-supplied computational state. It does **not** yet compile an
authenticated Model/View/History prefix into that snapshot/context, authenticate
execution, optimize a commitment-aware response, expose a public worker operation,
or grant operating authority. Stateful planning and the B-owned additive
simulation boundary/application controls remain outstanding. Imported evidence
cannot become live dispatch authority. No new installed/UI/model result is
claimed for this private extension.

For integration, retain the unchanged v1 API and stage the new Python bridge and
independent validator only when their private caller is deliberately introduced.
The reference also needs existing `eog_plan_validator.py`; the native extension
needs a freshly built response DLL. The existing production worker has no call
to this extension. Lucas owns public mapping, packaged adoption and shared status.
