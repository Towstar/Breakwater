# Nemotron Operations reliability

This Person B increment follows PR #96 at `25af722`. It addresses the retained
Operations completion failures and avoidable provider load. Numerical/scientific
implementations, exact independent validation, capture freshness, operator review
and operating authority remain separate and unchanged.

## Provider load and waits

The fallback retry base is now 8000 ms: the two default retries wait 8 and 16
seconds plus independently sampled 0..1000 ms jitter. Valid `Retry-After` still
takes precedence, and the shared six-second minimum POST interval still applies.
The retry count, original 120-second Sandbox / 240-second Operations limits and
standalone overall deadline are not increased. A wait that cannot fit fails with
retained evidence instead of posting early. Shared terminal cooldown remains at
least 60 seconds, including budget refusal, cancellation and client destruction.

Optional `transport_diagnostics.request_bytes` records the bounded serialized JSON
request length, not its contents or headers. It accepts an integer from zero to
2 MiB; zero requires no actual POST. Oversized input is rejected before posting
and omits this field rather than clipping the length. Legacy diagnostics without
the field remain accepted. C++ persistence and Python audits reject malformed or
inconsistent values. Numeric token usage remains available when supplied by the
provider; no account quota is inferred from a local interval.

NVIDIA documents shared hosted-endpoint saturation for this model and recommends
reducing concurrency or using separately provisioned capacity when consistent
throughput is required. This increment does not provision hosting or promise to
eliminate upstream throttling. [Official NVIDIA guidance](https://docs.nvidia.com/aiq-blueprint/latest/resources/troubleshooting.html)

## Production Operations evidence prerequisites

New production investigations withhold candidate evaluation/search tools until
the model completes a successful `read_observations` call. The model chooses the
captured asset, metric, window and tool order; topology and detection remain
available. No raw read is executed automatically. Failed reads do not satisfy
the prerequisite, and a new investigation resets it. Selecting a plan without
inspection is rejected. A successful read establishes inspection only, not
complete coverage, healthy equipment or a mechanical cause. The numerical
provider and independent validator still establish their own required coverage.

The Qt-only investigator limit flag enables this production prerequisite. It is
not an added sensor/science protocol or a source of numerical truth. Imported
historical sessions retain their existing read-only restrictions.

## Smaller model context, complete local evidence

The existing model presentation now previews at most 16 nested validation proof
rows and explicitly reports the complete retained count and omitted local proof.
All proof references, validation/plan IDs and hashes, cost, coverage, feasibility,
violations, horizons and margins remain available to the model. The complete
validated response stays in the controller and transactional History; independent
validation never consumes the presentation preview.

For message contexts larger than 32 KiB, the wire copy omits old copies of the
application-owned `retained_evidence` index. The current matching index remains
once in the latest matching tool result, or in the system context if no matching
tool result exists. Numerical result objects and tool call identities are not
rewritten. Original local tool-result hashes and full provider records remain
unchanged. This removes repeated metadata without selecting a plan for the model.

## Bounded completion recovery

One format correction and one exact-reference repair may cooperate within the
same original limits: at most 12 tool calls, three candidate refinements,
14 model requests and 240 seconds for Operations. Neither correction grants
another budget. Cancellation, capture replacement, invalid final references and
history imports retain their existing rejection behavior.

When the selected plan already has an admissible independent validation, a
reference repair is JSON-only and requests no repeated numerical validation.
When validation is genuinely missing, recovery offers one model-issued
`validate_plan` call and then a structured final. The application never fills in
IDs, selects another response, validates on the model's behalf, or authorizes
review from malformed/unsupported output. A wrong second answer still fails.

## Reproduce and qualify

Run the focused normal and ASan builds/tests, candidate auditor and evaluation
auditor using the existing commands in [the pacing instructions](NEMOTRON_PACING.md).
New regressions exercise both correction orders, exhausted tool/request budgets,
cancellation, source retirement, reuse of an existing validation, mandatory raw
inspection, large-context identity preservation, complete local proof retention
and oversized request diagnostics.

Hosted testing requires a clean source freeze, executable/input/harness hashes,
a new fixed declaration and serial cases with provider cooldown. Preserve all
failures and the original round denominators. A protocol smoke is not a completed
investigation. This document describes implementation; dated execution evidence
must identify its actual revision. See [the dated verification report](NEMOTRON_OPERATIONS_VERIFICATION.md)
for the clean `db1d0bd` executions, retained ASan timing failure and isolated pass,
4/4 targeted hosted supplement and separate 2/2 observed-origin GUI round.
Independent owner/licensing review and deferred
clean-Windows release acceptance are not granted by local tests or rehearsals.
