"""Private additive committed-snapshot ABI; no public worker/wire operation.

Initial inventories and eligibility are caller-supplied post-decision-event facts.
This layer does not infer them from simulator truth or grant operating authority.
The copied inputs and results are bound by exact canonical input fingerprints.
"""
import ctypes as C
import hashlib
import json

from native_eog import (NativeError, NativeEvaluator, U, D, Snapshot, Plan, Fleet, Well, Facility,
                        Horizon, Stop, Job, Trip, _struct, _shape, _number, _array)

METHOD = "eog-native-committed-seconds-0.1.0"
Occupied = _struct("Occupied", [("system", U), ("truck", U), ("available_s", D)])
Pending = _struct("Pending", [("facility", U), ("system", U), ("truck", U), ("arrival_s", D)])
Commitments = _struct("Commitments", [("abi_version", U), ("struct_size", U),
    ("occupied", C.POINTER(Occupied)), ("pickups", C.POINTER(Pending)), ("stopped_wells", C.POINTER(U)),
    ("occupied_count", U), ("pickup_count", U), ("stopped_count", U)])


def fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False, allow_nan=False,
                                    separators=(",", ":")).encode()).hexdigest()


def _preflight(snapshot, plan, commitments):
    _shape(snapshot, ("oil_per_bbl", "gas_per_mcf", "truck_capacity_bbl", "cost_per_trip", "fleets", "facilities", "wells"))
    _shape(plan, ("horizons", "stops", "jobs", "trips"))
    _shape(commitments, ("occupied", "pickups", "stopped_wells"))
    s = {key: _number(snapshot[key]) for key in ("oil_per_bbl", "gas_per_mcf", "truck_capacity_bbl", "cost_per_trip")}
    p, c = {}, {}
    for target, source, key, kind, maximum in ((s, snapshot, "fleets", Fleet, 64), (s, snapshot, "facilities", Facility, 256),
            (s, snapshot, "wells", Well, 32768), (p, plan, "horizons", Horizon, 256), (p, plan, "stops", Stop, 256),
            (p, plan, "jobs", Job, 256), (p, plan, "trips", Trip, 16384),
            (c, commitments, "occupied", Occupied, 16384), (c, commitments, "pickups", Pending, 16384)):
        copied = _array(kind, source[key], maximum)
        target[key] = [{name: getattr(row, name) for name, _ in kind._fields_} for row in copied]
    stopped = commitments["stopped_wells"]
    if type(stopped) not in (tuple, list) or len(stopped) > 32768:
        raise NativeError("limit_exceeded", "Committed well-stop list exceeds its bound.")
    c["stopped_wells"] = [_number(w, integer=True) for w in stopped]
    return s, p, c


class NativeCommittedEvaluator(NativeEvaluator):
    def __init__(self, library):
        super().__init__(library)
        try:
            lib = self._lib
            lib.bweog_committed_method.argtypes = []; lib.bweog_committed_method.restype = C.c_char_p
            lib.bweog_committed_layout_size.argtypes = [U]; lib.bweog_committed_layout_size.restype = U
            lib.bweog_evaluate_committed.argtypes = [C.POINTER(Snapshot), C.POINTER(Plan), C.POINTER(Commitments), C.POINTER(C.c_void_p)]
            lib.bweog_evaluate_committed.restype = C.c_int
            if (lib.bweog_committed_method().decode("ascii") != METHOD or
                    any(lib.bweog_committed_layout_size(i) != C.sizeof(t)
                        for i, t in enumerate((Occupied, Pending, Commitments), 1))):
                raise NativeError("abi_mismatch", "Committed native method/layout differs from the bridge.")
        except AttributeError as error:
            raise NativeError("committed_unavailable", "This DLL has no committed-state extension; no free-fleet fallback.") from error
        self.method = METHOD

    def evaluate(self, snapshot, plan, commitments):
        snapshot, plan, commitments = _preflight(snapshot, plan, commitments)
        occupied = _array(Occupied, commitments["occupied"], 16384)
        pickups = _array(Pending, commitments["pickups"], 16384)
        stopped = (U * len(commitments["stopped_wells"]))(*commitments["stopped_wells"])
        context = Commitments(1, C.sizeof(Commitments), occupied, pickups, stopped,
                              len(occupied), len(pickups), len(stopped))
        def invoke(s, p, handle):
            return self._lib.bweog_evaluate_committed(C.byref(s), C.byref(p), C.byref(context), C.byref(handle))
        result = self._evaluate(snapshot, plan, invoke)
        if result["status"] != "available":
            result["automatic_shutdown"] = None  # partial native traces do not establish a complete outcome
        result["bindings"] = {"snapshot": fingerprint(snapshot), "plan": fingerprint(plan), "commitments": fingerprint(commitments)}
        return result
