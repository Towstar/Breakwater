"""Independent EOG validation from causal observations and exact candidate IDs.

No detector, response compiler, native library, file access or model calls. The
validator reconstructs the active failures/onsets/productivity from View, derives
the physical/economic state again, checks calendars pairwise and recomputes the
candidate. Fraction arithmetic independently rounds each documented binary64
primitive, rather than executing the C++ evaluator. This is not a real-number
proof, calibrated onset estimate or a second use of the evaluator under a new ID.

The worker must first validate the public request/capture/hash. This module checks
the numerical claims; its private audit is retained separately from wire records.
"""
from __future__ import annotations

from dataclasses import dataclass
from fractions import Fraction
import hashlib
import json
import math
import operator
import time

METHOD = "eog-rational-validation-0.1.0"
SUPPORTED_EVALUATOR = "eog-native-seconds-0.1.1"
SUPPORTED_EVALUATORS = ("eog-native-seconds-0.1.0", SUPPORTED_EVALUATOR)


class ValidationUnavailable(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


class Arithmetic:
    """Exact rational primitive followed by one binary64 nearest/even conversion."""
    def __init__(self, maximum=1_000_000, *, deadline=None, clock=time.perf_counter):
        self.count = 0
        self.maximum = maximum
        self.deadline = deadline
        self.clock = clock

    def check_time(self):
        if self.deadline is not None and self.clock() >= self.deadline:
            raise ValidationUnavailable("validation_time_limit", "Independent validation reached its planning deadline.")

    def round(self, value, *, infinite=False):
        self.count += 1
        if self.count % 64 == 1:
            self.check_time()
        if self.count > self.maximum:
            raise ValidationUnavailable("validation_limit", "Independent arithmetic work bound exceeded.")
        try:
            result = float(value)
        except OverflowError:
            result = math.inf if value > 0 else -math.inf
        if not math.isfinite(result) and not infinite:
            raise ValidationUnavailable("numerical_unavailable", "Independent arithmetic is not finite binary64.")
        return result

    def op(self, operation, a, b, *, infinite=False):
        if math.isinf(a) or math.isinf(b):
            if infinite and operation is operator.add and a >= 0 and b >= 0:
                return math.inf
            raise ValidationUnavailable("numerical_unavailable", "Nonfinite independent intermediate.")
        try:
            return self.round(operation(Fraction(a), Fraction(b)), infinite=infinite)
        except ZeroDivisionError as error:
            raise ValidationUnavailable("numerical_unavailable", "Zero independent divisor.") from error

    def add(self, a, b): return self.op(operator.add, a, b)
    def sub(self, a, b): return self.op(operator.sub, a, b)
    def mul(self, a, b): return self.op(operator.mul, a, b)
    def div(self, a, b): return self.op(operator.truediv, a, b)
    def total(self, values): return self.round(sum((Fraction(v) for v in values), Fraction()))


@dataclass(frozen=True, slots=True)
class Failure:
    facility: object
    onset_unix: int
    repair_s: float
    initial_bbl: float
    threshold_bbl: float
    already_shutdown: bool
    productive: tuple[str, ...]
    water_bpd: tuple[float, ...]


@dataclass(frozen=True, slots=True)
class ValidationResult:
    record: dict
    audit: dict


@dataclass(frozen=True, slots=True)
class PreparedValidation:
    model: object
    view: object
    failures: dict
    source: tuple
    channels: dict
    model_fingerprint: str
    evidence_fingerprint: str
    preparation_primitives: int


def prepare(model, view):
    """Compile independent immutable-capture evidence once, without native state.

    This private object is made inside the trusted provider, never accepted from
    the wire. A search may reuse it for candidate validation at the same cutoff.
    """
    arithmetic = Arithmetic()
    failures, source, channels = reconstruct(model, view, arithmetic)
    return PreparedValidation(model, view, failures, source, channels, model.fingerprint(),
                              view.evidence_fingerprint(), arithmetic.count)


def _hash(value):
    raw = json.dumps(value, ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(raw).hexdigest()


def _unavailable(condition, code, message):
    if condition:
        raise ValidationUnavailable(code, message)


def reconstruct(model, view, arithmetic):
    """Select fault intervals after the last independently recognized recovery.

    Fresh required feedback establishes whether each facility currently fails.
    A last coherent recovery separates repeated incidents. Prior known well states
    establish economics even when current wells have already shut automatically.
    """
    failures, evidence = {}, {}
    channels = {channel.key: channel for channel in model.channels()}
    def retain(samples):
        for sample in samples:
            evidence[(sample.unix, sample.asset_id, sample.metric)] = sample
    for f in model.facilities:
        keys = [(f.pump_id, "status"), (f.pump_id, "flow_bpd"), (f.tank_id, "level_pct"), (f.id, "inflow_bpd")]
        keys += [(w.id, metric) for w in f.wells for metric in ("status", "water_bpd")]
        current = [view.latest(*key) for key in keys]
        _unavailable(any(r.state != "available" or r.sample is None or view.cutoff_unix - r.sample.unix > 300 for r in current),
                     "failure_context_incomplete", "Independent current failure evidence is missing, stale or unsupported.")
        samples = {key: reading.sample for key, reading in zip(keys, current)}
        _unavailable(len({s.unix for s in samples.values()}) != 1,
                     "failure_context_incomplete", "Independent snapshot needs a common acquisition timestamp.")
        retain(samples.values())
        series = {key: {s.unix: s for s in view.series(*key)} for key in keys}
        status_key, flow_key, level_key, inflow_key = keys[:4]
        zero = max(5.0, arithmetic.mul(f.pump_flow_bpd, 0.02))
        def known_states(timestamp):
            readings = [series[(w.id, "status")].get(timestamp) for w in f.wells]
            if any(r is None or r.value not in (0, 1) for r in readings):
                return None
            return tuple(w.id for w, reading in zip(f.wells, readings) if reading.value == 1)
        def recovery(timestamp):
            pump = series[status_key].get(timestamp)
            flow = series[flow_key].get(timestamp)
            if pump is None or flow is None:
                return False
            if pump.value == 1:
                return flow.value > zero and arithmetic.div(flow.value, f.pump_flow_bpd) <= 1.05
            level = series[level_key].get(timestamp)
            return (pump.value == 0 and abs(flow.value) <= zero and level is not None and
                    0 <= level.value < arithmetic.sub(f.start_pct, 0.5) and known_states(timestamp) is not None)
        now = samples[status_key].unix
        status = samples[status_key].value
        if status != 2:
            _unavailable(not recovery(now) or not 0 <= samples[level_key].value <= 100,
                         "failure_context_incomplete", "Independent feedback cannot establish a healthy facility.")
            continue
        recoveries = [t for t in series[status_key] if recovery(t)]
        last_recovery = max(recoveries, default=None)
        fault_times = [t for t, s in series[status_key].items() if s.value == 2 and (last_recovery is None or t > last_recovery)]
        _unavailable(not fault_times or last_recovery is None,
                     "onset_unavailable", "Independent onset is left-censored or lacks a preceding recovery/nonfault sample.")
        onset = min(fault_times)
        retain(s for values in series.values() for t, s in values.items() if t in (onset, last_recovery))
        # Independent backward selection, rather than the detector's carried event state.
        baseline_times = [t for t, s in series[status_key].items()
                          if t < onset and s.value in (0, 1) and known_states(t) is not None]
        baseline = max(baseline_times, default=None)
        productive = known_states(baseline) if baseline is not None else known_states(onset)
        _unavailable(productive is None or (baseline is None and not productive),
                     "productivity_unavailable", "Independent pre-trip productive wells are unknown, not zero.")
        if baseline is not None:
            retain(series[(w.id, "status")][baseline] for w in f.wells)
        repair = arithmetic.sub(arithmetic.mul(model.repair_hours, 3600), view.cutoff_unix - onset)
        _unavailable(not 0 < repair <= 604800, "repair_unavailable", "Independent causal repair horizon has elapsed or is unsupported.")
        level, inflow, flow = (samples[key].value for key in (level_key, inflow_key, flow_key))
        water = [samples[(w.id, "water_bpd")].value for w in f.wells]
        states = [samples[(w.id, "status")].value for w in f.wells]
        total = arithmetic.total(water)
        _unavailable(abs(flow) > zero or not 0 <= level <= 100 or inflow < 0 or any(q < 0 for q in water) or
                     abs(arithmetic.sub(total, inflow)) > max(20, arithmetic.mul(inflow, 0.05)) or
                     any(state == 0 and q > 5 for state, q in zip(states, water)),
                     "physics_unavailable", "Independent observations contradict the supported failed-pump model.")
        initial = arithmetic.mul(arithmetic.div(level, 100), f.capacity_bbl)
        threshold = arithmetic.mul(arithmetic.div(f.shutdown_pct, 100), f.capacity_bbl)
        all_off = all(state == 0 for state in states)
        shutdown = all_off and level >= arithmetic.sub(f.shutdown_pct, 0.5) and inflow <= 5
        if shutdown:
            water = [0.0] * len(water)
        else:
            active = tuple(w.id for w, state in zip(f.wells, states) if state == 1)
            _unavailable((all_off and productive != ()) or level >= f.shutdown_pct or set(active) != set(productive),
                         "physics_unavailable", "Independent well state does not define a supported current forecast.")
            rate = arithmetic.div(inflow, 24)
            _unavailable(inflow > 0 and rate == 0, "numerical_unavailable", "Independent hourly inflow underflows.")
            if rate > 0:
                _unavailable(threshold <= initial, "numerical_unavailable", "Independent inventory margin is not representable.")
                remaining_h = arithmetic.div(arithmetic.sub(threshold, initial), rate)
                age_h = arithmetic.div(view.cutoff_unix - now, 3600)
                if remaining_h <= age_h:
                    initial, shutdown, water = threshold, True, [0.0] * len(water)
                else:
                    initial = arithmetic.add(initial, arithmetic.mul(rate, age_h))
            if not shutdown:
                _unavailable(any(w.id not in productive and q != 0 for w, q in zip(f.wells, water)) or
                             ((inflow == 0) != (total == 0)), "inflow_ambiguous", "Independent rate allocation cannot reconcile the meters.")
                if total > 0:
                    allocated = [arithmetic.mul(inflow, arithmetic.div(q, total)) for q in water]
                    largest = max(range(len(water)), key=water.__getitem__)
                    allocated[largest] = arithmetic.sub(inflow, arithmetic.total(q for i, q in enumerate(allocated) if i != largest))
                    _unavailable(any(q < 0 or (raw > 0 and q == 0) for raw, q in zip(water, allocated)),
                                 "numerical_unavailable", "Independent proportional rates are not representable.")
                    water = allocated
        failures[f.id] = Failure(f, onset, repair, initial, threshold, shutdown, productive, tuple(water))
    _unavailable(not failures, "no_active_failure", "Independent validation needs at least one current failure.")
    _unavailable(len(evidence) > 4096, "validation_limit", "Required independent evidence exceeds the public reference budget.")
    return failures, tuple(evidence.values()), channels


def validate(model, view, plan, evaluator_method=SUPPORTED_EVALUATOR):
    """Recompute a prevalidated public candidate without evaluating it natively."""
    _unavailable(evaluator_method not in SUPPORTED_EVALUATORS, "unsupported_evaluator", "Independent validation supports the declared native seconds semantics only.")
    return validate_prepared(prepare(model, view), plan, evaluator_method)


def validate_prepared(prepared, plan, evaluator_method=SUPPORTED_EVALUATOR, *, deadline=None, clock=time.perf_counter):
    """Check a candidate using previously independently reconstructed evidence."""
    _unavailable(evaluator_method not in SUPPORTED_EVALUATORS, "unsupported_evaluator", "Independent validation supports the declared native seconds semantics only.")
    model, view = prepared.model, prepared.view
    failures, source, channels = prepared.failures, prepared.source, prepared.channels
    arithmetic = Arithmetic(deadline=deadline, clock=clock)
    arithmetic.check_time()
    _unavailable(len(plan["truck_jobs"]) > 256 or len(plan["well_shut_ins"]) > 256 or
                 sum(len(j["trips"]) for j in plan["truck_jobs"]) > 16384,
                 "validation_limit", "Independent plan action/trip work bound exceeded.")
    violations = []
    def reject(code, asset=None, time=None, message="Independent validation rejected a candidate claim or action."):
        _unavailable(len(violations) >= 256, "validation_limit", "Independent violation output bound exceeded.")
        violations.append({"code": code, "message": message, "asset_id": asset, "elapsed_s": time})
    if plan["start_unix"] != view.cutoff_unix:
        reject("stale_plan")
    claims = {h["facility_id"]: h["repair_elapsed_s"] for h in plan["repair_deadlines"]}
    if len(claims) != len(plan["repair_deadlines"]) or set(claims) != set(failures):
        reject("repair_facilities")
    for asset in set(claims) & set(failures):
        if claims[asset] != failures[asset].repair_s:
            reject("repair_horizon", asset)
    well_lookup = {w.id: (f, w) for f in model.facilities for w in f.wells}
    all_assets = {s.disposal_id for s in model.systems}
    for f in model.facilities:
        all_assets.update((f.id, f.tank_id, f.pump_id, f.pipeline_id, *(w.id for w in f.wells)))
    for asset in plan["inspection_assets"]:
        if asset not in all_assets:
            reject("unknown_inspection_asset", asset)
    stops = {}
    for action in plan["well_shut_ins"]:
        fid, wid, start, end = (action[k] for k in ("facility_id", "well_id", "start_elapsed_s", "end_elapsed_s"))
        if (fid not in failures or wid not in well_lookup or well_lookup[wid][0].id != fid or wid in stops or
                wid not in failures[fid].productive or not 0 <= start <= failures[fid].repair_s or end != failures[fid].repair_s):
            reject("well_shut_in", wid)
        stops[wid] = start
    fleets = {s.id: s.fleet_size for s in model.systems}
    calendars, arrivals, trip_count, jobs_seen = [], {fid: {} for fid in failures}, 0, set()
    labels = {}
    for job in plan["truck_jobs"]:
        arithmetic.check_time()
        fid, sid, truck = (job[k] for k in ("facility_id", "system_id", "truck_id"))
        if fid not in failures or sid not in fleets or failures[fid].facility.system_id != sid:
            reject("truck_asset", fid); continue
        f = failures[fid].facility
        if job["job_id"] in jobs_seen:
            reject("duplicate_job", fid)
        jobs_seen.add(job["job_id"])
        labels.setdefault(sid, set()).add(truck)
        call = job["called_elapsed_s"]
        if call < 0 or call >= failures[fid].repair_s or job["response_min"] != f.response_min or job["cycle_min"] != f.cycle_min or not job["trips"]:
            reject("truck_job", fid); continue
        first = arithmetic.add(call, arithmetic.mul(f.response_min, 60))
        cycle = arithmetic.mul(f.cycle_min, 60)
        previous = -1
        for k, trip in enumerate(job["trips"]):
            trip_count += 1
            t = trip["arrival_elapsed_s"]
            expected = arithmetic.add(first, arithmetic.mul(k, cycle))
            if (t < 0 or t != expected or t <= previous or t >= failures[fid].repair_s or
                    trip["volume_bbl"] != model.truck_capacity_bbl or trip["cost"] != model.cost_per_trip):
                reject("truck_trip", fid, t if t >= 0 else None)
            previous = t
            arrivals[fid].setdefault(t, []).append(job["job_id"])
        if job["available_again_elapsed_s"] != arithmetic.add(previous, cycle) or job["available_again_elapsed_s"] <= previous:
            reject("yard_return", fid)
        calendars.append({"system_id": sid, "truck_id": truck, "job_id": job["job_id"],
                          "start_s": call, "end_s": job["available_again_elapsed_s"]})
    _unavailable(trip_count > 16384, "validation_limit", "Independent trip work bound exceeded.")
    for sid, trucks in labels.items():
        if len(trucks) > fleets[sid]:
            reject("fleet_size", sid)
    # Different algorithm from native adjacent sorted slots: test every pair of
    # claimed jobs on the same opaque truck. A final cycle remains occupied.
    for i, a in enumerate(calendars):
        arithmetic.check_time()
        for b in calendars[i + 1:]:
            if ((a["system_id"], a["truck_id"]) == (b["system_id"], b["truck_id"]) and
                    a["start_s"] < b["end_s"] and b["start_s"] < a["end_s"]):
                reject("fleet_overlap", b["truck_id"], max(a["start_s"], b["start_s"]))
    audits, lost_total, minimum_margin, shutdowns = [], 0.0, None, []
    if not violations:
        for fid, failure in failures.items():
            arithmetic.check_time()
            f = failure.facility
            well_rates = {}
            for w, q in zip(f.wells, failure.water_bpd):
                rate = arithmetic.div(q, 86400)
                _unavailable(q > 0 and rate == 0, "numerical_unavailable", "Independent per-second inflow underflows.")
                well_rates[w.id] = rate
            def supply(t):
                q = 0.0
                for well in f.wells:
                    if well.id in failure.productive and stops.get(well.id, math.inf) > t:
                        q = arithmetic.add(q, well_rates[well.id])
                return q
            times = sorted({0.0, failure.repair_s, *arrivals[fid], *(stops[w.id] for w in f.wells if w.id in stops)})
            shutdown = 0.0 if failure.already_shutdown or failure.initial_bbl >= failure.threshold_bbl else None
            anchor_time, anchor_volume = 0.0, failure.initial_bbl
            rate = 0.0 if shutdown is not None else supply(0)
            inventory = failure.initial_bbl
            last_time, produced, removed = 0.0, Fraction(), Fraction()
            maximum_inventory = inventory
            for t in times:
                if shutdown is None and rate > 0 and t > last_time:
                    quotient = arithmetic.op(operator.truediv, arithmetic.sub(failure.threshold_bbl, anchor_volume), rate, infinite=True)
                    crossing = arithmetic.op(operator.add, anchor_time, quotient, infinite=True)
                    if crossing <= t:
                        _unavailable(crossing <= last_time, "numerical_unavailable", "Independent crossing does not advance time.")
                        produced += Fraction(rate) * Fraction(arithmetic.sub(crossing, last_time))
                        shutdown = crossing
                        anchor_time, anchor_volume, rate, last_time = crossing, failure.threshold_bbl, 0.0, crossing
                inventory = arithmetic.add(anchor_volume, arithmetic.mul(rate, arithmetic.sub(t, anchor_time)))
                _unavailable(shutdown is None and inventory >= failure.threshold_bbl,
                             "numerical_unavailable", "Independent rounded volume/crossing ordering is ambiguous.")
                produced += Fraction(rate) * Fraction(arithmetic.sub(t, last_time))
                maximum_inventory = max(maximum_inventory, inventory)
                last_time = t
                demand = arithmetic.mul(len(arrivals[fid].get(t, ())), model.truck_capacity_bbl)
                if demand:
                    if inventory < demand:
                        reject("insufficient_inventory", fid, t); break
                    inventory = arithmetic.sub(inventory, demand)
                    removed += Fraction(demand)
                    anchor_time, anchor_volume = t, inventory
                new_rate = 0.0 if shutdown is not None else supply(t)
                if new_rate != rate:
                    anchor_time, anchor_volume, rate = t, inventory, new_rate
            if violations:
                break
            losses = []
            facility_lost = 0.0
            for w in f.wells:
                if w.id not in failure.productive:
                    continue
                endpoints = [failure.repair_s]
                if w.id in stops: endpoints.append(stops[w.id])
                if shutdown is not None: endpoints.append(shutdown)
                duration = arithmetic.sub(failure.repair_s, min(endpoints))
                oil, gas = arithmetic.mul(w.oil_bpd, model.oil_per_bbl), arithmetic.mul(w.gas_mcfd, model.gas_per_mcf)
                daily = arithmetic.add(oil, gas)
                revenue = arithmetic.div(daily, 86400)
                _unavailable(duration > 0 and ((w.oil_bpd > 0 and model.oil_per_bbl > 0 and oil == 0) or
                             (w.gas_mcfd > 0 and model.gas_per_mcf > 0 and gas == 0) or (daily > 0 and revenue == 0)),
                             "numerical_unavailable", "Independent opportunity revenue underflows.")
                cost = arithmetic.mul(duration, revenue)
                _unavailable(duration > 0 and revenue > 0 and cost == 0, "numerical_unavailable",
                             "Positive independent revenue loss underflows binary64; zero cost cannot be certified.")
                facility_lost = arithmetic.add(facility_lost, cost)
                losses.append({"well_id": w.id, "stopped_s": duration, "lost_revenue": cost})
            lost_total = arithmetic.add(lost_total, facility_lost)
            margin = arithmetic.sub(failure.threshold_bbl, maximum_inventory)
            minimum_margin = margin if minimum_margin is None else min(minimum_margin, margin)
            if shutdown is not None:
                shutdowns.append({"facility_id": fid, "elapsed_s": shutdown})
            residual = Fraction(failure.initial_bbl) + produced - removed - Fraction(inventory)
            audits.append({"facility_id": fid, "onset_unix": failure.onset_unix, "repair_s": failure.repair_s,
                           "initial_bbl": failure.initial_bbl, "final_bbl": inventory,
                           "produced_bbl": arithmetic.round(produced), "removed_bbl": arithmetic.round(removed),
                           "balance_residual_bbl": arithmetic.round(residual), "well_losses": losses})
    evidence = []
    for sample in source:
        eid = "iv-" + _hash((sample.unix, sample.asset_id, sample.metric, sample.value))
        evidence.append({"evidence_id": eid, "asset_id": sample.asset_id, "metric": sample.metric, "timestamp_unix": sample.unix,
                         "value": sample.value, "unit": channels[sample.key].unit,
                         "quality": "good" if view.cutoff_unix - sample.unix <= 300 else "stale"})
    valid = not violations
    truck_cost = arithmetic.mul(trip_count, model.cost_per_trip) if valid else None
    cost = {"truck_cost": truck_cost, "lost_revenue": lost_total, "total": arithmetic.add(truck_cost, lost_total)} if valid else None
    arithmetic.check_time()
    record = {"validation_id": "validation-" + _hash((METHOD, evaluator_method, plan["plan_hash"], prepared.model_fingerprint, prepared.evidence_fingerprint)),
              "plan_id": plan["plan_id"], "plan_hash": plan["plan_hash"], "evaluator_method": evaluator_method,
              "validator_method": METHOD, "independent": True, "feasible": valid, "coverage": "available" if valid else "unavailable",
              "outcome": ("automatic_shutdown" if shutdowns else "avoids_shutdown") if valid else "unavailable",
              "shutdowns": shutdowns if valid else [], "validated_horizons": plan["repair_deadlines"] if valid else [],
              "cost": cost, "violations": violations, "minimum_shutdown_margin_bbl": minimum_margin if valid else None,
              "evidence_ids": [item["evidence_id"] for item in evidence], "evidence": evidence}
    audit = {"method": METHOD, "cutoff_unix": view.cutoff_unix, "arithmetic_primitives": prepared.preparation_primitives + arithmetic.count,
             "facilities": audits, "calendars": calendars, "limitations": [
                 "Independent code checks the declared binary64 fixed-rate model and point-onset assumption; it is not a real-arithmetic or hydraulic proof.",
                 "Causal archive/model parsing is shared infrastructure; detection, response compilation and native evaluation are not called."]}
    return ValidationResult(record, audit)
