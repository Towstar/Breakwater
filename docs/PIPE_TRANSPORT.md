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
   parameters. The result JSON and project file are separate formats.
7. After restarting, choose **Pipe transport** in the sidebar and **Import JSON** to
   reopen an exported native result, even without a current forecast or selected pipe.
   Charts, the signed ledger, recorded parameters and provenance are available together.
   Choose **Current calculation** to return to the current project's inputs and result.

## Reopen historical native evidence

Imported files are always displayed as **historical, unverified evidence with no approval**,
including files whose IDs and hashes match the current project. Importing does not run
the solver, verify authorship, change the network or pipe inputs, populate the authoritative
run store, or restore a plan's approval. The file's feasibility and quality tags remain
recorded claims. Recorded parameter fields are read-only. Current calculation inputs
and jobs are unavailable in that view until **Current calculation** is selected.

File reading, parsing and validation run outside the UI thread. The importer accepts
local files up to 1 MiB with the existing native `result_version: 1` payload and supported
native method version. It checks the exact documented field sets, bounded identities,
parameter fingerprint, finite numeric types, array/ledger dimensions, ordered native
indices and endpoints, cell-center positions, horizon and native/display budgets.
Previews contain at most 256 time samples by 128 cell samples. Signed ledger entries
and roundoff-sized negative predictions are preserved. These checks establish structural
compatibility; they do not re-execute or authenticate the imported calculation.

Malformed, oversized or unsupported files fail visibly while retaining the previous
displayed evidence. Cancellation discards a pending import's publication, and a second
job cannot overlap it. Project edits/reset can invalidate the hidden current calculation
while the imported historical view remains clearly detached. Re-export preserves the
original supported payload; reopening that copy is still historical and unverified.
This increment does not automatically retain native quality results in SQLite: save
the result JSON explicitly. Automatic investigation-session history remains separate.

For a reproducible restart/render check, run the desktop executable with:

```powershell
breakwater.exe --import-pipe-evidence 'C:\path\native-result.json'
# Optional: render the reopened evidence and exit.
breakwater.exe --import-pipe-evidence 'C:\path\native-result.json' --screenshot 'C:\path\reopened.png'
```

The screenshot command waits for a successful import and a rendered frame. An import
failure does not count as a passing smoke check.
`ctest --test-dir out/visual-studio -C RelWithDebInfo -R desktop.pipe_evidence_smoke --output-on-failure`
first runs its native-export fixture, then reopens that actual result in the desktop.

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
