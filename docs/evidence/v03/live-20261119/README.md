# Retained November 19 hosted evidence

These are genuine hosted Nemotron exchanges against self-authored synthetic
portfolio/local-monitoring inputs. No operational plant data, credentials,
authorization headers or raw reasoning are included. Every attempt is retained,
including unsuccessful ones. Dates/source identities and selected tool order
remain in the original archive bytes.

## EOG attempts

| Source | Canonical 1 | Canonical 2 | Canonical 3 | Renamed network | Declared supplement |
| --- | --- | --- | --- | --- | --- |
| `92119fb` | Pass | Pass | Pass | Pass | None |
| `62052b9` | Final-format failure | Pass | Pass | Pass | None |
| `c71249e` | Pass | Ungrounded final rejected | Pass | Ungrounded final rejected | One canonical pass |

The last source is **3/5**, including its single supplementary hosted canonical.
It contains three complete canonical sessions across four canonical attempts.
The fixed original round remains 2/4; the supplement does not erase failures or
provide a renamed-network pass. The earlier 4/4 round is not relabeled as the
latest revision's qualification.

Archives use `eog-<source>-run-<number>.eog-session.json`. The source-qualified
[EOG audit](eog-archive-audit.json) links all 13 files with byte counts, SHA-256,
model-request counts, exact call/result identities, actual tool order and failure
codes. [Original latest-round metadata](eog-c71249e-verification.json) and
[supplement metadata](eog-c71249e-supplement-verification.json) retain their own
outcomes. A separate [setup failure](eog-c71249e-zero-request-setup-failure.json)
made zero model requests because the harness omitted its explicit local provider
Python. That configuration was corrected once before the one hosted supplement.

Both retained [first smoke](protocol-smoke-92119fb.json) and
[latest smoke](protocol-smoke-c71249e.json) passed genuine model-issued tool
execution and continuation. Repeated validation requests still occur model-side;
exact completed results are reused without repeating the provider calculation.

## Observed attempts and native comparison

| Source and origin | Result | Stored numerical runs | Operating authority |
| --- | --- | ---: | --- |
| `92119fb` hosted pump fault | Completed no-plan; missed feasible response | 1 | None |
| `92119fb` hosted missing coverage | Completed no-plan explanation | 0 | None |
| `c71249e` hosted pump fault | Completed no-plan; coarse grid/malformed candidates | 1 | None |
| `c71249e` native search on the same pump capture | Validated response, advisory acknowledgement retained | 4 | None |
| `c71249e` hosted missing coverage | `response_truncated`, failed closed | 0 | None |
| `c71249e` native search on the same missing capture | Completed explicit no-plan | 0 | None |

The [observed audit](observed-archive-audit.json) links the six immutable checkpoint
files, their source/capture identities, coverage, exact native selected validation,
bounded request metadata and acknowledgements. The native pump result costs 5,280
**synthetic units**, with 5.7737 m3 nominal headroom over three hours. It is native
finite-grid evidence, not a hosted success or global optimum.

The latest pump's real search screened 16 coarse combinations and rejected all
16. Its 750-point catalog hint was not executed; malformed candidate-array
requests also executed no batch. Only the baseline trajectory was retained. The
model's prose claims wider screening than those records support. Treat numerical
run/tool records as evidence; do not present that prose as proof the larger grid
ran or the control space is infeasible. A safe null recommendation can still
fail the planning-quality qualification.

## Inspect and reproduce the audit

From the review checkout root:

```powershell
python scripts/verify_v03_evidence.py
```

This standard-library audit checks all 19 archived sessions, hashes, matching
continuations, exact accepted validations and advisory-only authority. It
deliberately reports hosted all-case qualification as **incomplete**. It verifies
retained evidence consistency, not every model sentence, a clean Windows machine
or release consent. Changed archive bytes and a false release-approval claim were
also checked and rejected in separate local copies.

Use Operations' History import for EOG archives and Sandbox History import for
native v2 checkpoints. Imported evidence is historical/read-only. Native
checkpoints preserve original source flags in their raw record; import cannot
restore the original source pointer, actionable approval or publisher epoch.
None of these observed records can apply operating controls.

See [the complete review packet](../../../RELEASE_REVIEW_V03.md) and
[final verification index](../final-verification.json) for tests, actual Windows
scaling, private bundle hashes and pending owner/license/VM gates.
