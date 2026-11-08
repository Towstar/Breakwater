"""Independent private committed-snapshot reference, not a public plan protocol.

Inputs are authenticated caller-supplied post-event decision state. This reference
does not reconstruct that state from View/History or read native/simulator truth.
It checks pairwise reservations and independently rounds rational primitives.
"""
from dataclasses import dataclass
import hashlib
import json
import math
import operator

from eog_plan_validator import Arithmetic, ValidationUnavailable

METHOD = "eog-rational-committed-validation-0.1.0"
EVALUATOR = "eog-native-committed-seconds-0.1.0"


class CommittedValidationError(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


def _need(condition, message):
    if not condition: raise CommittedValidationError("invalid_input", message)


def _shape(value, keys):
    _need(type(value) is dict and set(value) == set(keys), "Missing/unknown committed computational fields.")


def _scalar(value, integer):
    if integer:
        _need(type(value) is int and 0 <= value <= 0xffffffff, "Expected an unsigned integer index/count.")
        return value
    _need(type(value) in (int, float), "Expected a finite numeric scalar.")
    try: value = float(value)
    except (ValueError, OverflowError) as error:
        raise CommittedValidationError("invalid_input", "Scalar is not binary64.") from error
    _need(math.isfinite(value), "Scalar is not finite.")
    return value


SCHEMAS = {
    "fleets": ("trucks", ""),
    "facilities": ("system first_well well_count already_shut_in", "capacity_bbl initial_bbl shutdown_bbl repair_s response_min cycle_min"),
    "wells": ("productive", "water_bpd oil_bpd gas_mcfd"),
    "horizons": ("facility", "repair_s"), "stops": ("facility well", "start_s end_s"),
    "jobs": ("facility system truck first_trip trip_count", "called_s response_min cycle_min available_again_s"),
    "trips": ("", "arrival_s volume_bbl cost"),
    "occupied": ("system truck", "available_s"), "pickups": ("facility system truck", "arrival_s"),
}
LIMITS = dict(fleets=64, facilities=256, wells=32768, horizons=256, stops=256, jobs=256,
              trips=16384, occupied=16384, pickups=16384)


def _array(key, rows):
    _need(type(rows) in (tuple, list) and len(rows) <= LIMITS[key], "Computational array exceeds its bound.")
    ints, floats = (part.split() for part in SCHEMAS[key])
    copied = []
    for row in rows:
        _shape(row, ints + floats)
        copied.append({name: _scalar(row[name], name in ints) for name in ints + floats})
    return copied


def normalize(snapshot, plan, commitments):
    scalars = ("oil_per_bbl", "gas_per_mcf", "truck_capacity_bbl", "cost_per_trip")
    _shape(snapshot, (*scalars, "fleets", "facilities", "wells"))
    _shape(plan, ("horizons", "stops", "jobs", "trips")); _shape(commitments, ("occupied", "pickups", "stopped_wells"))
    s = {k: _scalar(snapshot[k], False) for k in scalars}
    for key in ("fleets", "facilities", "wells"): s[key] = _array(key, snapshot[key])
    p = {key: _array(key, plan[key]) for key in ("horizons", "stops", "jobs", "trips")}
    c = {key: _array(key, commitments[key]) for key in ("occupied", "pickups")}
    _need(type(commitments["stopped_wells"]) in (tuple, list) and len(commitments["stopped_wells"]) <= 32768,
          "Prior-stop array exceeds its bound.")
    c["stopped_wells"] = [_scalar(w, True) for w in commitments["stopped_wells"]]
    return s, p, c


def _fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode()).hexdigest()


@dataclass(frozen=True, slots=True)
class CommittedValidation:
    record: dict
    audit: dict


def validate(snapshot, plan, commitments, evaluated=None, *, maximum_primitives=1_000_000):
    s, p, c = normalize(snapshot, plan, commitments)
    bindings = {"snapshot": _fingerprint(s), "plan": _fingerprint(p), "commitments": _fingerprint(c)}
    fields, wells, fleets = s["facilities"], s["wells"], s["fleets"]
    _need(s["oil_per_bbl"] >= 0 and s["gas_per_mcf"] >= 0 and s["truck_capacity_bbl"] > 0 and s["cost_per_trip"] >= 0,
          "Invalid snapshot economics.")
    _need(all(f["trucks"] <= 256 for f in fleets), "Fleet exceeds its slot bound.")
    next_well = 0
    for f in fields:
        _need(f["system"] < len(fleets) and f["first_well"] == next_well and f["well_count"] <= 128 and
              f["well_count"] <= len(wells) - next_well and f["already_shut_in"] <= 1, "Invalid snapshot topology.")
        next_well += f["well_count"]
        _need(0 <= f["initial_bbl"] <= f["capacity_bbl"] and 0 < f["shutdown_bbl"] <= f["capacity_bbl"] and
              0 < f["repair_s"] <= 604800 and 0 <= f["response_min"] <= 10080 and 0 < f["cycle_min"] <= 10080,
              "Invalid initial state or horizon/travel settings.")
    _need(next_well == len(wells), "Unassigned snapshot wells.")
    _need(all(w["productive"] <= 1 and min(w["water_bpd"], w["oil_bpd"], w["gas_mcfd"]) >= 0 and
              (w["productive"] or w["water_bpd"] == 0) for w in wells), "Invalid snapshot well state.")
    reserved = {}
    for b in c["occupied"]:
        key = b["system"], b["truck"]
        _need(b["system"] < len(fleets) and b["truck"] < fleets[b["system"]]["trucks"] and b["available_s"] > 0 and
              key not in reserved, "Invalid/duplicate occupied slot.")
        reserved[key] = b["available_s"]
    _need(len(set(c["stopped_wells"])) == len(c["stopped_wells"]) and
          all(w < len(wells) and wells[w]["productive"] for w in c["stopped_wells"]), "Invalid prior-stop context.")
    pending = {}
    for t in c["pickups"]:
        _need(t["facility"] < len(fields), "Unknown committed facility.")
        f = fields[t["facility"]]; key = t["system"], t["truck"]
        _need(t["system"] == f["system"] and key in reserved and 0 < t["arrival_s"] < min(f["repair_s"], reserved[key]),
              "Invalid pending pickup/return context.")
        pending.setdefault(key, []).append(t)
    _need(all(len({t["facility"] for t in rows}) == 1 and len({t["arrival_s"] for t in rows}) == len(rows)
              for rows in pending.values()), "Conflicting pending trips on one occupied truck.")

    arithmetic = Arithmetic(maximum=maximum_primitives)
    issues, shutdowns, finals, losses = [], [], [], []
    total_points = 0
    audit = {"bindings": bindings, "reservations": [{"system": k[0], "truck": k[1], "free_s": v} for k, v in sorted(reserved.items())]}
    def invalid(code, facility=None, at=None): issues.append({"code": code, "facility": facility, "elapsed_s": at})
    try:
        horizons = p["horizons"]
        if (len(horizons) != len(fields) or len({h["facility"] for h in horizons}) != len(fields) or
                any(h["facility"] >= len(fields) or h["repair_s"] != fields[h["facility"]]["repair_s"] for h in horizons)):
            invalid("repair_horizons")
        stopped, extra_issues = set(), []
        for action in p["stops"]:
            fi, w = action["facility"], action["well"]
            if fi >= len(fields) or w >= len(wells): invalid("well_shut_in", fi); continue
            f = fields[fi]
            if (not f["first_well"] <= w < f["first_well"] + f["well_count"] or w in stopped or not wells[w]["productive"] or
                    not 0 <= action["start_s"] <= f["repair_s"] or action["end_s"] != f["repair_s"]): invalid("well_shut_in", fi)
            if w in c["stopped_wells"]: extra_issues.append({"code": "well_shut_in", "facility": fi, "elapsed_s": None})
            stopped.add(w)
        calendars, arrivals, consumed = [], [], 0
        for ji, job in enumerate(p["jobs"]):
            fi = job["facility"]
            if fi >= len(fields) or job["first_trip"] != consumed or not 0 < job["trip_count"] <= len(p["trips"]) - consumed:
                invalid("truck_job", fi); break
            consumed += job["trip_count"]; f = fields[fi]; key = job["system"], job["truck"]
            if (job["system"] != f["system"] or job["truck"] >= fleets[f["system"]]["trucks"] or job["called_s"] < 0 or
                    job["response_min"] != f["response_min"] or job["cycle_min"] != f["cycle_min"]): invalid("truck_job", fi)
            first = arithmetic.add(job["called_s"], arithmetic.mul(f["response_min"], 60))
            cycle = arithmetic.mul(f["cycle_min"], 60); previous = -1.0
            for k, trip in enumerate(p["trips"][job["first_trip"]:consumed]):
                expected = arithmetic.add(first, arithmetic.mul(k, cycle))
                if (trip["arrival_s"] != expected or trip["arrival_s"] <= previous or not 0 <= trip["arrival_s"] < f["repair_s"] or
                        trip["volume_bbl"] != s["truck_capacity_bbl"] or trip["cost"] != s["cost_per_trip"]): invalid("truck_trip", fi)
                previous = trip["arrival_s"]; arrivals.append((fi, previous))
            if job["available_again_s"] != arithmetic.add(previous, cycle) or job["available_again_s"] <= previous: invalid("truck_job", fi)
            if job["called_s"] < reserved.get(key, 0): extra_issues.append({"code": "fleet_overlap", "facility": fi, "elapsed_s": job["called_s"]})
            calendars.append((key, job["called_s"], job["available_again_s"], fi))
        if consumed != len(p["trips"]): invalid("truck_trip")
        for i, (key, start, end, fi) in enumerate(calendars):
            for other, begin, finish, _ in calendars[:i]:
                if key == other and max(start, begin) < min(end, finish): invalid("fleet_overlap", fi, start)
        if not issues and len(p["trips"]) + len(c["pickups"]) > 16384:
            raise ValidationUnavailable("validation_limit", "Combined future load bound exceeded.")
        if not issues: issues.extend(extra_issues)
        if not issues:
            arrivals += [(t["facility"], t["arrival_s"]) for t in c["pickups"]]
            for fi, f in enumerate(fields):
                points = 0
                def point():
                    nonlocal points, total_points
                    if points >= 8192 or total_points >= 262144:
                        raise ValidationUnavailable("validation_limit", "Complete trajectory point bound exceeded.")
                    points += 1; total_points += 1
                stop_at = {w: 0.0 for w in c["stopped_wells"] if f["first_well"] <= w < f["first_well"] + f["well_count"]}
                stop_at.update({a["well"]: a["start_s"] for a in p["stops"] if a["facility"] == fi})
                times = sorted({0.0, f["repair_s"], *stop_at.values(), *(t for field, t in arrivals if field == fi)})
                counts = {}
                for field, t in arrivals:
                    if field == fi: counts[t] = counts.get(t, 0) + 1
                tripped = 0.0 if f["already_shut_in"] or f["initial_bbl"] >= f["shutdown_bbl"] else None
                def rate(t):
                    q = 0.0
                    if tripped is None:
                        for w in range(f["first_well"], f["first_well"] + f["well_count"]):
                            if wells[w]["productive"] and stop_at.get(w, math.inf) > t:
                                r = arithmetic.div(wells[w]["water_bpd"], 86400)
                                if wells[w]["water_bpd"] > 0 and r == 0: raise ValidationUnavailable("numerical_unavailable", "Positive inflow underflow.")
                                q = arithmetic.add(q, r)
                    return q
                anchor, inventory, previous = 0.0, f["initial_bbl"], 0.0
                produced, trucked = 0.0, 0.0
                q = rate(0); volume = inventory
                point(); last_point = (0.0, volume)
                for t in times:
                    if tripped is None and q > 0 and t > previous:
                        offset = arithmetic.op(operator.truediv, arithmetic.sub(f["shutdown_bbl"], inventory), q, infinite=True)
                        root = arithmetic.op(operator.add, anchor, offset, infinite=True)
                        if root <= t:
                            if root <= previous: raise ValidationUnavailable("numerical_unavailable", "Rounded crossing does not advance.")
                            part = arithmetic.op(operator.mul, q, arithmetic.sub(root, previous), infinite=True)
                            produced = arithmetic.op(operator.add, produced, part, infinite=True)
                            previous = root
                            tripped, anchor, inventory, q = root, root, f["shutdown_bbl"], 0.0
                            point(); last_point = (root, inventory)
                    volume = arithmetic.add(inventory, arithmetic.mul(q, arithmetic.sub(t, anchor)))
                    if tripped is None and volume >= f["shutdown_bbl"]:
                        raise ValidationUnavailable("numerical_unavailable", "Rounded volume/crossing disagree.")
                    part = arithmetic.op(operator.mul, q, arithmetic.sub(t, previous), infinite=True)
                    produced = arithmetic.op(operator.add, produced, part, infinite=True)
                    if t != 0 and last_point != (t, volume):
                        point(); last_point = (t, volume)
                    count = counts.get(t, 0)
                    if count:
                        demand = arithmetic.op(operator.mul, count, s["truck_capacity_bbl"], infinite=True)
                        if demand > volume: invalid("insufficient_inventory", fi, t); break
                        if last_point != (t, volume): point()
                        volume = arithmetic.sub(volume, demand)
                        trucked = arithmetic.op(operator.add, trucked, demand, infinite=True)
                        point(); last_point = (t, volume)
                        anchor, inventory = t, volume
                    next_q = rate(t)
                    if next_q != q: anchor, inventory, q = t, volume, next_q
                    previous = t
                if issues: break
                loss = 0.0
                for w in range(f["first_well"], f["first_well"] + f["well_count"]):
                    if not wells[w]["productive"]: continue
                    beginning = min(stop_at.get(w, math.inf), math.inf if tripped is None else tripped)
                    if beginning >= f["repair_s"]: continue
                    oil = arithmetic.mul(wells[w]["oil_bpd"], s["oil_per_bbl"])
                    gas = arithmetic.mul(wells[w]["gas_mcfd"], s["gas_per_mcf"])
                    daily = arithmetic.add(oil, gas); revenue = arithmetic.div(daily, 86400)
                    if ((oil == 0 and wells[w]["oil_bpd"] > 0 and s["oil_per_bbl"] > 0) or
                            (gas == 0 and wells[w]["gas_mcfd"] > 0 and s["gas_per_mcf"] > 0) or (daily > 0 and revenue == 0)):
                        raise ValidationUnavailable("numerical_unavailable", "Positive revenue underflow.")
                    duration = arithmetic.sub(f["repair_s"], beginning); part = arithmetic.mul(revenue, duration)
                    if revenue > 0 and duration > 0 and part == 0: raise ValidationUnavailable("numerical_unavailable", "Positive loss underflow.")
                    loss = arithmetic.add(loss, part)
                if not math.isfinite(produced) or not math.isfinite(trucked):
                    raise ValidationUnavailable("numerical_unavailable", "Future volume ledger cannot be represented.")
                balance = arithmetic.sub(arithmetic.sub(arithmetic.add(f["initial_bbl"], produced), trucked), volume)
                losses.append(loss); finals.append({"facility": fi, "final_bbl": volume,
                    "produced_bbl": produced, "trucked_bbl": trucked, "balance_residual_bbl": balance, "lost_revenue": loss})
                if tripped is not None: shutdowns.append({"facility": fi, "elapsed_s": tripped})
            if not issues:
                truck = arithmetic.mul(len(arrivals), s["cost_per_trip"])
                lost = 0.0
                for loss in losses: lost = arithmetic.add(lost, loss)
                cost = {"truck_cost": truck, "lost_revenue": lost, "total": arithmetic.add(truck, lost)}
        status = "invalid" if issues else "available"
    except ValidationUnavailable as error:
        status = "unavailable"; issues.append({"code": error.code})
    record = {"method": METHOD, "evaluator_method": EVALUATOR, "independent": True, "bindings": bindings,
              "status": status, "cost": cost if status == "available" else None,
              "automatic_shutdown": bool(shutdowns) if status == "available" else None,
              "shutdowns": shutdowns if status == "available" else [],
              "limitations": ["Caller-supplied post-event state; Model/View/History reconstruction and authority are not established here.",
                  "Future committed/new cost only, excluding sunk cost; no optimization or public simulation operation."]}
    record["validation_id"] = "committed-validation-" + _fingerprint((METHOD, bindings, status, record["cost"]))
    audit.update(issues=issues, final_inventories=finals, facility_losses=losses, rounded_primitives=arithmetic.count)
    if evaluated is not None:
        matches = (evaluated.get("method") == EVALUATOR and evaluated.get("bindings") == bindings and
                   evaluated.get("status") == status and evaluated.get("cost") == record["cost"])
        if matches and status == "available":
            actual = [{"facility": f["facility"], "elapsed_s": f["shutdown_s"]} for f in evaluated["facilities"] if f["shut_in"]]
            actual_finals = [{key: f[key] for key in ("facility", "final_bbl", "produced_bbl", "trucked_bbl", "balance_residual_bbl", "lost_revenue")}
                             for f in evaluated["facilities"]]
            matches = actual == shutdowns and actual_finals == finals and evaluated["automatic_shutdown"] == bool(shutdowns)
        if not matches: raise CommittedValidationError("independent_disagreement", "Independent committed-state validation disagrees with the exact bound native result.")
    return CommittedValidation(record, audit)
