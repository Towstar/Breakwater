# Native selected-pipe transport

The desktop calls Person A's `breakwater::transport` reference outside the UI thread.
It displays a synthetic scalar advection/dispersion/decay calculation, independently
of volume feasibility and simulated operating approval. It requires neither Python
nor hosted model access. PyTorch and PhysicsNeMo worker methods remain reserved.

## Use the viewer

1. Load an example or edit a structurally valid network, then **Run forecast**. An
   investigation run with the same captured network may also be selected.
2. Click a pipe in the 3D scene. Under Properties, choose **Inspect pipe transport**.
3. Select the captured volume run. Historical runs are labeled and remain evidence;
   runs from a different network are excluded.
4. On Parameters, enter explicit length, cross-section, dispersion, decay, cell count,
   uniform initial/inlet concentration and source. Output interval, CFL safety and
   boundary mode are independent numerical settings. Length never comes from layout.
5. Choose **Run native transport**. Inspect the outlet trace, sampled time-distance
   field, signed mass ledger, CFL, minimum concentration and residual diagnostics.
6. Use **Export JSON** for bounded native evidence. Save the project to retain route
   parameters. Importing a transport-result JSON into the viewer is not implemented.

The selected route uses the actual resolved captured controls. A source feed uses
production minus curtailment; an unavailable pump carries zero flow. The UI never
silently changes that flow for a boundary condition. Closed-no-flux requires zero
captured flow. A Dirichlet inlet can still exchange tracer by diffusion at zero flow.

Quality parameter edits invalidate quality results and participate in project undo.
They preserve otherwise eligible volume runs and approval, because the constant-flow
volume model does not use pipe geometry. Layout moves preserve both. Numerical edits,
reset, route deletion, a different source run, or changed snapshot/provenance retire
the affected quality result. Merely selecting or calculating a result changes no
operating controls. Infeasible volume evidence is not made feasible by this calculation.

Cancel retires publication immediately. One bounded native calculation can finish in
the background; a new calculation starts after it retires. Closing the viewer does
not grant approval or apply a plan.

## Evidence and limits

Exports include route/run/snapshot identity, current numerical revision, separate
network and parameter hashes, method/version, actual flow, uniform physical inputs,
and historical provenance. `run_id` and `volume_run_id` identify the same source run.
Manual forecasts use their immutable run UUID as their snapshot identity.

The preview retains at most 256 actual output frames and 128 actual cell centers,
including endpoints. Explicit `time_indices` and `cell_indices` map back to the full
native result; no interpolated field is presented as native output. Outlet values use
the actual final cell. Diagnostics come from the complete native calculation. Sparse
display samples can omit intermediate visual features; consult the full-run diagnostics.

Concentrations are cell averages in mg/L; positions are metres and times are hours.
All inventories and cumulative terms are mg. The signed native ledger is
`final - initial - inlet + outlet - source + decay = residual`. Negative inlet exchange
represents net loss through the left boundary. Values and small negative roundoff are
retained. JSON export is atomic and capped at 1 MiB; an export failure preserves an
existing destination. Native resource/arithmetic errors are reported without clipping
concentrations, increasing CFL, or relaxing the requested horizon.

This reference does not model tank chemistry, network-wide quality coupling, pressure
hydraulics or industrial quality qualification. A4's standalone learned smoke is a
fixed case, not a model for arbitrary selected routes. See
[SCIENCE_CONTRACTS.md](SCIENCE_CONTRACTS.md) for the future worker boundary and
[STATUS.md](STATUS.md) for executed checks and remaining integration work.
