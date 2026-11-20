# Reliability and picking follow-up, November 19

Checkpoint: application source `7f6294d` builds in normal and ASan configurations.
The earlier normal checkpoint reached 87/87 passing checks after test-fixture
corrections and a sandbox-specific hard-link rerun. The final-source EOG suites
pass 77 investigator and 58 controller cases in normal; the new admissibility
regression also passes under ASan. The full final-source ASan suite, final hosted
round and updated-app handoff remain pending. Prior evidence is preserved.

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
IDs. Compact `completion_pairs` contain only admissible exact plan/own-validation
pairs. A completed validation loses final eligibility when a cheaper compatible
available evaluation exists; that cheaper evaluation still needs its own
independent validation. The index shares the final validator's unchanged
cheaper-compatible predicate and retains equal-cost alternatives and omitted
counts. The index is presentation only: it neither
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
A fresh exact inventory for 132 reproducible normal linker files (3.039 GiB) is
at `out/reliability-picking/STORAGE_REVIEW.md` and `storage-proposal.json`.
The user approved this inventory separately. The guarded cleanup checked every
path/size/mtime/hash, then removed only those unchanged individual files, recovering
3,262,720,941 bytes. See `cleanup-result.json`. Subsequent builds regenerate matching
symbols. Lossless NTFS compression preserves every rebuilt symbol/object/library
file, with content hashes checked before and after each operation. These records
are time-specific: later recompilation legitimately changes compiler output.

The native protocol smoke passed. The first fixed hosted round, source `726a4da`,
was 2/4: two canonical completions, one first-request 60.206-second timeout with
zero executed tools, and one renamed-network grounding rejection. Independent
archive audit confirmed both accepted finals and both failed records. Repeated
validation remains observable: three reused baseline validations in runs 2 and 4.
Run 4 retained cheaper responses but validated only the baseline, leaving no
admissible final pair. This demonstrated the index eligibility gap corrected in
`7f6294d`; it does not establish the exact rejected IDs, which are not retained.

Next: finish the ASan suite, hardware/default/dense picking checks and fresh fixed
hosted round for the changed source; retain all attempts and observed
pump/missing-coverage cases. Show the latest verified app and close older
instances gracefully with state preservation. Do not replace a failed round's
score with supplementary successful attempts.

Independent same-family agents reviewed the native, EOG and picking boundaries.
No consequential source defect was identified. This is not runtime acceptance,
human owner review or a complete transcript audit. Owner/license, clean-Windows
and final hosted release gates remain separate.
