# Base contracts

Person B owns this boundary. The native types are in `include/breakwater/types.hpp`.
Engine functions are published in `engine/include/breakwater/engine.hpp`.

## Project v1

`version: 1`, `synthetic: true`, `network: { assets: [], routes: [] }`, and
`layout: { positions: { asset_id: { x, z } }, camera: { yaw, pitch, distance } }`.

- Asset IDs and route IDs are globally unique and persist through save/load.
- Tank fields: `capacity_m3`, `initial_m3`.
- Source fields: `inflow_m3h`, `curtailment_m3h`, `curtailment_max_m3h`, `cost_per_m3`.
- Treatment/trucking fields: `capacity_m3h`, `cost_per_m3`.
- Routes: `source_id`, `target_id`, `capacity_m3h`, `setpoint_m3h`, `has_pump`,
  `available`, and `cost_per_m3`.
- Source-route flow is source inflow minus curtailment; its stored setpoint is not
  an independent control. A source feeds exactly one tank.
- Layout coordinates are display units, separate from the physical model.
- Drafts may have incomplete topology; execution calls native structural validation.

The native `Asset.capacity_m3` member is kind-dependent: storage m3 for tanks and
throughput m3/h for destinations. Serialization uses the explicit keys above.

## Controls and runs

Control IDs are `route:<id>:flow` and `source:<id>:curtail`, all in m3/h. Only catalog
controls may be overridden. Overrides never mutate the original network.

Times are hours relative to the incident snapshot; sample zero is the incident state.
Trajectories are tank-major. Capacity equality is allowed. A limit-reached event and
an actual bound violation are separate concepts. Validation covers the requested horizon.
Quality is **not evaluated** in this base.

Desktop runs have a UUID and are bound to a numerical revision. A topology/parameter
edit discards the displayed run and approval; moving equipment preserves both. The
manual forecast keeps one displayed run; the agent keeps up to 128 immutable runs
per session and can compare their numerical summaries without changing the preview.

## Scientific worker protocol v1

One JSON object per line on stdin/stdout, maximum 1 MiB. Example request:

```json
{"protocol_version":1,"request_id":"health-1","method":"health","payload":{}}
```

Responses echo the protocol/request ID and contain `status`, plus `result` or `error`.
Only `health` is implemented. Package detection is not an executed ML integration.
The desktop-to-worker process adapter and scientific model methods remain planned.

## Native agent tools v1

The additive application-side tools, immutable run-store envelope and final
recommendation are documented with examples in
[AGENT_INTEGRATION.md](../docs/AGENT_INTEGRATION.md). Existing native engine types,
control-ID conventions, project-v1 serialization and worker protocol are unchanged.
Only the human-facing approval controller applies a staged simulated intervention.
