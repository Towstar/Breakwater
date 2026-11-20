# Portfolio candidate readiness

This is the current release-preparation packet. It supplements the dated
[v0.2 owner packet](RELEASE_REVIEW.md) and [v0.3 packet](RELEASE_REVIEW_V03.md);
neither historical packet is rewritten as a later result. The executable remains
version 0.2.0 plus its source revision. No release tag, public binary, root release
license or main integration is authorized by this packet.

## Current engineering baseline

PR [85](https://github.com/Towstar/Breakwater/pull/85), exact head
`440cb3d73c6a8da398edf483438ef197ff93f199`, passes all six jobs in
[Windows run 44791528171](https://github.com/Towstar/Breakwater/actions/runs/44791528171).
Its framing implementation is application source `cd4dc5f`. Local normal/ASan
rendered containment, route focus and Windows Ctrl+Shift+F/Escape checks are
indexed by [workspace framing](WORKSPACE_FRAMING.md). Those checks do not supply
clean-Windows acceptance or final hosted qualification of a later candidate.

The previous full hosted Operations round at source `29df04f` passed all four
declared cases. Actual desktop observed pump-loss and missing-coverage cases also
passed their separate advisory/no-plan gates at that source. Earlier unsuccessful
rounds remain in the [reliability evidence](RELIABILITY_PICKING_FOLLOWUP.md).
These are source-qualified engineering observations, not a statistical reliability
estimate or a model-to-model comparison.

## Final candidate evidence

Freeze a clean engineering commit before building. Bind each executable, archive
and report to that original source and its SHA-256. Documentation committed later
does not relabel an older binary. The final evidence index must distinguish:

- Normal and ASan local checks and the six exact-source Windows CI jobs.
- One protocol smoke, three canonical Operations sessions, one renamed/topology
  session, one observed pump advisory and one missing-coverage no-plan session.
  Retain every attempt and actual model-selected tool order. No repaired final
  references, mixed-build successful subsets or substituted local-search results.
- Private portable/source ZIPs, actual extraction to a spaces/Unicode path,
  bundled offline checks, deployed-file/notices/source inventory and checksums.
- Actual screenshots with source, fixture/capture identity and historical status.
  Synthetic example metrics and official challenge metrics stay separate.

Until that index records actual results, these final-candidate checks are pending.
Do not infer a pass from this checklist or from the existence of a file.
The [offline seven-case audit](FINAL_CANDIDATE_CHECKS.md) checks immutable retained
evidence and source bindings; it does not launch a model or replace that round.

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
agent self-review cannot supply that agreement. No review or consent is recorded
by this packet. No message or review request is sent automatically.

## Private Windows handoff

Prepare a new kit without changing older kits or their version-1 result records.
New acceptance templates are version 2. All manual outcomes start as `not_run`;
owner, licensing and clean-Windows gates start pending. The current kit summary
must identify its actual packaged source and hashes and include both dated packets.

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
