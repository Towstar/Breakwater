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

## Provider integration review checkpoint — November 8, 2026

Lucas checked Ethan's updated PR #34 (`ae212e2`), PR #36 (`c622083`) and
[PR #38 detection core](https://github.com/Towstar/Breakwater/pull/38)
(`70f7c8e`). The first two updates merge the G2 baseline without changing their
previously reviewed numerical/observation source. Independent same-family review
ran all 113 standard-library EOG methods in PR #38, with zero failures/skips;
a second bounded detector review ran six selected causal/lifecycle tests. These
checks do not establish full-week or unseen-network numerical qualification.

PR #38 supplies `load_inputs`, immutable model/archive objects, causal detection,
ongoing-alert ranking and an offline CLI. It still supplies no protected-capture
NDJSON provider. Do not configure `BREAKWATER_EOG_WORKER` to `inspect_eog.py`.
The A-owned next boundary is capture/manifest verification, mapping to accepted
`72cadd1` envelopes and original sample IDs/units/coverage, system filtering,
then elapsed-seconds evaluation, joint search and independent plan validation.

One reproduced issue must be corrected before integrating that CLI: at
[`science/inspect_eog.py:68`](https://github.com/Towstar/Breakwater/blob/70f7c8e6b14d2e550be4a57729b4e898bf2e27f8/science/inspect_eog.py#L68),
`--report` can replace `network.json`, `ops.json` or `telemetry.csv`. An exact-source
probe with a generated temporary fixture returned success and replaced its network
input with a detector report. Ethan owns the pre-write resolved-path collision
guard and executable source-byte preservation regression, including failure reports.
No official input was overwritten and no detector mathematics was changed by B.

Ethan's own results retain incomplete qualification: inspection emission takes
5–16 hours in these practice cases, recovery can lag six hours, and three of four
correlated historical presence/lifecycle cuts fail. Physical inspection-onset bounds
are unavailable. The provider must return those limits; B's operator views preserve
global coverage, assumptions and limitations even when the returned event list is
empty. Practice presence matches are not timely or unseen-network qualification.

B also reproduced a client publication gap and corrected it locally: original
sample value/time/channel lookup now runs off the UI thread before any response is
published, using the same helper as history. Standalone shape/binding validation
does not have the captured CSV and cannot replace that check. Missing/null evidence
semantics and real negative, overflowing or stale readings remain intact.

## New provider boundary — PR #40

The later [PR #40](https://github.com/Towstar/Breakwater/pull/40), reviewed at
`6581259f46edec8b8f9099c5fb815222cb625a53`, now supplies the protected-capture
NDJSON worker and a native binary64-seconds evaluator. This supersedes the earlier
checkpoint's statement that a provider has not been delivered. Independent local
review ran 144 EOG Python methods without skips and the native response test with
6,766 assertions; real restricted-environment detect/evaluate processes completed.
Those checks do not establish desktop, installed or hosted-model acceptance.

Startup needs trusted absolute `--contracts-root` and `--native-library` arguments.
The test harness environment variables are not consumed by the worker CLI.
Installation must stage the EOG modules, five exact adopted contract files and
native library. B owns that application/runtime integration and its verification.
The existing credential-free child environment is sufficient.

Joint search and independent numerical validation still explicitly return
unavailable. Evaluation cannot establish minimum cost or review eligibility.
The earlier `inspect_eog.py --report` overwrite issue remains reproducible at this
head; the production worker does not invoke that CLI. Do not package the unsafe
offline CLI. A retains its correction and the numerical search/validation track.

The public B workflow is now [draft PR #42](https://github.com/Towstar/Breakwater/pull/42),
head `9152dac9bdb537ffba829d0f8279bed721d5ceb8`. It preserves the accepted interface,
published console ancestry and verified telemetry main. The additional local B
provider connection executes genuine #40 processes: 72 focused Qt cases per mode
pass normal/ASan, with the checked runtime staged from the actual compiler. Full
CPU discovery executes all EOG cases with zero unexpected skips (326 passes and
four explicitly optional PhysicsNeMo skips). Installed and hosted acceptance remain
separate. The installed module set deliberately excludes the unsafe offline CLI.

The newer [PR #41](https://github.com/Towstar/Breakwater/pull/41) is now available
for exact-head review. It adds independent validation and corrects positive-cost
underflow in the native evaluator, requiring the new native method identifier.
Do not treat #40's ordinary-case execution as acceptance of that corrected source.

