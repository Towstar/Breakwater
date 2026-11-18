"""Private causal accepted-control facts and independent truck reservations.

No files/native/model calls, future incident schedules, seed or dispatch authority.
The caller must authenticate a complete live history from an initially free fleet.
Scheduled pickups are commitments, not proof of inventory feasibility or execution.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
import hashlib
import json
import math

from eog_model import Model
from eog_observations import MAX_UNIX, View

METHOD = "eog-causal-commitments-0.1.0"
MAX_CONTROLS = 32768
MAX_TRIPS = 16384
MAX_ELAPSED = 31 * 86400


class CommitmentError(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


def _number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise CommitmentError("invalid_clock", "Control clocks must be finite nonnegative binary64 seconds.")
    try:
        value = float(value)
    except (OverflowError, ValueError) as error:
        raise CommitmentError("invalid_clock", "Control clock is not binary64.") from error
    if not math.isfinite(value) or not 0 <= value <= MAX_ELAPSED:
        raise CommitmentError("invalid_clock", "Control clock exceeds the supported 31-day window.")
    return value


def _id(value):
    try:
        valid = type(value) is str and bool(value) and len(value.encode("utf-8")) <= 1024
    except UnicodeError:
        valid = False
    if not valid:
        raise CommitmentError("invalid_id", "Control asset identity must be bounded nonempty UTF-8.")


def _hash(value):
    raw = json.dumps(value, ensure_ascii=False, sort_keys=True, allow_nan=False, separators=(",", ":")).encode()
    return hashlib.sha256(METHOD.encode() + b"\n" + raw).hexdigest()


@dataclass(frozen=True, slots=True)
class Dispatch:
    elapsed_s: float
    facility_id: str
    truck: int
    trips: int

    def __post_init__(self):
        object.__setattr__(self, "elapsed_s", _number(self.elapsed_s))
        _id(self.facility_id)
        if type(self.truck) is not int or not 0 <= self.truck < 128 or type(self.trips) is not int or not 1 <= self.trips <= 4095:
            raise CommitmentError("invalid_dispatch", "Dispatch needs a bounded truck ordinal and 1-4095 trips.")


@dataclass(frozen=True, slots=True)
class ShutIn:
    elapsed_s: float
    well_id: str

    def __post_init__(self):
        object.__setattr__(self, "elapsed_s", _number(self.elapsed_s))
        _id(self.well_id)


@dataclass(frozen=True, slots=True)
class History:
    model_fingerprint: str
    start_unix: int
    cutoff_elapsed_s: float
    complete_from_free_start: bool
    commands: tuple[Dispatch | ShutIn, ...]

    def __post_init__(self):
        if (type(self.model_fingerprint) is not str or len(self.model_fingerprint) != 64 or
                any(c not in "0123456789abcdef" for c in self.model_fingerprint)):
            raise CommitmentError("invalid_identity", "History needs an exact semantic model fingerprint.")
        clock = _number(self.cutoff_elapsed_s)
        object.__setattr__(self, "cutoff_elapsed_s", clock)
        if type(self.start_unix) is not int or not 0 <= self.start_unix <= MAX_UNIX - math.floor(clock):
            raise CommitmentError("invalid_clock", "History epoch/cutoff exceeds the supported Unix clock.")
        if self.complete_from_free_start is not True:
            raise CommitmentError("unknown_initial_fleet", "A partial history or unknown initial fleet cannot imply free trucks.")
        if type(self.commands) is not tuple or len(self.commands) > MAX_CONTROLS:
            raise CommitmentError("control_limit", "History must be an immutable complete bounded prefix.")
        previous = 0.0
        for command in self.commands:
            if type(command) not in (Dispatch, ShutIn):
                raise CommitmentError("invalid_control", "History contains an unsupported control record.")
            if not previous <= command.elapsed_s <= clock:
                raise CommitmentError("noncausal_history", "Accepted commands must be ordered and no later than the cutoff.")
            previous = command.elapsed_s

    def fingerprint(self):
        # Explicit operation tags prevent any accidental cross-type identity.
        return _hash({"model": self.model_fingerprint, "epoch": self.start_unix, "cutoff": self.cutoff_elapsed_s,
                      "commands": [(type(c).__name__, asdict(c)) for c in self.commands]})


@dataclass(frozen=True, slots=True)
class Pickup:
    command_index: int
    facility_id: str
    system_id: str
    truck: int
    arrival_elapsed_s: float
    volume_bbl: float
    cost: float


@dataclass(frozen=True, slots=True)
class Reservation:
    system_id: str
    truck: int
    busy: bool
    available_again_elapsed_s: float


@dataclass(frozen=True, slots=True)
class Commitments:
    method: str
    model_fingerprint: str
    observation_fingerprint: str
    history_fingerprint: str
    fingerprint: str
    start_unix: int
    cutoff_elapsed_s: float
    observation_cutoff_unix: int
    trucks: tuple[Reservation, ...]
    scheduled_pickups: tuple[Pickup, ...]
    limitations: tuple[str, ...]


def reconstruct(model: Model, view: View, history: History) -> Commitments:
    """Reconstruct known reservations, without claiming current process health.

    Only known accepted dispatches determine reservations. Model repair duration,
    hidden failure onset and predicted/recorded repairs never release a truck.
    No cost/load total is inferred from a planned pickup as actual execution.
    """
    if type(model) is not Model or type(view) is not View or type(history) is not History:
        raise CommitmentError("invalid_input", "Commitment reconstruction needs Model, causal View and History.")
    model_identity = model.fingerprint()
    if history.model_fingerprint != model_identity:
        raise CommitmentError("model_mismatch", "Accepted controls belong to a different immutable model.")
    if view.cutoff_unix != history.start_unix + math.floor(history.cutoff_elapsed_s):
        raise CommitmentError("cutoff_mismatch", "Observation and fractional control clocks disagree.")
    for channel in model.channels():
        if view.latest(*channel.key).channel != channel:
            raise CommitmentError("catalog_mismatch", "Observation catalog differs from the imported model.")
    fields = {f.id: f for f in model.facilities}
    wells = {w.id for f in model.facilities for w in f.wells}
    free = {(s.id, truck): 0.0 for s in model.systems for truck in range(s.fleet_size)}
    pickups = []
    for index, command in enumerate(history.commands):
        if type(command) is ShutIn:
            if command.well_id not in wells:
                raise CommitmentError("unknown_well", "Accepted shut-in names a foreign well.")
            continue
        f = fields.get(command.facility_id)
        if f is None or (f.system_id, command.truck) not in free:
            raise CommitmentError("unknown_truck", "Accepted dispatch names a foreign facility/truck slot.")
        key = (f.system_id, command.truck)
        if free[key] > command.elapsed_s:
            raise CommitmentError("overbooked_history", "Accepted dispatch history overlaps a truck's final return.")
        if command.trips > MAX_TRIPS - len(pickups):
            raise CommitmentError("trip_limit", "Complete known pickup schedule exceeds its 16384-trip bound.")
        response, cycle = f.response_min * 60.0, f.cycle_min * 60.0
        first = command.elapsed_s + response
        last = first + (command.trips - 1) * cycle
        returned = last + cycle
        if (not math.isfinite(returned) or not returned > last or first < command.elapsed_s or
                (response > 0 and not first > command.elapsed_s)):
            raise CommitmentError("timing_unavailable", "Accepted dispatch calendar cannot be represented faithfully.")
        previous = -math.inf
        for k in range(command.trips):
            at = first + k * cycle
            if not math.isfinite(at) or not at > previous:
                raise CommitmentError("timing_unavailable", "Pickup times do not advance strictly.")
            pickups.append(Pickup(index, f.id, f.system_id, command.truck, at, model.truck_capacity_bbl, model.cost_per_trip))
            previous = at
        free[key] = returned
    trucks = tuple(Reservation(s.id, truck, free[s.id, truck] > history.cutoff_elapsed_s, free[s.id, truck])
                   for s in model.systems for truck in range(s.fleet_size))
    pickups = tuple(sorted(pickups, key=lambda p: (p.arrival_elapsed_s, p.system_id, p.truck, p.command_index)))
    observed = view.evidence_fingerprint()
    control_identity = history.fingerprint()
    identity = _hash((model_identity, observed, control_identity, history.cutoff_elapsed_s))
    return Commitments(METHOD, model_identity, observed, control_identity, identity, history.start_unix,
                       history.cutoff_elapsed_s, view.cutoff_unix, trucks, pickups, (
        "Caller must authenticate a complete accepted-control prefix from an initially free fleet; these types grant no operating authority.",
        "Scheduled pickups are known commitments, not proof of actual pickup inventory, execution, charged cost or future feasibility.",
        "Only final return releases a truck; repair neither cancels a job nor makes a busy truck available.",
        "Observation freshness/health and future forecasts require separate causal analysis; a calendar alone cannot establish them.",))
