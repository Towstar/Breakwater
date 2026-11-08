"""Differentiable fixed-flow storage, independent of desktop/worker protocols.

Normalized inputs use the native Asset/Route member names, not project-v1 JSON.
The captured topology is immutable. Only explicitly supplied controls vary; invalid
controls raise rather than being clipped. Storage/destination feasibility belongs to
the independent C++ validator, not to autograd or this module.
"""
from __future__ import annotations

from collections.abc import Mapping, Sequence
from dataclasses import dataclass
import math
from numbers import Real

import torch

from storage_grid import storage_sample_times

METHOD_VERSION = "torch-constant-flow-0.1.1"
MAX_SAMPLES = 10_001


def _number(value, name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, Real):
        raise ValueError(f"{name} must be a finite number")
    try:
        value = float(value)
    except (OverflowError, ValueError) as exc:
        raise ValueError(f"{name} must be representable as float64") from exc
    if not math.isfinite(value):
        raise ValueError(f"{name} must be finite")
    return value


def _nonnegative(data: Mapping, name: str) -> float:
    value = _number(data.get(name, 0.0), name)
    if value < 0.0:
        raise ValueError(f"{name} must be nonnegative")
    return value


def _id(data: Mapping, name: str) -> str:
    value = data.get(name)
    if not isinstance(value, str) or not value:
        raise ValueError(f"{name} must be a nonempty string")
    return value


def _boolean(data: Mapping, name: str, default: bool) -> bool:
    value = data.get(name, default)
    if not isinstance(value, bool):
        raise ValueError(f"{name} must be boolean")
    return value


@dataclass(frozen=True)
class _Asset:
    id: str
    kind: str
    capacity: float
    initial: float
    inflow: float
    curtailment: float
    curtailment_max: float
    cost: float


@dataclass(frozen=True)
class _Route:
    id: str
    source_id: str
    target_id: str
    capacity: float
    setpoint: float
    has_pump: bool
    available: bool
    cost: float


@dataclass(frozen=True)
class ControlDescriptor:
    id: str
    value: float
    minimum: float
    maximum: float


@dataclass(frozen=True)
class Rollout:
    tank_ids: tuple[str, ...]
    time_h: torch.Tensor
    volumes_m3: torch.Tensor  # tank-major, sample zero is incident inventory
    cost: torch.Tensor
    rates_m3h: torch.Tensor
    control_values: torch.Tensor
    method_version: str = METHOD_VERSION


class StorageModel:
    """A captured finite network with CPU float64, differentiable controls.

    ``network`` is a private scientific input mapping: ``assets`` and ``routes``
    contain the native C++ member names. No shared application schema is replaced.
    Catalog order is sources in asset order, then tank-origin routes in route order.
    Returned tensors belong to the call; mutating them does not alter later calls.
    """

    def __init__(self, network: Mapping):
        if not isinstance(network, Mapping):
            raise ValueError("network must be a mapping")
        arrays = []
        for name, limit in (("assets", 128), ("routes", 256)):
            items = network.get(name)
            if not isinstance(items, Sequence) or isinstance(items, (str, bytes)):
                raise ValueError(f"{name} must be a sequence")
            if len(items) > limit or any(not isinstance(x, Mapping) for x in items):
                raise ValueError(f"{name} exceeds limits or contains a non-mapping")
            arrays.append(items)
        assets, routes = arrays
        self._assets = tuple(_Asset(
            _id(a, "id"), a.get("kind"), _nonnegative(a, "capacity_m3"),
            _nonnegative(a, "initial_m3"), _nonnegative(a, "inflow_m3h"),
            _nonnegative(a, "curtailment_m3h"), _nonnegative(a, "curtailment_max_m3h"),
            _nonnegative(a, "cost_per_m3")) for a in assets)
        self._routes = tuple(_Route(
            _id(r, "id"), _id(r, "source_id"), _id(r, "target_id"),
            _nonnegative(r, "capacity_m3h"), _nonnegative(r, "setpoint_m3h"),
            _boolean(r, "has_pump", False), _boolean(r, "available", True),
            _nonnegative(r, "cost_per_m3")) for r in routes)
        ids = [a.id for a in self._assets] + [r.id for r in self._routes]
        if len(set(ids)) != len(ids):
            raise ValueError("asset and route IDs must be globally unique")
        self._by_id = {a.id: a for a in self._assets}
        for a in self._assets:
            if a.kind not in ("tank", "source", "treatment", "trucking"):
                raise ValueError(f"Unknown asset kind: {a.kind}")
            if a.kind == "tank" and (a.capacity <= 0 or a.initial > a.capacity):
                raise ValueError(f"Invalid initial storage: {a.id}")
            if a.kind == "source" and not 0 <= a.curtailment <= a.curtailment_max <= a.inflow:
                raise ValueError(f"Invalid curtailment bounds: {a.id}")
            if a.kind in ("treatment", "trucking") and a.capacity <= 0:
                raise ValueError(f"Invalid destination capacity: {a.id}")
        self.tank_ids = tuple(a.id for a in self._assets if a.kind == "tank")
        if not self.tank_ids:
            raise ValueError("At least one storage tank is required")
        outgoing = {a.id: 0 for a in self._assets if a.kind == "source"}
        for r in self._routes:
            s, d = self._by_id.get(r.source_id), self._by_id.get(r.target_id)
            if s is None or d is None:
                raise ValueError(f"Missing endpoint: {r.id}")
            if s.id == d.id or not (
                (s.kind == "source" and d.kind == "tank") or
                (s.kind == "tank" and d.kind in ("tank", "treatment", "trucking"))
            ):
                raise ValueError(f"Unsupported connection: {r.id}")
            if s.kind == "source":
                outgoing[s.id] += 1
                if r.has_pump or not r.available:
                    raise ValueError(f"Invalid source route: {r.id}")
            if not r.available and (not r.has_pump or r.setpoint != 0):
                raise ValueError(f"Unavailable pump must request zero flow: {r.id}")
            q = s.inflow - s.curtailment if s.kind == "source" else r.setpoint
            if q > r.capacity + 1e-9:
                raise ValueError(f"Flow exceeds route capacity: {r.id}")
        if any(count != 1 for count in outgoing.values()):
            raise ValueError("Each source must feed exactly one tank")
        controls = [ControlDescriptor(f"source:{a.id}:curtail", a.curtailment, 0.0,
                                      a.curtailment_max)
                    for a in self._assets if a.kind == "source"]
        controls += [ControlDescriptor(f"route:{r.id}:flow", r.setpoint, 0.0,
                                       r.capacity if r.available else 0.0)
                     for r in self._routes if self._by_id[r.source_id].kind == "tank"]
        self.controls = tuple(controls)
        self.control_ids = tuple(c.id for c in controls)
        self._control_index = {c.id: i for i, c in enumerate(controls)}
        self._initial = tuple(self._by_id[id_].initial for id_ in self.tank_ids)
        tank_index = {id_: i for i, id_ in enumerate(self.tank_ids)}
        incidence = [[0.0] * len(self._routes) for _ in self.tank_ids]
        for j, r in enumerate(self._routes):
            if r.source_id in tank_index:
                incidence[tank_index[r.source_id]][j] -= 1.0
            if r.target_id in tank_index:
                incidence[tank_index[r.target_id]][j] += 1.0
        self._incidence = torch.tensor(incidence, dtype=torch.float64, device="cpu")

    def default_controls(self) -> torch.Tensor:
        return torch.tensor([c.value for c in self.controls], dtype=torch.float64, device="cpu")

    def rollout(self, overrides: Mapping | None = None, *, horizon_h=3.0, step_h=0.025) -> Rollout:
        """Override selected IDs without altering unspecified operating settings.

        Scalar CPU float64 tensors retain their autograd graph. Plain numbers are
        constants. Bounds/structure are checked on values, outside differentiation.
        """
        if overrides is None:
            overrides = {}
        if not isinstance(overrides, Mapping):
            raise ValueError("overrides must be a mapping of catalog IDs to scalar values")
        if any(key not in self._control_index for key in overrides):
            raise ValueError("Unknown control ID")
        values = []
        for c in self.controls:
            value = overrides.get(c.id, c.value)
            if not isinstance(value, torch.Tensor):
                value = torch.tensor(_number(value, c.id), dtype=torch.float64, device="cpu")
            if value.ndim != 0 or value.dtype != torch.float64 or value.device.type != "cpu":
                raise ValueError("Control tensors must be scalar CPU float64")
            values.append(value)
        vector = torch.stack(values) if values else self.default_controls()
        return self._rollout(vector, horizon_h=horizon_h, step_h=step_h,
                             explicit={self._control_index[key] for key in overrides})

    def rollout_vector(self, values: torch.Tensor, *, horizon_h=3.0, step_h=0.025) -> Rollout:
        """Explicitly override the full catalog; every control has strict bounds."""
        return self._rollout(values, horizon_h=horizon_h, step_h=step_h,
                             explicit=range(len(self.controls)))

    def _rollout(self, values, *, horizon_h, step_h, explicit) -> Rollout:
        if (not isinstance(values, torch.Tensor) or values.dtype != torch.float64 or
                values.device.type != "cpu" or values.ndim != 1 or
                values.shape[0] != len(self.controls)):
            raise ValueError("Controls must be a catalog-sized CPU float64 vector")
        # Detached reads reject invalid inputs; computation below uses original values.
        observed = values.detach().tolist()
        if any(not math.isfinite(v) or (i in explicit and (v < c.minimum or v > c.maximum))
               for i, (v, c) in enumerate(zip(observed, self.controls))):
            raise ValueError("Nonfinite or out-of-bounds control")
        horizon = _number(horizon_h, "horizon_h")
        step = _number(step_h, "step_h")
        times = storage_sample_times(horizon, step)
        flows = []
        cost_per_h = values[:0].sum()  # differentiable zero without summing large controls
        for a in self._assets:
            if a.kind == "source":
                cost_per_h = cost_per_h + values[self._control_index[f"source:{a.id}:curtail"]] * a.cost
        for r in self._routes:
            s, d = self._by_id[r.source_id], self._by_id[r.target_id]
            if s.kind == "source":
                q = s.inflow - values[self._control_index[f"source:{s.id}:curtail"]]
            else:
                q = values[self._control_index[f"route:{r.id}:flow"]]
            if float(q.detach()) > r.capacity + 1e-9:
                raise ValueError(f"Flow exceeds route capacity: {r.id}")
            flows.append(q)
            cost_per_h = cost_per_h + q * r.cost
            if d.kind != "tank":
                cost_per_h = cost_per_h + q * d.cost
        flow = torch.stack(flows) if flows else torch.empty(0, dtype=torch.float64, device="cpu")
        rates = self._incidence @ flow
        # Deliberately use repeated constant-rate Euler, matching native sample times;
        # native affine endpoints remain an independent forward reference.
        state = torch.tensor(self._initial, dtype=torch.float64, device="cpu")
        states = [state]
        for previous, current in zip(times, times[1:]):
            state = state + (current - previous) * rates
            states.append(state)
        volumes = torch.stack(states, dim=1)
        cost = horizon * cost_per_h
        if not bool(torch.isfinite(volumes.detach()).all()) or not bool(torch.isfinite(cost.detach())):
            raise ValueError("Nonfinite rollout result")
        return Rollout(self.tank_ids, torch.tensor(times, dtype=torch.float64, device="cpu"), volumes,
                       cost, rates.clone(), values.clone())
