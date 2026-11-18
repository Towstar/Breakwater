"""Private ctypes bridge to the independent native EOG response evaluator.

Flat indices refer to the caller's compiled causal model. This is not the public
EOG request protocol. An explicit absolute library path is required; there is no
Python substitute on load/evaluation failure. All returned memory is copied before
the native result is released. Invalid/unavailable costs become None, never zero.
"""
from __future__ import annotations

import ctypes as C
import math
import os
from pathlib import Path

U, D = C.c_uint32, C.c_double


class NativeError(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


def _struct(name, fields):
    return type(name, (C.Structure,), {"_fields_": fields})


Fleet = _struct("Fleet", [("trucks", U)])
Well = _struct("Well", [(x, D) for x in ("water_bpd", "oil_bpd", "gas_mcfd")] + [("productive", U)])
Facility = _struct("Facility", [(x, U) for x in ("system", "first_well", "well_count", "already_shut_in")] +
                   [(x, D) for x in ("capacity_bbl", "initial_bbl", "shutdown_bbl", "repair_s", "response_min", "cycle_min")])
Snapshot = _struct("Snapshot", [("abi_version", U), ("struct_size", U)] +
                   [(x, D) for x in ("oil_per_bbl", "gas_per_mcf", "truck_capacity_bbl", "cost_per_trip")] +
                   [("fleets", C.POINTER(Fleet)), ("facilities", C.POINTER(Facility)), ("wells", C.POINTER(Well))] +
                   [(x, U) for x in ("fleet_count", "facility_count", "well_count")])
Horizon = _struct("Horizon", [("facility", U), ("repair_s", D)])
Stop = _struct("Stop", [("facility", U), ("well", U), ("start_s", D), ("end_s", D)])
Trip = _struct("Trip", [(x, D) for x in ("arrival_s", "volume_bbl", "cost")])
Job = _struct("Job", [(x, U) for x in ("facility", "system", "truck", "first_trip", "trip_count")] +
              [(x, D) for x in ("called_s", "response_min", "cycle_min", "available_again_s")])
Plan = _struct("Plan", [("abi_version", U), ("struct_size", U), ("horizons", C.POINTER(Horizon)),
               ("stops", C.POINTER(Stop)), ("jobs", C.POINTER(Job)), ("trips", C.POINTER(Trip))] +
               [(x, U) for x in ("horizon_count", "stop_count", "job_count", "trip_count")])
Violation = _struct("Violation", [(x, U) for x in ("code", "facility", "action", "has_time")] + [("elapsed_s", D)])
Point = _struct("Point", [("elapsed_s", D), ("volume_bbl", D), ("kind", U)])
FacilityResult = _struct("FacilityResult", [(x, U) for x in ("facility", "first_point", "point_count", "shut_in")] +
                         [(x, D) for x in ("shutdown_s", "lost_revenue", "produced_bbl", "trucked_bbl", "final_bbl", "balance_residual_bbl")])
ResultView = _struct("ResultView", [("status", U), ("automatic_shutdown", U)] +
                     [(x, D) for x in ("truck_cost", "lost_revenue", "total_cost")] +
                     [("facilities", C.POINTER(FacilityResult)), ("points", C.POINTER(Point)),
                      ("violations", C.POINTER(Violation))] +
                     [(x, U) for x in ("facility_count", "point_count", "violation_count")])
LAYOUTS = (Fleet, Well, Facility, Snapshot, Horizon, Stop, Trip, Job, Plan, Violation, Point, FacilityResult, ResultView)
VIOLATIONS = {1: "repair_horizons", 2: "well_shut_in", 3: "truck_job", 4: "truck_trip",
              5: "fleet_overlap", 6: "insufficient_inventory", 7: "numerical_unavailable", 8: "limit_exceeded"}


def _shape(value, keys):
    if type(value) is not dict or set(value) != set(keys):
        raise NativeError("invalid_shape", "Private computational input has missing or unknown fields.")


def _number(value, integer=False):
    if integer:
        if type(value) is not int or not 0 <= value <= 0xffffffff:
            raise NativeError("invalid_index", "Index/count must be an unsigned 32-bit integer.")
        return value
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise NativeError("invalid_value", "Expected finite binary64 input.")
    try:
        result = float(value)
    except (OverflowError, ValueError) as error:
        raise NativeError("invalid_value", "Expected finite binary64 input.") from error
    if not math.isfinite(result):
        raise NativeError("invalid_value", "Expected finite binary64 input.")
    return result


def _record(cls, item):
    _shape(item, [key for key, _ in cls._fields_])
    return cls(*(_number(item[key], typ is U) for key, typ in cls._fields_))


def _array(cls, values, maximum):
    if type(values) not in (tuple, list) or len(values) > maximum:
        raise NativeError("limit_exceeded", "Private computational array exceeds its bound.")
    return (cls * len(values))(*(_record(cls, item) for item in values))


class NativeEvaluator:
    def __init__(self, library):
        path = Path(library)
        if not path.is_absolute() or not path.is_file():
            raise NativeError("native_unavailable", "An existing absolute native library path is required.")
        self._dll_directory = os.add_dll_directory(str(path.parent)) if os.name == "nt" else None
        try:
            self._lib = C.CDLL(str(path))
        except OSError as error:
            if self._dll_directory is not None:
                self._dll_directory.close()
            raise NativeError("native_unavailable", "Native EOG library could not be loaded.") from error
        lib = self._lib
        try:
            lib.bweog_abi_version.argtypes = []; lib.bweog_abi_version.restype = U
            lib.bweog_layout_size.argtypes = [U]; lib.bweog_layout_size.restype = U
            lib.bweog_method.argtypes = []; lib.bweog_method.restype = C.c_char_p
            lib.bweog_last_error.argtypes = []; lib.bweog_last_error.restype = C.c_char_p
            lib.bweog_evaluate.argtypes = [C.POINTER(Snapshot), C.POINTER(Plan), C.POINTER(C.c_void_p)]
            lib.bweog_evaluate.restype = C.c_int
            lib.bweog_get_result.argtypes = [C.c_void_p, C.POINTER(ResultView)]
            lib.bweog_get_result.restype = C.c_int
            lib.bweog_destroy.argtypes = [C.c_void_p]; lib.bweog_destroy.restype = None
            if lib.bweog_abi_version() != 1 or any(lib.bweog_layout_size(i) != C.sizeof(cls)
                                                   for i, cls in enumerate(LAYOUTS, 1)):
                raise NativeError("abi_mismatch", "Native EOG ABI/layout does not match the bridge.")
            self.method = lib.bweog_method().decode("ascii", "strict")
        except AttributeError as error:
            raise NativeError("abi_mismatch", "Native EOG exports are missing.") from error

    def evaluate(self, snapshot, plan):
        return self._evaluate(snapshot, plan)

    def _evaluate(self, snapshot, plan, call=None):
        _shape(snapshot, ("oil_per_bbl", "gas_per_mcf", "truck_capacity_bbl", "cost_per_trip", "fleets", "facilities", "wells"))
        _shape(plan, ("horizons", "stops", "jobs", "trips"))
        fleets = _array(Fleet, snapshot["fleets"], 64)
        facilities = _array(Facility, snapshot["facilities"], 256)
        wells = _array(Well, snapshot["wells"], 32768)
        horizons = _array(Horizon, plan["horizons"], 256)
        stops = _array(Stop, plan["stops"], 256)
        jobs = _array(Job, plan["jobs"], 256)
        trips = _array(Trip, plan["trips"], 16384)
        s = Snapshot(1, C.sizeof(Snapshot), *(_number(snapshot[key]) for key in
                     ("oil_per_bbl", "gas_per_mcf", "truck_capacity_bbl", "cost_per_trip")),
                     fleets, facilities, wells, len(fleets), len(facilities), len(wells))
        p = Plan(1, C.sizeof(Plan), horizons, stops, jobs, trips, len(horizons), len(stops), len(jobs), len(trips))
        handle = C.c_void_p()
        code = (self._lib.bweog_evaluate(C.byref(s), C.byref(p), C.byref(handle)) if call is None else
                call(s, p, handle))
        if code:
            raise NativeError("native_input" if code == 1 else "native_failure",
                              self._lib.bweog_last_error().decode("utf-8", "replace"))
        try:
            view = ResultView()
            if self._lib.bweog_get_result(handle, C.byref(view)):
                raise NativeError("native_failure", "Native result could not be read.")
            if (view.status not in (0, 1, 2) or view.facility_count > 256 or
                    view.point_count > 262144 or view.violation_count > 1024):
                raise NativeError("native_failure", "Native result exceeded the computational boundary.")
            copied_facilities = []
            for i in range(view.facility_count):
                f = view.facilities[i]
                if f.first_point + f.point_count > view.point_count:
                    raise NativeError("native_failure", "Native trajectory range is invalid.")
                item = {key: getattr(f, key) for key, _ in FacilityResult._fields_}
                item["shutdown_s"] = f.shutdown_s if f.shut_in else None
                item["points"] = [{key: getattr(view.points[k], key) for key, _ in Point._fields_}
                                  for k in range(f.first_point, f.first_point + f.point_count)]
                copied_facilities.append(item)
            violations = []
            for i in range(view.violation_count):
                v = view.violations[i]
                violations.append({"code": VIOLATIONS.get(v.code, "native_unknown"),
                                   "facility": None if v.facility == 0xffffffff else v.facility,
                                   "action": None if v.action == 0xffffffff else v.action,
                                   "elapsed_s": v.elapsed_s if v.has_time else None})
            return {"method": self.method, "status": ("available", "invalid", "unavailable")[view.status],
                    "automatic_shutdown": bool(view.automatic_shutdown), "facilities": copied_facilities,
                    "violations": violations, "cost": None if view.status else
                    {"truck_cost": view.truck_cost, "lost_revenue": view.lost_revenue, "total": view.total_cost}}
        finally:
            self._lib.bweog_destroy(handle)
