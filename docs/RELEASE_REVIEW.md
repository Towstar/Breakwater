# v0.2.0 owner review packet

This packet prepares a review candidate. It does not record an approval or
authorize public binary distribution. Lucas owns desktop/integration/delivery;
Ethan owns numerical/scientific meaning and its review.

## Exact review boundary

- Proposed numerical baseline: PR [58](https://github.com/Towstar/Breakwater/pull/58),
  head `b639161ab750413676669577320b77a4768e3cfc`, against main `3116b3d`.
- Current application candidate: PR [66](https://github.com/Towstar/Breakwater/pull/66),
  source `0cfee7225943df166431aff4f672adcfb9b83782`.
- The stack is **58 → 59 → 60 → 61 → 62 → 63 → 64 → 65 → 66**. Preserve merge
  ancestry and retarget each dependent PR after its parent reaches main.
- PR56 and the numerical work through PR53 are already ancestors of the candidate.
  Re-merging those changes is unnecessary. PR30 and PR32 remain separate research/
  multifeed work. Documentation PR57 has useful historical/remaining-work text;
  reconcile it without replacing the active portfolio status with an older one.

## Ethan: numerical/shared acceptance

Record a formal GitHub review on the exact PR58 head. Check these items before
main integration; a comment by this coding session is not Ethan's review.

- The integrated native evaluator/search/independent validator and scientific
  implementations preserve the intended equations, units, coverage and identities.
- In `science/eog_model.py`, optional `meta.demo_note` and
  `meta.demo_failure_onsets` are bounded descriptive annotations. They do not enter
  compiled model equality, numerical fingerprint or observation channels.
  Unknown metadata and invalid units still fail. The annotation regression cases
  live in `science/tests/test_eog_model.py`.
- The integration assertion uses the actual `eog-joint-search-0.1.3` identity.
  Search is bounded best-found; independent validation belongs to the exact plan.
- The PhysicsNeMo fit remains a bounded synthetic scalar-decay experiment with
  explicit application, rather than a qualified field chemistry/pressure model.
- The F1/front-accuracy limitations and standalone Lean theorem assumptions remain
  visible. Neither engine proofs nor sanitizer results prove the entire application.
- Generic telemetry-to-forecast mapping and commitment-aware dispatch are future
  integration work; the recorded Operations console does not complete them.

Source-qualified results are indexed by `docs/PORTFOLIO_RELEASE.md` and
`docs/evidence/portfolio/`. Exact GitHub job outcomes remain authoritative for their
published heads; failed attempts and explicit optional skips are retained.

## Both owners: distribution decision

Lucas selected the proposed GPLv3 route. Ethan must explicitly record agreement
for his contributions before the root release license is applied and public
binaries are distributed. Keep the agreement's URL and reviewed source revision
with the release record. No agreement is inferred from code availability, a test
pass, this packet or a model response.

Complete the deployed Qt/Python/compiler-runtime notices and corresponding-source
inventory described in `docs/LICENSING.md`. The basic package includes no Torch or
PhysicsNeMo weights/runtime. Use only the self-authored portfolio datasets and
genuine matching hosted traces for distributed examples.

## Lucas: desktop and release acceptance

- Native scene picking/editing, undo and four supported window sizes pass.
- Actual forecasts/comparisons retain original units and out-of-range values.
- Review displays exact run/validation/control identities; edits/reset retire
  eligibility and historical imports cannot restore it.
- Final live evidence contains real model calls, executed tools, matching
  continuations, independent validation and grounded final references.
- The Windows/source ZIPs, executable and evidence records have pinned hashes.
- Actual clean-Windows acceptance is recorded separately from sanitized developer
  processes. Prepare the VM kit while its result remains pending.

## Current external gates

Formal PR58 review, Ethan's license agreement and clean-Windows VM acceptance are
not recorded in this packet. Keep main merges/public distribution pending until
their required evidence exists. This document is a handoff, not a request sent to
another person or an automatic approval.
