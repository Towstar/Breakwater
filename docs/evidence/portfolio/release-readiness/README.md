# Final candidate qualification, November 18, 2026

This is a prepared verification candidate, not a published v0.2.0 release.
Binary/source revision: **ad45d4538bc30fd9839fc79171ae1ae8dc71d399**, clean.
Later README/evidence commits do not change that binary's identity. The machine-
readable index is [readiness.json](readiness.json); exact ZIP and executable hashes
are in [candidate-ad45d45.json](candidate-ad45d45.json).

## Executed checks

| Check | Actual result and scope |
|---|---|
| Native Windows correctness | Normal 14/14 CTests, ASan 14/14 CTests. |
| Combined desktop/native Windows | Normal 72/72 CTests, ASan 72/72 CTests. Each reports 1,021 Qt passes plus one explicit optional PhysicsNeMo skip; totals include fixture lifecycle methods. Four native UI window sizes and D3D11/WARP rendering are included. |
| CPU science | 458 run, 454 pass, four explicitly optional PhysicsNeMo skips. |
| Lean | Pinned build and negative-assumption checks; 54 algorithm assumption audits. This does not prove the desktop or field model. |
| Release/CI helpers | 43/43 in the CI bootstrap environment, including 16 handoff-integrity cases and six bundle-readiness cases. |
| Portable package | 9/9 real checks from a relocated path containing spaces/Unicode, sanitized development variables and no supplied key. Two non-live probes correctly exit 3 and report `live_not_enabled`; this is their expected result. |
| CLI regression | Seven actual CLI cases, 1/1 CTest in each normal/ASan build. Restricted hard-link failures were retained; the unchanged cases pass with the normal Windows token. |
| Real optional Science | 23 worker tests, three calibration tests and one actual desktop staged-fit/explicit-apply case pass. The desktop Qt total is three including init/cleanup. Executed at `3658f41`; engine/science/proofs/scenarios/contracts and desktop test trees are unchanged at `ad45d45`. Torch/PhysicsNeMo are not bundled. |
| View performance | One hardware D3D11 camera sweep each: default 62.489 FPS, maximum 128 assets/256 routes 62.490 FPS. All 6/128 actual bodies pickable. Radeon 880M laptop, 1440×900 client, DPR 1.75; no hardware-wide or VM FPS claim. |
| Manual desktop | Exact run/control/validation review, explicit simulated apply, disabled repeat apply, healthy reset, Escape cancellation of Open, real Operations search/validation/completion and historical hosted import checked. This is Codex Computer Use self-checking, not Ethan's review or actual Windows display-scaling acceptance. Manual restart is pending. |

[Windows CI run 44431840603](https://github.com/Towstar/Breakwater/actions/runs/44431840603)
is green for all six jobs. [CI metadata/counts](ci-windows-ad45d45.json) retain
actual runner/compiler versions, suite totals, exact optional skip reasons and
artifact IDs/digests/expiry. [Selected source-qualified log lines](ci-qualified-excerpts.log)
exclude credential setup/environment dumps. Full original logs and screenshots
are in that run's artifacts. The [seven actual gallery captures](../../../images/portfolio/README.md)
have [hashes and capture facts](gallery-ad45d45.json).

## Hosted qualification remains incomplete

The genuine protocol smoke passed at `3658f41`; the final build's smoke executable
has the same SHA. It verified model identity, a model-issued tool call, actual
execution and matching tool-result continuation through model completion.

The first readiness round stopped all four runs at `provider_unavailable`, before
any model request. That exposed the missing bundled-Python initialization in the
installed live CLI. `ad45d45` reuses the desktop initializer and preserves explicit
interpreter overrides. No evaluator/validator implementation was changed.

The single subsequent four-run round produced:

| Run | Dataset | Requests / executed tools | Result |
|---|---|---|---|
| 1 | Self-authored Operations | 13 / 12 | Passed, exact grounded plan and independent validation. |
| 2 | Self-authored Operations | 9 / 8 | Passed, exact grounded plan and independent validation. |
| 3 | Self-authored Operations | 13 / 12 | Incomplete. Numerical exchanges completed, but final JSON was invalid after the bounded call budget. The final was rejected. |
| 4 | Renamed assets and changed topology | 10 / 9 | Passed, fresh capture-bound plan and independent validation. |

See [the round report](live-round-2/verification.json) and complete genuine
[run 1](live-round-2/run-1.eog-session.json), [run 2](live-round-2/run-2.eog-session.json),
[run 3](live-round-2/run-3.eog-session.json), [run 4](live-round-2/run-4.eog-session.json)
archives. Model-selected order, numerical evidence and sanitized HTTP metadata
are retained. No control application or owner approval occurred. This is **3/4**,
not the required three successful Operations runs plus renamed run. No extra
round was run to hide the malformed final. Earlier successful baseline rehearsals
remain evidence for their earlier sources, not a replacement for this failed gate.
No comparative model/reliability benchmark is claimed.

## Packaging and retained failures

The sealed Windows ZIP is 104,779,250 bytes; matching source ZIP 18,440,807 bytes.
The local candidate directory is `out/portfolio-candidate-ad45d45-retry1`.
The unpacked app was moved, without changing the sealed ZIP, to
`out/release-readiness/Relocated ad45d45 α/Breakwater` for the relocation check.

[Package metadata](package-ad45d45.json), [portable checks](portable-ad45d45.json),
[actual dependency inventory](dependency-inventory-ad45d45.json) and
[handoff preparation](handoff-ad45d45.json) bind those artifacts.
The inventory records 42 Qt DLLs, seven Python package records and 94 notice files;
source/license/distribution review remains pending.

Retained failures include the helper run with missing PyYAML, four original CLI
startup failures, two restricted-token ASan hard-link attempts, the disk-full
PDB-link packaging attempt and the malformed hosted final. Only verified redundant
staging copies generated during this task were removed to recover space; sealed
archives and failure records remain. Original local paths are indexed in
`readiness.json`; corresponding reports/logs are retained here.
Copied terminal logs normalize line endings and trailing whitespace; their
byte-original local records remain under `out/release-readiness` and the failed
candidate directory. JSON archives and gallery captures are copied unchanged.

## Remaining gates and handoff

- Ethan's formal PR58 review, followed by ancestry-preserving integration of the stack.
- Ethan's GPLv3 agreement and actual deployed-notices/corresponding-source review.
- Complete the final hosted all-four gate without accepting malformed or ungrounded finals.
- Execute the prepared kit on an actual clean Windows VM, including restart, keyboard/file-dialog checks and real 100/125/150/200% display settings.

The local kit is `out/release-readiness/Breakwater v0.2.0 ad45d45 VM handoff`.
It includes exact Windows/source ZIPs, checksums, owner packet, checklist and a
pending manual result template. See [clean Windows instructions](../../../CLEAN_WINDOWS_CHECK.md),
[owner review](../../../RELEASE_REVIEW.md) and
[draft release notes](../../../RELEASE_NOTES_v0.2.0_DRAFT.md).
No root license, owner agreement, main merge, tag or public binary release is
created by this evidence record.
