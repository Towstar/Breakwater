# Nemotron completion and explanation results

Engineering source: `3d91452d81af7052effb35f98afe48c6f3661b20`, PR #90. Desktop SHA-256: `90f3b8e8f6b573cfbe3f3e3f3235ee7c9327d01a9618dfb9d6d24ce2c7427649`.

PR #89 completed all six Windows CI jobs successfully for its exact head `7df8c379e1178600bcb869cd62ab3d6751ea8a38`; no PR #89 failure required a source fix. This is separate from PR #90 and its new runtime source. At the retained 2026-11-21 00:59:29 UTC check, PR #90 had five passing jobs; Desktop ASan was still running. Its normal desktop job passed 88/88 with 1,181 Qt passes and one optional skip. [Exact CI snapshot](evidence/nemotron-completion/3d91452/ci/pr90/pre-push-current-status.json).

## What changed

The hosted loop now narrows completion after genuinely completed model-issued work. Repeated eligible validation can leave only explicit staging available; a successful model-issued staging request closes tools after its ordered batch. Required inspection, exact references, stale checks, tool/time budgets and one bounded format correction remain. No tool, run selection or staging action is invented or performed automatically.

Operations finishes after a repeated cached validation only while that exact plan/validation pair remains admissible after the current tool batch. New cheaper evidence can retire the pair and keep investigation open. Unchanged native validation reuses its original report; the first explicit check of a search-screened run still executes the independent validator.

The UI leads with application-owned statements from retained evidence. Raw model narratives are preserved separately and labeled unverified. Missing observed coverage is distinguished from absence of the native simulator. Observed acknowledgement records review only and cannot apply controls or clear an incident. Sandbox control application still requires separate explicit operator review.

## Fresh hosted evidence

All **seven** declared fresh cases passed: genuine hosted protocol smoke, three canonical Operations investigations, renamed/custom topology, observed pump loss and unavailable observed coverage. Every case belongs to this source/executable round; no older successful subset was substituted.

The read-only strict audit independently passed the sealed manifest and all required matching model-call, numerical-result and exact selected-validation checks. Manifest: [sealed manifest](evidence/nemotron-completion/3d91452/hosted/manifest-0006.json); SHA-256 `e51d9b8d5edbba4b6a408599001556c874a0a09a0501ab6638502e9de01767da`.

| Case | Tool calls, prior → current | Model requests, prior → current | Cached validation repeats, prior → current |
| --- | ---: | ---: | ---: |
| Canonical 1 | 12 → 7 | 13 → 8 | 6 → 1 |
| Canonical 2 | 12 → 7 | 13 → 8 | 6 → 1 |
| Canonical 3 | 12 → 7 | 13 → 8 | 6 → 1 |
| Renamed topology | 12 → 5 | 13 → 6 | 5 → 1 |
| Observed pump loss | 12 → 7 | 13 → 8 | 0 → 0 |
| Missing observed coverage | 12 → 5 | 13 → 6 | 0 → 0 |

Prior evidence is the retained `3bb77a3` round 2. These are small descriptive rehearsals, not a controlled performance benchmark, statistical reliability claim or comparison against another model. Observed captures differ: pump loss was frozen at 85.9009784 s previously and 115.1921495 s currently; missing coverage at 92.7308944 s and 116.7433549 s. Search remains `bounded_search`, `best_found`, `complete=false`. One cached duplicate per Operations case remains deliberately visible rather than being relabeled as new execution.

### Exact retained references

- Canonical sessions: `66af3ac5-65d8-4015-ab26-cd74c0f38b8b`, `98926236-54cf-4738-bce0-7a67bb789ce9`, `d38ab7a8-8ec4-40ef-872e-283531a7069e`. Capture `84745ddc89443d60c0effe6a163665e61884cb720598217baf4683b9206d7722`. Each selected plan `plan-873040552c79d1f7f3fb1a1489927cef` with its own matching deterministic validation reference `validation-ce8daf51cfda2951758658395331b08bc05d03f4ea4d3ef8fe9c2925baf85651`.
- Renamed session: `6481c700-7943-4754-96e1-75bbd6d4718f`. Capture `d4934a4536c9bd0e11079dc4975be69193190c2af9f01cf53fc5f7f3a06909fd`; plan `plan-218c56a2fc7a647969449e4b0281076d`; validation `validation-6bd797b389a1dc95bd3c9e867f5f5a8bc562b56f61fb4f6f9a96d259d27c8455`.
- Observed pump: session `bf6bf0b9-56bb-43ff-b66d-0adf611856f4`, capture `capture-0d16cb50-46ca-4899-8813-967cfbefe44a`, selected run `efbe934b-55dd-4471-a33c-c1302300ff8e`, validation `4fd78c26-c989-4ced-bcc8-1ff3a82da9d7`. Matching explicit `validate_run` evidence; zero approval records and no operating action.
- Missing coverage: session `07ebfcf3-6fd5-472e-be52-044146af3ec5`, capture `capture-b8f78516-6a80-4520-b26d-c2f4769543b4`. No selected run, numerical runs, validation or approval; no successful numerical/approval tool execution.

Numerical references authenticate retained evidence, not arbitrary prose. Raw new model narratives still include broad phrases such as “No cheaper validated plan exists”; the UI's qualified evidence summary and unverified-narrative label remain necessary. No global optimum or absence of cheaper untested alternatives is established.

## Local checks and current limits

- Normal build: all 88 CTest entries passed across the original 86 passes and two same-build Windows-filesystem-permission reruns. The original full invocation's two failures remain retained; this is not represented as a clean first-pass 88/88 run. Qt result files total 1,181 passes, zero failures and one explicitly optional PhysicsNeMo calibration skip.
- Local helper checks: 21 passed; CI/release helper checks: 78 passed using the configured bootstrap environment. The earlier attempt using a Python environment without `yaml` failed with import errors and remains retained.
- ASan: all 88 entries verified across 83 initial passes and five targeted reruns on the identical executable. The original run failed three UI checks and two sandbox-denied hard-link checks. All five passed outside the sandbox with the completed rehearsal app minimized and no agent UI input; source, assertions and deadlines were unchanged. The UI failure cause remains inconclusive because conditions changed. The initial failures remain retained. The Qt suites passed 1,181 cases with one optional calibration skip. [Combined verification](evidence/nemotron-completion/3d91452/local/combined-local-verification.json), [quiet rerun](evidence/nemotron-completion/3d91452/local/asan-quiet-rerun/verification.json), [UI triage](evidence/nemotron-completion/3d91452/review/review-asan-ui.md).
- PR #90 Windows CI: five jobs passed and Desktop ASan was pending at the retained check above. PR #89's completed checks do not qualify this new source. [PR #89 final status](evidence/nemotron-completion/3d91452/ci/pr89/final-status.json) records six successful jobs without retries or source fixes.
- Human owner/licensing review, ordered stack integration and actual clean-Windows acceptance remain separate release gates. No public binary release, merge or release approval is claimed.

## Actual UI evidence

Four original native JPEG captures were separately opened and visually inspected. All show source `3d91452` in the title. Their metadata source/app hash matches the sealed round; every size and SHA-256 matches, and all images are 1442×931 pixels. They show the qualified observed recommendation, acknowledgement-only review, missing-coverage explanation and reopened historical Operations summary. No image editing or compositing was performed according to the witnessed native-capture metadata.

Public artifacts: [native gallery](images/portfolio/nemotron-3d91452/README.md), [copy inventory](evidence/nemotron-completion/3d91452/inventory.json), [call comparison](evidence/nemotron-completion/3d91452/review/hosted-call-comparison-final.json), [independent sealed-round audit](evidence/nemotron-completion/3d91452/review/independent-sealed-delivery-audit.json), [workbench review](evidence/nemotron-completion/3d91452/review/review-workbench-completion-3d91452.md). Original databases, executables, environments, user project and private SDKs are excluded.

The latest source-qualified normal app was reopened maximized with the preserved user workspace, six assets and six routes. Exactly one Breakwater process remained. Workspace bytes were unchanged; [handoff record](evidence/nemotron-completion/3d91452/local/final-handoff.json). The existing portable review kit remains pinned to `3bb77a3`; it was not rebuilt or relabeled.

Reproduce the offline hosted integrity/grounding audit from the repository root with an existing Python 3.12:

```powershell
python scripts/audit_candidate_evidence.py docs/evidence/nemotron-completion/3d91452/hosted/manifest-0006.json
```

This checks retained references and hashes; it does not invoke a model or attest remote execution.

## Attention

Artifact review: delegated same-family agent. The workbench review and retained-evidence audit were independent of the workbench implementation; this reviewer authored the separate EOG investigator repeat guard earlier. No independent human or cross-family review is claimed. Complete transcript audit, clean-Windows acceptance and licensing clearance remain outside this task. Local ASan and the final single-instance handoff were completed as recorded above. PR #90 Desktop ASan CI remained pending at the retained cutoff. Original UI failures and their inconclusive cause are disclosed, and raw model prose remains unverified.
