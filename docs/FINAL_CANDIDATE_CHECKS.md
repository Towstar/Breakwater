# Final hosted candidate evidence

`scripts/audit_candidate_evidence.py` checks one declared seven-case round
offline. It does not launch the application, contact NVIDIA, recompute numerical
results or attest that a process ran. Source/executable bindings come from the
witnessed launch manifest; immutable archive checks verify their retained content.
This is separate from clean-Windows, author/licensing and numerical accuracy gates.

From a source checkout, with an existing Python 3.12 interpreter:

```powershell
& 'C:/ABSOLUTE/Python/python.exe' -B `
  'C:/ABSOLUTE/Breakwater/scripts/audit_candidate_evidence.py' `
  'C:/ABSOLUTE/NEW-ROUND/manifest.json'
```

Exit 0 means all seven declared cases passed the evidence audit. Exit 2 means a
valid round remains pending/incomplete. Exit 1 means malformed or inconsistent
evidence; diagnostics are sanitized. Preserve the manifest, original outputs and
audit report, including unsuccessful attempts. Do not repair model references.

## Manifest version 1

The top-level fields are `manifest_version: 1`, `source_commit` (40 lowercase hex
characters), `dirty: false`, `model_identity`, `status`, `executables` and `cases`.
The supported model is `nvidia/nemotron-3.5-lightning-30b-a3b`. Overall status is
`pending`, `failed`, `incomplete` or `passed`. Every launch records the same frozen
source and the actual executable checksum, rather than the later documentation
checkout's HEAD.

`executables` maps `desktop`, `eog_live_check` and `nemotron_smoke` to their actual
SHA-256 values. Each case appears once:

| Case | Executable | Evidence |
| --- | --- | --- |
| `protocol_smoke` | `nemotron_smoke` | Actual stdout log and process `exit_code` |
| `canonical_1`, `canonical_2`, `canonical_3` | `eog_live_check` | Each distinct exported session, CLI summary and process exit |
| `renamed_topology` | `eog_live_check` | Fresh session on a different dataset identity, summary and exit |
| `observed_pump` | `desktop` | Completed hosted observed-origin session with exact independent validation |
| `observed_missing_coverage` | `desktop` | Hosted no-plan session retaining unavailable coverage and no numerical runs |

Each row has `case`, `source_commit`, `executable`, `executable_sha256` and
`status` (`pending`, `failed` or `passed`). Its `evidence` reference contains
`path`, `bytes` and `sha256`. Operations also requires a `summary` reference with
the same three fields and `exit_code`. Paths are relative to the manifest's
directory, remain within it and refer to bounded files. Pending/failed cases may
have no evidence yet; a passing case cannot. Do not write invented hashes or
placeholder evidence into a passing manifest.

The auditor checks call/result order, actual model-requested validation linked to
the provider exchange, final plan/hash/reference identity, observed snapshot/run
bindings, source/executable consistency and advisory-only authority. Recovered
transport requests and safely rejected unavailable tools remain visible.
Incomplete tool prefixes may be retained in failed cases, never promoted to a
passing case. Canonical sessions must be distinct on the same dataset; the
renamed case must have a different dataset identity. These checks do not evaluate
every sentence of model prose or establish comparative LLM superiority.

## Execution and delivery boundary

Use the existing smoke executable and `Run-PortfolioRehearsals.ps1` with explicit
local worker configuration and fresh output directories. Exercise observed cases
in the actual desktop, retaining preset, seed, source/epoch, capture and completed
history. Keep credentials in the process/local account configuration, never argv,
manifests, source ZIPs, screenshots or logs. Keep the existing time/tool budgets.

After a code fix, freeze a new candidate and repeat the complete declared set.
Do not assemble a pass from successful cases on different builds. The older
`verify_v03_evidence.py` remains a dated audit of its original evidence; it is not
relabelled as current qualification. See [candidate readiness](CANDIDATE_READINESS.md)
for packaging, exact-head owner review and pending external acceptance.
