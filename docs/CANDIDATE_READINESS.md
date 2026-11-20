# Portfolio candidate readiness

This is the current release-preparation packet. It supplements the dated
[v0.2 owner packet](RELEASE_REVIEW.md) and [v0.3 packet](RELEASE_REVIEW_V03.md);
neither historical packet is rewritten as a later result. The executable remains
version 0.2.0 plus its source revision. No release tag, public binary, root release
license or main integration is authorized by this packet.

## Current engineering baseline

The clean engineering source is
`3bb77a353fb04267260764060bc5f58d61d5e7f5`. It includes the stored-candidate Science
convergence barrier and worker lifecycle fixture timing correction. The
[Science CI follow-up](STORED_SCIENCE_CI.md) and
[worker lifecycle follow-up](WORKER_LIFECYCLE_CI.md) preserve PR87's original
failures and targeted diagnostics. Later passing checks do not erase them.

The complete normal and ASan CTest configurations each pass 88/88 suites. The
normal tested executable matches the sealed Windows package. All 78 helper tests
and ten bundled checks pass, including ten checks after extracting the actual ZIP
to a spaces/Unicode path. Six jobs in
[Windows run 44823006438](https://github.com/Towstar/Breakwater/actions/runs/44823006438)
pass for PR head `3bb77a3` and its matching source tree. Raw job records preserve
their GitHub merge-ref checkout commit rather than relabeling it as the head.

The second fresh hosted round passes all seven declared cases under the unchanged
strict evidence checks. The first round remains sealed at 6/7: its observed-pump
case omitted model-issued `validate_run` for the exact selected run and failed.
Round 2 explicitly requested that validation in its pump objective. It is not an
unchanged-prompt retry or a statistical reliability estimate. Repeated calls and
inaccurate model prose remain documented in
[candidate results](CANDIDATE_RESULTS_3BB77A3.md).

Both approved cleanups (2.54 GiB and the separate 3.52 GiB inventory) completed
with recovery inputs unchanged. They do not authorize broader deletion. Earlier
PR85/framing and `29df04f` hosted results retain their original scope in
[workspace framing](WORKSPACE_FRAMING.md) and the
[reliability history](RELIABILITY_PICKING_FOLLOWUP.md).

## Final candidate evidence

The [source-qualified results](CANDIDATE_RESULTS_3BB77A3.md) link the retained raw
records, both sealed hosted rounds and independent artifact audits. Documentation
committed later does not relabel the original executable or source ZIP.

| Check | Actual result | Boundary |
|---|---|---|
| Normal CTest | 88/88 passed | Tested executable hash matches sealed package |
| ASan CTest | 88/88 passed | Owned C++ targets; prebuilt dependencies separate |
| Qt subcases | 1,162 passed and one optional skip per configuration | Zero outer CTest skips; 37 Qt suites |
| Helpers | 78/78 passed | Offline helper suite |
| Windows CI | Six jobs passed, run 44823006438 | Head-bound, tree-matching merge-ref checkout |
| Bundled runtime | 10/10 in place and 10/10 relocated | Sanitized development-machine process, not clean Windows |
| Hardware renderer | 118.62 FPS default; 73.45 FPS dense | One Radeon 880M device; diagnostic sweeps |
| Hosted round 1 | 6/7 passed; observed pump failed | Original failure and archives preserved |
| Hosted round 2 | 7/7 passed | Declared strict evidence/refusal gates; prompt change recorded |
| Private handoff | Windows/source ZIPs and hashes verified | Candidate only; no public release approval |
| Manual clean Windows | Pending | All 19 template-v2 checks remain `not_run` |
| Owner/licensing review | Not verified | Requires actual human review and consent records |

The [offline seven-case audit](FINAL_CANDIDATE_CHECKS.md) checks retained evidence;
it does not itself launch a model. Hash/source bindings are witnessed provenance,
not authenticated execution or wire attestation. No controlled rendering speedup,
model comparison, global plan optimum or field-accuracy result is claimed. The
[gallery](images/portfolio/README.md) identifies native and historical views;
synthetic example costs are not the official challenge's cost result.

## Ethan's review boundary

Record a formal GitHub review of PR58 head
`b639161ab750413676669577320b77a4768e3cfc` and exact-tip review of subsequent
numerical/shared changes in the PR58 through PR85 ancestry, followed by this
candidate's preparation increments. The review should address the numerical and
contract questions in RELEASE_REVIEW_V03.md, including calibration, uncertainty,
coverage, observed-state identity/expiry, detector assumptions, finite-grid search,
exact independent validation and historical nonactionability. The production
transport method and documented F1 accuracy limitations remain unchanged.

Separately record Ethan's explicit GPLv3 agreement for his contributions, including
the reviewed revision and evidence URL. Tests, repository visibility and coding
agent self-review cannot supply that agreement. This pass does not freshly verify
remote human review or GPLv3 consent; both remain unverified release gates. No
review request or message is sent automatically.

## Private Windows handoff

A private kit for `3bb77a3` is staged with matching Windows/source ZIPs, a source
map, package record, both dated review packets and acceptance template v2. The
[handoff audit](evidence/candidate-readiness/3bb77a3/audits/handoff-and-pump-record-audit-3bb77a3.json)
binds the retained hashes; [candidate results](CANDIDATE_RESULTS_3BB77A3.md) list
the exact executable and archive identities. All 19 manual outcomes remain
`not_run`, and owner, licensing and clean-Windows gates remain pending. Keep the
sealed kit, older kits and version-1 result records unchanged.

The recipient supplies an actual clean Windows 11 x64 VM/account with no SDK,
Python, Torch or model key. No VM is provisioned by this implementation pass.
Run the bundled verifier, then the [manual checklist](CLEAN_WINDOWS_CHECK.md),
including acquisition/capture/advisory/expiry, Connections/route focus, scene focus,
minimum client size and real Windows display scales. A sanitized process on the
development laptop is explicitly a different kind of evidence.

## Integration and distribution

Keep the PR stack in ancestry order. Integrate only after the required exact-head
human review, current passing validation and owner/licensing evidence exist.
Complete the [actual dependency/source review](DEPENDENCY_DISTRIBUTION.md) before
public distribution. The private kit is a test candidate, not an approved release.

Lucas owns desktop/UI, 3D interaction, adapters, Nemotron integration, contracts,
history, packaging and integration verification. Ethan owns simulation, numerical
search/validation, scientific methods, sensor mathematics, qualification and Lean
proofs. Codex assists implementation and tests; runtime Nemotron is a separate
optional investigator. This pass does not produce a demo video or narration.
