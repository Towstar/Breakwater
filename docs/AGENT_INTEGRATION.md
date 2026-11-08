# Native Nemotron integration

This is Person B's additive implementation of `KICKOFF_PERSON_B.md`. The existing
engine types and project-v1 format are unchanged. Engine/science/proofs/scenarios
remain Person A's write ownership.

## Configuration and launch

Copy the tracked `.env.example` to `.env` in the checkout root and fill in
`NVIDIA_API_KEY` locally. `.env` is ignored by Git; `.env.example` contains no key.
The desktop application and protocol/investigation smoke executable read `.env`
automatically. The desktop reloads configuration before each investigation.

Installed users do not need source code, scripts or a shell: open **Nemotron
investigator → Set API key**, paste the key and choose **Save key**. The masked
entry is cleared when the dialog closes; the saved key is never populated into
QML. **API key settings** can replace or remove it between investigations. Saving
does not validate model access or make an API request. The next investigation uses
the saved setting unless a launch configuration takes precedence.

The app saves an atomic `.env` in Qt's per-user `AppConfigLocation` for organization
`Breakwater` / application `BREAKWATER` (normally
`%LOCALAPPDATA%\Breakwater\BREAKWATER\.env` on Windows). This app-managed file
contains only the key and stays outside the installation and source checkout.
It is a local plaintext settings file, not an encrypted credential vault. The
desktop and smoke executable share this application identity. Removing the saved
key does not remove a key supplied separately by a launch configuration.

Lookup checks the working directory and its nearest Git checkout root, then the
executable directory and its nearest checkout root. This supports launches from
the root, Visual Studio and nested build folders. For an installed application,
place `.env` beside the executable or launch it from the directory containing it.
The real `.env` is never bundled or installed by CMake. The key-free `.env.example`
is included beside the installed executable for portable/manual configuration.

Precedence, per setting: nonempty process variable → nonempty launch/checkout
`.env` value → saved per-user `.env` value → built-in default. The dialog reports
whether the saved key or a launch configuration is active. File
values remain in the native Nemotron configuration; they are not added to the
process environment or inherited by the scientific worker. The reader accepts
the three settings below, blank lines, `#` comments, optional `export`, and optional
single/double quotes. Inline comments follow whitespace (or a closing quote).
Values are literal: shell commands, variable interpolation, escape expansion and
multiline values are unsupported. Missing/unreadable/oversized files or malformed
quoted settings supply no file values; missing credentials remain explicit.

Keep the real key in local `.env`, per-user settings or the process environment. Do not put it in
project JSON, QML, source, logs, or chat.
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
| `search_interventions` | `snapshot_id`, `axes`, optional `result_limit` | Actual engine search diagnostics and independently validated stored finalists |
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

An authoritative passing validation report is required for recommendation or staging.
Individually simulated runs need `validate_run`; native search finalists carry their
independent reference validation. Unknown/foreign run IDs, failures and stale snapshots are rejected.
The final model message must be JSON with `selected_run_id` and `explanation` only:

```json
{"selected_run_id":"actual-validated-run-id","explanation":"A concise operational explanation and caveats."}
```

Use `null` for the selected ID when no validated plan was selected. This never
establishes global infeasibility. Costs, settings, margins and feasibility are copied
from the stored run/report, never accepted from generated numerical fields.

Person A's search at `4eea5a7` is now integrated. `search_interventions` accepts up to
four axes `{control_id, values: [numbers...]}`, at most 10,000 Cartesian combinations,
and 1–5 requested finalists. It calls the actual `search_candidates` implementation
at the fixed session horizon. Native dense-reference trajectories and passing reports
are stored with real application IDs. Screening counts never imply validation of every
survivor. No simulator, validator or optimizer is duplicated in the agent layer.

```json
{"snapshot_id":"actual-id","axes":[{"control_id":"route:bypass:flow","values":[0,10,20,25]}],"result_limit":5}
```

A no-plan result applies only to the evaluated inputs. Engine search finalists already
carry authoritative independent validation; individually simulated candidates still
require an explicit `validate_run` call before recommendation or staging.

## Sessions, snapshots and approval

An immutable session snapshot copies the physical network and receives a fresh UUID,
physical hash and fixed forecast configuration. Stored runs retain complete engine
results, resolved controls, solver version, creation time and independent reports.
All simulations and validation run through the existing `compile`, `simulate` and
`validate` functions on background workers. Qt-owned activity updates return through
future-watcher signals on the UI thread.

Sessions stop at 12 tool calls, three candidate-refinement/search rounds, or two minutes.
The client uses non-streaming asynchronous HTTPS, 30-second request timeouts, bounded
transient retries, a 2 MiB request/response limit and manual redirect rejection.
Cancellation ignores late results; physical edits and reset invalidate sessions and
approval using both hash and numerical revision. Moving equipment preserves them.
Starting a new session replaces the live gateway. The desktop foundation automatically
archives completed evidence and reopens it as historical, non-actionable data; see
[DESKTOP_FOUNDATION.md](DESKTOP_FOUNDATION.md).

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
