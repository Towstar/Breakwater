# Nemotron functional evaluation

This increment extends evaluation of the production Sandbox and Operations
investigators. It does not replace numerical qualification, compare model brands,
or authorize release. The preceding private packaged candidate, source `ab29cdd`,
passed its seven declared hosted checks. Those results belong to that candidate;
new results must name the new source and executable hashes.

## What is tested

The generated corpus declares 22 command-line investigations and two actual
observed-sensor investigations before execution. Every attempt counts. There is
no resume, automatic replacement, or removal of failed cases from the denominator.

| Input family | Variation | Intended outcome, subject to native preflight |
|---|---|---|
| Sandbox | Canonical pump failure | Independently validated feasible run |
| Sandbox | Healthy network | Validated feasible run, including unchanged controls |
| Sandbox | Three-tank chain | Validated response on a different graph |
| Sandbox | Shared treatment | Validated response with interacting outlets |
| Sandbox | Balanced cycle | Validated response on cyclic topology |
| Sandbox | No remedy | Completed numerical investigation without a supported plan |
| Operations | Self-authored portfolio operations | Exact independently validated plan |
| Operations | Renamed assets and changed topology | Exact plan using returned identifiers |
| Operations | Multiple reported failures and one shared truck | Constrained plan search |
| Operations | Missing tank channel | Evidence-grounded coverage refusal |
| Operations | Stale tank channel | Evidence-grounded coverage refusal |
| Observed Sandbox | Actual synthetic pump-loss capture | Nominal advisory run with exact explicit validation |
| Observed Sandbox | Actual missing-coverage capture | Refusal without numerical plans or actionable approval |

Each of the first 11 inputs has two equivalent objective styles. Native preflight
executes all 11 inputs before any hosted request. Generated intentions are not
numerical results: a failed preflight stops hosted admission. Supported coverage
refusals are distinguished from setup errors, malformed evidence, deadlines,
exhausted budgets and model failures. Unknown failures never count as safe refusals.

The multiple-failure dataset changes reported pump/flow/pressure channels to
exercise acquisition and planning. Its original tank series is retained. It is
an illustrative channel-corruption fixture, not a physically consistent fault
trajectory or independent detector ground truth. All new data is synthetic and
local; no official challenge test data is added to the distributable corpus.

## Runtime behavior and limits

The Sandbox helper uses `AgentController`, `AgentOrchestrator`, `NemotronClient`
and the existing transactional history recorder. Operations uses its existing
production live helper. The runner does not choose model tools, reorder calls,
inject numerical results or apply controls. It retains the production limits:
12 tool calls, three refinement rounds, and each investigator's existing deadline.
The new absolute-deadline checks reject overdue replies even when the GUI event
loop has delayed timer delivery. Losing a borrowed hosted transport fails safely;
local native search remains independent of that transport.

A round has a maximum 1,800-second command-line budget, a 450-second outer bound
per helper, 8 MiB stdout and 1 MiB stderr bounds. Only subprocesses launched by the
runner are terminated on those outer limits. Partial artifacts and pending cases
remain recorded. Hosted helpers have a recorded 30-second completion-to-next-start
gap by default, within the same round deadline. `-CaseGapSeconds` accepts 0..60
for an explicitly recorded alternative. This courtesy spacing does not establish
the account's quota or guarantee that rate limits cannot occur. Production HTTP
retries and model tool selection are unchanged. Observed GUI sessions are a separately witnessed step, outside
the command-line time budget. No credentials, authorization headers or raw model
reasoning belong in evidence.

## Run locally

Build `breakwater_live_check`, `breakwater_eog_live_check`,
`breakwater_offline_check` and the desktop app from the same frozen revision.
Use PowerShell 7. Supply the Python/Qt installation actually used for that build;
the script can read Qt's location from its CMake cache. Evidence destinations must
be new directories with existing parents.

```powershell
$repo = 'C:\path\to\Breakwater'
$python = 'C:\path\to\science-cpu\Scripts\python.exe'
Set-Location -LiteralPath $repo
& "$repo\scripts\Run-NemotronEvaluation.ps1" `
    -EvidenceDirectory "$repo\out\nemotron-preflight-01" `
    -PythonExecutable $python
```

The default is credential-free native preflight. A dirty checkout is recorded as
dirty; it cannot produce a hosted qualification. For a hosted round, require a
clean exact commit and configure `NVIDIA_API_KEY` locally in the process or user
environment. The wrapper reads that setting without printing it, forwards it only
to hosted helpers, and restores its known process settings afterward. Never put a
key in a command, project, screenshot, repository or evidence archive.

```powershell
$commit = (& git rev-parse HEAD).Trim()
& "$repo\scripts\Run-NemotronEvaluation.ps1" `
    -EvidenceDirectory "$repo\out\nemotron-hosted-01" `
    -PythonExecutable $python -ExpectedSourceCommit $commit -Live
```

The model is `nvidia/nemotron-3.5-lightning-30b-a3b` at NVIDIA's official hosted
endpoint. Source, executable, fixture and objective bindings are checked during
the round. HTTP retry metadata and duplicate logical calls are counted, including
reused validation results. A cache hit is not another numerical execution.

After the 22 CLI cases, run the two real observed sessions in that same desktop
binary using a private history database. Freeze actual synthetic captures,
investigate through the GUI, preserve the exact exports and attach them once:

```powershell
& $python -B "$repo\scripts\run_nemotron_evaluation.py" attach-observed `
    --directory "$repo\out\nemotron-hosted-01" --case observed-pump `
    --archive 'C:\path\to\pump.session.json'
& $python -B "$repo\scripts\run_nemotron_evaluation.py" attach-observed `
    --directory "$repo\out\nemotron-hosted-01" --case observed-missing `
    --archive 'C:\path\to\missing.session.json'
& $python -B "$repo\scripts\run_nemotron_evaluation.py" seal `
    --directory "$repo\out\nemotron-hosted-01"
```

Attachment is operator-witnessed provenance, not cryptographic execution
attestation. Do not import an older passing trace as a fresh case. Sealing rechecks
raw archives, process summaries, input hashes, exact validation references,
completion/refusal semantics, no-apply authority, and the append-only journal.
All 24 cases and 11 preflights must pass for `overall_passed: true`. An incomplete
round can be sealed honestly; fix the demonstrated issue and start a new round.

## Reading the results

`summary.json`, `summary.md` and `SEALED.json` index the journal and raw artifacts.
Report the declared denominator, attempts, failures, pending cases, refusals,
logical requests, HTTP attempts/retries, duplicate calls, actual executions,
validation reuse and durations. Compare costs only for identical input hashes
and matching horizons. A native local search is a useful reference; neither it
nor the hosted search proves a global optimum.

Passing means grounded functional completion or the expected evidence-backed
refusal. It does not score every sentence of model prose, establish a statistical
reliability rate, prove field accuracy, or show superiority over another LLM.
Retain original narratives separately from evidence-owned UI metrics. Imported
and replayed history cannot restore actionable approval.

The first expanded round on `e1713b8` was sealed incomplete: 10 of 22 CLI cases
passed and two observed cases were left pending. Ten CLI cases failed transport
checks, including observed NVIDIA rate limits; one failed to find a supported
plan despite a successful native preflight. Another successfully grounded plan
was rejected by the new auditor because it incorrectly required a dispatch start
for a tool call refused before dispatch at the refinement limit. The follow-up
distinguishes those exact recorded rejections from missing execution evidence,
and marks their unretained arguments unknown in duplicate metrics. Original
attempts and their original verdicts remain preserved. Subsequent rounds use the
same declared inputs, objectives, numerical limits and outcome rubric.

The offline mutation, CLI and transport suites are credential-free CI checks.
Transport checks include JSON delivered over multiple reads, split UTF-8 and an
unexpected SSE body. Production requests `stream: false`; rejecting SSE does not
establish streaming support.
Hosted runs remain explicitly local. Human ownership/licensing review and actual
clean-Windows acceptance remain separate release gates.
