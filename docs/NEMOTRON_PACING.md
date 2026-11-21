# Bounded Nemotron pacing and diagnostics

This Person B increment follows PR #95. It changes hosted transport policy and
retained diagnostics, preserving the numerical engine, strict final evidence,
validation, freshness, tool budgets and explicit operator approval.

## Request policy

All production `NemotronClient` instances share one process-wide monotonic
pacer. It permits one local HTTP POST at a time and admits queued clients in
FIFO order. Every retry and continuation passes through the same gate. Waiting
is asynchronous, cancellable and included in the original investigation deadline.
Cancellation/destruction removes a queued request or releases its POST lease;
it does not clear provider cooldown. Separate processes/machines are not covered
by this gate and must coordinate their account allocation separately.

The initial minimum POST interval is 6000 ms. This is a conservative local
policy, not a measured NVIDIA allowance. Set `BREAKWATER_NEMOTRON_INTERVAL_MS`
in the process environment or local `.env` to an integer from 1000 through 60000.
An invalid configured value prevents posting. Process values take precedence.
Existing credential files and HTTPS credential-routing rules are preserved.
Policy reload is available only between logical requests. The API-key dialog
manages credentials; process/repository configuration is preferred for pacing.

Missing or invalid `Retry-After` uses 2000 ms then 4000 ms exponential backoff,
with independently sampled 0..1000 ms additional jitter. The default maximum
remains two retries. An explicit valid server wait takes precedence over that
fallback; minimum POST spacing still applies. Nonnegative seconds, ordinary
HTTP dates and the existing obsolete HTTP date formats remain supported.
An excessive server wait never gets clipped into an early POST. A wait that
cannot fit the remaining deadline ends with a typed failure and retained evidence.

429/503 responses establish a shared cooldown, including when the request has
no retry allowance left. The terminal cooldown is at least 60 seconds or the
longer valid server wait. A new request, client replacement or cancellation
cannot erase it. Other retryable server responses share an explicit server wait
when supplied. Actual provider waits can exceed displayed diagnostic bounds;
the transport continues to enforce the full wait, with a capping flag in evidence.
Ending a throttled logical request because of its budget, cancellation or client
destruction also establishes the terminal cooldown, even with retries left.

Hosted observed investigations check readiness before binding their existing
capture and initializing its numerical gateway. Known busy/cooldown state leaves
the current evidence untouched and asks the operator to wait and use fresh
observations. This is a readiness check, not a capacity reservation: a later
request may still queue or fail. Capture expiry and stale-source checks remain
unchanged throughout. Native local investigation stays available during cooldown.

## Closed diagnostics

Each logical request optionally retains `transport_diagnostics` version 1:

- Configured interval, fallback base, jitter bound and terminal cooldown.
- Actual time spent awaiting shared admission and bounded remaining cooldown,
  with an explicit flag when the displayed cooldown is capped.
- Up to four actual attempts, numbered in order, with relative monotonic start,
  elapsed duration and HTTP status. Status zero means no HTTP status was available.
- Parsed, bounded server wait and a capping flag when supplied.
- Numeric request/token allowance and remaining counts from explicitly named
  headers, when present and valid. No undocumented quota is inferred.
- Numeric prompt/completion/total usage, when present and valid.
- Only known literal upstream error codes, without provider error prose.

Raw request/response bodies, authorization, header values, unknown fields,
credentials and private reasoning are excluded. Unknown/nonfinite/oversized
diagnostics are rejected by persistence and offline auditors. Attempt sequence,
timing, terminal status and retry totals must match the parent request metadata.
Legacy archives without the block remain valid. Scheduled `retry_delays_ms`
remain distinct from actual `retry_count`; a cancelled wait is not a POST.

## Verification and hosted evidence

Build the affected normal/ASan targets and run:

```powershell
. .\scripts\ci\Initialize-Compiler.ps1
ctest --test-dir <configured-build-directory> --output-on-failure `
  -R '^desktop\.(nemotron_client|agent_orchestrator|agent_gateway|observed_forecast|session_repository|eog_investigator|eog_history|eog_analysis_controller)$'
python -B -m unittest discover -s scripts/ci/tests -p test_candidate_evidence.py -v
python -B -m unittest discover -s scripts/tests -p 'test_nemotron_evaluation*.py' -v
```

Fixtures exercise shared admission, FIFO continuation, queued cancellation,
unchanged deadlines, terminal cooldown after client destruction, original server
waits, bounded jitter, per-attempt status/count privacy and strict history import.
Observed integration checks keep local search available during hosted cooldown
and retain all current-source and approval rejections.

Live rehearsals need a clean source freeze, recorded executable/input hashes
and a fresh fixed declaration. Record the interval and whether the actual account
allowance is known. Retain every failure, deferred/pending case and original
round independently. A passing transport smoke is not a completed investigation.
Pacing does not reserve NVIDIA shared capacity. Any dedicated hosting change,
owner/licensing review or clean-Windows release acceptance is a separate decision.

See [the dated verification report](NEMOTRON_PACING_VERIFICATION.md) for actual
source-bound build/test and hosted outcomes. The targeted live regression is
2/4 and the separate observed-origin round is 1/2; this implementation does not
establish all-green hosted investigation reliability.
