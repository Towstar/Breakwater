# Person A adoption of G1

At the project owner's request, this session reviewed and accepted the G1
interfaces at commit **f12a82701641f1b4fd3af9ebba579b79f1db3db4**, tree
**8bea204cd22b55539304e2a7e306613787ebf8a3**. The decision is recorded in
[PR #31](https://github.com/Towstar/Breakwater/pull/31#issuecomment-6719191863).
This records the owner's requested adoption and the session's review, not an
independent human GitHub review.

The review compared the exact schema/DTO/validator implementation with
[A's numerical guidance](OBSERVATION_SEMANTICS.md) at `9941859`. It covered monotone
level-volume calibration and domain bounds; cell/interval/unsupported-point
concentration support and acquisition windows; measured delivery versus commands
and pump feedback; absolute uncertainty, required coverage and conflicts; and
non-actionable historical evidence. Original finite out-of-range observations
remain evidence, while unavailable mappings cannot silently repair native state.

Fresh isolated normal MSVC/Qt checks passed `desktop.project`,
`desktop.telemetry_contracts` and `contracts.telemetry`: **3/3**, 3.97 s observed.
The Qt telemetry suite reported **164 passed, zero failures/skips**. The Python
suite required the actual native comparator. Local raw logs are retained in
`out/g1-review-f12a827/`; [compact evidence](evidence/g1-adoption/review.json)
binds the source and executable hashes. The existing hosted/ASan claims remain
B's recorded evidence; this adoption check did not rerun ASan.

Adoption covers G1's declared interfaces, offline validation and synthetic
fixtures. Numerical mapping, a synthetic publisher, coherent forecast-state
construction, detection and physical deployment remain separate work. Both
workstreams must use the accepted contract before dependent G2 implementation;
this batch does not begin G2 or modify the accepted G1 files.

G1 reserves project version 2 for a sensor-bearing legacy-project envelope.
The separate source-feed feature therefore uses project version 3, keeping
storage method/result version 2 independent of serialization. Telemetry support
for allocated projects requires a later reviewed extension. PR #30's experimental
PDE comparator and its unmet targets remain separate and unchanged.
