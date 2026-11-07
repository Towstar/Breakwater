# B3: selected-pipe and scientific boundaries

This additive boundary connects the desktop to the native selected-pipe transport and
defines proposed future worker methods. Existing project-v1 files, health requests,
volume simulations and approval rules remain readable and authoritative.

The Python worker currently implements **health only**. Standalone Person A code now
contains a CPU float64 storage rollout and an actually executed fixed-case PhysicsNeMo
smoke; those private fixtures are not the application's wire format. Neither installing
packages nor publishing these schemas exposes a worker method. Person A must review
the units/array semantics and implement the agreed mapping before capability negotiation
can advertise scientific execution.

The B4 native selected-pipe viewer calls `breakwater::transport` directly, outside the UI
thread. It does not require Python. Native scalar transport is separate from tank-volume
feasibility and does not establish industrial quality, pressure hydraulics, or network
chemistry. A4's PhysicsNeMo model is a fixed-case experimental smoke, **not** an arbitrary
route predictor; `pipe_transport` v1 does not claim general PhysicsNeMo inference.

## Additive project persistence

An optional project-v1 top-level field carries explicit per-route settings:

```json
{
  "pipe_transport": {
    "version": 1,
    "routes": {
      "west-transfer": {
        "length_m": 100,
        "area_m2": 0.1,
        "dispersion_m2_h": 1,
        "decay_per_h": 0.1,
        "cells": 64,
        "initial_mg_l": 0,
        "inlet_mg_l": 1,
        "source_mg_l_h": 0,
        "output_step_h": 0.025,
        "cfl_safety": 0.9,
        "boundary": "dirichlet_inlet"
      }
    }
  }
}
```

The ordinary `version`, `network` and `layout` project fields still apply; the fragment
above is not a complete project. Omit `pipe_transport` when no explicit route profiles
exist, preserving the previous serialization of those projects. An absent profile uses
the displayed `PipeParameters` defaults. A present entry contains all eleven documented
fields, rejects unknown/missing keys and names a route in that project. Unknown extension
versions fail visibly. Route deletion removes its profile; undo/redo restores settings
with the rest of the project. The existing 2 MiB project-file limit still applies.

Physical length and cross-section do not come from drawn distance, elevation or layout.
Moving an asset does not change these settings or invalidate a captured physical result.
Editing quality parameters changes the quality parameter fingerprint and invalidates
quality results. It does not change `physics_hash`, tank-volume equations, volume-run
identity or otherwise eligible volume approval. Topology/operating/numerical changes
invalidate results through the existing volume revision/hash and run identity. Reset
invalidates prior jobs even if it restores the same hash.

`parse_pipe_parameters`, `pipe_parameters_json` and `pipe_parameters_hash` share strict
finite/sign/cell/CFL validation. Cells are integers in the native range 2..4096. Output
step is positive and CFL safety is in (0,1]. Horizon-dependent frame/value budgets,
derived arithmetic, closed-boundary flow compatibility and stepping remain validated
by the native solver, with explicit failure rather than clipping or silent fallback.

The parameter fingerprint is lower-case SHA256 of Qt's compact JSON serialization of
`{"parameter_version":1,"parameters":<complete pipe_parameters_json object>}`. It is a
desktop-owned opaque identity: workers echo the request value, and the desktop checks
against its captured settings. A generic Python JSON reserialization is not the hash
algorithm. This hash identifies settings; it is not a signature or proof of execution.

## Identity and actual captured flow

Every scientific result binds to:

| Field | Meaning |
|---|---|
| `snapshot_id` | Immutable captured network/simulation identity |
| `volume_run_id` | Actual native forecast/candidate run used by this calculation |
| `numerical_revision` | Captured application revision, an exactly represented integer |
| `physical_hash` | Native volume-network hash, separate from drawn layout |
| `route_id` | Required for selected-pipe requests; exact captured route ID |
| `parameter_hash` | Required for selected-pipe requests; captured quality settings |

Snapshot/run/route identifiers are opaque nonempty strings up to 256 characters.
Physical and parameter hashes are 64 lower-case hex characters. Control IDs allow
512 characters; do not split an ID on colons. Match the returned catalog explicitly.

The native adapter resolves overrides against the captured network using `compile`.
It obtains the selected route's actual flow from the resolved native state, including
pump availability. A source feed is production minus source curtailment; a tank-origin
route uses its effective available flow. It never uses a UI slider from a different
run, guessed nominal capacity, or rendered pipe length. The current volume model is
constant-flow, so v1 carries one `flow_m3h` and holds it over the captured run horizon.
Closed-no-flux transport requires that captured flow to be zero; it cannot silently
discard a nonzero captured flow. A Dirichlet inlet can exchange tracer by diffusion
even at zero flow; zero flow does not implicitly mean a closed boundary.

Completions attach only when route, source run, snapshot/revision/hash, parameter hash
and job generation still match. A cancelled, deleted-route or stale completion cannot
attach to a new selection. Cancellation retires UI authority; the bounded native call
may finish in its worker thread. Reading/changing a selected pipe or result never applies
operating controls. Historical evidence cannot restore an actionable volume approval.

## Units, array order and native ledger

| Quantity | Unit/order |
|---|---|
| Pipe length, cell centers | m; `x_m[j]` is the center of native cell `j` |
| Cross-section | m² |
| Flow | m³/h |
| Dispersion, decay | m²/h, 1/h |
| Time | h relative to snapshot time zero; includes exact requested horizon |
| Concentration, uniform initial/inlet | mg/L; pipe fields are **cell averages** |
| Uniform source | mg/L/h |
| Pipe field | `concentration_mg_l[time_index][cell_index]` |
| Storage rollout | `volumes_m3[tank_index][time_index]`, m³ |
| Cost | synthetic units over the complete fixed horizon |
| Terminal-volume Jacobian | `[tank_index][control_index]`, h for m³ per m³/h |
| Cost gradient | synthetic units per m³/h, aligned with `control_ids` |

Storage results retain the captured tank ordering and stable IDs. The worker translates
the project-v1 network explicitly: native `Asset.capacity_m3` means m³ for tanks but
m³/h for destinations, whereas project JSON uses `capacity_m3h` for destinations.
Omitted controls retain captured values. The scientific mapper cannot rescue invalid
drafts, relax bounds, change availability, shorten the horizon or create a native run.
Derivative scope is the continuous float64 rollout at admissible interior controls;
there is no gradient claim through bounds rejection, discrete search or approval.

Native transport converts m³ to litres using 1000 L/m³. Inventory is the sum of each
cell-average concentration multiplied by its cell volume in litres. Preserve the
native signed cumulative ledger and its residual:

```text
residual_mg = inventory_mg - initial_inventory_mg
              - cumulative_inlet_mg + cumulative_outlet_mg
              - cumulative_source_mg + cumulative_decay_mg
```

Inlet is signed net exchange into the left boundary and can be negative. Diagnostics
and predictions remain unclipped, including roundoff-sized negative concentrations.
Full native diagnostics cover every internal substep and full field, not just preview
points. A scientific result never supplies an authoritative volume `feasible` verdict.

## Versioned worker extension and bounds

One UTF-8 JSON object followed by a newline travels in each direction. Protocol version
1 and exact `request_id` matching remain mandatory. The serialized line, including its
newline, must fit **1 MiB**. Normal stdout contains protocol only; bounded stderr contains
diagnostics. No key, authorization header, environment dump or raw model reasoning belongs
in either stream or an artifact. Error responses use `status: error` and a bounded
`{code,message}` object; malformed framing can have null request identity. A well-formed
request error must retain the exact request ID.

Reserved methods in [the schemas](../contracts/science/README.md):

| Method | Scope and implementation status |
|---|---|
| `health` | Existing v1 exchange, unchanged; package discovery is not science execution |
| `capabilities` | Proposed versioned method availability/limits; not implemented by current worker |
| `storage_rollout` | Proposed CPU float64 storage/derivatives mapping to A3; not wired |
| `pipe_transport` | Proposed selected-pipe result envelope; only native-FVM provenance in v1; not a Python method yet |

The current worker returns `not_implemented` for all three reserved methods, rather
than advertising a passing stub. The future capability record separates `implemented`
from `execution_verified`; a detected package cannot set either scientific flag.
Actual execution records method/implementation versions, dtype, device, identity and
warnings. Unsupported runtime, parameters, outputs and work budgets fail explicitly.

Native transport supports at most 4096 cells, 10001 frames, four million field values,
one million substeps and 50 million cell updates. These native limits do **not** fit
automatically into a worker line. Storage likewise permits at most 128 assets, 256 routes,
10001 samples and 384 controls; a full derivative matrix can exceed the wire budget.

Wire display previews use at most **256 times × 128 cells** for pipes; storage previews
retain all captured tanks (at most 128) and at most 256 times. Keep endpoints and explicit
original `time_indices`/`cell_indices`; do not pretend a decimated field is full resolution.
Each displayed ledger/outlet sample aligns with `time_h`. `outlet_mg_l` comes from the
actual final native cell, not an arbitrary last selected preview cell. Preserve full-run
diagnostic extrema and residual maxima separately. Validate dimensions, monotonic times,
finite values, matching IDs and exact horizon after parsing. JSON Schema cannot express
all cross-array, native-network or provenance checks.

The receiver also validates actual serialized byte length. The preview maxima are not
a promise that every float representation or derivative matrix fits: reduce samples
within requested limits or return `output_too_large`; do not silently omit requested
derivatives, truncate JSON or drop conservation diagnostics. The current native desktop
can keep the complete result in memory and expose a bounded preview without a wire copy.

## Optional full-artifact boundary

Artifact references are reserved and must be rejected until a future implementation
explicitly advertises `artifacts_supported: true`. The current worker and health adapter
do not load artifacts. A future reference contains a safe relative `.json` path, exact
byte size (at most 64 MiB), SHA256 and `artifact_schema: breakwater.science.field.v1`.

The application supplies a dedicated output directory when launching the worker; requests
and results cannot choose an arbitrary host directory. Accept only the restricted path
grammar in the schema. Reject absolute/drive/UNC paths, backslashes, empty/dot/parent
segments, links/reparse points, alternate streams and escape after canonical resolution.
Read through an opened bounded file, verify its final location, exact size, schema and
streamed hash before parsing/using it. Enforce the native shape/value limits as well as
the artifact byte limit. No remote URL, pickle, executable object or checkpoint deserializer
is allowed. A full field that exceeds the artifact budget fails explicitly.

Artifact identity and staleness checks are identical to inline results. An artifact's
hash detects byte changes; it neither grants authority nor qualifies learned predictions.
