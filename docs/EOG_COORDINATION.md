# EOG shared revision: Lucas / Ethan coordination

Recorded November 8, 2026, America/Chicago.

## Shared adoption

Both developers adopt **72cadd1de7eed6666a60faff1d296e29c8f026ee** as the EOG
capture/analysis interface baseline.

- **Ethan / Person A:** [exact-revision numerical acceptance](https://github.com/Towstar/Breakwater/pull/33#pullrequestreview-5817527505).
  This is a recorded COMMENTED numerical review, not relabeled as a formal APPROVED
  GitHub review.
- **Lucas / Person B:** reciprocally adopts the same exact revision. The desktop
  integration branch includes it by merge ancestry and uses its capture/analysis
  schemas, Qt-free DTOs and interchange validation. Subsequent integration with
  G2 main preserves those wire/numerical files.
- [Ethan's source/binary-bound review](https://github.com/Towstar/Breakwater/blob/c526da1/science/EOG_CONTRACT_REVIEW.md)
  and [retained evidence](https://github.com/Towstar/Breakwater/tree/c526da1/science/evidence/eog-contract-review-72cadd1)
  are numerical/interchange acceptance, not a delivered production provider.

Ethan independently ran normal and MSVC ASan checks: 2/2 CTests, 70 Qt cases and
48 Python/native comparison methods per mode, no skips or sanitizer finding.
Eleven independent probes cover incomplete/one-ULP-short forecasts, validation
coverage, fractional crossings and explicit outcomes. Lucas's local checks also
cover 63 shared fixtures, action breakpoints, identity vectors and declared costs.

## Binding rules for the implementation tracks

1. Derive the actual complete affected-facility set from causal evidence.
2. Check proposed repair horizons against causal onset and captured operations.
3. Preserve binary64 elapsed seconds and required action/crossing breakpoints.
   Do not round-trip supplied action times through E1's hour-based fixture API.
4. Treat feasible as admissible supplied actions/outcomes; automatic shutdown can
   be the cheapest admissible response. Unknown horizon/evidence is unavailable,
   not demonstrated infeasibility or an invented zero cost.
5. Preserve threshold-first ordering, strictly pre-repair arrivals and half-open
   truck occupancy as described in REVIEW_CHANGES.md.
6. Bind independent numerical validation to the exact plan content and capture.
   Distinct method labels alone do not establish independent computation.
7. Captures contain only the three allowed files through cutoff. Answer keys,
   future observations, credentials and raw model reasoning remain excluded.

## Ownership and next work

Lucas owns desktop capture/process/UI/model/history integration and root checks.
Ethan owns causal observations/detection, numerical evaluation/search, the
independent numerical validator and provider implementation.

The shared-revision gate is now satisfied for dependent implementation. This does
not advertise a provider method as implemented or executed before a genuine
validated exchange. The B-owned process fixture remains protocol-only.

G2 is merged separately through PR #35 at 1713cc4. PR #32 source-feed/project-v3
compatibility and PR #30 transport experimentation retain their separate scope.
The old e8dab471 and e3190fb reviews are historical; their pending findings were
resolved by the accepted 72cadd1 revision.

