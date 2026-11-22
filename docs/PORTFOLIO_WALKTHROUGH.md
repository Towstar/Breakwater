# Offline portfolio walkthrough

Use Sandbox for editable networks and Operations for recorded/advisory evidence.
All examples are synthetic. Layout is schematic and supplies no hydraulic model.

## Sandbox

1. Open **Project → Pump failure example**, then **Run forecast**.
2. In Forecast, choose Tank A. Its baseline prediction reaches 236 m³ against a
   120 m³ capacity. The overflowing segment remains visible.
3. Open **Investigate**, then **Local search**. This runs the native finite
   control grid, stores numerical runs, and independently validates its selected
   candidate. It makes zero hosted model requests and applies no controls.
4. Use Compare to inspect compatible baseline/candidate costs, controls, margins
   and trajectories. This is best found within the displayed bounded grid.
   Drag the top of the results dock upward for a larger chart and evidence view.
5. Open **Review staged plan**. Inspect current/proposed controls, units, run ID,
   validation reference and margins. Only the explicit apply action changes
   simulated controls. Numerical/topology changes retire eligibility.
6. Ctrl-click or marquee-select assets, move the group, duplicate with Ctrl+D,
   use the scene Edit menu to align/distribute, and undo. Layout moves preserve numerical
   identity; duplication changes topology and retires approval.
7. Filter Network by equipment name, ID or type. Equipment hidden by the filter
   remains in the scene, and an existing selection stays selected. Shift-click
   ranges use the visible list. The inspector shows editable simulation inputs, their units,
   displayed forecast inventory and route/pump state. Clearing selection opens
   the network overview and draft checks.
8. Toggle **Layout grid** in the scene toolbar or View menu. It is a display
   guide aligned with the editor's 20-unit steps, with coarser guides on large
   sites. The orientation inset follows the camera's X/Y/Z axes. Paving, road
   markings and equipment pads are schematic decoration; moving them does not
   define hydraulics, physical clearances or truck travel times.
9. Zoom in to inspect the tank ladder landings and access covers, pump guards,
   tanker cab/wheel details and paired pipe joints with saddle supports. Decorative
   hardware does not intercept equipment or pipe selection. Large overviews keep
   simplified assemblies; the details do not introduce new operating controls.
10. Choose **Connections** in Network to filter/select routes. The inspector and
    scene card show endpoints, configured rate and separate pump availability.
    Focus frames both endpoints. The arrow denotes route direction, not measured
    delivery. **Top** and **Fit** use the available viewport; wheel/middle-drag
    adjust plan zoom/pan. Ctrl+Shift+F temporarily focuses the scene and Escape
    restores panels without changing saved panel preferences or numerical state.

## Local observed advisory workflow (v0.3 development)

1. Choose **Project > Local monitoring example**. Start the bundled local
   synthetic publisher with a preset and seed; record both with any result.
2. Pause and **Capture state**. Inspect coverage, sample references, calibration,
   uncertainty, source/epoch and expiry. Unsupported/stale coverage is unavailable
   and cannot silently fall back to design-state forecasting.
3. Open **Forecast / advisory review** and run **Local search**. This executes
   native numerical tools on the frozen observed state without a model request.
4. Compare the retained baseline/candidate and inspect **Review staged plan**.
   Acknowledging advisory review records that exact evidence. It does not change
   Sandbox controls, dispatch commands, or clear a detected incident.
5. Restart/replace the source and check that current-source eligibility retires.
   Completed history stays inspectable; imported/replayed evidence never restores
   operating authority. See [local monitoring](LOCAL_MONITORING.md) and the
   [current candidate acceptance packet](CANDIDATE_READINESS.md).

## Operations

1. Switch to Operations, choose **Examples → Synthetic operations demo**.
2. Select a system, facility, equipment and channel. The plot contains recorded
   samples through the selected cutoff, in original units, with gaps preserved.
3. Open **Investigate / history**, choose **Search plans**, then **Validate exact
   plan**. Inspect Plans and its compatible no-action baseline. Independent
   validation is a separate retained operation; search alone is insufficient.
4. Export the completed session from History, restart and import it. Imported
   evidence can be inspected but never restores actionable review.
   Use **Read full evidence** for stored validation, costs, repair policy/timing,
   actions and margins. The complete plain-text reader scrolls at small sizes;
   optional model narrative remains separately labeled and unverified.
5. Choose **Examples → Recorded Nemotron investigation** when a qualified archive
   is bundled. Its tool order and numerical evidence came from the real hosted
   model; playback is explicitly historical and performs no new model request.
6. Choose **Renamed / changed topology** and run a fresh local search. Previous
   results do not silently become advice for the new capture.

The basic portable bundle includes an isolated Python runtime for Operations.
It does not include Torch/PhysicsNeMo. For source builds, run
`scripts/Setup-OptionalScience.ps1`, then `scripts/Start-Portfolio.ps1 -Science`.
The experimental pipe tracer fit estimates a bounded decay parameter from entered
synthetic samples. It stages a result; applying it is a separate explicit action
that retires old numerical approval. It is not a calibrated field chemistry or
network hydraulic model.

## Suggested 100-second portfolio demonstration

- 0–20 s: introduce the editable Sandbox and its failed-pump overflow forecast.
- 20–45 s: show the real local search, comparison and explicit review boundary.
- 45–70 s: switch to Operations and show source samples, events and plan costs.
- 70–90 s: open the genuine recorded Nemotron trace and exact validation.
- 90–100 s: show saved history and explain Lucas/Ethan ownership and limitations.

Use the actual retained cost for the selected capture. Do not reuse hackathon
cost claims on the new synthetic examples, claim universal optimality, or imply
that a recording is a fresh hosted investigation.
