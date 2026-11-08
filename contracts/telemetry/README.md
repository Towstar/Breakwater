# G1 telemetry contracts

These are implemented **interfaces, validators and acceptance fixtures** for a
future local synthetic monitoring system. They do not implement sensor placement,
acquisition, a synthetic publisher, observation mathematics, detectors, forecasts
or operating-control application. Both developers must adopt the same reviewed
contract commit before G2 implementation.

Person A's recorded numerical review input is
[`science/OBSERVATION_SEMANTICS.md`](../../science/OBSERVATION_SEMANTICS.md), commit
`9941859b202d804b024d788b1238d94166b33309`. Its worked examples are retained in
[`fixtures/person-a-worked-examples.json`](fixtures/person-a-worked-examples.json).
That guidance is distinct from approval of the wire format. Person A acceptance of
exact commit `f12a827` is now recorded in [G1_REVIEW.md](../../docs/G1_REVIEW.md),
and PR #31 was formally approved and merged at `2bd7bb9`. G2 adopts that baseline.

## Documents and compatibility

All telemetry documents have `telemetry_version: 1` and a strict `kind` discriminator.
JSON Schema Draft 2020-12 definitions use local references only. Definitions in
`common-v1.schema.json` are shared by the public schemas; JSON files are the source
of truth. Unknown fields, quantities, versions and variants are rejected.

| Document | Contents |
|---|---|
| `sensor-configuration-v1.schema.json` | Stable sensor/source/channel IDs, type/unit, attachment, calibration, physical support, uncertainty, sampling/age policy and explicit redundancy |
| `stream-frame-v1.schema.json` | `start`, `readings`, `heartbeat`, `ack`, `error`, `end`; stream/epoch/sequence and configuration/network identity |
| `stream-command-v1.schema.json` | `pause`, `resume`, `set_rate`, `restart`, `stop`; request identity and typed payload |
| `observed-snapshot-v1.schema.json` | Original samples, conversions, receive times, separate commands/derived state, coverage, assumptions, availability and expiry |
| `detection-event-v1.schema.json` | Evidence/threshold versions, onset/update/clear, severity, unresolved/hypothesis diagnosis and separate acknowledgement |
| `project-v2.schema.json` | Explicit v2 envelope containing an unchanged v1 `project`, `sensor_configuration` and separate `sensor_layout` |

The project-v2 envelope is used by G2's sensor-bearing editor. G1 alone delivered
the interface; G2's runtime acceptance is recorded separately in STATUS.md.
Existing project-v1 serialization is unchanged. Current older readers reject
`version: 2` rather than silently dropping the sensor fields on a later save. The
embedded v1 object preserves its existing draft-network semantics. V2 validation
requires a registered `network` and opaque `physical_hash` context; the embedded
network must equal that registered network. Concentration also binds the embedded
pipe parameters to registered parameters and `pipe_parameter_hashes`. Without this
context the wrapper is rejected, rather than reported as fully bound. Native validation
remains authoritative for structural/physical executability. New telemetry
does not alter the scientific-worker request/response envelope or session archives.
Continuous acquisition needs a separate lifecycle adapter in G2: a one-shot
scientific worker's clean-exit publication rule is not a streaming protocol.

## Measurements and physical interpretation

Sensor configuration units are canonical: inventory `m3`, level `m`, source/route
flow `m3/h`, pump state `state`, concentration `mg/L`, pressure `kPa`, temperature
`degC`; custom numeric channels declare their own unit. Wire observations retain
original values and units. Declared conversions are inventory `L`→`m3`, level
`cm`→`m`, and flow `L/s` or `L/h`→`m3/h`; other channels require their canonical unit.
No calibration or detector mathematics is executed by these validators.

Inventory/level attach to a tank; source flow to a source; route flow and
concentration to a route; pump feedback to a route containing a pump. Concentration
requires declared physical pipe geometry when validating against a network.
Each concentration sensor also requires a `pipe_parameter_hash`, matched to the
registered route's parameter identity. Pipe geometry, diffusion, decay, boundary or
discretization edits therefore retire the observation operator even when the storage
network hash is unchanged. Other sensor types cannot carry a pipe-parameter hash.
Cell-average support names a valid cell; interval-average support names an interval
within that pipe. `point_unavailable` explicitly retains unsupported point support;
it does not assert that cell averages determine an exact point concentration.
Acquisition windows and delay are separate from event and receive times. Level
calibration requires finite, strictly increasing level/volume knots. Values outside
the calibration domain remain evidence; A's later mapper must report unavailable
instead of extrapolating or clipping.
Sensor `uncertainty_bound` uses the sensor's canonical unit. The level calibration's
`error_bound` is a volume bound in m3; identity calibration error uses the quantity's
canonical unit. These are declared absolute bounds, not standard deviations or
calibrated probabilities.

Typed pump states are `commanded_off`, `running`, `delivery_loss`, `unavailable`,
`unknown`. A commanded-off observation does not establish a fault. A new intended
setpoint does not establish recovered delivery. Pressure, temperature and custom
channels are observation/alarm-only and cannot appear in mapped native state.

Instrument limits are descriptive. Finite negative or above-capacity observations
are retained. A snapshot marked available cannot map an out-of-bounds inventory
into a supplied native network. Measurements, commanded controls and derived state
are separate arrays. Available coverage means the explicitly named observation
coverage passed contract checks; it does not claim that a complete network forecast
has been built or independently validated.
Mapped state cannot consume disabled, bad/stale or unsupported point observations,
or out-of-domain level values. Inferred state requires source observations and named
assumptions. Raw observations remain retainable in unavailable snapshots.
`measured` and `converted` derived values have exactly one source with the same
quantity, attachment and canonical unit/value. `converted` means unit conversion
only. A level-to-inventory calibration or other model mapping is `inferred`, with
source references and named assumptions; G1 does not execute that mathematics.
Every snapshot sample retains its actual acquisition start and end, ordered before
the sample event/capture times. Nominal configuration windows do not replace these
per-sample observations.

Each configured group has exactly one primary, including disabled definitions.
Redundant members observe the same quantity/unit/attachment and use distinct
source/channel pairs. Disabling a primary does not implicitly promote another
sensor. A later coherent-state builder must report missing/disagreement states;
this contract never averages conflicting measurements or uses last-writer-wins.

## Identity, clocks and bounds

New IDs are 1–128 ASCII characters matching `[A-Za-z0-9][A-Za-z0-9_.:-]*`.
Asset/route references retain the existing maximum of **256 UTF-16 code units**;
labels can contain Unicode. Revisions and sequences are exact integers through
`2^53-1`. A start frame has sequence zero; subsequent frames have positive sequence
numbers. A batch has at most one reading per sensor. Sample identity is
`(stream_id, epoch_id, sequence, sensor_id)`.

`configuration_hash` is SHA-256 of the following deterministic semantic projection:

1. Keep `telemetry_version`, `physical_hash`, `policy` and `sensors` sorted by ASCII
   `sensor_id`. Exclude each sensor's `label`; exclude logical configuration ID and
   revision. Layout is already a separate project field.
2. Preserve Booleans as Booleans. Replace each finite numeric value recursively by
   `{"$f64":"<16 lowercase hex digits of big-endian IEEE binary64>"}`. Normalize
   negative zero to positive zero. This removes Python/Qt decimal-format differences.
3. Sort object keys, encode compact UTF-8 JSON without ASCII escaping, then hash the
   exact bytes. Published byte/hash vectors include ordering, Unicode and signed zero.

Existing network hashes are opaque and unchanged. The publisher echoes the supplied
configuration hash; it cannot replace the receiver's registered definition or
current network identity. Label/marker moves preserve semantic identity. Attachment,
physical support, enabled state, calibration, uncertainty and clock-policy edits
change it. These hashes establish content identity, not source authentication.

Event time is relative to an epoch. Restart and replay use a new epoch; old epochs
cannot become current. Receive UTC and monotonic times are receiver-owned snapshot
fields and are forbidden in publisher readings. Replay age uses the playback event
clock, which pauses with playback; it is not inferred from wall-clock processing speed.

An exact duplicate frame is idempotent; conflicting content with the same current
sequence is `conflict`. Old epochs are `old_epoch`; beyond the future allowance is
`future`; earlier than `watermark - reorder_window` is `history_only`; within the
window but before the watermark is `buffered`; otherwise it is `accepted`.
These are pure fixed-clock classifications, not an ingestion loop. Valid late
evidence is not malformed and must not replace newer accepted state.

| Limit/default | Value |
|---|---:|
| Sensors / readings per batch | 256 / 256 |
| UTF-8 stream frame including newline | 256 KiB |
| Other contract document | 4 MiB |
| Reorder queue | 16 batches or 4 MiB |
| Recent traces | 32 MiB or 32,768 readings |
| Ingress | 1,024 readings/s and 1 MiB/s |
| Example sampling period / maximum age | 1 s / 5 s |
| Coherent skew / reorder window / future allowance | 1 s / 2 s / 0.25 s |
| Heartbeat period / source timeout | 1 s / 5 s |
| JSON nesting | 64 levels |

Rate/queue/trace bounds are published requirements for G2, not claims of a running
recorder. Period/age are explicit per-sensor values; coherent timing and source
timeouts are explicit configuration policy. Missing coverage, stale/bad required
samples and declared conflicts cannot be marked available. Snapshot expiry is a
separate classification: advancing a clock does not rewrite immutable evidence.
Replay/historical snapshots are historical; expired synthetic snapshots remain
reviewable. Receiver-owned replay mode overrides a document claiming live mode.
A document claiming live mode without a supplied receiver clock or registered
configuration/network/physical-hash context is classified unavailable. A capture
beyond that clock's current time plus the declared future allowance is unavailable.
Receiver clocks require stream/epoch IDs, finite nonnegative current/watermark event
times and an explicit mode. Contradictory network/project or pipe/project contexts
are rejected. Conflicting observations cannot supply a mapped state, even when
other fields describe the snapshot as unavailable. Every snapshot/event has
`actionable: false`. Neither imports, event
acknowledgements nor stream lifecycle commands grant operating-control authority.

## Offline validation and acceptance

Use the existing project CPU Python environment (`jsonschema==4.25.1` is already
in the committed worker-test lock):

```powershell
.\.tools\science-cpu\Scripts\python.exe -B contracts/telemetry/validate.py contracts/telemetry/examples/canonical-sensors.json
.\.tools\science-cpu\Scripts\python.exe -B contracts/telemetry/tests/test_contracts.py --native out/visual-studio/desktop/RelWithDebInfo/breakwater_telemetry_contract_check.exe -v
```

`validate.py` optionally accepts `--kind` and `--context`. Context can bind a
configuration, network, opaque physical hash and declared pipe parameters with
their registered `pipe_parameter_hashes`. Semantic observed-snapshot validation
requires registered `configuration`, `network` and `physical_hash`; project-v2
validation requires its registered network/physical hash and any concentration pipe
identities. JSON Schema alone can inspect structure without that context, but it
does not establish source binding. `frame_disposition` and `snapshot_disposition` are pure fixture helpers;
they do not acquire data, mutate controls or start a forecast.

The shared fixture corpus covers canonical and renamed/custom networks, strict
shape/unit/calibration/support checks, original/canonical values, clock ordering,
duplicates, conflicts, missing coverage, historical authority and versioned project
compatibility. Raw-byte tests cover malformed/escaped duplicate keys, invalid UTF-8,
nonfinite values, excessive nesting and exact frame-size boundaries. Native/Python
differential validation is mandatory when `--native` is passed; CI must pass it.
Portable-only execution explicitly skips that comparator and is not cross-language
acceptance. Fixtures are synthetic software checks, never live-stream/detector evidence.

### Person A adoption checklist — pending

- Review the exact format/semantics against the source note at `9941859`.
- Confirm level calibration/domain/error-bound interpretation and concentration
  cell/interval/unsupported-point support, acquisition windows and zero-flow meaning.
- Confirm measured delivery versus command/intervention assumptions and pump states.
- Confirm required coverage, primary/redundant disagreement and uncertainty semantics.
- Review the retained conversion, calibration, concentration, residual and missing-data
  examples together; numerical implementations and independent oracles remain A-owned.
- Record the exact adopted G1 commit in both workstreams before G2 implementation.
