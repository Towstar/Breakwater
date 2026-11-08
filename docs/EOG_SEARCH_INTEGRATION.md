# EOG search provenance boundary

Person B integration of A's `a420deb` keeps the adopted `72cadd1` public wire and
existing history format unchanged. Search responses and their complete retained
evaluations, family declaration, limitations and capture/request bindings already
fit that format. Old archives remain readable and historical, without actionable
review authority.

The version-1 response has no explicit worker implementation identity or health
operation. `method_version: 1` names the operation contract. The client's sanitized
`method` metadata names the requested operation, such as `search_plans`; it does
not assert a numerical or worker implementation. The worker's internal method
constant participates in an opaque analysis hash, which cannot be reversed into
an attestation. No version is inferred from a filename, environment variable or
an injected fixture. Installation/source-hash evidence records which packaged
worker was exercised separately.

The complete response retains the provider's free-form search family declaration
and explicit evaluator/validator fields where supplied. A bounded tool-result
summary also copies actual search scope, optimality, completeness and candidate
counts. This is a projection of received claims, not a new implementation identity
or independent verification. It does not make `best_found` global optimality and
does not turn an unavailable selection into advice. Current review still requires
the separate complete validation response for the exact selected plan.

Direct client tests use a conspicuous protocol-only process and require absent
implementation labels. Investigator tests cover bounded, complete-family,
unavailable and malformed summary fields, preservation of the original tool
response and no inferred final selection. These additions require the integrated
normal/ASan gates; source changes alone do not establish executed search or live
model behavior.

## Executed integration evidence

The CPU discovery ran at `fef231e919d85f42f63e9e15a9109228283b9f29`.
Science, native source, contracts and the discovery runner were unchanged from
requested `6613b66`; science was also identical to adopted A source
`a420debc7b6518a348272f168d752c88fdd58221`. No installation/setup or source edits
were needed for this run. Genuine NTFS hard-link tests ran in native elevated
execution rather than being skipped after a sandbox denial.

| Scope | Actual result |
|---|---|
| Full CPU science discovery | **377 run: 373 passed, 4 expected PhysicsNeMo skips**, 0 failures/errors/unexpected skips; runner exit 0 |
| Discovery timing | 50.809 s unittest time; 56.6587579 s wall time |
| Real-provider focused integration at `6e89fa6` | **6/6 normal**, 49.59 s; **6/6 ASan**, 100.24 s |
| Controller follow-up at `fef231e` | **1/1 normal**, 13.73 s; **1/1 ASan**, 28.39 s |
| Relocated installed UI at `fd170cf` | Normal and compact each pass **9 Qt interaction gates**; this is not a hosted-search gate |
| Historical release follow-ups | Full normal/ASan/VS and prompt checks recorded below, each at its stated source |
| Finalization at `b9d8244` | **7/7 critical normal**, 58.63 s; **7/7 critical ASan**, 136.93 s; latest full/live gates remain pending |

The controller follow-up bounds diagnostic previews by UTF-8 bytes, reserves
tool-envelope space and says complete evidence is retained locally/in the
registry. It does not promise a completed asynchronous SQLite save. The real
provider case executes search, then a separate exact-plan validation and current
advisory review, and checks historical reopening cannot renew authority. The
large-response model exercise is explicitly a protocol fixture, not numerical
qualification or a hosted model execution.

The only skips are the four `ActualPhysicsNeMoTests`, each with the exact allowed
reason `PhysicsNeMo is optional in the A3-only environment`. Torch/native search,
worker processes and the hard-link regression were exercised. ASan results cover
owned native code, not Python/Torch or prebuilt Qt.

## Retained bytes, method declarations and provenance

[The public evidence directory](evidence/eog-search-integration/) retains exact
JSON/log copies, with [SHA-256/size/source records](evidence/eog-search-integration/manifest.json).
Local attributes disable text normalization so hashes survive Windows checkout.

- [Discovery result](evidence/eog-search-integration/search-science-results.json),
  [full discovery log](evidence/eog-search-integration/search-science-tests.log)
  and [run/source record](evidence/eog-search-integration/search-science-run.json).
- [Actual native export identity](evidence/eog-search-integration/search-science-native-identity.json):
  `eog-native-seconds-0.1.1`; DLL SHA-256
  `63b3e8bf7d3d4c5d41b3b01915eba850ca34a51bc8128c640233bc9a19cf0366`,
  unchanged before/after discovery. Native sources were unchanged from verified
  UI base `6cfa3ee`, with their individual hashes recorded.
- Packaged source declares worker `eog-worker-0.3.2` and search
  `eog-joint-search-0.1.2`. Worker file SHA-256 is
  `3f0e3c71f3faa9f1738b3ce1ac5df908b43fa65b94d62d0a73d814bf52f048f8`;
  search file SHA-256 is
  `09fa12c0634aa8cafcb27a6107d6c275cea848aae968f189ebfb876c0bdb22ed`.
  These are source declarations/pins, not identity attested by the version-1 wire.
- [Focused normal](evidence/eog-search-integration/focused-normal.log) /
  [ASan CTests](evidence/eog-search-integration/focused-asan.log) and
  [controller normal](evidence/eog-search-integration/controller-normal.log) /
  [ASan follow-up](evidence/eog-search-integration/controller-asan.log)
  were produced by the integration owner and copied without rewriting.

Absolute paths in the retained identity/run/probe records describe local build,
interpreter and contracts provenance. Source-path fields do not direct archive
imports. This directory contains no raw protected capture, credentials, hosted
request/response transcript or claim of a complete human-driven workflow.

## Open A-owned finite-extreme limit

A P3 availability issue remains at the finite extreme: the retained
[direct probe](evidence/eog-search-integration/finite-overflow-probe.log) uses an
accepted finite truck capacity `1e308`. In `science/eog_search.py:283`, multiplying
trip count by capacity overflows to infinity; `_greedy` at line 111 then raises
`OverflowError` while constructing `Fraction(removed)`. The worker's existing
generic exception handler maps such failure to `failed` / `provider_failed`,
not selected advice. That failure-envelope conclusion is source review; this
retained probe directly exercised the priority helper, not a full worker exchange.
Related products at lines 161 and 314 are source-review observations only.

Ethan owns the numerical correction and its regression; B has not changed that
math or represented the extreme as feasible. A's later stateful-simulation head
`091b490` is a separate increment and is outside the search evidence above.

## Completed release follow-ups

[Additive release records](evidence/eog-search-integration/release-checks.json)
retain the original nine-artifact manifest unchanged and add exact log/JSON/XML
copies with their source revision, original path, byte count and SHA-256.

| Revision / completed scope | Actual result | Retained record |
|---|---|---|
| `fef231e` full normal CTest | **64/64**, 418.66 s | [Log](evidence/eog-search-integration/full-normal-fef.log), [JUnit](evidence/eog-search-integration/full-normal-fef.xml) |
| `fef231e` full ASan CTest | **64/64**, 933.33 s | [Log](evidence/eog-search-integration/full-asan-fef.log), [JUnit](evidence/eog-search-integration/full-asan-fef.xml) |
| `fef231e` Visual Studio EOG/UI CTest | **23/23**, 87.22 s | [VS](evidence/eog-search-integration/full-vs-fef.log) |
| `fd170cf` operator-goal focused normal / VS | **2/2**, 21.09 s / **2/2**, 21.03 s | [Normal](evidence/eog-search-integration/operator-goal-normal-fd170.log), [VS](evidence/eog-search-integration/operator-goal-vs-fd170.log) |
| `fd170cf` relocated installed UI | **9 Qt gates per mode**, normal + compact; both exit 0 | [Combined](evidence/eog-search-integration/installed-ui-fd170-results.json) |

Both full-suite JUnit records contain 64 test cases without failure/skipped nodes.
This CTest count is distinct from discovery's four explicitly allowed optional
PhysicsNeMo skips. Full-suite records belong to `fef231e`; the later prompt change
has its own focused evidence rather than an inherited new full-suite claim.

The installed records include exact [normal](evidence/eog-search-integration/installed-ui-fd170-normal.json)
and [compact](evidence/eog-search-integration/installed-ui-fd170-compact.json)
stdout, plus their loaded app/Qt module hashes
([normal](evidence/eog-search-integration/installed-ui-fd170-normal-modules.json),
[compact](evidence/eog-search-integration/installed-ui-fd170-compact-modules.json)).
The module-enumeration harness refreshed an earlier cached enumeration; the
retained final observations match the installed files. This was a harness-state
issue, not a failed UI interaction. These runs requested no hosted model or
scientific-worker operation.

`fd170cf` treats the operator's objective as a user task within fixed tool,
safety, causality and evidence rules. Context, asset labels and tool outputs
remain data. The objective cannot authorize file access, dispatch, physical
control, sandbox editing or approval-rule overrides. This is a prompt-boundary
correction, not a new numerical capability. Earlier live failures remain
retained; no claim is made that this wording caused or cured those failures.

The additive release record above preserves its earlier checkpoint, including
then-pending prompt/live entries. It is historical evidence, not the current
status of later attempts or revisions. The latest finalization scope follows.

## Bounded final-format correction at `b9d8244`

`b9d82443bdf90e9d828a0dee096973d9f3093e69` gives one investigation **180 seconds
overall**, with the existing **12-tool / 3-candidate-refinement** defaults. The
model still chooses tool order. A tool-free reply declares final intent; only
after that reply fails strict bounded final-format checks may the loop request
**one** JSON-only correction, within the same deadline and 13-request ceiling.
No rejected final text is inserted into the conversation, displayed or archived.
The correction instruction contains only a fixed rejection-stage literal.

The separate transport request sends `response_format: {"type":"json_object"}`
and `temperature: 0`, omitting both `tools` and `tool_choice`. Normal adaptive
requests retain their existing tool-calling behavior; JSON mode is never combined
with tools. The existing token/reasoning budgets remain unchanged. JSON syntax
does not establish the required fields, truthful evidence or successful advice:
the corrected response must pass the same strict final schema and controller
checks for retained IDs, cheapest compatible admissible plan and its exact
independent validation. A fabricated reference or forbidden correction-phase
tool call remains a terminal failure. Cancellation, retirement and deadline
guards also apply to the queued correction.

NVIDIA's [Helix hosted example](https://docs.nvidia.com/nemo-helix/v0.7/documentation/evaluate-models/metrics/agentic-metrics/)
uses this exact model/endpoint with JSON-object output. Its
[model-specific NIM guide](https://docs.nvidia.com/nim/large-language-models/2.0.10/get-started/advanced/get-started-nemotron-3.5-lightning.html)
demonstrates tool calling and JSON mode separately and requires client schema
validation. That guide is for self-hosted NIM; it does not guarantee hosted
concurrent tool/structured output or `json_schema` support. The
[hosted reference](https://docs.api.nvidia.com/nim/reference/nvidia-nemotron-3-5-lightning-30b-a3b-infer)
also does not establish those combinations. The final-only transition is our
engineering choice, not an NVIDIA guarantee of successful project completion.

The controller records a nonterminal existing `failure` event with code
`final_format_rejected_<literal-stage>`, keeps the session open and labels it
“Final format correction requested.” Terminal format failure instead persists
`invalid_final_<literal-stage>` and disables current review. No arbitrary parser
message or model payload supplies those codes. The CLI exempts **only ten exact
nonterminal codes**, through the shared finite allowlist; unknown prefixes,
terminal failures, cancellation and failed grounding still fail its strict gate.
The analysis/history schemas are unchanged.

[Finalization records](evidence/eog-search-integration/finalization-checks.json)
add exact [normal](evidence/eog-search-integration/finalization-normal-b9.log)
and [ASan](evidence/eog-search-integration/finalization-asan-b9.log) logs: **7/7 in
58.63 s / 7/7 in 136.93 s** at `b9d8244`. The seven CTests cover transport,
orchestration, client, SQLite history, investigator, controller and real provider.
Protocol fixtures check retry persistence before completion, strict corrected
grounding, malformed terminal classifications, payload exclusion and imported
history's lack of current review authority. They do not qualify hosted behavior.
The manifest records eight runtime source hashes matched across both build trees
and the later test-only checkout, while preserving both older manifests unchanged.

At that historical checkpoint, the new full normal/ASan runs and attempt07 were
still running. No complete strict search-plus-final pass was qualified for
attempts01–06; their distinct successful partial exchanges/gates remain evidence.
The later terminal local results and corrected objective provenance are below.

## Windows generated-fixture line endings

The downloaded Windows CI artifact associated with `4aea2f2` records actual PR
merge checkout `6c67052e77c3bd7b5da1891da842b26d57bc239a`, run `42493697968`.
It has **63/64** desktop CTests passed, 536.26 s; the controller report has
**30 passed / 1 failed** at `source.contains(branch)`. Retained exact
[controller output](evidence/eog-search-integration/ci-4aea-controller-results.txt),
[CTest log](evidence/eog-search-integration/ci-4aea-ctest.log),
[verification metadata](evidence/eog-search-integration/ci-4aea-verification.json)
and [job outcome](evidence/eog-search-integration/ci-4aea-job-finish.json) preserve
that failure rather than converting it into a pass.

The generated protocol-worker insertion used an LF-only branch needle against
Windows CRLF bytes. An [in-memory reproduction](evidence/eog-search-integration/ci-4aea-crlf-reproduction.json)
confirms the mismatch and successful match after normalization.
`05f906f987ea6096012a992014a48a743cc7ba16` normalizes **only that generated test
script** before insertion. Its single-file delta changes no production runtime,
captured input, telemetry text or numerical provider. The manifest distinguishes
the tested `b9d8244` runtime from this test-only follow-up; neither the byte
reproduction nor the earlier local passes constitute a completed CI rerun of it.

## Final release checks at `a5bbbd5`

[The additive final-release checkpoint](evidence/eog-search-integration/final-release-checks.json)
pins 23 further artifacts without changing the three earlier manifests.

The full-suite application checkpoint is
`a5bbbd572d9bd9436acc5a16376fcfb484c1cbf0`. Its shared investigation limit is
**240,000 ms (four minutes)**, and the hosted transport's default per-attempt
timeout is **60,000 ms**, following `8d91b8e`. The 12-tool, 3-refinement,
13-model-request and one-format-correction ceilings are unchanged. The four-minute
investigation deadline still cancels an outstanding tool/model operation; it is
not an extra allowance for each retry. Historical `b9d8244` results and its
three-minute limit remain recorded at their own revision above.

The equipment sidebar now keeps its 208 px width through the full content height.
Five existing equipment actions and native Connect occupy a 3 × 2 icon grid;
hover or keyboard focus reveals their labels. The network list receives the
remaining sidebar height and an always-visible native scrollbar with reserved
text space. The four lower actions and status remain outside that list. The
adjacent scene/inspector and forecast/investigator column retains the viewport
and original placement, picking, drag and keyboard handlers. Source review found
no additional consequential issue; the author reviewed some own B code, with a
separate same-family source review of the sidebar. This is not a manual OS-input
or complete palette-interaction qualification.

The latest critical checks are **5/5 normal, 28.76 s**, and **5/5 ASan, 68.28 s**:
both UI sizes, hosted-client offline tests, investigator and controller. The
completed full normal run is **64/64, 377.75 s**, and full ASan is **64/64,
996.96 s**, both with verified exit 0. Their logs/JUnit belong to `a5bbbd5`,
rather than inheriting an earlier source's pass. The later additive delta record
retains [ASan log](evidence/eog-search-integration/full-asan-a5.log) and
[JUnit](evidence/eog-search-integration/full-asan-a5.xml); the older checkpoint's
then-pending entries remain unchanged.

The Visual Studio build and 25-test gate are retained separately: the initial
sandbox execution was **23/25, 141.55 s**, because actual `os.link` calls were
denied in both CLI suites ([log](evidence/eog-search-integration/vs-a5-sandbox-failed.log),
[JUnit](evidence/eog-search-integration/vs-a5-sandbox-failed.xml)). No mandatory
hard-link check was skipped. The same 25 tests then passed in native execution,
**25/25, 114.09 s** ([log](evidence/eog-search-integration/vs-a5-native.log),
[JUnit](evidence/eog-search-integration/vs-a5-native.xml)); the
[full build log](evidence/eog-search-integration/vs-a5-build.log) is also retained.

The later `bd3e3be` / `a23fb70` follow-up allows only a well-formed, explicitly
stopped empty assistant response with present content to reach the caller's
strict validator. In EOG that permits the existing single format correction;
it does not make empty content advice. Missing content or unknown/null/missing
finish reason, truncation, malformed payload and private visible reasoning stay
failures. At `a23fb70`, **7/7 normal, 49.28 s**, and **7/7 ASan, 100.20 s**, pass
the focused client/loop/controller/provider/CLI checks
([normal](evidence/eog-search-integration/stopped-final-normal-a23.log),
[ASan](evidence/eog-search-integration/stopped-final-asan-a23.log)). This is a
later delta gate, not a new full-suite result. The actual stop reason of attempt09
was not recorded; none is inferred from its empty-response error.

The earlier `b9d8244` full normal run completed **63/64, 387.83 s**; its
[failed log](evidence/eog-search-integration/full-normal-b9-failed.log) and
[JUnit](evidence/eog-search-integration/full-normal-b9-failed.xml) retain
`outside_notice_escape_did_not_cancel_connection` at DPR 1.75. `0c0dbf4` makes
the verification harness wait for actual window activation and settled field
focus before testing the native global Shortcut. It does not weaken the product
Escape rule or bypass the assertion. The historical full ASan run was
**64/64, 936.22 s** ([log](evidence/eog-search-integration/full-asan-b9.log),
[JUnit](evidence/eog-search-integration/full-asan-b9.xml)). Both results remain
separate from the later source's full normal pass, rather than being overwritten.

The separate [hosted JSON-only smoke](evidence/eog-search-integration/json-mode-smoke-verification.json)
completed one logical request: **HTTP 200, 5,887 ms, zero retries**, with the exact
configured/returned `nvidia/nemotron-3.5-lightning-30b-a3b` model. Its
[compiled harness](evidence/eog-search-integration/json-mode-smoke.cpp) requires
exact parsed equality to a fixed four-field public JSON object, rejects duplicate
root keys, and sends no tools through the production JSON-only transport path.
The [stdout](evidence/eog-search-integration/json-mode-smoke-stdout.json) and
[compilation provenance](evidence/eog-search-integration/json-mode-smoke-compile-provenance.json)
are retained unchanged. This qualifies that JSON-only protocol exchange, not
concurrent tool/JSON mode, EOG numerical results, adaptive search, a plan or control
authority. The raw API body and credentials are not retained. Compilation records
pin actual harness/object/executable hashes; they do not independently attest a
whole application source revision.

### Rehearsal objective correction

The rehearsal harness had a PowerShell case-insensitive-name collision: its local
`objective` variable overwrote the `Objective` parameter. Consequently attempts
**01–07 all received the default objective**. Their failures do not show that the
model ignored an explicitly supplied search objective. This corrects the harness
interpretation without erasing any incomplete/failed attempt or its partial gates.
The [archive-objective audit](evidence/eog-search-integration/objective-harness-correction.json)
retains the seven actual objectives and the correction. The fixed harness's
[offline Unicode propagation check](evidence/eog-search-integration/objective-propagation-v2.json)
requests no hosted model. Attempt08 was the first verified explicit-objective
launch: [requested, effective and actual process arguments](evidence/eog-search-integration/objective-arguments-08.json)
match. Attempt09 later ended incomplete; its functional metrics and the earlier
outcomes are owned by the separate rehearsal archive/projection. This local-test
and JSON-smoke evidence does not qualify a complete live search-plus-final gate.

The [05f906f Windows CI run](https://github.com/Towstar/Breakwater/actions/runs/42497354023)
now reports **six successful jobs**: normal/ASan native, normal/ASan desktop,
CPU science and Lean audit. Retained GitHub connector
[run](evidence/eog-search-integration/ci-05f906f-runs.json) and
[job](evidence/eog-search-integration/ci-05f906f-jobs.json) projections establish
that result. They do not qualify the later `a5bbbd5` runtime/UI changes on hosted
CI, and do not replace the preserved `4aea2f2` failure.

A's newer robustness head `6e58ed5762f8fb40dd1fe0693fafe786bfbcf0aa` contains the
finite-extreme fix at `343d7e93d9aafae329ab3815dccf4ed275832d95`: convert each
finite truck load to `Fraction` before aggregate seed multiplication. It is
**not adopted by this frozen application**. The inspection/stateful/synthetic
changes in that ancestry remain a separate adoption and qualification boundary;
their evidence cannot relabel the runtime tested here.

## Panel polish and real Qt navigation at `e8210a4`

[The append-only delta/panel manifest](evidence/eog-search-integration/delta-panel-checks.json)
pins 21 additional files and the hashes of all four earlier manifests.

`e8210a4e4f5eebc78fc43b18fed25542a619c2b2` changes seven QML views, preserving
their controller callbacks, numerical inputs, validation and approval conditions.
The investigator has a responsive objective/action area, compact status, two
scrollable evidence cards and aligned empty states. Sensor, monitoring and pipe
views keep the sidebar accessible; their native buttons visibly reflect the open
view, a second click closes it, and selecting another view closes the previous
one. Top-right close actions retain the original dialog close/key-clear behavior.

The separate native [panel-probe result](evidence/eog-search-integration/panel-probe-e821-results.json)
passes **16 actual Qt-input checks per size (32 total)** at **DPR 1.75**:
normal **1440 × 900**, compact **1100 × 700**. Checks cover opening and same-button
closing all four sidebar views, the three panel close buttons, exclusive
navigation, and preserved network/control state. Both processes exit 0 with empty
stderr. This probe links the compiled production QML/resources and controllers,
with an out-only verification entry point; it is **not the installed executable**
or a human OS-driven session. No hosted or scientific operation is requested.
Its [compilation pins](evidence/eog-search-integration/panel-probe-e821-compile.json)
and [source](evidence/eog-search-integration/panel-probe-e821.cpp) preserve that
boundary. The six screenshots below are copied unchanged from actual frames.

| Actual size | Sensor editor | Local monitoring | Pipe transport |
|---|---|---|---|
| Normal | [PNG](evidence/eog-search-integration/panel-sensor-normal-e821.png) | [PNG](evidence/eog-search-integration/panel-monitoring-normal-e821.png) | [PNG](evidence/eog-search-integration/panel-pipe-normal-e821.png) |
| Compact | [PNG](evidence/eog-search-integration/panel-sensor-compact-e821.png) | [PNG](evidence/eog-search-integration/panel-monitoring-compact-e821.png) | [PNG](evidence/eog-search-integration/panel-pipe-compact-e821.png) |

The final normal panel scope passes **25/25, 226.14 s**
([log](evidence/eog-search-integration/panel-polish-normal-e821.log),
[JUnit](evidence/eog-search-integration/panel-polish-normal-e821.xml)). The first
Visual Studio panel run is **24/25, 106.32 s**: the compact UI check reports
`actual_drag_did_not_move` after its first three gates
([failed log](evidence/eog-search-integration/panel-polish-vs-e821-failed.log),
[JUnit](evidence/eog-search-integration/panel-polish-vs-e821-failed.xml)). This is
retained as a failure, not explained away by the distinct passing click-only
panel probe. The `e8210a4` ASan panel run subsequently passed **25/25, 376.43 s**
([log](evidence/eog-search-integration/panel-polish-asan-e821.log),
[JUnit](evidence/eog-search-integration/panel-polish-asan-e821.xml)). Later current
source checks follow; the initial VS failure remains preserved.

## Final popup and installed-bundle completion at `28d873c`

[The final completion manifest](evidence/eog-search-integration/release-completion-28.json)
adds 24 exact artifacts without overwriting prior checkpoints. `d855c1e` and
`28d873c` keep the three tool panels below the application toolbar and align titles
with content padding. Their focused gates pass with verified exit 0:

| Final source scope | Result | Exact log |
|---|---|---|
| Normal | **9/9, 41.35 s** | [Normal](evidence/eog-search-integration/popup-header-normal-28.log) |
| ASan | **9/9, 89.46 s** | [ASan](evidence/eog-search-integration/popup-header-asan-28.log) |
| Visual Studio | **9/9, 43.68 s** | [VS](evidence/eog-search-integration/popup-header-vs-28.log) |

These are delta checks, not a new full-64-suite claim. Both UI sizes, including
actual drag, pass in the later VS focused run. Possible overlapping window
activation during the earlier failed run is not an established root cause.

The fresh [32-check panel probe](evidence/eog-search-integration/panel-probe-28-results.json)
again passes 16 checks per size, with popup `y >= 76` and title insets 14/16 px.
At compact size the observed popup is `x=220, y=76, 864 × 604`, keeping the sidebar
and toolbar clear. [Compilation pins](evidence/eog-search-integration/panel-probe-28-compile.json)
identify the new compiled resources; the older `e8210a4` images remain historical.

| Final actual size | Sensor editor | Local monitoring | Pipe transport |
|---|---|---|---|
| Normal | [PNG](evidence/eog-search-integration/panel-sensor-normal-28.png) | [PNG](evidence/eog-search-integration/panel-monitoring-normal-28.png) | [PNG](evidence/eog-search-integration/panel-pipe-normal-28.png) |
| Compact | [PNG](evidence/eog-search-integration/panel-sensor-compact-28.png) | [PNG](evidence/eog-search-integration/panel-monitoring-compact-28.png) | [PNG](evidence/eog-search-integration/panel-pipe-compact-28.png) |

The actual relocated Ready bundle separately passes **9 Qt interaction gates per
size**, 2,724/2,509 ms, exit 0, at DPR 1.75. Its
[installed report](evidence/eog-search-integration/installed-ready28-results.json)
and per-mode module records pin **25 loaded app/Qt modules** each; their hashes
were rechecked against the installed files. Main SHA-256 is
`25b4f4bc73f183196af5c5d3a85edfe3dfa19a6b0242b8d8d7bfb8966ee4da68`.
This is separate from the out-only panel probe. Neither requests hosted or
scientific execution. Actual installed investigator-drawer frames are also
retained ([normal](evidence/eog-search-integration/drawer-normal-28.png),
[compact](evidence/eog-search-integration/drawer-compact-28.png),
[render record](evidence/eog-search-integration/drawer-checks-28.json)). These are
real Qt event/render checks, not a complete human OS-input workflow.

## Genuine hosted search rehearsal 10

The [sanitized canonical-10 projection](evidence/eog-search-integration/live/canonical-10.json)
passes the complete search-plus-final gate in **221.980 s**: **11 hosted requests
and replies**, **10 model-issued calls / 9 successful tool executions**, numerical
search, separate exact-plan independent validation, a grounded final with current
review eligibility, and a written evidence archive. It retains the genuine model
order, including one unavailable search result and subsequent recovery; numerical
exchanges were **8 completed of 9**. No controls were applied.

The installed rehearsal runtime was **`a23fb70`**, while the checkout had advanced
to QML-only `e8210a4`. This run used **no JSON-format correction**; its success
does not prove that the new correction caused the final response to work. It is
**one new successful search rehearsal, not three**, and does not qualify a new
renamed/topology-edit rehearsal. The [01–10 ledger](evidence/eog-search-integration/live/attempt-ledger-01-through-10.json)
preserves the incomplete earlier outcomes and the objective-harness correction.
The [live manifest](evidence/eog-search-integration/live/manifest.json) and
[completion update](evidence/eog-search-integration/rehearsal-10-update.json)
pin the public bytes; the complete protected archive remains local. The projection
excludes credentials, raw reasoning and runtime capture paths. This is a real
hosted-model/production-controller result, distinct from protocol fixtures and
the separate fixed-object JSON smoke.
