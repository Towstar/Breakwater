# Local synthetic acquisition and receiver-timed replay

Local acquisition and immutable observed-state capture now feed bounded symptom
detection, durable incident history and an explicit
[native forecast/advisory workflow](OBSERVED_FORECAST.md). No operating application
action is available in observed monitoring.

1. Start the desktop with an existing Python 3.12+ executable configured through
   `BREAKWATER_PYTHON` or the installed optional runtime. No Torch or model key is
   needed for acquisition.
2. Choose **Project > Local monitoring example**. This single undoable edit loads
   the healthy example with ten instruments and declared measurement headroom.
3. In Local monitoring, choose a preset and integer seed, then **Start synthetic
   input**. The seed and preset are frozen for that process. Restart creates a new
   epoch; pause, resume, rate and stop require matching G1 acknowledgements.
4. **Capture state** freezes actual received samples and the separate exact
   command catalog at one receiver-clock instant. Inspect availability, expiry
   and mapping issues. Selecting or capturing does not change design controls.

The bundled stdlib publisher uses independent piecewise constant volume balances.
Presets include healthy, pump delivery loss, unmetered loss, drift, stuck, missing,
delayed, reordered and mixed observations. Instrument noise is seeded and bounded.
Hidden fault truth is available only to offline tests, never to the detector or
wire. Negative and overflowing measurements are retained. Unsupported chemistry,
pressure and temperature channels report bad quality instead of invented values.
This is a local synthetic plant, not qualified field monitoring.

Custom `BREAKWATER_TELEMETRY_EXECUTABLE` and argument overrides retain precedence.
The child gets only runtime environment essentials and two temporary input files:
sensor configuration, and native network plus physical identity. It has no model
credential or actuator interface. Only acquisition lifecycle commands are accepted.
The publisher echoes the supplied semantic configuration hash.

## Recording compatibility and limits

**Save timed recording** exports a separate local `receiver_journal` version 1.
This is not a change to the G1 wire protocol. Its strict header binds the network
and configuration; frame records retain original bytes as canonical base64,
receiver monotonic/event clocks and UTC. A required completion footer distinguishes
clean received end, prefix, source fault and size limit. Completion also requires
normal child-process exit. At 32 MiB, retention stops and reports truncation.

Timed replay schedules original receipts and uses their recorded receiver clock.
It preserves late, future, duplicate and old-epoch classifications, including source
heartbeat loss while the source event clock is paused. Playback speed changes do
not rewrite source payloads. Footer/clock conflicts fault the replay; absent or
incomplete footers remain visibly incomplete evidence.

**Export event frames** preserves the older G1 NDJSON export and reader. Event-only
replay lacks original receipt timing and cannot reproduce receipt classifications.
Both forms of replay and imported history remain nonactionable.

Ingress remains bounded to 256 sensors/readings per batch, 256 KiB per raw frame,
16 batches/4 MiB reorder backlog and 32 MiB/32,768 recent readings. The publisher
also bounds command/delay queues, duplicate acknowledgements and aggregate sample
rate. No new third-party Python dependency is needed.

See [acquisition evidence](evidence/v03/local-acquisition.json) for the actual
checks, initial shutdown failure, regression fix and remaining boundaries.
