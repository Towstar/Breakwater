# Nemotron Operations verification, November 21, 2026

PR #96 was checked before work and its existing Windows run finished with all
six jobs successful at `25af7221c6c5e17552b7d8cdea9b5255c9fdb6ab`.
Native normal/ASan, desktop normal/ASan, CPU Python science and Lean passed.
No CI fix or rerun was needed. [Actual workflow run](https://github.com/Towstar/Breakwater/actions/runs/44953368367).

The new implementation and all execution below use clean source
`db1d0bdad88cf876ca649b4d8ea114b30f3f1674`, stacked above PR #96. Later report
edits do not change the executed source. See [implementation and policy](NEMOTRON_OPERATIONS_RELIABILITY.md).
The numerical/scientific implementations, original investigation budgets, exact
independent validation, source freshness and human approval remain unchanged.

## Local checks

Toolchain: Qt 6.12.0, MSVC 14.44.35219.0, Visual Studio 17.14.36623.8.

| Check | Actual result at `db1d0bd` |
|---|---|
| Focused transport, gateway, orchestrator, observed, persistence and Operations suites | Normal 8/8 passed, 131.54 seconds |
| Same AddressSanitizer matrix | Initial 7/8 passed; one existing correction-phase timing precondition failed |
| Failed ASan orchestrator suite, once in isolation | Passed in 37.01 seconds with the original deadline and no source change |
| Candidate evidence auditor | 33 Python tests passed |
| Full evaluation auditor/fixtures/runner | 68 tests run, zero failures, one unavailable directory-symlink capability skip |
| Latest application and production verification helpers | Normal builds passed |
| Sandbox/Operations normal and compact rendering | D3D11/WARP 4/4 passed, 38.60 seconds |

The initial ASan failure is retained, not replaced by a claim of one clean 8/8
run. `formatCorrectionKeepsDeadlineAndCancellation` did not reach its correction
phase before the existing ten-second artificial deadline under concurrent build
load. It reported no sanitizer memory error. Its isolated suite passed without
extending a test or production deadline. ASan covers owned C++ code, not prebuilt
Qt/Python/Torch. Initial development fixture/count failures are also retained;
the fixtures now explicitly issue the newly required raw read, rather than
disabling that production prerequisite.

New regressions exercise both completion-correction orders, unchanged request
and tool caps, wrong-again rejection, cancellation/source retirement, existing
validation reuse, mandatory successful raw inspection, oversized pre-POST failure
diagnostics, and large-context projection. The controller/SQLite test retains all
32 proof rows and references while presenting a 16-row preview to the model.

## Four once-only hosted cases

All four inputs passed production native preflight before any hosted case ran.
Cases ran serially, with at least 60 seconds from completion to the next start
and the declared six-second POST interval. The model was
`nvidia/nemotron-3.5-lightning-30b-a3b` at NVIDIA's existing hosted endpoint.
There were no retries of failed cases or replacements of an earlier round.

| Declared case | Grounded completion and required tools | Model requests / actual POSTs | HTTP 429 |
|---|---|---|---|
| Healthy Sandbox, scientific sensitivities | Passed | 9 / 9 | 0 |
| Unfamiliar three-tank chain, scientific sensitivities | Passed | 8 / 8 | 0 |
| Operations, explicit candidate | Passed | 7 / 7 | 0 |
| Renamed Operations topology, explicit candidate | Passed | 10 / 10 | 0 |

This supplement is **4/4**, with **34 HTTP 200 responses and zero HTTP 429**.
Both scientific cases executed actual sensitivity tools and retained linked
scientific archives. Both Operations cases inspected original observations,
evaluated an explicitly composed candidate and obtained independent validation.
The production controller also grounded the selected response in its exact
retained plan/hash/validation pair. No operator acknowledgement or control
application occurred; model `request_approval` calls only staged review.

Maximum reported successful prompt-token counts were 24,553 and 42,044 for the
two Operations cases, compared with 48,489 and 51,654 in the preceding pacing
round. Their tool selections and request counts differ, so this is an observed
context-size comparison, not a controlled causal benchmark. New request-byte
diagnostics retained actual lengths, with maxima 53,871 and 93,711 bytes for
these cases. The full local evidence and original tool-result hashes remain.

The model still voluntarily repeated exact Operations validations once and
twice respectively. Existing duplicate-call completion bounds stopped further
looping; this is not a claim of zero repetition. One optional model explanation
also contains inconsistent repair-time arithmetic. The UI's authoritative
summary comes from stored numerical evidence; optional model narrative remains
separate and explicitly unverified. Grounded IDs do not certify arbitrary prose.

## Separate witnessed sensor-origin round

Two cases were declared before execution, using the saved ten-sensor project,
seed 1337, onset 60 seconds and recovery 180 seconds. Actual acquisition was
paused before recovery, and each current immutable capture was used by the
production GUI. The pump case was captured at event 108.44 seconds with available
coverage; missing coverage was captured at 159.21 seconds with immediate expiry.
Cases respected the completion gap and shared provider policy.

| Declared case | Actual result | Requests / POSTs | HTTP 429 |
|---|---|---|---|
| Observed pump loss | Exactly validated advisory; real forecast/search/validation | 7 / 7 | 0 |
| Observed missing coverage | Grounded no-plan refusal; zero numerical runs | 4 / 4 | 0 |

This separate round is **2/2**, with **11 HTTP 200 responses and zero HTTP 429**.
The missing case used one existing bounded format correction. Both have zero
human approvals. No case applied controls. Archives were exported as exact
production SQLite checkpoint bytes through a query-only connection and audited.

Starting the next epoch correctly retired the earlier pump advisory's eligibility.
Its original completed checkpoint remains immutable in the evidence packet.
A separate later checkpoint differs only in current/approval-status annotations
and monotonic checkpoint/time metadata; numerical results, source evidence and
events are identical. The unchanged completion auditor rejects that altered
recommendation as the original final. It is retained as retirement evidence,
not substituted for or counted as another completed case.

## Evidence and reproduction

Private artifacts are under ignored `out/nemotron-operations/`: normal/ASan logs
and XML, the original timing failure and isolated rerun, Python logs, render
checks, scoped self-review, sanitized request metrics and exact archived results.
The four-case declaration binds source, executable, input and harness hashes.
The separate sensor declaration binds the desktop, input project and publisher.
These are witnessed byte/source bindings, not signed execution attestation.

| Retained seal | SHA-256 |
|---|---|
| `live-db1d0bd-01/SEALED.json` | `26ce667bc0330ab83d6184ab45556ad9915aa7a13ab6fe0be1a6304b699c697a` |
| `observed-live/SEALED.json` | `cfc8779452f517afcb35d9f0b409b12f1ab35d334c934831de80f000df9f74d3` |

Use the focused normal/ASan and Python commands in [the pacing instructions](NEMOTRON_PACING.md#verification-and-hosted-evidence).
Run the four renderer checks from a configured normal build with the pinned Qt
bin directory on PATH and credentials absent:

```powershell
ctest --test-dir out/portfolio-desktop --output-on-failure -j 1 `
  -R '^desktop\.(ui_overhaul|eog_workspace)_(normal|compact)$'
```

Fresh hosted runs require their own declaration and artifacts; use the production
evaluation flow in [the evaluation instructions](NEMOTRON_EVALUATION.md).
Never relabel or overwrite the retained results. Prior targeted 2/4, observed
1/2 and expanded 22/24 rounds retain their original denominators and failures.

This pass did not verify the account's allowance or exercise real upstream
backoff, because none of its 45 POSTs received HTTP 429. Retry timing remains
covered by controlled transport tests. Shared capacity can still throttle later
sessions; the exact provider quota remains unknown. This is scoped self-review
and focused qualification, not complete transcript, independent owner,
clean-Windows, numerical-accuracy or public-release approval.
