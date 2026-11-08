# EOG analysis, advisory review and history

The EOG workflow uses the shared capture/analysis contract `72cadd1`. It has no
reference to the workbench's operating controls. A captured dataset, numerical
analysis, model explanation and operator review remain distinct evidence.

## Local provider configuration

PR #40 and PR #41 supply the real version-1 provider for detection, candidate
evaluation and separately computed independent validation.
Use the existing pinned Python environment, with its frozen schema dependencies:

```powershell
$env:BREAKWATER_PYTHON = 'C:\path\to\Breakwater\.tools\science-cpu\Scripts\python.exe'
$env:BREAKWATER_EOG_WORKER = 'C:\path\to\Breakwater\science\eog_worker.py'
$env:BREAKWATER_EOG_CONTRACTS = 'C:\path\to\Breakwater\contracts\eog'
$env:BREAKWATER_EOG_NATIVE = 'C:\path\to\Breakwater\out\provider-normal\engine\breakwater_eog_response.dll'
$env:BREAKWATER_EOG_EVALUATOR_METHOD = 'eog-native-seconds-0.1.1'
& $env:BREAKWATER_PYTHON -m pip install --require-hashes -r .\science\requirements-eog.in
```

The example native path is the Ninja normal build; Visual Studio uses its actual
configuration subdirectory. All startup paths must be absolute local paths. The
desktop translates the contract/library configuration into trusted worker startup
arguments; requests and model tools cannot select executable code or schemas.
Direct worker invocation still requires explicit `--contracts-root` and
`--native-library` flags; the worker CLI does not read those environment variables.

The evaluator identifier comes from the provider's documented implementation;
`method_version: 1` identifies the operation contract, not a numerical algorithm.
Manual validation asks for this identifier explicitly. The model receives the
locally configured identifier as context. The provider must check it.

The default lookup checks the installed `share/breakwater/science/eog_worker.py`,
`share/breakwater/contracts/eog` and the native DLL beside the application. Source
and build paths are fallbacks only in the exact configured development executable
directory. Explicit local configuration takes precedence. Existence means configured,
not executed or numerically qualified. A successful bound process exchange
establishes execution only. Missing methods/evidence remain explicitly unavailable.

Joint search remains unavailable. The independent validator reconstructs candidate
costs and constraints from causal observations without loading the native DLL or
consuming its results. Parsing and fixed-rate/point-onset assumptions remain shared;
this is implementation independence, not a real-number proof or physical validation.
Only an exact successful validation can enable advisory review. A minimum among
retained candidates is not a global optimum. Detection retains the documented
timing, onset and coverage limitations. These are real methods with bounded
supported scope, separate from the protocol-only fixtures below.

The test workers in `desktop/tests/eog_fixtures/` are protocol fixtures. They must
not be configured as the application's production provider or described as a
working detector, cost evaluator or independent numerical validator.

Installation uses the normal CMake deployment:

```powershell
cmake --build out/provider-normal --parallel 1
cmake --install out/provider-normal --prefix C:/path/to/new/breakwater-install
```

The install contains the production worker's eight modules, five adopted contract
files, native library and verification runner. Supply the pinned external Python environment when
launching; Python/Torch are not silently bundled. The offline inspection and
qualification CLIs are excluded. A checked MSVC build stages and installs the ASan
runtime from its actual compiler directory; a normal build needs no ASan runtime.
Installed-prefix startup and actual process execution remain acceptance checks,
not consequences inferred from a successful install command.

## Rehearsal runner

`breakwater_eog_live_check` uses the same production desktop controller, worker,
Nemotron transport and durable history. It never applies controls. Without
`--live`, it performs readiness checks and exits 3 with zero model requests.
With the existing local model credentials configured, run:

```powershell
.\out\provider-normal\desktop\breakwater_eog_live_check.exe --live `
  --dataset C:/path/to/canonical/data `
  --archive C:/path/to/evidence/canonical-1.eog-session.json `
  --db C:/path/to/evidence/eog-rehearsals.sqlite
```

The archive must be new and outside the dataset; use a different archive name for
each session. Optional `--cutoff` selects an exact historical Unix timestamp.
Optional `--objective` supplies operator intent without prescribing tool order.
The fixed production limits still apply, including the 12-tool/three-refinement
budget. The runner's outer deadline includes startup and evidence export.

Exit 0 requires actual hosted model identity, model-issued/executed tools, a
grounded final plan with successful independent exact-plan validation, current
review eligibility and a successful full archive export. Incomplete capabilities,
failed or cancelled investigations retain available evidence with nonzero status;
an exported archive alone does not pass the rehearsal. Offline CLI tests never
enable `--live` and use private temporary databases, including actual hardlink
input-protection checks. No credentials or raw model reasoning are printed.

For the edited-network rehearsal, `scripts/prepare-eog-demo.py` produces a new
three-file dataset plus provenance from an existing practice dataset. It removes
one whole facility, updates its operations/observations consistently and renames
all retained IDs, including Unicode identifiers. Retained measurement bytes,
timestamps, units and economics are preserved. The output must be new and outside
the source, with an existing parent directory:

```powershell
& $env:BREAKWATER_PYTHON -B scripts/prepare-eog-demo.py `
  --dataset C:/path/to/canonical/data --output C:/path/to/evidence/renamed-network
```

This is a derived practice variation for adaptation testing, not independent
held-out accuracy evidence. The transformer reads only the three fixed input
names, never labels or answer keys. Eleven offline regressions cover raw CSV
preservation, references, source changes, bounds and failed/raced publication.

## Operator flow

1. Open the three-file dataset in the EOG console and choose the analysis cutoff.
2. Open **Investigate & history**. Run **Detect events**, **Search plans**, an
   explicit proposal, or **Ask Nemotron**. Loading, navigating and charting make
   no hosted or numerical request.
3. Inspect original evidence, onset bounds, coverage, assumptions and limitations.
   An empty list before successful detection does not establish healthy operation.
   Acknowledging an event never clears it.
4. Select a baseline and candidate. Costs and overlaid trajectories require the
   same capture, context, start and complete repair horizons. Both runs remain
   individually viewable when incompatible. Volume is in bbl and elapsed time in
   hours; discrete truck removals preserve duplicate-time vertical steps. Negative
   or overflowing predictions remain visible.
5. Execute independent validation of the exact proposed plan, then **Review
   validated advice**. The review dialog retains the selected plan/hash,
   validation reference, itemized costs, actions, units and reported margins.
   A changed selection or invalidated capture disables its confirmation.

Review records advice viewed. There is no EOG dispatch, well control, source-file
rewrite or application to the sandbox. Do-nothing automatic shut-in may be an
admissible, cheaper outcome; feasibility does not mean shutdown was avoided.

Dataset/cutoff changes, replacement loads and cancellation retire the current
investigation. Selecting equipment, metric or scene does not alter the captured
analysis. New source hashes are verified before any worker sees input.

## Nemotron and evidence

The existing hosted client is reused in a separate EOG conversation. Credentials
reload when an explicit new investigation begins, including after replacement or
forgetting through the existing credentials dialog. Model configuration/identity,
HTTP status, duration and retries are the only retained request metadata.

The six tools are `read_topology`, `read_observations`, `detect`, `evaluate_plan`,
`search_plans` and `validate_plan`. The model chooses their order. All numerical
operations execute through the same bounded subprocess adapter. Raw read queries
are paginated/bounded and causal to the fixed cutoff. The loop allows at most 12
tool calls, three candidate refinements and 120 seconds including tool time.
Cancellation and stale completions cannot finish a newer investigation.

Complete numerical results stay in local evidence. Model tool outputs can compact
long trajectories/candidate lists and explicitly disclose that compaction. The
final explanation must reference retained events and an available, independently
validated exact plan with minimum cost among compatible retained admissible
candidates. The UI rejects unsupported final references. Search scope and
limitations remain visible; no global optimum is inferred from a bounded search.

## Durable evidence

EOG uses its own worker-owned SQLite database. The normal application-data location
is separate from native/scientific workbench history. Supplying `--history-db`
uses a sibling path ending `.eog.sqlite`; the original database is not repurposed.
Storage failures are visible and do not become numerical success.

Completed evidence includes the actual compressed three-file capture, manifest,
requests, responses, trajectories, validation and ordered tool-call/result metadata.
The application stores each operation transactionally. Unfinished sessions become
interrupted after restart. **Finish session** seals manual evidence; recording
review also completes its session before appending the review record.

Export requires a new `*.eog-session.json` destination and never overwrites an
existing file. Imports are bounded, validated and atomic. Reopened/imported
sessions are historical and cannot restore current review eligibility. The
historical explanation and final selected plan are reconstructed from the retained
final event; operator records remain separate from event clearing or control
authority. See [history format and limits](EOG_HISTORY.md).

## Verification scope

The first normal-build controller run passed 12 cases and failed two. One was a
test destination with the wrong export suffix; the other reproduced enabled review
after a rejected model final. The corrected normal run passed **14/14 Qt cases**
with zero skips. The isolated model loop passed **41/41 Qt cases**, and the history
suite passed **13/13**. Original failed logs are retained under local `out/`.

Chart tests reproduced a QML final-property name collision and initialization
ordering defect. Both were corrected. Exact checkpoint `7810985` then passed
**16/16 scoped EOG CTests** in normal (46.07 s) and ASan (138.09 s) builds, with
**212 Qt cases** per mode, zero skips and no sanitizer diagnostic. The executed
checks include 48 Python/native contract methods, eight actual CLI methods,
chart pixel regressions and six normal/compact application render checks.

A subsequent independent same-family review reproduced a missing publication
gate: shape-valid evidence with a wrong source value/time/channel could reach
the UI before history rejected it. Commit `c15269a` moves the shared original-sample
check into background client validation before publication. That corrected source
gate passed **16/16 CTests**, **230 Qt cases** per mode, in normal (49.89 s) and
ASan (188.59 s), with no skips or sanitizer diagnostic.

The later operator panes show readable truck schedules, well actions, itemized
costs, independent validation/margins and original samples with units, timestamps,
reported quality and qualifications. Large lists render lazily, exact JSON opens
on demand, and session changes clear selected event evidence and old dialogs.
The current runs passed **18/18 EOG CTests**, **250 Qt cases per mode**, zero skips:
normal 65.78 s and MSVC ASan 167.60 s. The first ASan bulk-history test exceeded
its 15-second wait while completing all 102 separate durable writes at about
19.7 seconds. Independent bounded-path review supports a 60-second fixture wait;
write counts, pagination, heartbeats and production deadlines remain intact.
The heartbeat count now resets at workload start. The first plan-details
test failure is retained: its QObject-only lookup missed real visual ListView
delegates; corrected discovery preserves all evidence and lazy-bound assertions.

Reproduce the scoped checks from a compiler-enabled shell with the pinned Qt/Python
runtime and actual D3D11/WARP rendering:

```powershell
cmake --build out/eog-normal --parallel 1
ctest --test-dir out/eog-normal -R '^(contracts\.eog|desktop\.eog_.*)$' --output-on-failure
cmake --build out/eog-asan --parallel 1
ctest --test-dir out/eog-asan -R '^(contracts\.eog|desktop\.eog_.*)$' --output-on-failure
```

Use native Windows execution for fixture hard links; a sandbox access denial is a
setup failure, not a skip or numerical result. The ASan build instruments project
code, including the chart/details harnesses; prebuilt Qt/Python are outside that
instrumentation claim. Final combined sandbox/EOG, packaging and genuine
production-provider/hosted EOG acceptance remain pending. PR #40 and PR #41 supply
real detection, evaluation and independent validation; the joined focused checks
pass 4/4 CTests in normal (29.44 s) and ASan (42.02 s), with 73 Qt cases and seven
offline Python CLI methods per mode, zero skips or sanitizer diagnostics. Joint
search remains unavailable. Existing workbench rehearsals do not count as EOG
rehearsals, and presentation/protocol fixtures do not qualify numerical methods.
