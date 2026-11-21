# Nemotron pacing verification, November 21, 2026

This increment follows PR #95 at `a7d76a4`. GitHub was checked before work and
fetched again before delivery. Main and the inspected Person A refs did not
advance. PR #95 remains an open draft; its existing six-job Windows CI passed.
This is not approval to merge the stack or distribute a public binary.

## Source boundaries

- `5304ef2` implements process-wide admission, shared cooldown, longer bounded
  backoff, closed diagnostics and observed-start readiness.
- `7c7aff1` additionally forwards the declared interval into rehearsal child
  processes. The fixed hosted rounds below executed this clean revision with
  recorded executable/input/harness hashes.
- `7ed7d715df2e0d98738de12173cbee39642749e4` closes a subsequent terminal-cooldown
  edge case. Budget refusal, cancellation and destruction during throttled
  backoff retain the terminal cooldown even before retry exhaustion. Five new
  regression cases failed before that correction and passed afterward. These
  later changes do not retroactively qualify the earlier hosted rounds.

The numerical engine, science implementations, proof code, exact-evidence
requirements, original tool/time/refinement budgets and operator approval
implementations are unchanged. No rehearsal applied controls or recorded human
advisory acknowledgement. A model `request_approval` call only stages review.

## Offline execution

Local toolchain: Qt 6.12.0, MSVC 14.44.35219.0, Visual Studio 17.14.36623.8.

| Check | Actual result | Executed source |
|---|---|---|
| Transport, gateway, orchestrator, observed, session/EOG persistence and EOG controllers | 8/8 CTest suites passed, normal | `7ed7d71` |
| Same focused matrix, AddressSanitizer | 8/8 CTest suites passed | `7ed7d71` |
| Candidate evidence auditor | 33 Python tests passed | `7c7aff1` |
| Full evaluation audit/fixtures/runner suite | 68 tests, zero failures, one unavailable directory-symlink capability skip | Python files unchanged from `7c7aff1` |
| Normal/compact Sandbox and Operations rendering, D3D11/WARP | 4/4 passed before terminal edge correction | `7c7aff1` |
| Final normal application build and D3D11/WARP normal/compact rendering | Build passed; 4/4 rendering checks passed | `7ed7d71` |
| Fresh production Qt hosted protocol smoke | Passed once: genuine structured tool call, actual execution, matching result continuation and model identity | `7ed7d71` |

Normal and ASan also passed the preceding frozen eight-suite matrix before the
terminal-edge addition. Those runs are retained separately rather than counted
as additional distinct coverage. ASan covers owned C++ targets, not established
instrumentation of prebuilt Qt/Python/Torch. Self-review is not independent owner
review. Full transcript-dependent audit coverage is unavailable.

## Fixed four-case hosted tool regression

The once-only round predeclared two scientific and two Operations investigations.
All four native preflights passed. Requests used the production model
`nvidia/nemotron-3.5-lightning-30b-a3b`, the six-second POST policy, two default
retries and the original 120-second Sandbox / 240-second Operations budgets.
Cases ran serially with at least a 60-second completion-to-next-start gap and
additional retained provider cooldown. No earlier round or failed case was
replaced.

| Case | Required-tool result | Production result |
|---|---|---|
| Scientific healthy network | Passed | Grounded, explicitly validated advisory plan |
| Scientific unfamiliar three-tank chain | Passed | Grounded, explicitly validated advisory plan |
| Explicit Operations plan | Failed | Invalid final format, then ungrounded selection rejected |
| Explicit plan on renamed topology | Failed: omitted raw `read_observations` | Grounded, explicitly validated advisory plan |

**Required-tool qualification is 2/4. Production grounded completion is 3/4.**
Both scientific cases executed sensitivity catalog/analysis tools and separate
native validation. Both Operations cases omitted the required raw-observation
inspection. The rejected Operations final lacked one exact matching validation.
Existing evidence checks rejected it; a completed HTTP exchange is not a
completed investigation or required-tool pass.

There were 31 logical model requests, 43 actual POST attempts, 12 HTTP 429
responses and 31 HTTP 200 replies. All 31 logical requests recovered at the
transport level within their existing budgets. Usage fields were present on the
31 successful replies; no recognized numeric rate-limit headers or provider error
codes were supplied. The exact account request/token allocation remains unknown.
These outcomes do not establish that pacing alone caused recovery or guarantee
shared endpoint capacity.

## Separate observed-origin GUI round

Two cases were declared before execution against the preserved, ten-sensor
project. The local publisher used seed 1337, onset 60 seconds and recovery 180
seconds. Acquisition was paused and a new immutable state captured for each
case. Exact terminal SQLite checkpoint bytes were extracted in query-only mode,
without rewriting evidence. GUI witnessing plus byte hashes is not signed
execution attestation.

| Case | Frozen state | Actual outcome |
|---|---|---|
| Pump loss | Available at 90.805 seconds, nominal expiry 95 seconds; paused playback clock | Failed after a real baseline forecast: three consecutive HTTP 429 replies exhausted retries |
| Missing coverage | Unavailable at 81.089 seconds; no usable numerical initial state | Passed: model inspected observations and returned `no_validated_plan`; no numerical runs or approvals |

**This separately declared sensor round is 1/2.** The pump case retained five
logical requests / seven POSTs / three HTTP 429 replies. The missing case retained
five logical requests / six POSTs / one recovered HTTP 429 reply. The second case
started after the preceding terminal cooldown and declared minimum case gap.
Neither case changes the four-case denominator above.

An initial missing-case UI action was rejected by automatic approval review
because the dialog displayed the preceding retired source. Source inspection
confirmed that start binds the latest eligible telemetry capture and rechecks it
before the model runs. The rejected click created no request; the subsequent
allowed run retained the new unavailable capture identity. Missing initial
coverage permits an explanation only; it cannot authorize numerical planning.

## Evidence and remaining limits

The private, ignored evidence directory is `out/nemotron-pacing/`. It includes
separate fixed-round seals (`live-7c7aff1-01/SEALED.json`,
`observed-live/SEALED.json`), read-only re-audits, normal/ASan logs, exact
regression-before/after results and the preserved user project. Hash checks bind
the recorded bytes; they are not execution attestation or prose certification.
Private traces, credentials, raw authorization/HTTP bodies and model reasoning
are not published with this report.

The preceding `a7d76a4` 0/4 rate-limited round and earlier dated evaluations remain
unchanged. Remaining work includes Operations raw-observation adherence and final
reference reliability, provider allocation/capacity confirmation and a future
fresh qualification on the release candidate. Owner/licensing reviews and the
deferred clean-Windows acceptance remain separate gates.
