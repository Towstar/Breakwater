# Bounded Nemotron recovery and evidence explanations

This Person B increment follows the expanded evaluation at PR #94. It addresses
completion references, server retry waits and useful factual explanations. The
engine, scientific methods and numerical qualification remain unchanged.

## Completion references

A final selection that names a known stored run or plan without its own eligible
explicit validation receives one recovery opportunity, if the original tool,
request and absolute-time budgets permit it. During recovery the model can issue
one `validate_run` or `validate_plan` call, or decline selection. Candidate search
and staging tools are closed. The application does not select a replacement or
execute validation on the model's behalf.

Successful completion still needs the exact stored selection, independent
validation, matching identity and current captured state. Operations also keeps
its compatible-plan cost and event-reference checks. Repeated wrong references,
an extra tool, cancellation, stale state or exhausted budgets cannot become a
recommendation. Semantic and format repairs share one recovery opportunity; a
semantic repair cannot start a second format-repair cycle. Unknown references
remain subject to the complete evidence store's rejection checks. A truncated
model-facing index never proves that an omitted record is absent from storage.
An already completed infeasible model-issued check remains rejected without
starting another reference-repair validation.

History retains the rejected-reference event and subsequent actual tool order.
Reopening or importing recovered history does not restore actionable approval or
current advisory review. A live Sandbox recommendation still needs a matching
staged plan and explicit operator approval before simulated controls can change.

## Retry waits and deadlines

Production request owners pass the remaining investigation time to the hosted
transport. This budget covers HTTP attempts and backoff, and does not reset for
a retry or completion repair. The transport accepts nonnegative `Retry-After`
seconds and HTTP dates, including the obsolete HTTP date formats. A valid server
wait is honored when it fits the remaining budget; it is never shortened to five
seconds. A wait that cannot fit stops with a typed failure instead of another
POST. Invalid headers use the existing bounded backoff and retry-count policy.

The monotonic budget is checked before sending and accepting a reply, including
after a delayed event loop. Retry and budget timers use precise timing. Cancelling
backoff stops the scheduled retry. A subsequent request gets a fresh identity and
cleared retry metadata. These changes cannot guarantee hosted service availability
or establish the account's quota or reset time.

Optional history metadata contains only a closed `retry_wait_policy` literal and
up to three integer `retry_delays_ms` values. These are **scheduled waits**; a
cancelled wait need not correspond to a posted retry. `retry_count` continues to
count actual additional HTTP attempts. Raw header values, credentials, response
bodies and private reasoning are excluded. Legacy archives without the optional
pair remain valid; malformed pairs and unknown metadata fields are rejected.

## Evidence-owned explanations

Sandbox renders stored cost, changed controls and units, validation reference and
headroom, initial/terminal tank volumes and actual capacity, and the first
recorded violation. Operations renders the exact plan hash and cutoff, response
horizons, cost components, planned truck trips and well shut-in intervals,
predicted automatic shutdown records, tank bounds and independent shutdown margin.
Long sections explicitly refer to the full stored evidence. Terminal inventory
is never relabeled capacity; planned shut-ins are separate from automatic shutdown.

These facts come from the retained numerical records. Original model prose stays
separately labeled. Nominal feasibility does not establish global optimality,
mechanical diagnosis, field accuracy or an uncertainty guarantee. The renderer
executes no model/tool call and changes no controls. Reopened Operations history
rebuilds the same factual explanation from its stored evidence.

## Local verification

Use the configured normal and AddressSanitizer build trees, pinned Qt and CPU
Python environments. For each build:

```powershell
. .\scripts\ci\Initialize-Compiler.ps1
cmake --build <build-directory> --parallel 2 --target `
  breakwater_nemotron_client_tests breakwater_agent_orchestrator_tests `
  breakwater_agent_gateway_tests breakwater_eog_investigator_tests `
  breakwater_eog_analysis_controller_tests breakwater_eog_history_tests
ctest --test-dir <build-directory> --output-on-failure `
  -R '^desktop\.(nemotron_client|agent_orchestrator|agent_gateway|eog_investigator|eog_analysis_controller|eog_history)$'
python -B -m unittest discover -s scripts/ci/tests -p test_candidate_evidence.py -v
```

The offline fixtures cover successful model-issued recovery, wrong references,
restricted tools, finite budgets, cancellation, stale captures, one shared repair
opportunity, persisted recovery and imported-history review rejection. Transport
checks include a real six-second scheduled wait, HTTP date parsing, invalid and
over-budget waits, delayed retry callbacks, cancellation, metadata privacy and
request reuse. Fixed fixture facts distinguish 450 bbl terminal inventory from
725 bbl capacity and planned truck trips from automatic shutdown records.

Live rehearsals require a clean exact source revision, recorded binary/input
hashes and a newly declared attempt set. Earlier sealed rounds and failures remain
unchanged. Offline protocol fixtures are not hosted execution evidence. Fresh
hosted results, CI and clean-Windows acceptance must be reported separately;
this implementation document is not a passing qualification or release approval.

The local implementation checks passed six focused CTest suites in each of the
normal and ASan builds. A subsequent self-review found that a known infeasible
model-validated run could unnecessarily enter reference recovery. The added guard
and counterexample passed the affected orchestrator suite in both configurations.
The 63 combined candidate/evaluation-auditor unit checks also passed. Initial
failed attempts remain retained; these local results precede the source freeze
and do not qualify the later hosted executable by themselves.
