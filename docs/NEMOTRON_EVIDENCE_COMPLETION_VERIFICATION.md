# Nemotron evidence-completion verification

Recorded November 21, 2026, America/Chicago; execution continued into November 22
UTC. The original [PR #97](https://github.com/Towstar/Breakwater/pull/97) failure was
fixed at `f8b0067235677c6a32e3cd5781a71310bd4e508f`, preserving mandatory
model-issued raw-observation inspection. Its [Windows run](https://github.com/Towstar/Breakwater/actions/runs/44969784199)
passes all six jobs, including 94/94 tests in each desktop job.

The fresh hosted corpus, scientific supplement and first private package below
use clean application source **`a6b6aa94c8bfe18acc03197bc8faaae013e92eef`**.
The later UI-test correction uses **`9e97afa7025e4b3ed69a9e80eea7610452450fad`**.
It changes the verification driver's rendered-layout wait, with no production
planning, numerical, scientific, validation or approval changes. Later report
commits and packages do not relabel these executed sources or binaries.
See [implementation and policy](NEMOTRON_EVIDENCE_COMPLETION.md).

## Full 24-case corpus

All 11 input families passed native preflight. Every declared case was attempted
once. The result is **23/24**, so full hosted qualification remains incomplete.
The 22 CLI cases pass 21/22, and both witnessed observed GUI cases pass.

| Input family | Passed / declared | Logical requests / actual POSTs |
| --- | ---: | ---: |
| Canonical pump failure | 2/2 | 11/11 |
| Healthy Sandbox | 2/2 | 14/14 |
| Three-tank chain | 2/2 | 9/9 |
| Shared treatment | 1/2 | 18/18 |
| Balanced cycle | 2/2 | 16/16 |
| No supported remedy | 2/2 | 24/24 |
| Recorded Operations | 2/2 | 22/22 |
| Renamed Operations topology | 2/2 | 20/20 |
| Multiple failures and shared fleet | 2/2 | 24/24 |
| Missing tank channel | 2/2 | 26/26 |
| Stale tank channel | 2/2 | 25/25 |
| Observed pump loss | 1/1 | 6/6 |
| Observed missing coverage | 1/1 | 4/4 |

All **219 POSTs returned HTTP 200**. There were zero retries and zero HTTP 429
responses. Every retained request records the 6,000 ms minimum interval, 8,000 ms
fallback base, 1,000 ms jitter bound and 60,000 ms terminal cooldown. The declared
60-second CLI gaps and provider-deferral records passed hash-bound re-audit.
The GUI transitions additionally retain conservative admission records based on
the previous terminal checkpoint and its latest cooldown diagnostics.

The core retains 105 numerical runs and 207 requested tool calls. It records 21
known duplicate calls, 19 known expensive duplicates, four exact validation
reuses, and **zero duplicate expensive executions**. Four rejected calls lack
dispatched arguments, so duplicate-count completeness is false. These counts
do not establish zero repetition or a causal improvement over another model/run.
Guidance is advisory; exact dispatch/cache/budget checks remain authoritative.

### Retained failure

`sandbox-shared-treatment-p1` exhausted the existing candidate-refinement
allowance and requested further refinement tools that were no longer offered.
Four calls were rejected. One candidate failed independent validation; the final
selection had no eligible validation evidence and was rejected as
`ungrounded_recommendation`. All 13 requests returned 200. No recommendation was
restored and no allowance was increased. The separate p2 objective passed but
does not replace this failure. The trace demonstrates model noncompliance with
existing guidance; it does not prove an absent budget check or numerical defect.

### Observed GUI execution

The two cases used the actual production GUI, the saved ten-sensor synthetic
project and seed 1337, with onset 60 s and recovery 180 s. Pump acquisition was
paused/captured at 116.06 event seconds, with available coverage and expiry 121.03 s.
The selected 5280-synthetic-unit response has its own exact native validation and
minimum retained headroom 5.310559576 m³ over three hours. This is nominal modeled
evidence, not a saving against an infeasible zero-control-cost baseline.

The next `missing` epoch was paused/captured at 113.27 s. Its ten sample references
retain one unusable inventory mapping, unavailable coverage and immediate expiry.
The model returned a grounded no-plan result with zero numerical runs. Both
cases have zero human approvals. Neither applies controls or clears incidents.

Original completed SQLite BLOBs were extracted query-only before source
retirement. The later pump checkpoint changes only current/approval annotations
and checkpoint/time metadata. Numerical evidence and events are unchanged. It
is retained separately, and the unchanged completion auditor rejects it as the
original completed case. The live panel also showed the retired displayed source
and distinct unavailable next capture before starting the second investigation.

## Separate four-case supplement

All four inputs passed fresh native preflight. Required-tool qualification is
**3/4**; all four production investigations returned grounded native plans.
This supplement neither repairs nor changes the 24-case denominator.

| Case | Required-tool result | Requests / POSTs |
| --- | --- | ---: |
| Healthy scientific sensitivities | Passed | 9/9 |
| Three-tank scientific sensitivities | Failed evidence-link qualification | 13/13 |
| Explicit Operations candidate | Passed | 9/9 |
| Explicit renamed-topology candidate | Passed | 10/10 |

All 41 supplement POSTs returned 200, with zero retries, HTTP 429 responses or duplicate expensive
executions. Provider-deferral re-audit passed. Both explicit-plan cases inspected
original observations, evaluated model-composed candidates and obtained matching
independent validation. No case applied controls or recorded human approval.

The chain case first requested sensitivity at a captured control bound, which was
rejected. Two later sensitivity calculations completed within tolerance, but
used runs `e707e9a5-7b88-457a-9100-8b29e880fa2d` and
`36eec59f-f966-4d54-830b-996731f8175a`. The model explicitly validated a different
selected run, `356f1e8b-0104-47c3-8d92-0c08977214c0`. The unchanged scientific
checker therefore rejected the required matching-run evidence. Both scientific
BLOBs were preserved separately for diagnosis. Their existence does not transfer
validation or qualify the selected plan scientifically. The numerical/scientific
implementations and acceptance rules were not weakened to obtain a pass.

Together, the recorded rounds contain 260 successful HTTP responses and zero HTTP 429 responses.
This establishes availability under these recorded conditions, not account quota,
general completion reliability, calibrated field accuracy or model superiority.
Live 429 recovery was unnecessary here; offline tests cover its bounded behavior.

## Desktop and offline checks

Local tools: Qt 6.12.0, actual MSVC 19.44.35219.0, Visual Studio 17.14.36623.8,
CPython 3.12.13 and Torch 2.15.0+cpu. Hosted CI reported MSVC 19.44.35230.0.

| Check | Actual result |
| --- | --- |
| Frozen-source normal full CTest run | 91/94 in the sandbox; three hard-link creation capability denials |
| Same three suites in inspected credential-free host processes | All 25 constituent cases passed; original denials remain retained |
| Frozen-source focused ASan runtime matrix | 8/8 passed |
| Frozen-source ASan workspace/observed UI with captures | 8/8 passed at normal, compact, short and minimum sizes |
| Evaluation helper suite | 72 tests, zero failures, one unavailable directory-symlink capability skip |
| Candidate evidence auditor | 33 passed |
| Release handoff helper | 20 passed |
| Original PR #98 normal desktop CI | 94/94 passed |
| Original PR #98 ASan desktop CI | 90/94; all four failures at the new reader interaction |
| Corrected reader, screenshot capture disabled | 4/4 normal and 4/4 ASan UI helper executions passed |
| Corrected reader, actual PNG capture enabled | 4/4 normal executions passed; expected PNGs verified |

The [original PR #98 run](https://github.com/Towstar/Breakwater/actions/runs/44973699936)
remains preserved. Its four failures were reproduced locally without screenshot
capture. History readiness was followed by a mouse click before explicit layout
settling. Optional `grabWindow()` capture had made earlier successful checks
depend on rendering as a side effect. The correction waits for rendered frames
before the real click and boundedly observes popup content. It preserves text,
read-only, viewport, no-review, Escape and focus-return assertions, plus the
original 90-second test and production investigation budgets.

One private capture attempt supplied the wrong environment variable. Its four
assertion passes did not save images; its original declaration and a superseding
correction remain retained. The actual replacement requires
`BREAKWATER_UI_EVIDENCE_DIR`, an artifact directory and the expected PNG files.
The [three-image gallery](images/portfolio/README.md#evidence-reader-and-workspace-9e97afa)
uses only inspected, unmodified images from that verified attempt.

These local clients reported DPR 1.75; hosted failures reported DPR 1. They are
not acceptance tests for every real Windows display-scale setting. ASan covers
owned C++, with prebuilt Qt/Python/Torch outside that instrumentation scope.
Current PR checks are linked from [PR #98](https://github.com/Towstar/Breakwater/pull/98);
the original failed attempt is not relabeled as green.

## Private package and release boundary

The first frozen-source package passes ten sanitized installed checks and ten
more after actual ZIP extraction into a path containing spaces and Unicode.
Read-only audits verified 3,856 files in each deployed copy and 2,535 source ZIP
members. The inventory contains 94 notice files and 24 Qt SBOM files, plus actual
CPython and six Python-package metadata/license records. The basic bundle omits
Torch/PhysicsNeMo; optional scientific execution used the separate pinned CPU
environment. An initial open-executable packaging failure remains preserved.

The private handoff retains 19 manual checks as `not_run`, clean-machine status
false, owner/licensing reviews pending and public-release approval false. Later
candidate refreshes retain their own exact source/app/ZIP manifests and never
inherit the hosted qualification of these earlier binaries. The user elected to
keep the clean-Windows test kit ready for later. No public binary release, root
license grant or integration into main is authorized by this verification.

| Frozen evidence | SHA-256 |
| --- | --- |
| 24-case `SEALED.json` | `46a6d0482c8e622917bdbc641d689b88b272714c277384f50ebe9725501c78a1` |
| Four-case `SEALED.json` | `e714f70f2d5c39430bb2984860a234eb99d0418ac08cbfe440369514f02abaa1` |
| Frozen desktop executable | `54ea919703ecd06bff064444501a48e3a5bbe1af3bde663f168ec7ec625201bb` |

Private artifacts remain under ignored `out/evidence-completion/`: every original
attempt, declarations, source/input/binary bindings, request diagnostics, original
and retired checkpoints, scientific archives, full normal/ASan outputs, actual
PNG checks and private delivery manifests. Active databases and the user's
mutable working project must stay outside any immutable evidence index.

## Attention

Review provenance is scoped implementation self-review, not independent owner
approval. Actual callers, authority/cache boundaries, history restoration and UI
state transitions were inspected and exercised. Complete transcript-dependent
audit coverage is unavailable. The two hosted qualification failures, original
CI failure, sandbox capability failures and capture-label correction remain
visible. Remaining work includes model completion/compliance and scientific-run
linkage, owner/licensing review, ordered integration and actual clean-Windows
acceptance. No earlier failed round or unexecuted check is counted as a pass.
