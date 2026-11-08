"""Immutable numerical EOG configuration from already-decoded official inputs.

No file access or capture/wire adapter. Unknown shapes and unsupported units fail
explicitly. Coordinates are checked input metadata and never determine physics.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
import hashlib
import json
import math

from eog_observations import Channel, ObservationError

METHOD = "eog-model-0.1.0"


class ModelError(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


def _object(value, required, optional=()):
    if type(value) is not dict or not set(required) <= set(value) or set(value) - set(required) - set(optional):
        raise ModelError("invalid_shape", "Missing or unsupported input fields.")
    return value


def _number(value, *, positive=False, maximum=None):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ModelError("invalid_value", "Expected a finite numeric configuration value.")
    try:
        number = float(value)
    except (ValueError, OverflowError) as error:
        raise ModelError("invalid_value", "Configuration is not finite binary64.") from error
    if not math.isfinite(number) or number < 0 or (positive and number == 0) or (maximum is not None and number > maximum):
        raise ModelError("invalid_value", "Configuration value lies outside its supported range.")
    return number


def _array(value, maximum):
    if type(value) is not list or not 1 <= len(value) <= maximum:
        raise ModelError("limit_exceeded", "Expected a nonempty bounded input array.")
    return value


def _coordinate(value, maximum):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ModelError("invalid_value", "Coordinate must be numeric.")
    try:
        number = float(value)
    except (ValueError, OverflowError) as error:
        raise ModelError("invalid_value", "Coordinate is not finite.") from error
    if not math.isfinite(number) or abs(number) > maximum:
        raise ModelError("invalid_value", "Coordinate lies outside latitude/longitude range.")


@dataclass(frozen=True, slots=True)
class Well:
    id: str
    baseline_water_bpd: float
    oil_bpd: float
    gas_mcfd: float
    revenue_per_hour: float


@dataclass(frozen=True, slots=True)
class Facility:
    system_id: str
    id: str
    tank_id: str
    pump_id: str
    pipeline_id: str
    capacity_bbl: float
    warning_pct: float
    shutdown_pct: float
    start_pct: float
    stop_pct: float
    pump_flow_bpd: float
    pipeline_max_bpd: float
    response_min: float
    cycle_min: float
    wells: tuple[Well, ...]


@dataclass(frozen=True, slots=True)
class System:
    id: str
    disposal_id: str
    disposal_pressure_psi: float
    fleet_size: int


@dataclass(frozen=True, slots=True)
class Model:
    systems: tuple[System, ...]
    facilities: tuple[Facility, ...]
    repair_hours: float
    truck_capacity_bbl: float
    cost_per_trip: float
    oil_per_bbl: float
    gas_per_mcf: float

    def channels(self):
        channels = []
        for f in self.facilities:
            channels.extend((Channel(f.id, "inflow_bpd", "bpd"),
                             Channel(f.tank_id, "level_pct", "%"),
                             Channel(f.pump_id, "status", "state", (0, 1, 2)),
                             Channel(f.pump_id, "flow_bpd", "bpd"),
                             Channel(f.pump_id, "discharge_psi", "psi"),
                             Channel(f.pipeline_id, "receive_flow_bpd", "bpd"),
                             Channel(f.pipeline_id, "pressure_psi", "psi")))
            for well in f.wells:
                channels.extend((Channel(well.id, "water_bpd", "bpd"), Channel(well.id, "status", "state", (0, 1))))
        if len(channels) > 8192:
            raise ModelError("limit_exceeded", "Compiled observation catalog exceeds 8192 channels.")
        return tuple(channels)

    def fingerprint(self):
        # Private semantic identity. Public capture identity is verified separately.
        raw = json.dumps(asdict(self), sort_keys=True, ensure_ascii=False, allow_nan=False,
                         separators=(",", ":")).encode("utf-8")
        return hashlib.sha256(b"breakwater-private-eog-model-v1\n" + raw).hexdigest()


def compile_model(network, ops):
    """Compile the supported official network/ops dictionaries, without I/O."""
    _object(network, ("meta", "defaults", "systems"))
    _object(network["meta"], ("units", "timezone"))
    if network["meta"]["units"] != {"volume": "bbl", "rate": "bpd", "gas_rate": "mcfd", "pressure": "psi"} or network["meta"]["timezone"] != "UTC":
        raise ModelError("unsupported_units", "EOG input units must be bbl/bpd/mcfd/psi with UTC timestamps.")
    _object(network["defaults"], ("tank_limits_pct",))
    limits = _object(network["defaults"]["tank_limits_pct"], ("warning", "shutdown"))
    warning = _number(limits["warning"], maximum=100)
    shutdown = _number(limits["shutdown"], positive=True, maximum=100)
    if warning >= shutdown:
        raise ModelError("invalid_thresholds", "Warning must be below shutdown.")
    _object(ops, ("prices", "expected_repair_hours", "emergency_trucking", "systems"), ("dataset_id",))
    if "dataset_id" in ops:
        try:
            Channel(ops["dataset_id"], "dataset", "identity")
        except ObservationError as error:
            raise ModelError(error.code, "Dataset ID must be bounded nonempty Unicode.") from error
    prices = _object(ops["prices"], ("oil_per_bbl", "gas_per_mcf"))
    oil, gas = _number(prices["oil_per_bbl"]), _number(prices["gas_per_mcf"])
    repair = _object(ops["expected_repair_hours"], ("pump_failure",))
    repair_h = _number(repair["pump_failure"], positive=True, maximum=168)
    trucking = _object(ops["emergency_trucking"], ("truck_capacity_bbl", "cost_per_trip"))
    truck_capacity = _number(trucking["truck_capacity_bbl"], positive=True)
    cost = _number(trucking["cost_per_trip"])
    if type(ops["systems"]) is not dict:
        raise ModelError("invalid_shape", "Operations systems must be an ID mapping.")
    seen, system_ids, facilities, systems = set(), set(), [], []

    def identity(value):
        try:
            Channel(value, "identity", "identity")
        except ObservationError as error:
            raise ModelError(error.code, str(error)) from error
        if value in seen:
            raise ModelError("duplicate_identity", "Asset and system IDs must be globally unique.")
        seen.add(value)
        return value

    for system in _array(network["systems"], 64):
        _object(system, ("id", "center", "truck_yard", "disposal_site", "facilities"))
        sid = identity(system["id"])
        system_ids.add(sid)
        for key in ("center", "truck_yard"):
            pair = system[key]
            if type(pair) is not list or len(pair) != 2:
                raise ModelError("invalid_shape", "Map coordinates need latitude and longitude.")
            _coordinate(pair[0], 90)
            _coordinate(pair[1], 180)
        disposal = _object(system["disposal_site"], ("id", "lat", "lon", "pressure_psi"))
        disposal_id = identity(disposal["id"])
        _coordinate(disposal["lat"], 90)
        _coordinate(disposal["lon"], 180)
        system_ops = _object(ops["systems"].get(sid), ("emergency_fleet_size", "facilities"))
        fleet = system_ops["emergency_fleet_size"]
        if type(fleet) is not int or not 0 <= fleet <= 256:
            raise ModelError("invalid_value", "Fleet size must be an integer in [0, 256].")
        if type(system_ops["facilities"]) is not dict:
            raise ModelError("invalid_shape", "Facility operations must be an ID mapping.")
        systems.append(System(sid, disposal_id, _number(disposal["pressure_psi"]), fleet))
        facility_ids = set()
        for source in _array(system["facilities"], 256):
            _object(source, ("id", "lat", "lon", "wells", "tank", "pump", "pipeline"))
            fid = identity(source["id"])
            facility_ids.add(fid)
            _coordinate(source["lat"], 90)
            _coordinate(source["lon"], 180)
            tank = _object(source["tank"], ("id", "capacity_bbl"))
            pump = _object(source["pump"], ("id", "max_flow_bpd", "start_level_pct", "stop_level_pct"))
            pipe = _object(source["pipeline"], ("id", "max_bpd", "max_pressure_psi"))
            tank_id, pump_id, pipe_id = identity(tank["id"]), identity(pump["id"]), identity(pipe["id"])
            stop, start = _number(pump["stop_level_pct"], maximum=100), _number(pump["start_level_pct"], maximum=100)
            if not stop < start < shutdown:
                raise ModelError("invalid_thresholds", "Supported pump setpoints require stop < start < shutdown.")
            _number(pipe["max_pressure_psi"], positive=True)
            travel = _object(system_ops["facilities"].get(fid), ("response_min", "cycle_min"))
            wells = []
            for source_well in _array(source["wells"], 128):
                _object(source_well, ("id", "baseline_water_bpd", "oil_bpd", "gas_mcfd"))
                wid = identity(source_well["id"])
                water = _number(source_well["baseline_water_bpd"])
                oil_rate, gas_rate = _number(source_well["oil_bpd"]), _number(source_well["gas_mcfd"])
                revenue = (oil_rate * oil + gas_rate * gas) / 24
                if not math.isfinite(revenue):
                    raise ModelError("unrepresentable_arithmetic", "Well revenue cannot be represented in binary64.")
                wells.append(Well(wid, water, oil_rate, gas_rate, revenue))
            try:
                total = math.fsum(w.revenue_per_hour for w in wells)
            except OverflowError as error:
                raise ModelError("unrepresentable_arithmetic", "Facility exposure overflowed.") from error
            if not math.isfinite(total):
                raise ModelError("unrepresentable_arithmetic", "Facility exposure overflowed.")
            facilities.append(Facility(sid, fid, tank_id, pump_id, pipe_id,
                                      _number(tank["capacity_bbl"], positive=True), warning, shutdown, start, stop,
                                      _number(pump["max_flow_bpd"], positive=True), _number(pipe["max_bpd"], positive=True),
                                      _number(travel["response_min"]), _number(travel["cycle_min"], positive=True), tuple(wells)))
            if len(facilities) > 256:
                raise ModelError("limit_exceeded", "At most 256 facilities are supported.")
        if facility_ids != set(system_ops["facilities"]):
            raise ModelError("unknown_identity", "Network and operations facility sets differ.")
    if system_ids != set(ops["systems"]):
        raise ModelError("unknown_identity", "Network and operations system sets differ.")
    model = Model(tuple(systems), tuple(facilities), repair_h, truck_capacity, cost, oil, gas)
    model.channels()
    return model
