# Reliability and picking follow-up, November 19

Completed application source: `29df04f63a6af81671e1d223cb005c350a4a7289`.
Both normal and ASan builds pass. The latest fixed hosted EOG round passed all
four cases; both desktop observed rehearsals also completed correctly. Normal
WARP and ASan WARP each picked all six canonical assets. The latest app was
shown and is the only open Breakwater instance. This is a bounded local
qualification, not a release or a general model-reliability benchmark.
GitHub run `44647353356` subsequently exposed compact label culling: its normal
job passed 86/87 checks, with six correct body picks but five visible labels at
1008x672. The other five jobs passed. The [raw CI checkpoint](evidence/v03/reliability-picking-20261120/ci-checkpoint.json)
preserves this failure and distinguishes the PR head from GitHub's merge checkout.
The compact verification correction remains pending in this checkpoint.

The earlier normal checkpoint resolved all 87 checks after test-fixture
corrections and a sandbox-specific hard-link rerun. A single full ASan run at
`7f6294d` passed 87/87. Its EOG investigator/controller suites passed 77/58 cases
in normal. At `29df04f`, the subsequent concise-generation payload change was
checked with the client and real WARP tests in both builds and the live cases.
The full 87-suite matrix was not rerun for that Boolean payload change.
Exact source distinctions, hashes and earlier failures are retained in the
[test/render manifest](evidence/v03/reliability-picking-20261120/tests-and-rendering.json).

GitHub was fetched before changes. Main remained `3116b3d`; no remote refs
changed. PR58 still had no formal review. No main merge or release is authorized
by these fixes. The previous `c71249e` handoff and its failures remain historical.
PR82 is stacked on PR81; neither this task nor the evidence authorizes merging
the release stack. A later fetch again found main unchanged.

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
and 4,096 total output cap. `29df04f` uses `enable_thinking=false` for ordinary
turns as well as forced JSON finals, favoring concise tool output within the
existing limits. The exact wire payload has a regression check. Self-hosted
NIM documentation uses a different reasoning-budget parameter; it does not
justify substituting the hosted parameter. The latest actual hosted round
confirms tool calling works with concise generation on this endpoint.

Regression coverage added: one correction only, repeated truncation, fatal errors,
original deadline, cancellation/reentrant retirement, missing observed coverage
and inspection, exact validated run references, and EOG current/stale/unavailable
evidence. The tests were executed as recorded in the source-qualified manifest.

## Renderer picking

See [WARP picking verification](WARP_PICKING.md). The previous five-of-six report
did not name its failed asset. Guessed probes could land between treatment's
vessels, and probing immediately after camera motion could use unsynchronized
transforms. The new verifier projects actual visible pickable bodies, requires
the exact unrestricted nearest-object hit, and waits two rendered frames.
It retains failed IDs, probe coordinates/hits, actual adapter, screenshot and
measurement duration. Geometry and user pick masks are unchanged.

All six canonical bodies pass under D3D11/WARP in normal and ASan builds.
Hardware default and dense checks pass 6/6 and 128/128 picks, with 62.49 and
55.58 FPS respectively, exceeding their 60/30 targets. Actual adapters, full
ten-second samples, two settled frames and unchanged numerical identity were
verified. Software FPS stays diagnostic. One successful probe per asset at the
settled view does not establish every mesh or every viewing angle is pickable.

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

The second thinking-enabled round, code `7f6294d` / checkout `efd82a6`, was also
2/4. Both rounds remain unchanged. The third fixed round at `29df04f` passed
4/4: three canonical cases and one renamed/custom topology, each selecting an
independently validated compatible-minimum response costing 1,810 on these
synthetic portfolio fixtures. All 52 hosted requests returned HTTP200, with no
transport retry, format correction or transport error. Each case used all twelve
tool calls and thirteen model requests. There were 26 cached validation reuses;
repeated model requests have not been eliminated. These are not the original
official EOG October 8 cost figures. See the [three-round manifest](evidence/v03/reliability-picking-20261120/hosted-rounds.json).

The latest desktop-to-publisher pump-loss case, seed 1337, paused at event
174.27 seconds, produced a nominally validated advisory at cost 5,280 and
minimum headroom 4.466 m3. The missing preset, same seed, paused at 76.47 seconds,
retained an unusable tank-A inventory sample (17.44 seconds old versus a
five-second maximum age) and completed with `no_validated_plan`, zero
runs and zero approvals. Both were genuine hosted investigations through the
desktop, not local-search substitutions. Their exact completed SQLite session
archive bytes were copied using read-only queries after a native save-dialog
automation limitation. No operating controls were applied. See the
[observed-case manifest](evidence/v03/reliability-picking-20261120/observed-cases.json).

Model explanations remain untrusted: the pump prose asks for approval/application
even though observed review only acknowledges an advisory; the missing-case
prose overstates unavailable coverage as mathematical infeasibility. The actual
mapper, retained numeric references and acknowledgement-only UI remain
authoritative. A four-case round and two observed cases do not establish broad
reliability, diagnosis accuracy or comparative model superiority.

The original workspace was preserved as a validated project-v2 snapshot and
reopened on `29df04f`. Completed evidence remains in History. The publisher is
paused, popouts are closed, and one latest Sandbox window remains open. No
approval was restored. Legacy Python health detected Python 3.12.13 and Torch;
that health result does not establish PhysicsNeMo or scientific execution.

Independent same-family agents reviewed the native, EOG and picking boundaries.
No consequential source defect was identified. The independent same-family
[evidence audit](evidence/v03/reliability-picking-20261120/AUDIT.md) checked the
archives and records the prose limitations. This is not human owner review,
cross-family review or a complete transcript audit. Owner/license and actual
clean-Windows release gates remain separate.
