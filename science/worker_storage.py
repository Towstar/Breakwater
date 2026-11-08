"""Strict project-v1 to standalone float64 storage mapping for worker protocol v1.

Importing this module uses only the standard library. Runtime packages are loaded
after the complete request, draft network and selected controls have been checked.
Native C++ remains authoritative for volume feasibility and stored-run provenance.
"""
from __future__ import annotations

import copy
from dataclasses import dataclass
import math
import re

from storage_grid import StorageGridError, storage_sample_times


class StorageError(Exception):
    """A bounded protocol error; deliberately contains no environment diagnostics."""

    def __init__(self, code: str, message: str):
        self.code = code
        self.message = message
        super().__init__(message)


def _fail(code, message):
    raise StorageError(code, message)


def _object(value, fields, required=None, *, code="invalid_request"):
    if not isinstance(value, dict) or set(value) - set(fields) or not set(required or fields) <= set(value):
        _fail(code, "Object fields do not match the documented storage contract.")
    return value


def _text(value, limit=256, *, empty=False, code="invalid_request", native=False):
    if not isinstance(value, str) or (not value and not empty):
        _fail(code, "Expected a bounded UTF-8 string.")
    try:
        value.encode("utf-8")
        length = len(value.encode("utf-16-le")) // 2 if native else len(value)
    except UnicodeError:
        _fail(code, "String must contain valid Unicode scalar values.")
    if length > limit:
        _fail(code, "String exceeds the documented identifier or label limit.")
    return value


def _number(value, *, code="invalid_parameters", nonnegative=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        _fail(code, "Expected a finite numeric value.")
    try:
        number = float(value)
    except (OverflowError, ValueError):
        _fail(code, "Numeric value cannot be represented as float64.")
    if not math.isfinite(number) or (nonnegative and number < 0):
        _fail(code, "Numeric value must be finite and have the documented sign.")
    return number


def _integer(value, minimum, maximum, *, code="invalid_parameters"):
    number = _number(value, code=code)
    if number != math.floor(number) or not minimum <= number <= maximum:
        _fail(code, "Integer value lies outside the documented range.")
    return int(number)


def _identity(value):
    required = {"snapshot_id", "volume_run_id", "numerical_revision", "physical_hash"}
    _object(value, required | {"route_id", "parameter_hash"}, required)
    for key in ("snapshot_id", "volume_run_id", "route_id"):
        if key in value:
            _text(value[key])
    _integer(value["numerical_revision"], 0, 2**53 - 1, code="invalid_request")
    for key in ("physical_hash", "parameter_hash"):
        if key in value and (not isinstance(value[key], str) or
                             re.fullmatch(r"[0-9a-f]{64}", value[key]) is None):
            _fail("invalid_request", "Identity hashes must be 64 lower-case hexadecimal characters.")
    return copy.deepcopy(value)


@dataclass(frozen=True)
class _Control:
    value: float
    maximum: float
    feed: tuple[float, float] | None = None  # production, feed capacity


def _network(value):
    _object(value, {"assets", "routes"}, code="invalid_network")
    for key, limit in (("assets", 128), ("routes", 256)):
        if not isinstance(value[key], list):
            _fail("invalid_network", "Network assets and routes must be arrays.")
        if len(value[key]) > limit:
            _fail("work_limit", "Network exceeds the native asset or route limit.")
    assets, routes, by_id, ids = [], [], {}, set()
    for item in value["assets"]:
        if not isinstance(item, dict) or not {"id", "label", "kind"} <= set(item):
            _fail("invalid_network", "Every asset requires an id, label and kind.")
        identifier = _text(item["id"], code="invalid_network", native=True)
        _text(item["label"], empty=True, code="invalid_network", native=True)
        kind = item["kind"]
        if not isinstance(kind, str) or kind not in ("tank", "source", "treatment", "trucking"):
            _fail("invalid_network", "Unknown asset kind.")
        if identifier in ids:
            _fail("invalid_network", "Asset and route IDs must be globally unique.")
        ids.add(identifier)
        # Match ProjectDocument's optional numeric defaults and explicit unit
        # translation. A destination's private capacity_m3 field is not a wire alias.
        asset = {"id": identifier, "kind": kind}
        capacity_key = "capacity_m3" if kind == "tank" else "capacity_m3h"
        asset["capacity_m3"] = _number(item.get(capacity_key, 0.0), code="invalid_network", nonnegative=True)
        for key in ("initial_m3", "inflow_m3h", "curtailment_m3h", "curtailment_max_m3h", "cost_per_m3"):
            asset[key] = _number(item.get(key, 0.0), code="invalid_network", nonnegative=True)
        if kind == "tank" and (asset["capacity_m3"] <= 0 or asset["initial_m3"] > asset["capacity_m3"]):
            _fail("invalid_network", "Tank initial inventory must lie within its positive capacity.")
        if kind == "source" and not asset["curtailment_m3h"] <= asset["curtailment_max_m3h"] <= asset["inflow_m3h"]:
            _fail("invalid_network", "Source curtailment must lie within production and its configured maximum.")
        if kind in ("treatment", "trucking") and asset["capacity_m3"] <= 0:
            _fail("invalid_network", "Destination requires a positive capacity_m3h.")
        assets.append(asset)
        by_id[identifier] = asset
    if not any(asset["kind"] == "tank" for asset in assets):
        _fail("invalid_network", "A storage network requires at least one tank.")
    feeds = {asset["id"]: [] for asset in assets if asset["kind"] == "source"}
    route_fields = {"id", "source_id", "target_id", "capacity_m3h", "setpoint_m3h", "has_pump", "available", "cost_per_m3"}
    for item in value["routes"]:
        if not isinstance(item, dict) or not route_fields <= set(item):
            _fail("invalid_network", "Every route requires all documented wire fields.")
        route = {key: _text(item[key], code="invalid_network", native=True)
                 for key in ("id", "source_id", "target_id")}
        if route["id"] in ids:
            _fail("invalid_network", "Asset and route IDs must be globally unique.")
        ids.add(route["id"])
        for key in ("capacity_m3h", "setpoint_m3h", "cost_per_m3"):
            route[key] = _number(item[key], code="invalid_network", nonnegative=True)
        for key in ("has_pump", "available"):
            if not isinstance(item[key], bool):
                _fail("invalid_network", "Pump and availability fields must be booleans.")
            route[key] = item[key]
        source, target = by_id.get(route["source_id"]), by_id.get(route["target_id"])
        if source is None or target is None or source["id"] == target["id"]:
            _fail("invalid_network", "Route endpoints must exist and differ.")
        if not ((source["kind"] == "source" and target["kind"] == "tank") or
                (source["kind"] == "tank" and target["kind"] in ("tank", "treatment", "trucking"))):
            _fail("invalid_network", "Unsupported route direction.")
        if source["kind"] == "tank" and target["kind"] != "tank" and not math.isfinite(
                route["cost_per_m3"] + target["cost_per_m3"]):
            _fail("invalid_network", "Combined route/destination control price must be finite.")
        if source["kind"] == "source":
            feeds[source["id"]].append(route)
            if route["has_pump"] or not route["available"]:
                _fail("invalid_network", "Production feed routes cannot have pumps or be unavailable.")
            flow = source["inflow_m3h"] - source["curtailment_m3h"]
        else:
            flow = route["setpoint_m3h"]
        if not route["available"] and (not route["has_pump"] or route["setpoint_m3h"] != 0):
            _fail("invalid_network", "An unavailable pump must request zero flow.")
        if flow > route["capacity_m3h"] + 1e-9:
            _fail("invalid_network", "Captured route flow exceeds its capacity.")
        routes.append(route)
    if any(len(items) != 1 for items in feeds.values()):
        _fail("invalid_network", "Each production source must feed exactly one tank.")
    catalog = {}
    for asset in assets:
        if asset["kind"] == "source":
            catalog[f"source:{asset['id']}:curtail"] = _Control(asset["curtailment_m3h"], asset["curtailment_max_m3h"],
                (asset["inflow_m3h"], feeds[asset["id"]][0]["capacity_m3h"]))
    for route in routes:
        if by_id[route["source_id"]]["kind"] == "tank":
            catalog[f"route:{route['id']}:flow"] = _Control(route["setpoint_m3h"],
                route["capacity_m3h"] if route["available"] else 0.0)
    return {"assets": assets, "routes": routes}, catalog


def _validate(payload):
    _object(payload, {"method_version", "identity", "network", "controls", "horizon_h", "step_h", "derivatives", "preview"})
    if isinstance(payload["method_version"], bool) or not isinstance(payload["method_version"], (int, float)):
        _fail("invalid_request", "method_version must be numeric version 1.")
    if payload["method_version"] != 1:
        _fail("unsupported_version", "Unsupported storage method version.")
    identity = _identity(payload["identity"])
    horizon, step = _number(payload["horizon_h"]), _number(payload["step_h"])
    try:
        storage_sample_times(horizon, step)
    except StorageGridError as error:
        _fail(error.code, str(error))
    if not isinstance(payload["derivatives"], bool):
        _fail("invalid_request", "derivatives must be boolean.")
    _object(payload["preview"], {"max_times"}, code="invalid_parameters")
    preview = _integer(payload["preview"]["max_times"], 2, 256)
    network, catalog = _network(payload["network"])
    controls = payload["controls"]
    if not isinstance(controls, list):
        _fail("invalid_controls", "Controls must be an array of selected overrides.")
    if len(controls) > 384:
        _fail("work_limit", "Storage requests support at most 384 selected controls.")
    overrides = {}
    for item in controls:
        _object(item, {"control_id", "value"}, code="invalid_controls")
        identifier = _text(item["control_id"], 512, code="invalid_controls")
        value = _number(item["value"], code="invalid_controls", nonnegative=True)
        if identifier in overrides or identifier not in catalog:
            _fail("invalid_controls", "Selected control IDs must be unique catalog entries.")
        descriptor = catalog[identifier]
        if not 0 <= value <= descriptor.maximum:
            _fail("invalid_controls", "Selected control lies outside its current availability or bounds.")
        if descriptor.feed and descriptor.feed[0] - value > descriptor.feed[1] + 1e-9:
            _fail("invalid_controls", "Selected curtailment exceeds the connected feed capacity.")
        if payload["derivatives"] and (not 0 < value < descriptor.maximum or
                (descriptor.feed and descriptor.feed[0] - value >= descriptor.feed[1])):
            _fail("invalid_controls", "Derivatives require selected controls strictly inside their admissible two-sided domain.")
        overrides[identifier] = value
    return identity, network, overrides, horizon, step, preview


def _load_backend():
    try:
        import torch
        from breakwater_science.rollout import METHOD_VERSION, StorageModel
    except (ImportError, OSError, RuntimeError) as error:
        raise StorageError("runtime_unavailable", "The CPU float64 PyTorch storage runtime could not be loaded.") from error
    return torch, StorageModel, METHOD_VERSION


def storage_rollout(payload):
    """Execute the published storage method; never returns a feasibility verdict."""
    identity, network, overrides, horizon, step, preview = _validate(payload)
    torch, StorageModel, version = _load_backend()
    try:
        model = StorageModel(network)
    except ValueError as error:
        raise StorageError("invalid_network", "The normalized network failed scientific-model validation.") from error
    except MemoryError as error:
        raise StorageError("work_limit", "Storage preparation exceeded available bounded runtime resources.") from error
    except (OSError, RuntimeError) as error:
        raise StorageError("runtime_unavailable", "The CPU float64 storage model could not be initialized.") from error
    try:
        # The requested full time grid is evaluated without retaining an autograd
        # graph. Its preview remains sampled from actual repeated Euler outputs.
        with torch.no_grad():
            run = model.rollout(overrides, horizon_h=horizon, step_h=step)
        if not math.isfinite(run.cost.item()) or run.cost.item() < 0:
            _fail("numerical_failure", "Storage cost must be a finite nonnegative float64 value.")
        samples = run.time_h.numel()
        count = min(samples, preview)
        indices = [i * (samples - 1) // (count - 1) for i in range(count)]
        result = {"method": "storage_rollout", "method_version": 1, "identity": identity,
            "provenance": {"implementation": "torch_storage_float64",
                "implementation_version": version + "+worker-v1", "dtype": "float64",
                "device": "cpu", "execution_status": "completed"},
            "time_h": run.time_h[indices].tolist(), "time_indices": indices,
            "tank_ids": list(run.tank_ids), "volumes_m3": run.volumes_m3[:, indices].tolist(),
            "cost": run.cost.item(), "warnings": [
                "C++ remains authoritative for volume feasibility; this calculation supplies no feasible verdict.",
                "Identity is echoed caller provenance, not verification that a native run exists or was approved."]}
        if payload["derivatives"]:
            identifiers = list(overrides)
            if identifiers:
                values = torch.tensor(list(overrides.values()), dtype=torch.float64, device="cpu", requires_grad=True)

                def terminal_and_cost(selected):
                    # Constant-rate closed form uses one interval; do not build a
                    # 10,000-step backward graph for a time-independent Jacobian.
                    candidate = model.rollout(dict(zip(identifiers, selected.unbind())),
                        horizon_h=horizon, step_h=horizon)
                    return torch.cat((candidate.volumes_m3[:, -1], candidate.cost.reshape(1)))

                jacobian = torch.autograd.functional.jacobian(terminal_and_cost, values)
                if not bool(torch.isfinite(jacobian).all()):
                    _fail("numerical_failure", "Storage derivatives are not finite float64 values.")
                terminal = jacobian[:-1].tolist()
                cost_gradient = jacobian[-1].tolist()
            else:
                terminal, cost_gradient = [[] for _ in run.tank_ids], []
            result["derivatives"] = {"control_ids": identifiers,
                "terminal_volume_jacobian_h": terminal, "cost_gradient": cost_gradient}
            result["warnings"].append("Derivatives cover only selected continuous interior controls, not bounds rejection, discrete search or approval.")
        return result
    except MemoryError as error:
        raise StorageError("work_limit", "Storage calculation exceeded available bounded runtime resources.") from error
    except (ValueError, RuntimeError, OverflowError) as error:
        raise StorageError("numerical_failure", "Storage calculation could not produce finite float64 outputs.") from error
