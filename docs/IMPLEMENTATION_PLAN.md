# BREAKWATER: Editable 3D Windows Foundation

## Summary

Build a small Windows simulation editor using **C++20, Qt Quick/QML, and Qt Quick 3D**. Users can place equipment, connect routes, configure operating parameters, inject pump failures, and forecast the resulting network.

**Nemotron performs most of the planning:** it investigates the network, chooses interventions to explore, proposes candidates, and revises them using actual numerical results. C++ supplies fast simulation and independent validation.

Preserve the scientific corrections and ownership principles in [For Dev Agents.md](../For%20Dev%20Agents.md). This plan replaces its web-first architecture and the earlier fixed-network UI plan.

The milestone delivers a functional foundation. Presentation polish, realistic hydraulic physics, extensive ML training, and a rehearsed demo remain later work.

## Architecture and editor behavior

### Native application

- Use CMake, Ninja, MSVC x64, and dynamically linked Qt 6.12 with Quick, Quick Controls, Quick 3D, and Network.
- Keep the numerical library independent of Qt, Python, and the renderer.
- Run numerical jobs outside the UI thread.
- Use a separate Python 3.12 worker for differentiable PyTorch calculations and PhysicsNeMo. Communicate asynchronously through versioned newline-delimited JSON.
- Pin resolved dependencies and compatible Lean/Mathlib versions. Verify missing runtimes during setup.

### Editable 3D facility

Use built-in primitive geometry and object picking supported by [Qt Quick 3D](https://doc.qt.io/qt-6/qml-qtquick3d-model.html).

| Component | Visual representation | Simulation role |
|---|---|---|
| Tank | Cylinder with visible fill indicator | Stateful storage |
| Production source | Simple tower and equipment boxes | External inflow and optional curtailment |
| Treatment destination | Grouped boxes/cylinders | Shared discharge-capacity constraint |
| Trucking outlet | Primitive tanker truck | Costed external discharge |
| Pipe route | Cylinder segments | Directed flow connection |
| Pump | Small assembly attached to a route | Flow capacity, setpoint, and availability |

Implement these interactions:

- Drag equipment from a palette onto a ground plane.
- Move equipment with grid snapping; orbit, pan, and zoom the camera.
- Connect compatible equipment through visible connection handles.
- Drop a pump onto a tank-origin pipe route.
- Select equipment or routes to edit their numerical properties.
- Delete components, undo/redo edits, and save/load projects.
- Keep connected pipe geometry attached when equipment moves.

Supported connections are source → tank, tank → tank, tank → treatment, and tank → trucking outlet. Each production source feeds one tank; treatment and trucking destinations may receive multiple routes. Branching occurs through tanks.

Incomplete networks remain editable drafts. Running requires valid endpoints, capacities, initial inventories, flow limits, and source connections.

### Separate layout from physical configuration

Store a versioned project containing:

- **Network:** stable asset IDs, component types, connections, capacities, initial inventories, setpoints, availability, control bounds, and synthetic costs.
- **Layout:** positions, orientation, pipe routing points, and camera settings.
- **Control catalog:** stable control IDs with units, current values, bounds, and cost information.

Moving an object changes its layout. Changing connections or numerical parameters creates a new simulation revision and invalidates recommendations and approvals based on the previous revision.

Retain the handoff’s **capacity-limited, constant-flow model**. Position, elevation, and rendered pipe length do not yet determine flow. Tank fill indicators and flow displays consume engine results.

## Person A: engine, performance, science, and Lean

**Own:** numerical engine, scenario fixtures, Python science worker, Lean project, and subsystem tests/benchmarks.

### General network simulation

- Compile validated networks into indexed storage nodes, internal edges, and external inflow/discharge terms.
- Provide pure interfaces for simulation, validation, candidate evaluation, bounded search, and selected-pipe transport.
- Support custom networks using the same equations as the supplied BREAKWATER scenario.
- Use double precision and retain unrounded results. Default forecasts cover three hours with 0.025-hour display samples.
- Independently check storage bounds, route capacities, pump availability, shared destination throughput, control limits, and volume conservation.
- Reject invalid controls explicitly. Preserve negative or overflowing predictions as evidence rather than clipping them.
- Permit capacity equality and report the time a limit is reached.

Preserve the supplied scenario’s constants. Adopt healthy main/backup discharges of **44/16 m³/h**, with the temporary accumulation clearly identified.

### Efficient candidate evaluation

- Use contiguous candidate buffers, precompiled topology, reusable workspace, and allocation-free evaluation loops.
- Maintain an independently coded dense-incidence reference and a sparse edge-update implementation.
- Evaluate constant-flow candidates through their affine trajectories and endpoint bounds; generate sampled trajectories for retained candidates.
- Supply a generic batch evaluator and a bounded search helper. The helper accepts up to four model-selected controls and at most 10,000 combinations.
- Independently reference-validate returned recommendations and rank them by computed synthetic cost.
- Report finite-search results as best among evaluated candidates. Return an explicit no-feasible-plan result when appropriate.
- Use precise floating-point semantics in optimized builds. Defer custom SIMD, parallel search, and GPU kernels until profiling identifies a useful workload.

### Lean algorithm proofs

Deliver checked proofs that:

1. Sparse edge updates equal incidence-matrix updates.
2. Internal transfers conserve total volume.
3. Constant-rate repeated updates equal the affine closed-form trajectory.
4. Endpoint bounds imply bounds throughout the constant-rate forecast interval.

Prove these for valid finite graphs so they apply to supported user-built networks. Pin dependencies, prohibit `sorry`, and audit theorem assumptions.

Document the mapping to C++. These are algorithm proofs; compiled C++ memory safety and floating-point behavior receive separate implementation checks.

### Scientific foundation

- Implement authoritative one-dimensional finite-volume transport for a selected positive-flow pipe, including stable stepping and boundary-flux accounting.
- Implement a differentiable PyTorch rollout and finite-difference gradient checks.
- Execute an actual PhysicsNeMo model, residual calculation, bounded training smoke, checkpoint round-trip, and inference/reference comparison.
- Keep learned quality results separate from volume-feasibility decisions and explicitly experimental until evaluated.
- Use the documented [PhysicsNeMo Python/PyTorch installation workflow](https://docs.nvidia.com/physicsnemo/latest/getting-started/installation.html).

## Person B: editor, Nemotron, and integration

**Own:** desktop application, public contracts, serialization, root build configuration, dependency locks, integration tests, and delivery documentation.

### Application and project state

- Establish the shared buildable baseline and ownership instructions before the workstreams diverge.
- Publish the project schema, engine-facing types, scientific-worker protocol, and agent tool schemas.
- Implement the editor, property inspector, forecast timeline, candidate comparison, and tool-event panel.
- Store immutable incident snapshots and runs with IDs, network/snapshot hashes, units, method versions, and warnings.
- Cancel or disregard stale jobs after numerical edits. Layout-only changes preserve numerical results.
- Save projects atomically and allow draft projects to be reopened.
- Bind approval to a validated run and matching snapshot. Only an explicit user action applies controls to simulated operating settings.

### Nemotron-led planning

Use a configurable native HTTPS client for the [NVIDIA-hosted API](https://docs.api.nvidia.com/nim/reference/nvidia-nemotron-3-5-lightning-30b-a3b-infer).

Give Nemotron structured topology, available controls, constraints, costs, and concise run summaries. Its responsibilities are to:

- Interpret the operator’s goal.
- Identify threatened assets and relevant controls.
- Propose interventions for the actual edited network.
- Request simulations, comparisons, and targeted batch searches.
- Revise unsuccessful candidates.
- Recommend a validated plan with evidence.

Provide real tools for:

- Reading network state and the control catalog.
- Forecasting current operation.
- Simulating batches of candidate control overrides.
- Independently validating runs.
- Searching selected control ranges.
- Comparing runs.
- Requesting simulated operator approval.

Keep existing controls fixed unless explicitly overridden. The forecast horizon and hard constraints remain those of the planning request.

Limit an agent session to 12 tool calls, three refinement rounds, and two minutes. Preserve completed work on timeout. C++ enforces IDs, bounds, validation requirements, and approval transitions.

Custom-network planning is a bounded heuristic capability: it may return no validated plan and makes no global-optimality claim. Without model access, manual editing, simulation, and validation remain available.

## Verification and delivery

Deliver through four integration gates:

1. **Contracts and startup:** Desktop, native tests, project loading, and Python health checks work.
2. **Editor and engine:** A user-created network produces real forecasts and validation reports.
3. **Planning and proofs:** Nemotron operates on custom asset/control IDs; Lean proofs compile; scientific smoke checks execute.
4. **Foundation release:** Setup instructions, dependency pins, measured benchmarks, proof scope, and component status are recorded.

Required acceptance cases:

- Original baseline reaches Tank A capacity at approximately **0.76923 h**.
- Isolated bad reroute reaches Tank B capacity at approximately **2.66667 h**.
- Known mixed plan ends at **80/117 m³** and costs **12,780 synthetic units**.
- Dense, sparse, affine, and PyTorch calculations agree within documented tolerances.
- Custom networks pass conservation and shared-destination-capacity checks.
- Adding or reconnecting equipment changes the simulated network; moving equipment preserves numerical results.
- Save/load and undo/redo preserve IDs, connections, parameters, and layout.
- Invalid drafts, deleted assets, concurrent jobs, and stale approvals are handled correctly.
- Agent tests use real tools against multiple network shapes and renamed assets.
- Missing credentials, malformed calls, fabricated run IDs, and skipped validation produce explicit failures.
- Native checked builds pass MSVC AddressSanitizer.
- Benchmarks measure 100, 1,000, and 10,000 candidates, separating sparse-kernel gains from endpoint-evaluation gains.

**Defaults:** synthetic data, directed nonnegative flows, nominal three-hour volume feasibility, separate quality reporting, simulated approval, and no fixed event deadline. Full hydraulic modeling, freeform construction physics, and automatic topology redesign remain outside this foundation.
