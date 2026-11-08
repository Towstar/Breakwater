"""Private imported-model synthetic plant and causal observation projection.

This does not load files or extend the accepted analysis protocol. Future truth,
seed and full scenario receipt are host/offline information. Detectors/planners
receive only view(), never this host, its native handle or its provenance().
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
import hashlib
import json
import math
import random
from types import MappingProxyType

from eog_model import Model
from eog_observations import Archive, MAX_SAMPLES, MAX_UNIX, Sample
from native_eog_plant import NativePlant

METHOD = "eog-synthetic-observations-0.1.0"
PERIOD_S = 300


class SyntheticError(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


def _integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise SyntheticError("invalid_parameter", "Synthetic integer lies outside its supported bound.")
    return value


def _number(value, low=0, high=math.inf):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise SyntheticError("invalid_parameter", "Synthetic numeric parameters must be finite numbers.")
    try: result = float(value)
    except (ValueError, OverflowError) as error:
        raise SyntheticError("invalid_parameter", "Synthetic parameter is not binary64.") from error
    if not math.isfinite(result) or not low <= result <= high:
        raise SyntheticError("invalid_parameter", "Synthetic numeric parameter lies outside its supported bound.")
    return result


def _json(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, allow_nan=False, separators=(",", ":"))


@dataclass(frozen=True, slots=True)
class Initial:
    facility_id: str
    volume_bbl: float
    pump_running: bool
    productive_well_ids: tuple[str, ...]  # declared production eligibility, not inferred current shut-in status


@dataclass(frozen=True, slots=True)
class Failure:
    facility_id: str
    start_s: float


@dataclass(frozen=True, slots=True)
class RateChange:
    well_id: str
    elapsed_s: float
    water_bpd: float
    oil_bpd: float
    gas_mcfd: float


@dataclass(frozen=True, slots=True)
class Noise:
    level_pct_amplitude: float = 0.1
    rate_relative_amplitude: float = 0.005
    rate_absolute_bpd_amplitude: float = 0.0
    pressure_psi_amplitude: float = 1.0
    dropout_every: int = 0

    def __post_init__(self):
        for name, high in (("level_pct_amplitude", 10), ("rate_relative_amplitude", 0.25),
                           ("rate_absolute_bpd_amplitude", 1e6), ("pressure_psi_amplitude", 1e4)):
            object.__setattr__(self, name, _number(getattr(self, name), high=high))
        _integer(self.dropout_every, 0, 10000)
        if self.dropout_every == 1:
            raise SyntheticError("invalid_parameter", "Dropout period must be zero or at least two frames.")


@dataclass(frozen=True, slots=True)
class Experiment:
    start_unix: int
    duration_s: int
    seed: int
    initial: tuple[Initial, ...]
    failures: tuple[Failure, ...] = ()
    rate_changes: tuple[RateChange, ...] = ()
    noise: Noise = Noise()
    sample_limit: int = MAX_SAMPLES
    point_limit: int = 262144
    event_limit: int = 1000000
    trip_limit: int = 16384
    control_limit: int = 32768

    def __post_init__(self):
        _integer(self.duration_s, 1, 31 * 86400)
        _integer(self.start_unix, 0, MAX_UNIX - self.duration_s)
        _integer(self.seed, 0, 2**64 - 1)
        _integer(self.sample_limit, 1, MAX_SAMPLES)
        _integer(self.control_limit, 1, 262144)
        for key, high in (("point_limit", 262144), ("event_limit", 1000000), ("trip_limit", 16384)):
            _integer(getattr(self, key), 1, high)
        for key, cls, maximum in (("initial", Initial, 256), ("failures", Failure, 16384), ("rate_changes", RateChange, 16384)):
            values = getattr(self, key)
            if type(values) is not tuple or len(values) > maximum or any(type(v) is not cls for v in values):
                raise SyntheticError("invalid_parameter", "Synthetic configuration needs bounded immutable typed tuples.")
        if type(self.noise) is not Noise:
            raise SyntheticError("invalid_parameter", "Synthetic noise policy must be explicit and immutable.")


@dataclass(frozen=True, slots=True)
class Compiled:
    model: Model
    experiment: Experiment
    native_json: str
    facility_indices: object
    well_indices: object
    fingerprint: str

    def native_input(self):
        return json.loads(self.native_json)


def compile_experiment(model, experiment):
    if type(model) is not Model or type(experiment) is not Experiment:
        raise SyntheticError("invalid_parameter", "Synthetic compilation requires the imported Model and explicit Experiment.")
    facilities = {f.id: i for i, f in enumerate(model.facilities)}
    wells = {w.id: index for index, w in enumerate(w for f in model.facilities for w in f.wells)}
    well_parameters = {w.id: w for f in model.facilities for w in f.wells}
    systems = {s.id: i for i, s in enumerate(model.systems)}
    initial = {}
    for state in experiment.initial:
        if type(state.facility_id) is not str or state.facility_id not in facilities or state.facility_id in initial:
            raise SyntheticError("invalid_initialization", "Initial states must uniquely cover imported facilities.")
        if type(state.pump_running) is not bool or type(state.productive_well_ids) is not tuple or any(type(w) is not str for w in state.productive_well_ids):
            raise SyntheticError("invalid_initialization", "Initial pump/production state must be explicit, typed and immutable.")
        field = model.facilities[facilities[state.facility_id]]
        if len(set(state.productive_well_ids)) != len(state.productive_well_ids) or set(state.productive_well_ids) - {w.id for w in field.wells}:
            raise SyntheticError("invalid_initialization", "Initial productivity references a duplicate/foreign well.")
        _number(state.volume_bbl, high=field.capacity_bbl)
        if state.pump_running and state.volume_bbl < field.stop_pct / 100 * field.capacity_bbl or not state.pump_running and state.volume_bbl > field.start_pct / 100 * field.capacity_bbl:
            raise SyntheticError("invalid_initialization", "Initial pump state contradicts imported hysteresis thresholds.")
        initial[state.facility_id] = state
    if set(initial) != set(facilities):
        raise SyntheticError("invalid_initialization", "No facility receives an implicit initial condition.")
    frames = sum(not (experiment.noise.dropout_every and n % experiment.noise.dropout_every == experiment.noise.dropout_every - 1)
                 for n in range(experiment.duration_s // PERIOD_S + 1))
    if frames * len(model.channels()) > experiment.sample_limit:
        raise SyntheticError("sample_limit", "Complete synthetic observation history exceeds its declared sample bound.")
    native = {"duration_s": experiment.duration_s, "oil_per_bbl": model.oil_per_bbl, "gas_per_mcf": model.gas_per_mcf,
              "truck_capacity_bbl": model.truck_capacity_bbl, "cost_per_trip": model.cost_per_trip,
              "fleet_sizes": [s.fleet_size for s in model.systems], "facilities": [], "wells": [],
              "failures": [], "rate_changes": [], **{key: getattr(experiment, key) for key in ("point_limit", "event_limit", "trip_limit")}}
    for field in model.facilities:
        state = initial[field.id]
        native["facilities"].append({"system": systems[field.system_id], "first_well": len(native["wells"]), "well_count": len(field.wells),
            "initially_running": int(state.pump_running), "capacity_bbl": field.capacity_bbl, "initial_bbl": state.volume_bbl,
            "start_bbl": field.start_pct / 100 * field.capacity_bbl, "stop_bbl": field.stop_pct / 100 * field.capacity_bbl,
            "shutdown_bbl": field.shutdown_pct / 100 * field.capacity_bbl, "pump_bpd": field.pump_flow_bpd,
            "pipeline_capacity_bpd": field.pipeline_max_bpd, "response_min": field.response_min, "cycle_min": field.cycle_min})
        native["wells"].extend({"water_bpd": w.baseline_water_bpd, "oil_bpd": w.oil_bpd, "gas_mcfd": w.gas_mcfd,
                               "productive": int(w.id in state.productive_well_ids)} for w in field.wells)
    repair_s = _number(model.repair_hours * 3600)
    if repair_s <= 0:
        raise SyntheticError("invalid_parameter", "Synthetic repair duration must advance the clock.")
    intervals = {f.id: [] for f in model.facilities}
    for failure in experiment.failures:
        if type(failure.facility_id) is not str or failure.facility_id not in facilities:
            raise SyntheticError("unknown_facility", "Synthetic failure references an unknown imported facility.")
        start = _number(failure.start_s, high=experiment.duration_s)
        if start >= experiment.duration_s:
            raise SyntheticError("invalid_parameter", "Synthetic failure must start before the run ends.")
        end = _number(start + repair_s)
        if end <= start or any(start < b and a < end for a, b in intervals[failure.facility_id]):
            raise SyntheticError("invalid_parameter", "Synthetic pump failures overlap or fail to advance.")
        intervals[failure.facility_id].append((start, end))
        native["failures"].append({"facility": facilities[failure.facility_id], "start_s": start, "repair_s": repair_s})
    seen_changes = set()
    for change in experiment.rate_changes:
        if type(change.well_id) is not str or change.well_id not in wells:
            raise SyntheticError("unknown_well", "Synthetic rate forcing references an unknown imported well.")
        when = _number(change.elapsed_s, high=experiment.duration_s)
        parameters = well_parameters[change.well_id]
        if change.oil_bpd != parameters.oil_bpd or change.gas_mcfd != parameters.gas_mcfd:
            raise SyntheticError("unobservable_economics", "Fixed-file telemetry cannot communicate future oil/gas economics changes; use the imported constant economics.")
        if (change.well_id, when) in seen_changes:
            raise SyntheticError("invalid_parameter", "Duplicate synthetic rate forcing.")
        seen_changes.add((change.well_id, when))
        native["rate_changes"].append({"well": wells[change.well_id], "elapsed_s": when,
            **{key: _number(getattr(change, key)) for key in ("water_bpd", "oil_bpd", "gas_mcfd")}})
    native_json = _json(native)
    identity = hashlib.sha256((METHOD + "\n" + model.fingerprint() + "\n" + _json(asdict(experiment)) + "\n" + native_json).encode()).hexdigest()
    return Compiled(model, experiment, native_json, MappingProxyType(facilities), MappingProxyType(wells), identity)


def seeded_experiment(model, *, start_unix, duration_s, seed, failure_count=None, rate_change_count=2, noise=Noise()):
    """Explicit synthetic defaults; not inferred initialization or official noise."""
    if type(model) is not Model:
        raise SyntheticError("invalid_parameter", "Seeded generation needs an imported Model.")
    _integer(seed, 0, 2**64 - 1); _integer(duration_s, 301, 31 * 86400)
    count = min(3, len(model.facilities)) if failure_count is None else _integer(failure_count, 0, 16384)
    _integer(rate_change_count, 0, 16384)
    rng = random.Random(seed)
    initial = tuple(Initial(f.id, f.capacity_bbl * (f.stop_pct + rng.random() * (f.start_pct - f.stop_pct)) / 100,
                            False, tuple(w.id for w in f.wells)) for f in model.facilities)
    failures, intervals = [], {f.id: [] for f in model.facilities}
    for _ in range(100000):
        if len(failures) == count: break
        field = model.facilities[rng.randrange(len(model.facilities))]
        start = rng.randrange(300, duration_s) + 0.125
        end = start + model.repair_hours * 3600
        if any(start < b and a < end for a, b in intervals[field.id]): continue
        failures.append(Failure(field.id, start)); intervals[field.id].append((start, end))
    if len(failures) != count:
        raise SyntheticError("scenario_space_unavailable", "Bounded generator could not place the requested nonoverlapping failures.")
    all_wells = [w for f in model.facilities for w in f.wells]
    changes, seen = [], set()
    for _ in range(100000):
        if len(changes) == rate_change_count: break
        well = all_wells[rng.randrange(len(all_wells))]
        when = rng.randrange(300, duration_s) + 0.25
        if (well.id, when) in seen: continue
        seen.add((well.id, when))
        changes.append(RateChange(well.id, when, well.baseline_water_bpd * rng.uniform(0.75, 1.25), well.oil_bpd, well.gas_mcfd))
    if len(changes) != rate_change_count:
        raise SyntheticError("scenario_space_unavailable", "Bounded generator could not place the requested rate changes.")
    result = Experiment(start_unix, duration_s, seed, initial, tuple(failures), tuple(changes), noise)
    compile_experiment(model, result)
    return result


class SyntheticStream:
    """Host-side plant owner. Analysis consumers receive only immutable View."""
    def __init__(self, library, compiled):
        if type(compiled) is not Compiled:
            raise SyntheticError("invalid_parameter", "Synthetic stream needs the explicitly compiled scenario.")
        self._compiled = compiled
        self._native = NativePlant(library, compiled.native_input())
        self._rows = []; self._commands = []; self._elapsed = 0.0; self._next_sample = PERIOD_S
        try: self._emit(self._native.snapshot(), 0)
        except BaseException: self._native.close(); raise

    def _signed_noise(self, unix, asset, metric):
        # Seed/time/channel only: future schedule, advance partition and missing
        # acquisitions cannot change an earlier channel's perturbation.
        raw = _json((METHOD, self._compiled.experiment.seed, unix, asset, metric)).encode()
        bits = int.from_bytes(hashlib.sha256(raw).digest()[:8], "big") >> 11
        return 2 * (bits / 2**53) - 1

    def _emit(self, truth, elapsed):
        if truth["status"] not in ("running", "completed"):
            raise SyntheticError("plant_" + truth["status"], truth["diagnostic"])
        model, config = self._compiled.model, self._compiled.experiment
        index = elapsed // PERIOD_S
        if config.noise.dropout_every and index % config.noise.dropout_every == config.noise.dropout_every - 1:
            return
        unix = config.start_unix + elapsed
        pressure = {s.id: s.disposal_pressure_psi for s in model.systems}
        frame = []
        def sample(asset, metric, value):
            amplitude = (config.noise.level_pct_amplitude if metric == "level_pct" else config.noise.pressure_psi_amplitude
                         if metric.endswith("psi") else abs(value) * config.noise.rate_relative_amplitude + config.noise.rate_absolute_bpd_amplitude
                         if metric.endswith("bpd") else 0)
            measured = value + amplitude * self._signed_noise(unix, asset, metric)
            frame.append(Sample(unix, asset, metric, measured))
        for fi, field in enumerate(model.facilities):
            current = truth["facilities"][fi]; running = current["pump_status"] == 1
            sample(field.id, "inflow_bpd", current["inflow_bpd"])
            sample(field.tank_id, "level_pct", current["volume_bbl"] / field.capacity_bbl * 100)
            sample(field.pump_id, "status", current["pump_status"])
            sample(field.pump_id, "flow_bpd", current["pump_flow_bpd"])
            sample(field.pump_id, "discharge_psi", pressure[field.system_id] + (150 if running else 0))
            sample(field.pipeline_id, "receive_flow_bpd", current["pump_flow_bpd"])
            sample(field.pipeline_id, "pressure_psi", pressure[field.system_id] + (50 if running else 0))
            for well in field.wells:
                state = truth["wells"][self._compiled.well_indices[well.id]]
                sample(well.id, "water_bpd", state["water_bpd"])
                sample(well.id, "status", state["producing"])
        if len(self._rows) + len(frame) > config.sample_limit:
            raise SyntheticError("sample_limit", "A complete observation frame exceeds the retained sample bound.")
        self._rows.extend(frame)

    def _advance_native(self, elapsed, cancelled):
        result = self._native.advance(elapsed, cancelled)
        self._elapsed = result["elapsed_s"]
        if result["status"] not in ("running", "completed"):
            raise SyntheticError("plant_" + result["status"], result["diagnostic"])
        return result

    def advance(self, elapsed_s, cancelled=None):
        target = _number(elapsed_s, high=self._compiled.experiment.duration_s)
        if target < self._elapsed:
            raise SyntheticError("invalid_clock", "Synthetic plant clock must be monotone; reset to restart.")
        while self._next_sample <= target:
            tick = self._next_sample
            result = self._advance_native(tick, cancelled)
            self._emit(result, tick); self._next_sample += PERIOD_S
        if target > self._elapsed or cancelled is not None:
            self._advance_native(target, cancelled)
        return {"elapsed_s": self._elapsed, "sample_count": len(self._rows), "last_acquisition_unix": self._rows[-1].unix}

    def view(self, cutoff_unix=None):
        latest_cutoff = self._compiled.experiment.start_unix + math.floor(self._elapsed)
        cutoff = latest_cutoff if cutoff_unix is None else _integer(cutoff_unix, self._compiled.experiment.start_unix, latest_cutoff)
        rows = tuple(row for row in self._rows if row.unix <= cutoff)
        return Archive(self._compiled.model.channels(), rows, capture_cutoff_unix=cutoff,
                       max_samples=self._compiled.experiment.sample_limit).at(cutoff)

    def truth(self):
        """Current host/offline diagnostics; never pass this object to analysis."""
        return self._native.snapshot()

    def provenance(self):
        """Full host/offline receipt, containing future truth and seed."""
        physical_actions = [dict(c) for c in self._commands if c["accepted"]]
        run_fingerprint = hashlib.sha256((METHOD + "\n" + self._compiled.fingerprint + "\n" + _json(physical_actions) + "\n" + _json(self._elapsed)).encode()).hexdigest()
        return {"method": METHOD, "native_method": self._native.method, "model_fingerprint": self._compiled.model.fingerprint(),
            "scenario_fingerprint": self._compiled.fingerprint, "experiment": asdict(self._compiled.experiment),
            "run_fingerprint": run_fingerprint, "controls": [dict(c) for c in self._commands],
            "limitations": ["Explicit synthetic initialization/forcing, not recorded or predicted real operation.",
                "Seeded bounded perturbations are declared assumptions, not the official noise-generating process.",
                "Pressure offsets are placeholders; no hydraulic or calibrated confidence claim.",
                "This receipt contains future truth; analysis receives only view()."]}

    def dispatch(self, facility_id, truck, trips):
        if type(facility_id) is not str or facility_id not in self._compiled.facility_indices:
            raise SyntheticError("unknown_facility", "Dispatch references an unknown imported facility.")
        self._control_capacity()
        result = self._native.dispatch(self._compiled.facility_indices[facility_id], truck, trips)
        self._commands.append({"operation": "dispatch", "elapsed_s": self._elapsed, "facility_id": facility_id,
                               "truck": truck, "trips": trips, **result})
        return result

    def shut_in(self, well_id):
        if type(well_id) is not str or well_id not in self._compiled.well_indices:
            raise SyntheticError("unknown_well", "Shut-in references an unknown imported well.")
        self._control_capacity()
        result = self._native.shut_in(self._compiled.well_indices[well_id])
        self._commands.append({"operation": "shut_in", "elapsed_s": self._elapsed, "well_id": well_id, **result})
        return result

    def _control_capacity(self):
        if len(self._commands) >= self._compiled.experiment.control_limit:
            raise SyntheticError("control_limit", "The accepted/rejected control journal is full; no further control is applied.")

    def reset(self):
        self._native.reset(); self._rows.clear(); self._commands.clear(); self._elapsed = 0.0; self._next_sample = PERIOD_S
        self._emit(self._native.snapshot(), 0)

    def close(self): self._native.close()
    def __enter__(self): return self
    def __exit__(self, *_): self.close()
