# Nemotron evidence and completion

This Person B increment follows PR #97. It changes model guidance, desktop
evidence presentation and evaluation bookkeeping. Exact validation, original
investigation limits, source retirement and explicit approval remain required.
It changes no numerical or scientific implementation.

## Validation guidance

Sandbox and Operations offer `validate_run` / `validate_plan` with the IDs of
retained candidates that still need their own model-issued validation. A complete
empty catalogue removes that tool from the next request. New numerical evidence
can reopen it. Other available tools, including Science, remain available under
their original prerequisites and budgets; validation does not force an early
final answer.

The catalogue is presentation only. Dispatch, exact identity and the existing
context-and-arguments cache remain authoritative. Queued calls retain their
identities, order and budget charges. Unknown evaluator provenance, invalid IDs
or an oversized catalogue preserves the original schema. Operations scans all
retained evaluations, separately from its eight-row preview, with 256-ID / 16 KiB
catalogue bounds.

## Readable retained evidence

Operations has a full, selectable plain-text numerical explanation, available
without exposing unverified model narrative or the exact JSON trace. It groups
stored validation, costs, capture/timing, actions, trajectories/margins and scope.
Repair policy comes from frozen operations inputs. Historical loading derives
that small presentation context on the existing history worker after archive
validation; it neither changes the version-1 archive nor decompresses on the UI
thread. Retained event onsets and validated repair deadlines are reported without
recomputing a horizon or inventing absent quantities.

The observed panel distinguishes its displayed investigation from the latest
monitor capture. The next investigation's source has its own ID, status and
expiry. Showing it does not replace evidence, revive a recommendation, extend
freshness or apply controls.

## Evaluation and provider waits

The runner still declares 24 core cases: 22 CLI cases over 11 input families,
plus two witnessed observed GUI cases. The four-case Science / Operations
supplement is separate. Every original attempt remains recorded.

The default CLI round remains 1,800 seconds. Explicit `-DurationSeconds 7200`
allows the full serial corpus, including courtesy spacing and provider waits.
The maximum is 7,200 seconds; helper and production deadlines are unchanged.
This longer evaluation window is not a larger model/tool budget.

New rounds declare a provider-deferral policy. The runner waits until both its
courtesy gap and the previous terminal request's retained provider cooldown
permit admission. Missing or capped diagnostics stop with cases pending. A wait
beyond the round deadline does the same. Sealing re-audits these waits against
hash-bound diagnostics. Legacy rounds keep their original policy.

```powershell
$repo = 'C:\path\to\Breakwater'
$python = 'C:\path\to\science-cpu\Scripts\python.exe'
$commit = (& git -C $repo rev-parse HEAD).Trim()
& "$repo\scripts\Run-NemotronEvaluation.ps1" `
    -EvidenceDirectory "$repo\out\new-full-round" `
    -PythonExecutable $python -ExpectedSourceCommit $commit `
    -DurationSeconds 7200 -CaseGapSeconds 60 -Live
```

Use clean frozen source and matching executables. This is an instruction, not
a qualification result. Record normal/ASan checks, renders, offline auditors and
actual hosted outcomes against that revision. Exclude credentials, authorization
headers and raw reasoning. Completion tests do not establish account quota,
field accuracy, comparative model superiority or global optimality.

Owner/licensing review and deferred clean-Windows testing remain separate release
gates. Local preparation does not authorize public binary distribution.
