"""Independent seeded local plant for G1 acquisition and offline qualification.

Standard library only. Piecewise-constant delivered flows are integrated directly
at fault boundaries, independently of the native forecast/detector. Truth is a
test-side API and is never included in sensor frames or detector context.
"""
from __future__ import annotations
from copy import deepcopy
from dataclasses import dataclass
import bisect
import math
import random

PRESETS = ("healthy", "pump_loss", "unmetered_loss", "drift", "stuck", "missing", "delayed", "reordered", "mixed")
METHOD = "local-piecewise-plant-0.1.0"


def number(value, name, minimum=None):
    if type(value) not in (int, float) or not math.isfinite(value) or (minimum is not None and value < minimum):
        raise ValueError(f"Invalid finite {name}")
    return float(value)


@dataclass(frozen=True)
class Fault:
    kind: str
    start_s: float
    end_s: float
    attachment_id: str
    magnitude: float = 0.0


class Plant:
    def __init__(self, network: dict, configuration: dict, *, seed=1337, preset="healthy", onset_s=60.0, recovery_s=180.0):
        if type(seed) is not int or not 0 <= seed <= 2**32-1 or preset not in PRESETS:
            raise ValueError("Unsupported seed or preset")
        onset_s = number(onset_s, "fault onset", 0)
        recovery_s = number(recovery_s, "fault recovery", 0)
        if recovery_s <= onset_s:
            raise ValueError("Recovery must follow onset")
        self.network = deepcopy(network)
        self.configuration = deepcopy(configuration)
        self.seed, self.preset = seed, preset
        self.rng = random.Random(seed)
        self.time_s = 0.0
        assets, routes = self.network["assets"], self.network["routes"]
        if not 1 <= len(assets) <= 128 or len(routes) > 256:
            raise ValueError("Native network bounds exceeded")
        self.assets = {a["id"]: a for a in assets}
        self.routes = {r["id"]: r for r in routes}
        if len(self.assets) != len(assets) or len(self.routes) != len(routes) or set(self.assets) & set(self.routes):
            raise ValueError("Duplicate network identity")
        self.volumes = {a["id"]: number(a["initial_m3"], "initial inventory") for a in assets if a["kind"] == "tank"}
        if not self.volumes:
            raise ValueError("A local plant needs storage")
        for a in assets:
            if a["kind"] == "source":
                q = number(a["inflow_m3h"], "source delivery", 0)-number(a["curtailment_m3h"], "curtailment", 0)
                if q < 0:
                    raise ValueError("Negative configured source delivery")
        for r in routes:
            if r["source_id"] not in self.assets or r["target_id"] not in self.assets:
                raise ValueError("Unknown route endpoint")
            number(r["setpoint_m3h"], "route delivery", 0)
        self.sensors = tuple(s for s in configuration["sensors"] if s["enabled"])
        if not 1 <= len(self.sensors) <= 256 or len({s["sensor_id"] for s in self.sensors}) != len(self.sensors):
            raise ValueError("Invalid sensor count or identity")
        for s in self.sensors:
            number(s["expected_period_s"], "sampling period", 0.01)
            number(s["uncertainty_bound"], "uncertainty", 0)
        if sum(1/s["expected_period_s"] for s in self.sensors) > 1024:
            raise ValueError("Configured sampling exceeds the receiver ingress budget")
        tank = next(iter(self.volumes))
        pump = next((r["id"] for r in routes if r["has_pump"] and r["available"]), None)
        if preset in ("pump_loss", "mixed") and pump is None:
            raise ValueError("This preset requires an available configured pump")
        self.faults = []
        if preset in ("pump_loss", "mixed"):
            self.faults.append(Fault("pump_loss", onset_s, recovery_s, pump))
        if preset in ("unmetered_loss", "mixed"):
            self.faults.append(Fault("unmetered_loss", onset_s, recovery_s, tank, 18.0))
        for kind in ("drift", "stuck", "missing", "delayed", "reordered"):
            if preset == kind:
                self.faults.append(Fault(kind, onset_s, recovery_s, tank, 0.002 if kind == "drift" else 0))
        self._stuck = {}
        self._last_sample = {}

    def active(self, kind, attachment, at=None):
        at = self.time_s if at is None else at
        return next((f for f in self.faults if f.kind == kind and f.attachment_id == attachment and f.start_s <= at < f.end_s), None)

    def delivered(self, at=None):
        values = {}
        for route in self.routes.values():
            source = self.assets[route["source_id"]]
            if source["kind"] == "source":
                value = source["inflow_m3h"]-source["curtailment_m3h"]
            else:
                value = route["setpoint_m3h"] if route["available"] else 0.0
                if route["has_pump"] and self.active("pump_loss", route["id"], at):
                    value = 0.0
            values[route["id"]] = value
        return values

    def advance(self, event_s):
        event_s = number(event_s, "event clock", 0)
        if event_s < self.time_s:
            raise ValueError("A plant epoch cannot run backwards")
        boundaries = sorted({self.time_s, event_s} | {t for f in self.faults for t in (f.start_s,f.end_s) if self.time_s < t < event_s})
        for left, right in zip(boundaries, boundaries[1:]):
            rates = {tank: 0.0 for tank in self.volumes}
            for identity, flow in self.delivered((left+right)/2).items():
                route = self.routes[identity]
                if route["source_id"] in rates:
                    rates[route["source_id"]] -= flow
                if route["target_id"] in rates:
                    rates[route["target_id"]] += flow
            for tank in rates:
                leak = self.active("unmetered_loss", tank, (left+right)/2)
                if leak:
                    rates[tank] -= leak.magnitude
                self.volumes[tank] += rates[tank]*(right-left)/3600
                if not math.isfinite(self.volumes[tank]):
                    raise ValueError("Local inventory arithmetic overflow")
        self.time_s = event_s

    @staticmethod
    def level_for_volume(sensor, volume):
        knots = sensor["calibration"]["knots"]
        index = bisect.bisect_right([k["volume_m3"] for k in knots], volume)
        # Out-of-domain values are extrapolated only by this independent synthetic
        # instrument generator, never clipped. The receiver mapper rejects them.
        index = max(1, min(len(knots)-1, index))
        lo, hi = knots[index-1], knots[index]
        return lo["level_m"]+(volume-lo["volume_m3"])*(hi["level_m"]-lo["level_m"])/(hi["volume_m3"]-lo["volume_m3"])

    def sample(self, *, due_only=False):
        flows = self.delivered()
        rows = []
        for sensor in self.sensors:
            identity, quantity = sensor["sensor_id"], sensor["quantity"]
            attachment = sensor["attachment"]["id"]
            if due_only and self.time_s-self._last_sample.get(identity, -math.inf) < sensor["expected_period_s"]-1e-9:
                continue
            self._last_sample[identity] = self.time_s
            if self.active("missing", attachment):
                continue
            quality = "good"
            if quantity in ("inventory", "level"):
                value = self.volumes[attachment]
                if quantity == "level":
                    value = self.level_for_volume(sensor, value)
            elif quantity == "source_flow":
                asset = self.assets[attachment]
                value = asset["inflow_m3h"]-asset["curtailment_m3h"]
            elif quantity == "route_flow":
                value = flows[attachment]
            elif quantity == "pump_state":
                route = self.routes[attachment]
                value = "unavailable" if not route["available"] else "delivery_loss" if self.active("pump_loss",attachment) else "running" if route["setpoint_m3h"] else "commanded_off"
            else:
                # No invented chemistry/pressure/temperature simulator.
                value, quality = 0.0, "bad"
            if type(value) in (int,float):
                # Zero delivered flow uses an exact zero instrument; adding signed
                # noise to it would deliberately create an out-of-model reading.
                if quantity not in ("source_flow","route_flow") or value != 0:
                    value += self.rng.uniform(-sensor["uncertainty_bound"],sensor["uncertainty_bound"])
                drift = self.active("drift",attachment)
                if drift:
                    value += drift.magnitude*(self.time_s-drift.start_s)
                if self.active("stuck",attachment):
                    value = self._stuck.setdefault(identity,value)
                else:
                    self._stuck.pop(identity,None)
                if not math.isfinite(value):
                    raise ValueError("Local instrument arithmetic overflow")
            rows.append(dict(sensor_id=identity,event_time_s=self.time_s,acquisition_start_s=self.time_s,
                             acquisition_end_s=self.time_s,value=value,unit=sensor["unit"],quality=quality))
        return rows

    def delivery_delay_s(self):
        if any(f.kind == "delayed" and f.start_s <= self.time_s < f.end_s for f in self.faults):
            return 3.0
        if any(f.kind == "reordered" and f.start_s <= self.time_s < f.end_s for f in self.faults):
            return 1.5 if int(self.time_s) % 2 == 0 else 0.0
        return 0.0

    def truth(self):
        """Offline qualification only. The publisher never emits this structure."""
        return dict(method=METHOD,seed=self.seed,preset=self.preset,event_time_s=self.time_s,
                    inventories_m3=dict(self.volumes),delivered_m3h=self.delivered(),
                    faults=[dict(kind=f.kind,start_s=f.start_s,end_s=f.end_s,attachment_id=f.attachment_id,
                                 magnitude=f.magnitude) for f in self.faults])
