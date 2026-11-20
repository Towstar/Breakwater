# Candidate results: 3bb77a3

Recorded 20 November 2026 UTC. The engineering source is
`3bb77a353fb04267260764060bc5f58d61d5e7f5`, clean when built. This records completed
candidate checks and their limits, not a public release. The executable still
uses application version 0.2.0 plus its source identity. Later evidence/documentation
commits do not change the sealed binary or source ZIP.

## Executable and package identity

| Artifact | SHA-256 |
|---|---|
| Packaged normal application | `880cc3130120f658ae49fe13c030a0a7f85d5ca3e46c31335fe7584813b580c7` |
| Windows ZIP | `ea77ece7b52393f7d9434b9aa177bbc0971519ab1137882f9c8ff8f60432c794` |
| Matching source ZIP | `0d13506053149ac7899e35dd1dbe5322882b4b3a7f9ed49b22b1652d56daabf8` |

The [package record](evidence/candidate-readiness/3bb77a3/package/package.json),
[dependency/version record](evidence/candidate-readiness/3bb77a3/audits/dependency-versions-3bb77a3.json)
and [handoff audit](evidence/candidate-readiness/3bb77a3/audits/handoff-and-pump-record-audit-3bb77a3.json)
bind these identities. The private ZIPs, databases, symbols, credentials and user
projects are not committed as public evidence. Selected reports and archives are
copied byte for byte; their original absolute/relative paths remain provenance,
not instructions to restore a developer's environment.

## Local and Windows checks

| Check | Result | Retained record |
|---|---|---|
| Normal CTest | 88/88 passed, zero failed or skipped suites | [Packaged-executable binding audit](evidence/candidate-readiness/3bb77a3/audits/normal-packaged-evidence-audit-3bb77a3.json) |
| ASan CTest | 88/88 passed, zero failed or skipped suites | [ASan audit](evidence/candidate-readiness/3bb77a3/audits/asan-evidence-audit-3bb77a3.json) |
| Qt subcases | 1,162 passed and one optional skip per configuration, across 37 suites | The two audits above and retained Qt results |
| Helpers | 78 tests passed | [Helper output](evidence/candidate-readiness/3bb77a3/audits/helpers-3bb77a3.log) |
| Bundled runtime in place | Ten checks passed | [In-place verification](evidence/candidate-readiness/3bb77a3/package/offline-bundled/verification.json) |
| Actual ZIP relocation | Ten checks passed after extraction to a spaces/Unicode path | [Relocated verification](evidence/candidate-readiness/3bb77a3/relocated/verification.json) |
| Windows CI | All six jobs passed in run 44823006438 | [Head/tree audit](evidence/candidate-readiness/3bb77a3/ci/audit-2026-11-20T22-46-31-374623+00-00.json) |

The normal CTest executable matches the sealed package app. All 88 suites were
not rerun inside the relocated isolated folder; runtime deployment is covered by
the separate ten-step checks. Both bundled runs used a sanitized process on the
development machine, with no supplied API key. They are not clean-Windows tests.
ASan coverage is established for owned C++ targets, not prebuilt Qt, Python or
Torch dependencies. The one optional Qt subcase skip is retained separately from
the zero outer CTest skips.

[CI run 44823006438](https://github.com/Towstar/Breakwater/actions/runs/44823006438)
is attached to PR head `3bb77a3`. Raw job records report the GitHub merge-ref
checkout `adf625a03423aed55e98bc9c63f7dd67a145719e`; its source tree
`71475d970f784daa61c2da70dd3a03a66ad0ab7a` matches the candidate head. The
[parsed toolchains](evidence/candidate-readiness/3bb77a3/ci/toolchain-summary.json)
preserve both identities and actual versions: Windows runner image
`20261116.370.1`, MSVC `14.44.35230.0`, Visual Studio `17.14.37906.3`, Windows SDK
`10.0.26100.0`, CMake `3.31.6`, Ninja `1.13.2` and Qt `6.12.0` where used.
The science job records Python `3.12.13` and Torch `2.15.0+cpu`; Lean records its
actual pinned toolchain separately. Credential-free CI makes no hosted requests.

Local UI reports exercise actual client sizes 1440 x 900, 1100 x 700, 1008 x 672
and 960 x 540 at DPR 1.75. The
[CI layout records](evidence/candidate-readiness/3bb77a3/ci/ui-layout-manifest.json)
instead report 1008 x 672 for three named layouts and 960 x 540 for the minimum.
Four test names do not establish four distinct actual CI window sizes. Normal and
ASan WARP reports identify the Microsoft Basic Render Driver and retain six body
picks, six labels and zero overlaps; WARP FPS is diagnostic only.

The original [packaging wrapper failure](evidence/candidate-readiness/3bb77a3/audits/package-3bb77a3-wrapper.json)
and [dependency-inventory failure](evidence/candidate-readiness/3bb77a3/audits/dependency-inventory-failed-attempt-3bb77a3.json)
remain unchanged alongside the later successful package, runtime and handoff
audits. The [Science convergence](STORED_SCIENCE_CI.md) and
[worker timing](WORKER_LIFECYCLE_CI.md) records preserve earlier CI failures too.

## Hosted Nemotron qualification

The model is `nvidia/nemotron-3.5-lightning-30b-a3b`. Both actual hosted rounds use
the same clean engineering source and recorded executable hashes. Round 2 uses
fresh sessions and evidence, rather than combining successful cases from round 1.

| Declared case | Round 1 | Round 2 |
|---|---|---|
| Genuine protocol tool-call/result continuation | Passed | Passed |
| Canonical Operations investigation 1 | Passed | Passed |
| Canonical Operations investigation 2 | Passed | Passed |
| Canonical Operations investigation 3 | Passed | Passed |
| Renamed assets and changed topology | Passed | Passed |
| Observed-pump advisory with exact selected-run validation | Failed | Passed |
| Missing observed coverage, no validated plan | Passed | Passed |

The [first sealed manifest](evidence/candidate-readiness/3bb77a3/hosted/round1/manifest-0006.json)
remains incomplete at 6/7. Its pump attempt relied on search validation without a
model-issued `validate_run` for the exact selected run. The strict check rejected
it, and the [failure audit](evidence/candidate-readiness/3bb77a3/audits/observed-pump-attempt-0001-audit-3bb77a3.json)
and original archive remain retained.

The [second sealed manifest](evidence/candidate-readiness/3bb77a3/hosted/round2/manifest-0006.json)
passes 7/7; its SHA-256 is
`d00818a36ac6cc016699a6cec0637702e9966818badfd399a2498eaa2f018df4`.
The [independent final audit](evidence/candidate-readiness/3bb77a3/audits/round2-seven-case-final-audit-3bb77a3.json)
checks genuine continuations, retained identities and exact selected-plan/run
validation, plus the missing-coverage refusal. For the pump case, the round-2
objective explicitly added exact stored-run `validate_run` before staging and
asked to avoid unchanged repeated calls. This is a documented objective change,
not an unchanged-prompt reliability experiment. No admission check was relaxed.

The selected pump run `0135ae36-efa4-4c6e-ba51-033e92bb0188` has a matching
model-issued validation `ff213d2c-5685-4fbf-b1f6-6839d03b9bfa` and a retained cost
of 5,280 synthetic units. The exact earlier selected validation remains valid
even though later tools validate different stored candidates. The missing-coverage
case retains no numerical run, selected run, approval or applied event. That is a
refusal under unavailable coverage, not proof that every possible response is
numerically infeasible. Both observed archives are nonactionable.

The declared qualification passes while these limitations remain:

- Each of the six analysis sessions still consumes twelve tool calls and thirteen
  matching-model completion records. The four Operations cases reuse 6/6/6/5
  unchanged validations; the pump case validates several candidates, and the
  missing-coverage case repeats reads. Redundant investigation is not eliminated.
- Every Operations search reports `bounded_search`, `best_found` and
  `complete=false`. Native candidate grids are finite too. Model wording about
  lowest cost or no cheaper alternative does not establish global optimality.
- Pump prose incorrectly implies advisory acknowledgment can make controls take
  effect. Observed-origin review only acknowledges evidence: the archive is
  `actionable=false`, with no approvals or applied events.
- Missing-coverage prose incorrectly says the network has no native simulation
  capability. Native simulation exists for eligible states; this capture lacks
  usable observed coverage. Original model prose is preserved, not repaired.
- The six analysis archives retain 78 HTTP/model metadata records with status 200
  and zero recorded retries. Those are client records, not independent wire-level
  request attestation. Source/process bindings are witnessed provenance, not
  authenticated execution attestation.

This finite declared round is not a statistical reliability estimate, a model
comparison, numerical accuracy qualification or approval of every model sentence.
Numerical methods and their documented accuracy limits remain separate.

## Renderer and actual application views

The [packaged hardware audit](evidence/candidate-readiness/3bb77a3/audits/hardware-packaged-evidence-audit-3bb77a3.json)
records AMD Radeon(TM) 880M Graphics, a 1440 x 900 client and DPR 1.75. A view-only
camera sweep used a 2.5-second warmup and ten-second sample. The six-asset/six-route
scene averaged 118.62 FPS. The 128-asset/256-route scene averaged 73.45 FPS with
low detail, 27 visible labels and 101 culled labels. All 128 bodies were picked at
the settled dense view; reports recorded zero visible-label overlaps. Numerical
identity remained unchanged. These are individual diagnostics, not a controlled
speedup comparison or a performance guarantee for other hardware or views.

The [source-labeled gallery](images/portfolio/README.md) shows actual `3bb77a3`
views: Sandbox overview, plan/scene focus, Connections, observed native comparison,
qualified observed advisory review, missing-coverage capture, historical Operations
plans and a genuine retained hosted trace. The observed review acknowledges
advisory evidence and cannot apply controls. Historical views make no new model
request. Viewer source and archive-generator source are recorded separately;
example costs are not substituted for official EOG challenge results.

## Remaining release gates and ownership

The private kit contains matching Windows/source ZIPs and acceptance template v2.
All nineteen manual clean-Windows checks remain `not_run`. Actual clean Windows
11 x64 acceptance without development Qt/Python/Torch is pending. Ethan's required
formal numerical/shared-change review and explicit GPLv3 agreement have not been
freshly verified in this pass. Notices/source-delivery review and ordered stack
integration remain required. Tests, a public repository and agent review supply
neither owner consent nor release authorization; `public_release_approved=false`.
See [readiness and review boundaries](CANDIDATE_READINESS.md).

Lucas owns desktop/UI, utilitarian 3D interaction, adapters, contracts, Nemotron
integration, history, packaging and integration verification. Ethan owns the
numerical engine, evaluator/validator/search, scientific methods, sensor-generation
mathematics, numerical qualification and Lean proofs. Codex assisted implementation
and tests; runtime Nemotron is a separate optional investigation agent.
