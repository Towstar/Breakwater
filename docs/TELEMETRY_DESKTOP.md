# G2 desktop sensors, local input and replay

Person B's implementation uses the accepted G1 interfaces at `f12a827`, merged
through PR #31 (`2bd7bb9`). All monitoring remains synthetic/local. This increment
does not implement a numerical plant, observation mapper, detector or a forecast
from observations. Native forecasts and operating controls remain separate.

## Editor

Open **Sensors**. Add a measurement type and a compatible asset/route attachment;
the scene shows a selectable sensor marker. Configure source/channel, units,
sampling, maximum age, uncertainty and primary/redundant group. Tank level needs
explicit monotone level-volume knots. Concentration needs configured physical pipe
parameters and an explicit cell/interval or unsupported-point support. Pressure,
temperature and custom channels remain display/history only.

Select a marker or sensor row to edit it. Marker coordinates are presentation
only; moving a marker or editing its label preserves semantic identity. Attachment,
calibration, policy and numerical changes retire current input. Deleting equipment
with attached sensors requires handling those sensors first. Invalid edits roll
back atomically. Project undo/redo includes the sensor configuration and layout;
incoming readings never add undo entries or write controls.

Sensor-free files keep their existing v1 shape. Sensor-bearing editor files use
the reserved G1 project-v2 wrapper around an unchanged v1 document. Engine,
scientific-worker and investigation payloads still receive that v1 design.
Malformed/unsupported imports fail without partially replacing the editor. All
saved files must fit the 2 MiB local project limit and are written atomically.
Allocated source-feed project v3 is a separate upcoming extension; do not silently
downgrade it into the telemetry envelope.

## Local monitoring

Open **Local monitoring** for Manual, Synthetic live, Replay and Historical modes.
Each view identifies its source and clock. It shows reported quality separately
from receiver freshness, typed pump feedback, missing channels, finite negative or
out-of-range measurements and bounded recent traces. A sensor chart contains one
channel/unit and uses reported event times, not the forecast timeline.

Continuous input runs in a dedicated worker thread, separate from the one-shot
scientific worker. A local publisher must emit accepted G1 NDJSON and handle the
versioned lifecycle commands. Configuration is supplied locally to the child; its
hash must be echoed. Startup, command acknowledgement, timeout, process failure,
partial lines, invalid IDs and size/rate limits are checked. Publisher availability
is explicit: no A-owned numerical generator is bundled by B, and its absence does
not make the manual simulator unavailable. Test publishers are protocol fixtures,
not a validated plant.

To configure an existing local publisher, set `BREAKWATER_TELEMETRY_EXECUTABLE` to
its absolute executable path and `BREAKWATER_TELEMETRY_ARGUMENTS_JSON` to a JSON
array of argument strings before launching the desktop. No shell command string
is evaluated. Model credentials are not supplied to the publisher. No network
listener or hardware discovery is added.

Replay reads local recorded NDJSON through the same core. This is event-time
reconstruction of emitted sample payloads, not reconstruction of original receiver
arrival jitter or its freshness/reorder classifications. The UI identifies that
limit. A receipt-timing recording extension needs separate review before faithful
detector evaluation in G3. Pause freezes the
playback clock; speed changes affect playback rate. Restart generates a fresh
epoch. Historical evidence never becomes actionable. Export uses the retained
original frames and an atomic local file; errors are visible. A valid prefix may be
exported/replayed, but it is marked incomplete without an actual end frame; a
buffer-truncated epoch cannot be passed off as complete. Save evidence before
starting another capture/replay: recent buffers are bounded, not an unlimited archive.

## Bounds and identity

G1 limits remain 256 sensors/readings per batch, 256 KiB per frame, a 16-batch/4 MiB
reorder queue and 32 MiB/32,768-reading recent traces. External live ingress is
limited to 1 MiB and 1,024 readings per receive-second. Local playback uses bounded
file/frame limits and at most 16 frames per worker tick; accelerated replay is not
charged against the external source's wall-clock ingress quota. Exact retained duplicates are idempotent;
conflicting duplicates fault. Late evidence does not replace newer current state.
Future records do not establish freshness or eligibility. Counters expose rejection,
buffering and eviction. The bounded sequence-identity ledger retains 32,768 recent
identities; a retry older than its retained floor requires a fresh epoch/resync
rather than guessing whether it conflicts. Acknowledgements never clear a fault or
authorize a simulated operating action.

## Acceptance and next boundary

Source implementation and tests are separate from executed evidence: current
normal/ASan/QML results are recorded in STATUS.md and the increment's PR. Tests
exercise real fixture QProcess output, clock/quality/sequence failures, project
round-trips and replayed emitted recordings. Fixture truth is not a detector input.
A's seeded plant and physical mapping remain external follow-ups; G3 owns event
detection, coherent observed captures, forecasts and freshness-aware approval.

For the newly supplied EOG track, see [EOG_TRACK_ALIGNMENT.md](EOG_TRACK_ALIGNMENT.md).
The challenge's dataset adapter and discrete shared-fleet planner require their own
reviewed extension; these local G1 streams are not yet an EOG dataset importer.
