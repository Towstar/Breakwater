# Reliability and picking follow-up, November 19

Status: source changes and independent same-family source reviews complete;
compilation, normal/ASan execution, hosted qualification and updated-app handoff
are pending. This document does not supersede the prior candidate's evidence.

GitHub was fetched before changes. Main remained `3116b3d`; no remote refs
changed. PR58 still had no formal review. No main merge or release is authorized
by these fixes. The previously verified application is `c71249e`.

## Completion reliability

Both investigation loops now route a transport `response_truncated` failure into
the existing single final-format correction. Partial text and partial tool calls
are discarded. Recovery shares the original deadline and request allowance,
offers no tools, and respects cancellation, source retirement and generation.
Another truncation is terminal. Native recovery additionally requires the
network/control inspection and, for observed sessions, observation inspection.

The native loop presents exact current independently validated run references.
The EOG controller supplies a bounded current-capture index with up to eight
existing plans, their own validation references, eligibility and bounded event
IDs. It records omitted counts. The index is presentation only: it neither
selects a response nor repairs IDs, runs tools or changes final admission.
Exact plan/hash/validation/current-capture and cheapest-compatible checks remain.
Every validation tool call still consumes its allowance, even when reused.

The observed pump failure previously screened only 16 combinations; its larger
catalog hint was not executed. Prompt guidance now distinguishes suggested grids
from performed experiments and explains that candidate batches must be arrays.
Malformed candidate arguments remain rejected. No search/tool budget is enlarged.

The [NVIDIA hosted API reference](https://build.nvidia.com/nvidia/nemotron-3.5-lightning-30b-a3b/apireference)
documents `reasoning_budget`. Keep the original 1,024-token ordinary allowance
and 4,096 total output cap. Thinking is explicit for ordinary turns and disabled
for forced JSON finals. Self-hosted NIM documentation uses a different parameter;
it does not justify substituting the hosted parameter.

Regression coverage added: one correction only, repeated truncation, fatal errors,
original deadline, cancellation/reentrant retirement, missing observed coverage
and inspection, exact validated run references, and EOG current/stale/unavailable
evidence. These are source additions until executed.

## Renderer picking

See [WARP picking verification](WARP_PICKING.md). The previous five-of-six report
did not name its failed asset. Guessed probes could land between treatment's
vessels, and probing immediately after camera motion could use unsynchronized
transforms. The new verifier projects actual visible pickable bodies, requires
the exact unrestricted nearest-object hit, and waits two rendered frames.
It retains failed IDs, probe coordinates/hits, actual adapter, screenshot and
measurement duration. Geometry and user pick masks are unchanged.

Acceptance requires all six canonical bodies under D3D11/WARP in normal and
ASan builds, followed by hardware default/dense scene checks. Software FPS stays
diagnostic. One successful probe per asset at the settled view does not establish
every mesh or every viewing angle is pickable.

## Verification and handoff boundary

The accepted release ledger requires at least 8 GiB free for full builds,
qualification or packaging. About 7.1 GiB was free at this task's storage review.
A fresh exact proposal for 132 reproducible normal linker files (3.039 GiB) is
at `out/reliability-picking/STORAGE_REVIEW.md` and `storage-proposal.json`.
No file from that proposal has been deleted; user approval is pending. The
earlier 23.225 GiB approval does not authorize this new inventory.

After approval: verify each listed path/size/mtime/hash, remove only unchanged
approved files, measure free space, and run focused normal/ASan client,
orchestrator, observed, EOG and WARP checks. Run broader affected suites and
hardware scene checks once those pass. Commit the qualified source before live
rehearsals, retain every hosted attempt in a new evidence directory, and report
the fixed three-canonical/one-renamed round plus observed pump/missing-coverage
cases without overwriting older results. Only then install and show the latest
verified app, closing older instances gracefully with state preservation.

Independent same-family agents reviewed the native, EOG and picking boundaries.
No consequential source defect was identified. This is not runtime acceptance,
human owner review or a complete transcript audit. Owner/license, clean-Windows
and final hosted release gates remain separate.
