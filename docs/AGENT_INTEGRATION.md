# Native Nemotron integration

This is Person B's additive implementation of `KICKOFF_PERSON_B.md`. The existing
engine types and project-v1 format are unchanged. Engine/science/proofs/scenarios
remain Person A's write ownership.

## Configuration and launch

Configure `NVIDIA_API_KEY` in the launching process environment using your local
credential workflow. Do not put it in project JSON, QML, source, logs, or chat.
`NVIDIA_BASE_URL` accepts an HTTPS `/v1` base or a full `/chat/completions` URL.
`NEMOTRON_MODEL` defaults to `nvidia/nemotron-3.5-lightning-30b-a3b`. The default
endpoint is `https://integrate.api.nvidia.com/v1/chat/completions`.

Build the `desktop` preset as described in README. Open **Nemotron investigator**
in the workbench, describe the operator objective, and press **Investigate**.
The model chooses which registered tools to call. **Cancel** stops further work;
completed numerical runs remain visible. Missing credentials produce an explicit
unavailable error without affecting manual forecasts.

The activity list shows actual requests/results, run IDs and validation references.
The numerical fields in the recommendation come from stored native runs. Model text
supplies the explanation only. **Apply staged simulated plan** becomes available
only after a matching validated run is staged and the investigation has finished.
That explicit human action changes simulated setpoints; no hardware commands are sent.

## Tool protocol v1

Every definition uses an OpenAI-compatible function tool with JSON Schema, required
properties and `additionalProperties: false`. Argument types and bounds are checked
again in C++. Responses contain `ok`, with data on success or
`error: {code, message}` on failure. A failed result is returned to the model using
the exact tool-call ID so it can revise its approach.

| Tool | Arguments | Result |
|---|---|---|
| `get_network_state` | `{}` | Snapshot, physical hash, topology, draft issues, fixed horizon, capabilities |
| `get_control_catalog` | `{}` | Stable IDs, current values, units, bounds, availability, synthetic costs |
| `forecast_baseline` | `snapshot_id` | Actual stored baseline run |
| `simulate_candidates` | `snapshot_id`, `candidates` | Actual stored candidate runs |
| `validate_run` | `snapshot_id`, `run_id` | Independent authoritative validation, validation ID |
| `compare_runs` | `snapshot_id`, `run_ids` | Comparable stored summaries from the same snapshot/horizon |
| `request_approval` | `snapshot_id`, `run_id` | Human review request; `applied: false` |

Example candidate request (obtain the actual snapshot/control IDs from inspection):

```json
{
  "snapshot_id": "actual-incident-snapshot-id",
  "candidates": [
    {
      "candidate_id": "reduced-bypass",
      "controls": [{"control_id": "route:bypass:flow", "value": 20}]
    }
  ]
}
```

Unspecified controls retain their original incident values, not values from the
previous candidate. The planning request fixes the horizon; tools cannot shorten it.
Candidate requests allow 1–32 candidates, the immutable run store holds at most 128
runs, and candidate IDs must be unique within a batch. Full trajectories remain
available through the application's `SimulationGateway::runDetails`; the model sees
compact initial/middle/final samples, limits, cost and validation evidence.

Only an explicitly successful `validate_run` makes a run eligible for recommendation
or staging. Unknown/foreign run IDs, failed validation and stale snapshots are rejected.
The final model message must be JSON with `selected_run_id` and `explanation` only:

```json
{"selected_run_id":"actual-validated-run-id","explanation":"A concise operational explanation and caveats."}
```

Use `null` for the selected ID when no validated plan was selected. This never
establishes global infeasibility. Costs, settings, margins and feasibility are copied
from the stored run/report, never accepted from generated numerical fields.

There is **no published search tool** in this increment. Person A's future native
search implementation is the dependency for adding one. The model can propose and
evaluate bounded candidate batches now. No simulator, validator or optimizer is
duplicated in the agent layer.

## Sessions, snapshots and approval

An immutable session snapshot copies the physical network and receives a fresh UUID,
physical hash and fixed forecast configuration. Stored runs retain complete engine
results, resolved controls, solver version, creation time and independent reports.
All simulations and validation run through the existing `compile`, `simulate` and
`validate` functions on background workers. Qt-owned activity updates return through
future-watcher signals on the UI thread.

Sessions stop at 12 tool calls, three candidate-refinement batches, or two minutes.
The client uses non-streaming asynchronous HTTPS, 30-second request timeouts, bounded
transient retries, a 2 MiB request/response limit and manual redirect rejection.
Cancellation ignores late results; physical edits and reset invalidate sessions and
approval using both hash and numerical revision. Moving equipment preserves them.
Starting a new session replaces the previous in-memory session history; persistence
across application restarts is not implemented.

Activity includes session ID, monotonic sequence, UTC timestamp, elapsed time, tool
name, arguments, status/duration, actual results and available run/validation IDs.
No raw HTTP bodies, credentials or model reasoning are logged. Structured reasoning
fields are discarded; unparsed `<think>` content is rejected.

## Verification commands

```powershell
.\scripts\build.ps1 -Preset desktop
.\out\desktop\desktop\breakwater_nemotron_smoke.exe
.\out\desktop\desktop\breakwater_nemotron_smoke.exe --investigate
```

Qt's `bin` directory must be on PATH for direct executable launches. The first smoke
requires an actual model-issued `get_network_state` call, executes it against the
real gateway, sends the result with its matching ID, verifies response model identity
and observes continuation. `--investigate` runs the production adaptive loop against
the canonical incident and emits its structured execution events. Neither command
applies controls. Without credentials the executable exits 2 and makes no request.

Automated tests use controlled fake HTTP/model responses with real numerical tools.
They prove protocol and orchestration behavior, not live hosted-model access. See
STATUS.md for commands actually run and current limits.

Vendor references used for the transport:
[hosted API](https://docs.api.nvidia.com/nim/reference/nvidia-nemotron-3-5-lightning-30b-a3b-infer),
[model tool-calling guide](https://docs.nvidia.com/nim/large-language-models/2.0.10/get-started/advanced/get-started-nemotron-3.5-lightning.html),
[NIM tool-message format](https://docs.nvidia.com/nemo/guardrails/v0.24.1/guardrails-python-sdk/nemoguardrails/guardrails/model_engine).
Self-hosted parser flags are not sent to the hosted API. Hosted behavior, model access
and non-streaming reasoning fields remain live-smoke checks until credentials exist.
