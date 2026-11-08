# EOG evidence history

The EOG history repository is separate from the simulation workbench's session
database and archive format. Its default file is `eog-history.sqlite` in Qt's
application-local data directory. It never stores or restores control authority.

## Thread, database and lifecycle

`EogHistoryRepository` queues all SQLite, capture I/O, compression, input validation
and import/export work to a dedicated worker thread. Database connection creation
and destruction also happen there. Public failures identify operation, session,
code and a bounded message; callers must display these while allowing forecasting
to continue independently.

A nonblocking `QLockFile` protects each database for the repository's lifetime.
A second preview/application cannot mark a live owner's sessions interrupted.
After the owner closes, reopening the database marks unfinished `running` sessions
as `interrupted`. Completed numerical evidence remains unchanged.

The repository checks SQLite application ID `0x454f4731` before creating its
schema. An unrelated database with user tables and application ID zero is refused;
passing the native workbench database path does not migrate or claim it.
Foreign keys, WAL and full synchronous commits are enabled for EOG databases.

Captures and all three compressed inputs are retained in the same transaction as
the new session. Capture rows are deduplicated by immutable `capture_id`. Analysis
appends are transactional; a request ID cannot replace existing request, response
or metadata. Identical retransmission is idempotent. Completed, failed or cancelled
numerical evidence cannot receive a new analysis.

## Public API

- `beginSession(id, objective, Capture lease)` creates a new UUID session and retains
  its complete immutable input evidence. Objective text is bounded at 4,000 characters.
- `appendAnalysis(id, request, response, metadata)` retains a checked request/result.
- `appendAgentEvent(id,event)` retains an ordered sanitized model/tool trace in an
  unsealed current local session.
- `finishSession(id, status)` seals numerical evidence with `completed`, `failed`
  or `cancelled` status.
- `recordOperatorReview(id, analysisRequestId, note)` records explicit review of a
  successful, available, feasible, independently validated exact plan.
- `acknowledgeEvent(id, analysisRequestId, eventId, note)` records acknowledgement
  of an event present in the named successful detection result. It does not clear it.
- `retireSession(id)` removes current-process operator-review eligibility.
- `listSessions(offset=0)` returns 100 rows in deterministic update-time/session-ID
  order. `sessionsListed(rows,nextOffset)` returns `-1` after the last page. Offsets
  must be between zero and 1,000,000; exceeding a bound is an explicit failure.
- `loadSession`, `exportSession` and `importSession` return or transfer historical
  evidence asynchronously. Loading a session retires its live review eligibility.

`analysisRequestId` is the wire request's `request_id`; it is deliberately distinct
from the provider result's `analysis_id`. Operator records refer to the exact stored
request/response pair. Reviews and acknowledgements are append-only and idempotent
for `(kind, request_id, event_id)` with the same note; existing notes cannot change.

Only a newly begun, unretired session in the current repository process can receive
operator records. A plan review additionally requires completed investigation
status; event acknowledgement may also occur while its investigation is running.
Numerical evidence remains sealed while later operator records may be appended.
Imported, reopened and retired evidence cannot regain review eligibility. The
desktop must additionally check its current capture and explicit operator action.
No review performs a dispatch, changes sandbox controls or authorizes hardware.

## Validation and metadata

Request/response shape, identities, cutoff and exact plan/validation binding use the
published EOG contracts. All three retained input hashes/sizes are checked against
the capture manifest and parsed through the strict dataset reader in private
temporary storage. First/last/cutoff rules are checked on actual CSV rows. Nonmissing
source-evidence references must match a real captured asset/metric/time/value.
This checks provenance; it does not execute or qualify numerical methods.

Absolute runtime `capture_directory` paths are removed from archived requests.
A fixed local placeholder is inserted only while performing native contract
validation, never to create a runnable request or restore approval.

Metadata accepts only `model_identity`, `http_status`, `duration_ms` and
`retry_count`. Unknown fields, including credentials, headers and raw reasoning,
are rejected rather than retained. Process-client status/method/exit fields should
remain in process diagnostics; callers construct the small history metadata object.

## Archive v1 and compression

The format is a standalone JSON object with `eog_session_version: 1`, session
metadata, capture manifest, exactly three `captured_inputs`, complete analyses,
operator records and `agent_events`. It always declares `historical: true` and `actionable: false`.
Session/record timestamps use canonical UTC ISO dates with milliseconds so stored
pagination ordering is consistent across exported and imported records.
Unknown fields, actionable claims and embedded runtime capture paths are rejected.
Running imported investigations are marked interrupted. Import cannot overwrite
different evidence under an existing session ID.

Each captured input carries `codec: mszip-v1`, its raw byte length/SHA-256, compressed
SHA-256 and canonical base64 compressed bytes. Windows' built-in Compression API
is linked through the OS SDK's Cabinet library; no downloaded codec is required.
The decompressor queries the stream's actual required output length and compares
it with the validated declared limit **before allocating**. The second operation
uses an exact fixed buffer and never grows/retries it. Both compressed and raw
hashes are verified. Other platforms report this codec unsupported explicitly.

Bounds are 256 MiB per complete archive, 128 MiB for CSV, 4 MiB for each input JSON,
12 bounded analyses and 128 operator records. Analysis request/response bounds
remain 1/4 MiB. No evidence is silently truncated to fit an export.

Export requires a new local `.eog-session.json` destination. It prepares a durable
temporary file with `QSaveFile`, then publishes by a same-directory Windows rename
that refuses to replace any existing destination. Input filename collisions and
runtime capture directories are refused. Import checks the whole archive before
committing capture, analyses and operator records together.

## Verification boundary

The history tests use small explicitly labeled protocol fixtures. They exercise
actual SQLite restart recovery/locking, capture and analysis rollback, deduplication,
immutable completed evidence, full input/trajectory round-trip, historical review
rejection, tampered/oversized/decompression-length inputs, strict metadata, safe
exports, paged listing and UI timer progress. These fixtures do not establish an
EOG detector, optimizer, independent numerical execution or hosted model rehearsal.

Executed results and tested revisions belong in the increment's verification
report after the checks actually run; this document alone is not passing evidence.

In an initialized MSVC/Qt environment, build and execute the actual SQLite suite:

```powershell
cmake --build out/eog-normal --target breakwater_eog_history_tests --parallel 2
ctest --test-dir out/eog-normal -R '^desktop.eog_history$' --output-on-failure
cmake --build out/eog-asan --target breakwater_eog_history_tests --parallel 2
ctest --test-dir out/eog-asan -R '^desktop.eog_history$' --output-on-failure
```

The Qt results file is `desktop/eog_history-results.txt` under each configured
build directory. Retain failed attempts and report the actual test exit code.

## Agent trace

Each persisted trace wrapper has repository-assigned `sequence` (starting at one),
`recorded_at` and an `event` object. Sequence/order is immutable. Limits are 128
events, 64 KiB per wrapped event and 4 MiB per complete trace; overflow fails visibly.
The event discriminator is `type`:

- `model_request`: `model_identity`, `duration_ms`, `retry_count`, with optional
  `http_status` when an actual HTTP status exists. Connection failures do not invent
  an HTTP status. The same strict metadata whitelist applies.
- `tool_call`: `call_id`, `name`, and an `arguments` object.
- `tool_result`: matching `call_id`, `name`, Boolean `ok`, optional
  `analysis_request_id`, bounded `summary` and lowercase `output_sha256`.
- `final`: bounded `summary`, nullable `selected_plan_id` / `validation_id`, and
  `event_ids`. A supplied validation reference must match a successful available
  independent validation of the selected exact plan.
- `failure` / `cancel`: a bounded `code` only.

Tool results must match an earlier unfinished model-issued call. Numerical and
event references must exist in the same retained session. Import verifies the
same structure, sequence and binding. Recursive credential/header/reasoning/runtime
capture-directory fields are rejected; current private capture paths are not
retained in event text. Callers sanitize transport content before appending; these
events contain no raw model reasoning or authorization headers. Trace persistence
does not establish hosted execution or numerical qualification by itself.
