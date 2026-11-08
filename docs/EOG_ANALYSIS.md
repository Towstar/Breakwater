# EOG analysis, advisory review and history

The EOG workflow uses the shared capture/analysis contract `72cadd1`. It has no
reference to the workbench's operating controls. A captured dataset, numerical
analysis, model explanation and operator review remain distinct evidence.

## Local provider configuration

Use the existing pinned local Python environment. When Ethan supplies the real
version-1 provider, configure its actual entry point in the launching process:

```powershell
$env:BREAKWATER_PYTHON = 'C:\path\to\Breakwater\.tools\science-cpu\Scripts\python.exe'
$env:BREAKWATER_EOG_WORKER = 'C:\path\to\the\real\eog_worker.py'
$env:BREAKWATER_EOG_EVALUATOR_METHOD = '<actual evaluator implementation identifier>'
```

The evaluator identifier must come from the provider's documented implementation;
`method_version: 1` identifies the operation contract, not a numerical algorithm.
Manual validation asks for this identifier explicitly. The model receives the
locally configured identifier as context. The provider must check it.

The default worker lookup checks an installed `share/breakwater/science/eog_worker.py`
and the development source location. Existence means configured, not executed or
numerically qualified. A successful, bound process exchange establishes protocol
execution only. Missing methods or evidence remain explicitly unavailable.

The test workers in `desktop/tests/eog_fixtures/` are protocol fixtures. They must
not be configured as the application's production provider or described as a
working detector, cost evaluator or independent numerical validator.

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

Chart tests initially reproduced an actual QML final-property name collision;
the chart input was renamed before further render checks. Complete normal/ASan,
render, packaging and production-provider/hosted EOG acceptance are still pending
for this increment. Existing workbench rehearsals do not count as EOG rehearsals.
