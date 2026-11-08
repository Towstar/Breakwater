"""Bounded joint EOG planning with native evaluation and independent selection.

Only the declared finite policy/schedule family is searched. Neither a completed
enumeration of that family nor a fractional-knapsack seed proves a global optimum
over continuous control times and arbitrary dispatch schedules. No files/models.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
from fractions import Fraction
import hashlib
import heapq
import json
import math
import time

from eog_plan_validator import ValidationUnavailable, prepare, validate_prepared
from eog_response import ResponseUnavailable, compile_context, evaluate

METHOD = "eog-joint-search-0.1.3"


class _PlanningDeadline(Exception):
    pass


@dataclass(frozen=True, slots=True)
class Rules:
    maximum_jobs: int = 256
    jobs_per_truck: int = 256
    small_trip_counts: int = 8
    local_well_options: int = 24
    joint_well_options: int = 64
    prefix_pivots: int = 6
    queue_limit: int = 2048
    state_key_limit: int = 20000
    retained_evaluations: int = 8
    retained_bytes: int = 3500000

    def __post_init__(self):
        bounds = {"maximum_jobs": (1, 256), "jobs_per_truck": (1, 256), "small_trip_counts": (1, 64),
                  "local_well_options": (2, 128), "joint_well_options": (2, 256), "prefix_pivots": (1, 32),
                  "queue_limit": (1, 10000), "state_key_limit": (1, 100000), "retained_evaluations": (2, 64),
                  "retained_bytes": (4096, 3800000)}
        for key, (low, high) in bounds.items():
            value = getattr(self, key)
            if type(value) is not int or not low <= value <= high:
                raise ValueError("Unsupported search rule: " + key)


@dataclass(frozen=True, order=True, slots=True)
class Job:
    facility: int
    truck: int
    call_s: float
    trips: int


@dataclass(frozen=True, slots=True)
class SearchResult:
    result: dict
    audit: dict
    validation: object | None


def _bytes(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, allow_nan=False, separators=(",", ":")).encode()


def _hash(value):
    return hashlib.sha256(_bytes(value)).hexdigest()


def _arrivals(snapshot, job):
    f = snapshot["facilities"][job.facility]
    first = job.call_s + f["response_min"] * 60
    cycle = f["cycle_min"] * 60
    return tuple(first + k * cycle for k in range(job.trips))


def _return(snapshot, job):
    return _arrivals(snapshot, job)[-1] + snapshot["facilities"][job.facility]["cycle_min"] * 60


def _well_data(snapshot, fi):
    f = snapshot["facilities"][fi]
    result = []
    for wi in range(f["first_well"], f["first_well"] + f["well_count"]):
        w = snapshot["wells"][wi]
        if w["productive"]:
            q = w["water_bpd"] / 86400
            revenue = (w["oil_bpd"] * snapshot["oil_per_bbl"] + w["gas_mcfd"] * snapshot["gas_per_mcf"]) / 86400
            if not math.isfinite(q) or not math.isfinite(revenue):
                raise ResponseUnavailable("search_numerical_limit", "Search seed economics cannot be represented.")
            result.append((wi, Fraction(q), Fraction(revenue)))
    return result


def _greedy(snapshot, fi, horizon, removed, order="ratio", *, target=None):
    """Exact-rational terminal/prefix relaxation used only to generate controls.

    For no trucks, positive fixed rates give a fractional-knapsack stopped-volume
    relaxation. With trips, prefix/full-load constraints still need real evaluation.
    This seed never supplies the reported physical feasibility or candidate cost.
    """
    f = snapshot["facilities"][fi]
    if f["already_shut_in"] or horizon <= 0:
        return (), Fraction()
    wells = [(wi, q, r) for wi, q, r in _well_data(snapshot, fi) if q > 0]
    needed = Fraction(f["initial_bbl"]) + sum((q * Fraction(horizon) for _, q, _ in wells), Fraction())
    needed -= Fraction(removed) + Fraction(f["shutdown_bbl"] if target is None else target)
    if needed <= 0:
        return (), Fraction()
    if order == "ratio": key = lambda w: (w[2] / w[1], w[0])
    elif order == "revenue": key = lambda w: (w[2], w[0])
    else: key = lambda w: (-w[1], w[0])
    stops, estimate = [], Fraction()
    for wi, q, revenue in sorted(wells, key=key):
        volume = min(needed, q * Fraction(horizon))
        duration = volume / q
        start = float(Fraction(horizon) - duration)
        stops.append((wi, start))
        estimate += revenue * (Fraction(f["repair_s"]) - Fraction(start))
        needed -= volume
        if needed <= 0: break
    return tuple(sorted(stops)), estimate


def _shift(stops, direction, steps=1):
    result = []
    for wi, t in stops:
        if t > 0:
            for _ in range(steps): t = math.nextafter(t, direction)
        result.append((wi, t))
    return tuple(result)


def _pivot_times(values, maximum):
    values = sorted(set(values))
    if len(values) <= maximum: return values
    indices = {round(i * (len(values) - 1) / (maximum - 1)) for i in range(maximum)} if maximum > 1 else {0}
    return [values[i] for i in sorted(indices)]


def _local_policies(snapshot, fi, schedule, rules, check):
    check()
    f = snapshot["facilities"][fi]
    arrivals = sorted(t for job in schedule if job.facility == fi for t in _arrivals(snapshot, job))
    # These are optimistic rational seed volumes, not evaluated inventories.
    # Convert each finite load before multiplying: a legal binary64 load times
    # a bounded integer count may overflow binary64 while the seed remains exact.
    capacity = Fraction(snapshot["truck_capacity_bbl"])
    horizon = f["repair_s"]
    pivots = [horizon, *_pivot_times(arrivals, rules.prefix_pivots)]
    targets = [f["shutdown_bbl"]]
    for steps in (1, 4):
        target = f["shutdown_bbl"]
        for _ in range(steps): target = math.nextafter(target, 0.0)
        targets.append(target)
    # A volume-ULP seed changes the proposed control time, never the physical
    # shutdown threshold. Time ULPs alone may be much smaller than a volume ULP.
    preferred_map = {}
    for t in pivots:
        policy, _ = _greedy(snapshot, fi, t, sum(a < t for a in arrivals) * capacity, target=targets[1])
        for wi, value in policy: preferred_map[wi] = min(preferred_map.get(wi, horizon), value)
    preferred = _shift(tuple(sorted(preferred_map.items())), 0.0)
    options = {(), preferred}
    for order in ("ratio", "revenue", "water"):
        check()
        combined = {}
        for t in pivots:
            check()
            for target in targets:
                policy, _ = _greedy(snapshot, fi, t, sum(a < t for a in arrivals) * capacity, order, target=target)
                if policy:
                    options.update((policy, _shift(policy, 0.0), _shift(policy, 0.0, 4), _shift(policy, horizon)))
                    for wi, start in policy: combined[wi] = min(combined.get(wi, horizon), start)
        if combined:
            policy = tuple(sorted(combined.items()))
            options.update((policy, _shift(policy, 0.0), _shift(policy, 0.0, 4)))
    options.add(tuple((wi, 0.0) for wi, _, _ in _well_data(snapshot, fi)))
    rates = {wi: revenue for wi, _, revenue in _well_data(snapshot, fi)}
    def estimate(policy):
        return sum((rates[wi] * (Fraction(horizon) - Fraction(t)) for wi, t in policy), Fraction())
    ordered = sorted(options - {(), preferred}, key=lambda p: (estimate(p), p))
    menu = list(dict.fromkeys([(), preferred, *ordered]))[:rules.local_well_options]
    return tuple(menu), preferred


def _well_combinations(snapshot, schedule, rules, check, targets):
    # The first baseline evaluation establishes economic targets before this
    # generator resumes. Already-shut/no-loss facilities cannot benefit from
    # nonnegative-cost controls in the declared independent-facility model.
    yield ()
    local = [_local_policies(snapshot, i, schedule, rules, check) if i in targets else (((),), ())
             for i in range(len(snapshot["facilities"]))]
    preferred = tuple(policy for _, policy in local)
    empty = tuple(() for _ in local)
    seen = {()}
    def emit(choices):
        stops = tuple(sorted(item for policy in choices for item in policy))
        if stops in seen or len(seen) >= rules.joint_well_options: return None
        seen.add(stops); return stops
    for choices in (empty, preferred, tuple(_shift(p, 0.0, 4) for p in preferred)):
        value = emit(choices)
        if value is not None: yield value
    # Uniform menu indices and deviations preserve an optimized background even
    # when a full Cartesian product would exceed the declared generation bound.
    maximum = max((len(menu) for menu, _ in local), default=0)
    for index in range(1, maximum):
        value = emit(tuple(menu[min(index, len(menu) - 1)] for menu, _ in local))
        if value is not None: yield value
    for fi, (menu, _) in enumerate(local):
        check()
        for policy in menu:
            for background in (preferred, empty):
                choices = list(background); choices[fi] = policy
                value = emit(tuple(choices))
                if value is not None: yield value
                if len(seen) >= rules.joint_well_options: return


def make_plan(context, schedule, stops, clock_context, identity):
    snapshot = context["snapshot"]
    plan = {"context": clock_context, "start_unix": context["detection"].cutoff_unix,
            "repair_deadlines": [{"facility_id": f.id, "repair_elapsed_s": n["repair_s"]}
                                 for f, n in zip(context["facilities"], snapshot["facilities"])],
            "truck_jobs": [], "well_shut_ins": [], "inspection_assets": []}
    for spec in schedule:
        f, n = context["facilities"][spec.facility], snapshot["facilities"][spec.facility]
        truck = "truck-" + _hash(f.system_id)[:16] + "-" + str(spec.truck + 1)
        plan["truck_jobs"].append({"job_id": "job-" + _hash((f.id, truck, spec.call_s, spec.trips))[:32],
            "truck_id": truck, "system_id": f.system_id, "facility_id": f.id, "called_elapsed_s": spec.call_s,
            "response_min": n["response_min"], "cycle_min": n["cycle_min"], "available_again_elapsed_s": _return(snapshot, spec),
            "trips": [{"arrival_elapsed_s": t, "volume_bbl": snapshot["truck_capacity_bbl"], "cost": snapshot["cost_per_trip"]}
                      for t in _arrivals(snapshot, spec)]})
    by_index = {index: (fi, well) for well, (fi, index) in context["well_indices"].items()}
    for wi, start in stops:
        fi, well = by_index[wi]
        plan["well_shut_ins"].append({"facility_id": context["facilities"][fi].id, "well_id": well,
                                     "start_elapsed_s": start, "end_elapsed_s": snapshot["facilities"][fi]["repair_s"]})
    plan["plan_id"] = "plan-" + _hash(plan)[:32]
    plan["plan_hash"] = identity(plan)
    return plan


def _call_times(snapshot, fi, free):
    f = snapshot["facilities"][fi]
    response, horizon = f["response_min"] * 60, f["repair_s"]
    q = sum(float(rate) for _, rate, _ in _well_data(snapshot, fi))
    times = {free}
    if q > 0:
        ready = (snapshot["truck_capacity_bbl"] - f["initial_bbl"]) / q - response
        if math.isfinite(ready): times.update((max(free, ready), max(free, math.nextafter(ready, math.inf))))
        crossing = (f["shutdown_bbl"] - f["initial_bbl"]) / q - response
        if math.isfinite(crossing): times.update((max(free, crossing), max(free, math.nextafter(crossing, 0.0))))
    for fraction in (0.25, 0.5, 0.75): times.add(max(free, fraction * horizon - response))
    return sorted(t for t in times if math.isfinite(t) and t >= 0 and t + response < horizon)


def _trip_counts(snapshot, fi, call, rules, check):
    f = snapshot["facilities"][fi]
    first, cycle = call + f["response_min"] * 60, f["cycle_min"] * 60
    if cycle <= 0 or first + cycle <= first: return ()
    ratio = (f["repair_s"] - first) / cycle
    maximum = min(4095, math.ceil(ratio)) if math.isfinite(ratio) else 4095
    while maximum > 0 and first + (maximum - 1) * cycle >= f["repair_s"]: maximum -= 1
    counts = set(range(1, min(rules.small_trip_counts, maximum) + 1))
    if maximum > 0: counts.add(maximum)
    q = sum(float(rate) for _, rate, _ in _well_data(snapshot, fi))
    needed = (f["initial_bbl"] + q * f["repair_s"] - f["shutdown_bbl"]) / snapshot["truck_capacity_bbl"]
    if math.isfinite(needed):
        for value in range(math.ceil(needed) - 1, math.ceil(needed) + 2):
            if 1 <= value <= maximum: counts.add(value)
    wells = [(wi, q, revenue) for wi, q, revenue in _well_data(snapshot, fi) if q > 0]
    required = Fraction(f["initial_bbl"]) + sum((q * Fraction(f["repair_s"]) for _, q, _ in wells), Fraction()) - Fraction(f["shutdown_bbl"])
    removed = Fraction()
    for _, q, _ in sorted(wells, key=lambda w: (w[2] / w[1], w[0])):
        check()
        boundary = (required - removed) / Fraction(snapshot["truck_capacity_bbl"])
        for value in range(math.floor(boundary) - 1, math.ceil(boundary) + 2):
            if 1 <= value <= maximum: counts.add(value)
        removed += q * Fraction(f["repair_s"])
    def priority(count):
        check()
        _, loss = _greedy(snapshot, fi, f["repair_s"], count * Fraction(snapshot["truck_capacity_bbl"]))
        return Fraction(snapshot["cost_per_trip"]) * count + loss, count
    return tuple(sorted(counts, key=priority))


def _children(context, schedule, rules, check, targets):
    if len(schedule) >= rules.maximum_jobs: return
    snapshot = context["snapshot"]
    total_trips = sum(j.trips for j in schedule)
    for sid in sorted({f["system"] for f in snapshot["facilities"]}):
        fleet = snapshot["fleets"][sid]
        used = {j.truck for j in schedule if snapshot["facilities"][j.facility]["system"] == sid}
        first_unused = next((i for i in range(fleet["trucks"]) if i not in used), None)
        for truck in sorted(used | ({first_unused} if first_unused is not None else set())):
            check()
            own = [j for j in schedule if j.truck == truck and snapshot["facilities"][j.facility]["system"] == sid]
            if len(own) >= rules.jobs_per_truck: continue
            free = max((_return(snapshot, j) for j in own), default=0.0)
            for fi, f in enumerate(snapshot["facilities"]):
                check()
                if f["system"] != sid or fi not in targets: continue
                for call in _call_times(snapshot, fi, free):
                    for count in _trip_counts(snapshot, fi, call, rules, check):
                        if total_trips + count <= 16384:
                            yield tuple(sorted((*schedule, Job(fi, truck, call, count))))


def _estimate(snapshot, schedule, check):
    cost = Fraction(snapshot["cost_per_trip"]) * sum(j.trips for j in schedule)
    for fi, f in enumerate(snapshot["facilities"]):
        check()
        removed = sum(j.trips for j in schedule if j.facility == fi) * Fraction(snapshot["truck_capacity_bbl"])
        _, loss = _greedy(snapshot, fi, f["repair_s"], removed)
        cost += loss
    try: return float(cost)
    except OverflowError: return math.inf


def _rank(evaluation):
    p = evaluation["plan"]
    return (evaluation["cost"]["total"], len(p["truck_jobs"]) + len(p["well_shut_ins"]),
            sum(len(j["trips"]) for j in p["truck_jobs"]), p["plan_hash"])


def _family(evaluation):
    plan = evaluation["plan"]
    return (bool(plan["truck_jobs"]), bool(plan["well_shut_ins"]))


def _retain(evaluations, baseline_hash, maximum):
    ordered = sorted(evaluations.values(), key=_rank)
    # The baseline and overall best take precedence under unusually small
    # retention budgets; defaults also retain each available response family.
    keep = [baseline_hash, ordered[0]["plan"]["plan_hash"]]
    families = {}
    for item in ordered:
        families.setdefault(_family(item), item["plan"]["plan_hash"])
    keep.extend(families.values())
    keep.extend(item["plan"]["plan_hash"] for item in ordered)
    selected = set(list(dict.fromkeys(keep))[:maximum])
    return {key: value for key, value in evaluations.items() if key in selected}


def search(model, view, native, *, identity, clock_context, candidate_limit=10000,
           time_limit_s=90.0, rules=Rules(), clock=time.perf_counter):
    if type(candidate_limit) is not int or not 1 <= candidate_limit <= 10000:
        raise ValueError("Candidate limit must be an integer in [1,10000].")
    if isinstance(time_limit_s, bool) or not isinstance(time_limit_s, (int, float)) or not math.isfinite(time_limit_s) or not 0 < time_limit_s <= 120:
        raise ValueError("Planning time limit must be finite in (0,120].")
    if clock_context not in ("snapshot_now", "historical_cursor"):
        raise ValueError("Unsupported planning clock context.")
    # Input/capture loading and causal snapshot compilation are explicitly outside
    # the planning clock. Both independent reconstructions precede the timed loop.
    context = compile_context(model, view)
    prepared = prepare(model, view)
    start = clock(); deadline = start + time_limit_s
    search_deadline = deadline - min(2.0, time_limit_s * 0.2)
    frontier = [(0.0, 0, ())]; serial = 0
    seen_states = {_hash(())}; seen_plans = set(); retained = {}
    baseline_hash = None; unretained_best = math.inf
    targets = set()
    counts = {"evaluated": 0, "admissible": 0, "invalid": 0, "numerical_unavailable": 0,
              "output_unavailable": 0, "visited_states": 0, "queued_peak": 1, "dropped_states": 0,
              "plan_action_limit": 0}
    reason = None
    def capped():
        return counts["evaluated"] >= candidate_limit or clock() >= search_deadline
    def check_time():
        if clock() >= search_deadline: raise _PlanningDeadline()
    while frontier and not capped():
        _, _, schedule = heapq.heappop(frontier); counts["visited_states"] += 1
        policies = iter(_well_combinations(context["snapshot"], schedule, rules, check_time, targets))
        while True:
            try: stops = next(policies)
            except StopIteration: break
            except _PlanningDeadline:
                reason = "planning_time_limit"; break
            if capped(): break
            if len(stops) > 256:
                counts["plan_action_limit"] += 1; continue
            plan = make_plan(context, schedule, stops, clock_context, identity)
            if plan["plan_hash"] in seen_plans: continue
            seen_plans.add(plan["plan_hash"]); counts["evaluated"] += 1
            try:
                evaluated = evaluate(context, plan, native)
            except ResponseUnavailable:
                counts["numerical_unavailable"] += 1
                if not schedule and not stops:
                    raise ResponseUnavailable("baseline_unavailable", "The no-action baseline cannot be represented; no comparison is selected.")
                continue
            if not evaluated["feasible"]:
                if not schedule and not stops:
                    raise ResponseUnavailable("baseline_unavailable", "The complete no-action baseline was not assessed as admissible.")
                counts["invalid"] += 1; continue
            counts["admissible"] += 1
            if baseline_hash is None:
                baseline_hash = plan["plan_hash"]
                shutdowns = {item["facility_id"]: item["elapsed_s"] for item in evaluated["shutdowns"]}
                for fi, (f, state) in enumerate(zip(context["facilities"], context["snapshot"]["facilities"])):
                    if (not state["already_shut_in"] and f.id in shutdowns and shutdowns[f.id] < state["repair_s"] and
                            any(revenue > 0 for _, _, revenue in _well_data(context["snapshot"], fi))):
                        targets.add(fi)
            if len(_bytes(evaluated)) > rules.retained_bytes:
                if not schedule and not stops:
                    raise ResponseUnavailable("search_output_limit", "The complete no-action baseline exceeds the retained output bound.")
                counts["output_unavailable"] += 1
                unretained_best = min(unretained_best, evaluated["cost"]["total"])
                continue
            retained[plan["plan_hash"]] = evaluated
            retained = _retain(retained, baseline_hash, rules.retained_evaluations)
            if evaluated["cost"]["total"] == 0:
                reason = "nonnegative_cost_bound"; break
        if reason == "nonnegative_cost_bound": break
        if capped(): reason = "candidate_limit" if counts["evaluated"] >= candidate_limit else "planning_time_limit"; break
        try:
            for child in _children(context, schedule, rules, check_time, targets):
                if capped(): reason = "planning_time_limit"; break
                key = _hash([(j.facility, j.truck, j.call_s, j.trips) for j in child])
                if key in seen_states: continue
                if len(frontier) >= rules.queue_limit or len(seen_states) >= rules.state_key_limit:
                    counts["dropped_states"] += 1; continue
                seen_states.add(key); serial += 1
                heapq.heappush(frontier, (_estimate(context["snapshot"], child, check_time), serial, child))
                counts["queued_peak"] = max(counts["queued_peak"], len(frontier))
        except _PlanningDeadline:
            reason = "planning_time_limit"
        if reason: break
    if reason is None and frontier:
        reason = "candidate_limit" if counts["evaluated"] >= candidate_limit else "planning_time_limit"
    complete = (not frontier and reason is None and counts["dropped_states"] == 0 and
                counts["numerical_unavailable"] == 0 and counts["output_unavailable"] == 0)
    complete = complete and counts["plan_action_limit"] == 0
    best = min(retained.values(), key=_rank) if retained else None
    if best is not None and unretained_best < best["cost"]["total"]:
        raise ResponseUnavailable("search_output_limit", "A cheaper candidate's complete trajectory cannot be retained within the output bound.")
    # Retain the baseline and complete selected trace; expensive alternatives may
    # be omitted under the declared retained-result budget, never decimated.
    if best is not None:
        while len(_bytes(list(retained.values()))) > rules.retained_bytes:
            choices = [e for e in retained.values() if e is not best and e["plan"]["plan_hash"] != baseline_hash]
            if not choices:
                raise ResponseUnavailable("search_output_limit", "Complete baseline and selected trajectories exceed the output bound.")
            retained.pop(max(choices, key=_rank)["plan"]["plan_hash"])
    validation = None
    if best is not None:
        try:
            validation = validate_prepared(prepared, best["plan"], native.method, deadline=deadline, clock=clock)
        except ValidationUnavailable as error:
            if error.code != "validation_time_limit": raise
            reason = "selected_validation_time_limit"; complete = False
        if validation is not None:
            checked = validation.record
            if (not checked["feasible"] or not checked["independent"] or
                    checked["plan_id"] != best["plan"]["plan_id"] or checked["plan_hash"] != best["plan"]["plan_hash"] or
                    checked["evaluator_method"] != native.method or checked["validated_horizons"] != best["plan"]["repair_deadlines"] or
                    any(checked[key] != best[key] for key in ("cost", "outcome", "shutdowns"))):
                raise ResponseUnavailable("independent_disagreement", "Independent validation disagrees with the selected native candidate.")
    limitations = ["Method " + METHOD + "; finite policy family only, not global optimization over continuous actions.",
        "Planning time excludes protected loading and the two causal snapshot preparations; it includes candidate evaluation and selected-plan validation.",
        "All current failures are modeled together; no historical dispatch is assumed and evolving prior commitments are unsupported.",
        "Already-shut or native-baseline zero-loss facilities receive no cost-increasing controls; other facilities remain in the complete cost/forecast context.",
        "Control seeds target the input shutdown inventory or one/four representable inventory steps below it; evaluation always uses the original input threshold, without feasibility tolerance or clipping.",
        "Full baseline/selected traces are retained; at most " + str(rules.retained_evaluations) + " evaluations are returned.",
        "Available no-action/truck-only/well-only/mixed family leaders are retained when the result budget permits; small count/byte budgets may omit comparisons.",
        "Only the selected plan is independently recomputed in this search; other retained native evaluations require their own validate_plan request.",
        "Inspection timing and physical onset uncertainty remain unqualified."]
    if reason: limitations.append("Termination: " + reason + ".")
    if counts["dropped_states"]: limitations.append("Frontier/state-key bound omitted " + str(counts["dropped_states"]) + " states; enumeration is incomplete.")
    if counts["numerical_unavailable"]: limitations.append(str(counts["numerical_unavailable"]) + " candidate evaluations were numerically unavailable, not demonstrated infeasible.")
    if counts["plan_action_limit"]: limitations.append(str(counts["plan_action_limit"]) + " generated policies exceeded the public action bound and were not clipped.")
    if validation is not None:
        limitations.append("Selected plan independently recomputed by " + validation.record["validator_method"] +
                           "; record " + validation.record["validation_id"] + ". Use validate_plan for the complete bound record; this is not operating approval.")
    elif best is not None:
        limitations.append("No recommendation is selected because independent validation did not finish within the planning budget.")
    rules_id = _hash(asdict(rules))[:16]
    result = {"evaluations": sorted(retained.values(), key=_rank),
              "selected_plan_id": best["plan"]["plan_id"] if validation is not None else None,
              "search": {"scope": "enumerated_space" if complete else "bounded_search",
                  "optimality": "proven_within_space" if complete and validation is not None else "best_found" if validation is not None else "unavailable",
                  "space_description": (METHOD + " rules=" + rules_id + "; complete current failures; terminal/prefix greedy well policies and ULP neighbors; "
                      + str(rules.joint_well_options) + " joint-policy combinations/schedule; yard/stock/crossing/quarter-horizon calls; small/needed/economic/extreme trip counts; "
                      + str(rules.maximum_jobs) + " jobs total, " + str(rules.jobs_per_truck) + " jobs/truck; symmetric unused trucks canonicalized; no controls in already-shut/zero-loss fields. No continuous/global claim."),
                  "action_time_resolution_s": None, "evaluated_candidates": counts["evaluated"], "candidate_limit": candidate_limit,
                  "time_limit_s": time_limit_s, "elapsed_s": max(0.0, clock() - start), "complete": complete},
              "limitations": limitations}
    audit = {"method": METHOD, "rules": asdict(rules), "counts": counts, "termination": reason,
             "baseline_plan_hash": baseline_hash, "selected_plan_hash": best["plan"]["plan_hash"] if validation is not None else None,
             "independent_validation": validation.record if validation is not None else None}
    return SearchResult(result, audit, validation)
