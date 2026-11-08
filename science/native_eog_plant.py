"""Private checked ctypes boundary to the persistent native synthetic plant.

No Python physics fallback or public protocol. Inputs are copied by the native
plant; returned memory is copied under a per-handle lock before another call.
Native snapshots expose current state only, not future forcing/incident schedules.
"""
from __future__ import annotations

import ctypes as C
import math
import os
from pathlib import Path
from threading import RLock

from native_eog import NativeError, _array, _number, _shape, _struct

U, D = C.c_uint32, C.c_double
Well = _struct("PlantWell", [(x, D) for x in ("water_bpd", "oil_bpd", "gas_mcfd")] + [("productive", U)])
Facility = _struct("PlantFacility", [(x, U) for x in ("system", "first_well", "well_count", "initially_running")] +
    [(x, D) for x in ("capacity_bbl", "initial_bbl", "start_bbl", "stop_bbl", "shutdown_bbl", "pump_bpd", "pipeline_capacity_bpd", "response_min", "cycle_min")])
Failure = _struct("PlantFailure", [("facility", U), ("start_s", D), ("repair_s", D)])
RateChange = _struct("PlantRateChange", [("well", U)] + [(x, D) for x in ("elapsed_s", "water_bpd", "oil_bpd", "gas_mcfd")])
SCALARS = ("duration_s", "oil_per_bbl", "gas_per_mcf", "truck_capacity_bbl", "cost_per_trip")
BOUNDS = ("point_limit", "event_limit", "trip_limit")
Scenario = _struct("PlantScenario", [("abi_version", U), ("struct_size", U)] + [(x, D) for x in SCALARS] +
    [("fleet_sizes", C.POINTER(U)), ("facilities", C.POINTER(Facility)), ("wells", C.POINTER(Well)),
     ("failures", C.POINTER(Failure)), ("rate_changes", C.POINTER(RateChange))] +
    [(x, U) for x in ("fleet_count", "facility_count", "well_count", "failure_count", "rate_change_count", *BOUNDS)])
FacilityView = _struct("PlantFacilityView", [(x, D) for x in ("volume_bbl", "inflow_bpd", "pump_flow_bpd")] +
    [("pump_status", U), ("automatic_shut_in", U)] + [(x, D) for x in ("produced_bbl", "pumped_bbl", "trucked_bbl", "lost_revenue", "balance_residual_bbl", "event_boundary_rounding_bbl")])
WellView = _struct("PlantWellView", [("producing", U)] + [(x, D) for x in ("water_bpd", "oil_bpd", "gas_mcfd", "lost_revenue")])
TruckView = _struct("PlantTruckView", [(x, U) for x in ("system", "truck", "busy")] + [("available_again_s", D)])
Snapshot = _struct("PlantSnapshot", [(x, U) for x in ("abi_version", "struct_size", "status", "has_cost", "completed_full_loads", "facility_count", "well_count", "truck_count", "trace_count")] +
    [(x, D) for x in ("elapsed_s", "truck_cost", "lost_revenue", "total_cost")] + [("diagnostic", C.c_char_p),
     ("facilities", C.POINTER(FacilityView)), ("wells", C.POINTER(WellView)), ("trucks", C.POINTER(TruckView))])
Point = _struct("PlantPoint", [("elapsed_s", D), ("volume_bbl", D)] + [(x, U) for x in ("facility", "kind", "pump_status", "automatic_shut_in")])
Action = _struct("PlantAction", [("accepted", U), ("diagnostic", C.c_char_p)])
LAYOUTS = (Well, Facility, Failure, RateChange, Scenario, FacilityView, WellView, TruckView, Snapshot, Point, Action)
CancelFn = C.CFUNCTYPE(C.c_int, C.c_void_p)
STATES = ("running", "completed", "invalid_action", "unavailable", "cancelled", "limit")


def _copy(item, cls):
    result = {key: getattr(item, key) for key, _ in cls._fields_}
    if any(not math.isfinite(value) for key, typ in cls._fields_ if typ is D for value in (result[key],)):
        raise NativeError("native_failure", "Native plant returned nonfinite current-state evidence.")
    return result


class NativePlant:
    def __init__(self, library, scenario):
        self._handle = C.c_void_p()
        self._lock = RLock()
        self._busy = False
        self._dll_directory = None
        path = Path(library)
        if not path.is_absolute() or not path.is_file():
            raise NativeError("native_unavailable", "An existing absolute plant library path is required.")
        self._dll_directory = os.add_dll_directory(str(path.parent)) if os.name == "nt" else None
        try:
            self._lib = C.CDLL(str(path))
            lib = self._lib
            lib.bwes_abi_version.argtypes = []; lib.bwes_abi_version.restype = U
            lib.bwes_layout_size.argtypes = [U]; lib.bwes_layout_size.restype = U
            lib.bwes_method.argtypes = []; lib.bwes_method.restype = C.c_char_p
            lib.bwes_last_error.argtypes = []; lib.bwes_last_error.restype = C.c_char_p
            lib.bwes_create.argtypes = [C.POINTER(Scenario), C.POINTER(C.c_void_p)]; lib.bwes_create.restype = C.c_int
            lib.bwes_advance.argtypes = [C.c_void_p, D, CancelFn, C.c_void_p, C.POINTER(U)]; lib.bwes_advance.restype = C.c_int
            lib.bwes_get_snapshot.argtypes = [C.c_void_p, C.POINTER(Snapshot)]; lib.bwes_get_snapshot.restype = C.c_int
            lib.bwes_dispatch.argtypes = [C.c_void_p, U, U, U, C.POINTER(Action)]; lib.bwes_dispatch.restype = C.c_int
            lib.bwes_shut_in.argtypes = [C.c_void_p, U, C.POINTER(Action)]; lib.bwes_shut_in.restype = C.c_int
            lib.bwes_copy_trace.argtypes = [C.c_void_p, U, C.POINTER(Point), U, C.POINTER(U)]; lib.bwes_copy_trace.restype = C.c_int
            lib.bwes_reset.argtypes = [C.c_void_p]; lib.bwes_reset.restype = C.c_int
            lib.bwes_destroy.argtypes = [C.c_void_p]; lib.bwes_destroy.restype = None
            if lib.bwes_abi_version() != 1 or any(lib.bwes_layout_size(i) != C.sizeof(cls) for i, cls in enumerate(LAYOUTS, 1)):
                raise NativeError("abi_mismatch", "Plant ABI/layout differs from the bridge.")
            self.method = lib.bwes_method().decode("ascii", "strict")
            if self.method != "eog-synthetic-events-0.1.0":
                raise NativeError("method_mismatch", "Plant numerical method is not supported by this bridge.")
            _shape(scenario, (*SCALARS, "fleet_sizes", "facilities", "wells", "failures", "rate_changes", *BOUNDS))
            fleet = scenario["fleet_sizes"]
            if type(fleet) not in (tuple, list) or len(fleet) > 64:
                raise NativeError("limit_exceeded", "Plant fleet array exceeds its bound.")
            fleets = (U * len(fleet))(*(_number(v, True) for v in fleet))
            facilities = _array(Facility, scenario["facilities"], 256)
            wells = _array(Well, scenario["wells"], 16384)
            failures = _array(Failure, scenario["failures"], 16384)
            changes = _array(RateChange, scenario["rate_changes"], 16384)
            native = Scenario(1, C.sizeof(Scenario), *(_number(scenario[k]) for k in SCALARS),
                fleets, facilities, wells, failures, changes, len(fleets), len(facilities), len(wells), len(failures), len(changes),
                *(_number(scenario[k], True) for k in BOUNDS))
            self._check(lib.bwes_create(C.byref(native), C.byref(self._handle)))
        except (OSError, AttributeError) as error:
            self.close()
            raise NativeError("native_unavailable" if isinstance(error, OSError) else "abi_mismatch", "Native plant library could not be loaded/verified.") from error
        except BaseException:
            self.close(); raise

    def _check(self, code):
        if code:
            raise NativeError("native_input" if code == 1 else "native_unavailable", self._lib.bwes_last_error().decode("utf-8", "replace"))

    def _open(self):
        if self._busy:
            raise NativeError("native_reentry", "Cancellation callbacks cannot reenter the live plant.")
        if not self._handle.value:
            raise NativeError("native_closed", "Native plant handle is closed.")

    def _snapshot(self):
        self._open()
        view = Snapshot(abi_version=1, struct_size=C.sizeof(Snapshot))
        self._check(self._lib.bwes_get_snapshot(self._handle, C.byref(view)))
        if (view.status >= len(STATES) or view.has_cost != int(view.status in (0, 1)) or view.facility_count > 256 or
                view.well_count > 16384 or view.truck_count > 8192 or view.trace_count > 262144):
            raise NativeError("native_failure", "Native plant snapshot violates its boundary.")
        arrays = {}
        for name, cls, count in (("facilities", FacilityView, view.facility_count), ("wells", WellView, view.well_count), ("trucks", TruckView, view.truck_count)):
            pointer = getattr(view, name)
            if count and not pointer:
                raise NativeError("native_failure", "Native plant snapshot has a null populated array.")
            arrays[name] = [_copy(pointer[i], cls) for i in range(count)]
        cost = {"truck_cost": view.truck_cost, "lost_revenue": view.lost_revenue, "total": view.total_cost} if view.has_cost else None
        if not math.isfinite(view.elapsed_s) or cost is not None and any(not math.isfinite(v) for v in cost.values()):
            raise NativeError("native_failure", "Native plant returned unrepresentable time/cost.")
        return {"method": self.method, "elapsed_s": view.elapsed_s, "status": STATES[view.status],
                "diagnostic": (view.diagnostic or b"").decode("utf-8", "replace"), "cost": cost,
                "completed_full_loads": view.completed_full_loads, "trace_count": view.trace_count, **arrays}

    def snapshot(self):
        with self._lock: return self._snapshot()

    def advance(self, elapsed_s, cancelled=None):
        if cancelled is not None and not callable(cancelled):
            raise NativeError("invalid_callback", "Cancellation must be callable.")
        with self._lock:
            self._open(); callback_errors = []
            def cancel(_):
                try: return int(bool(cancelled()))
                except BaseException as error: callback_errors.append(error); return 1
            callback = CancelFn(cancel) if cancelled is not None else CancelFn()
            advanced = U()
            requested = _number(elapsed_s)
            self._busy = True
            try:
                self._check(self._lib.bwes_advance(self._handle, requested, callback, None, C.byref(advanced)))
            finally:
                self._busy = False
            if callback_errors:
                raise NativeError("callback_failed", "Plant cancelled after the cancellation callback failed.") from callback_errors[0]
            return {**self._snapshot(), "advance_succeeded": bool(advanced.value)}

    def _action(self, name, values):
        with self._lock:
            self._open(); out = Action()
            self._check(getattr(self._lib, name)(self._handle, *(_number(v, True) for v in values), C.byref(out)))
            if out.accepted not in (0, 1):
                raise NativeError("native_failure", "Native action returned an unsupported acceptance flag.")
            return {"accepted": bool(out.accepted), "diagnostic": (out.diagnostic or b"").decode("utf-8", "replace")}

    def dispatch(self, facility, truck, trips): return self._action("bwes_dispatch", (facility, truck, trips))
    def shut_in(self, well): return self._action("bwes_shut_in", (well,))

    def trace(self, offset=0, limit=4096):
        if type(limit) is not int or not 1 <= limit <= 262144:
            raise NativeError("limit_exceeded", "Trace copy limit must be in [1,262144].")
        with self._lock:
            self._open(); out = (Point * limit)(); written = U()
            self._check(self._lib.bwes_copy_trace(self._handle, _number(offset, True), out, limit, C.byref(written)))
            if written.value > limit: raise NativeError("native_failure", "Native trace exceeded the requested copy bound.")
            return [_copy(out[i], Point) for i in range(written.value)]

    def reset(self):
        with self._lock:
            self._open(); self._check(self._lib.bwes_reset(self._handle)); return self._snapshot()

    def close(self):
        with self._lock:
            if self._busy:
                raise NativeError("native_reentry", "Cancellation callbacks cannot close the live plant.")
            if self._handle.value:
                self._lib.bwes_destroy(self._handle); self._handle = C.c_void_p()
            if self._dll_directory is not None:
                self._dll_directory.close(); self._dll_directory = None

    def __enter__(self): return self
    def __exit__(self, *_): self.close()
    def __del__(self):
        if hasattr(self, "_lock"): self.close()
