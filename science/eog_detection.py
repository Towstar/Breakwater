"""Causal, deterministic EOG event evidence; no files, model calls or controls.

Rules are declared heuristics, not calibrated fault probabilities. Normal off
states cannot start inspection events. Repair expectations never resolve a fault.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass, replace
import hashlib
import json
import math

from eog_model import Facility, Model, ModelError
from eog_observations import ObservationError, Sample, View

METHOD = "eog-detection-0.1.0"


@dataclass(frozen=True, slots=True)
class Rules:
    inspection_samples: int = 4
    zero_flow_bpd: float = 5.0
    zero_flow_fraction: float = 0.02
    leak_fraction: float = 0.10
    leak_absolute_bpd: float = 15.0
    degradation_fraction: float = 0.15
    level_tolerance_pct: float = 0.5
    inflow_balance_fraction: float = 0.05
    inflow_balance_bpd: float = 20.0
    running_upper_fraction: float = 0.05
    max_events: int = 1024

    def __post_init__(self):
        if type(self.inspection_samples) is not int or not 2 <= self.inspection_samples <= 24:
            raise ModelError("invalid_rules", "Inspection confirmation needs 2-24 consecutive running frames.")
        if type(self.max_events) is not int or not 1 <= self.max_events <= 4096:
            raise ModelError("invalid_rules", "Event budget must be in [1, 4096].")
        for name in ("zero_flow_bpd", "zero_flow_fraction", "leak_fraction", "leak_absolute_bpd",
                     "degradation_fraction", "level_tolerance_pct", "inflow_balance_fraction", "inflow_balance_bpd", "running_upper_fraction"):
            value = getattr(self, name)
            if isinstance(value, bool) or not isinstance(value, (int, float)):
                raise ModelError("invalid_rules", "Thresholds must be finite nonnegative numeric values.")
            try:
                finite = math.isfinite(value)
            except OverflowError:
                finite = False
            if not finite or value < 0:
                raise ModelError("invalid_rules", "Thresholds must be finite nonnegative numeric values.")
        if not (0 < self.leak_fraction < 1 and 0 < self.degradation_fraction < 1 and
                self.zero_flow_fraction < 1 and self.inflow_balance_fraction < 1 and self.running_upper_fraction < 1 and
                self.level_tolerance_pct <= 5):
            raise ModelError("invalid_rules", "Unsupported fractional/level threshold range.")


@dataclass(frozen=True, slots=True)
class Event:
    id: str
    system_id: str
    facility_id: str
    asset_id: str
    category: str
    state: str
    first_evidence_unix: int
    detected_unix: int
    onset_unix: int | None
    onset_bounds: tuple[int, int] | None
    updated_unix: int
    end_unix: int | None
    coverage: str
    countdown_hours: float | None
    countdown_state: str
    repair_remaining_hours: float | None
    hourly_revenue_exposure: float | None
    observed_volume_bbl: float | None
    forecast_volume_at_cutoff_bbl: float | None
    assumed_inflow_bpd: float | None
    recommended_action: str
    evidence: tuple[Sample, ...]
    assumptions: tuple[str, ...]
    limitations: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class Coverage:
    facility_id: str
    category: str
    state: str
    reason: str


@dataclass(frozen=True, slots=True)
class Detection:
    method: str
    cutoff_unix: int
    model_fingerprint: str
    evidence_fingerprint: str
    rules_fingerprint: str
    events: tuple[Event, ...]
    coverage: tuple[Coverage, ...]


def _event(f, category, first, previous, evidence):
    asset = f.pipeline_id if category == "pipeline_leak" else f.pump_id
    raw = json.dumps((METHOD, asset, category, first), ensure_ascii=False, separators=(",", ":")).encode()
    identifier = "eog-" + hashlib.sha256(raw).hexdigest()[:32]
    onset = first if previous is not None else None
    bounds = (previous, first) if previous is not None else None
    limitations = (() if previous is not None else
                   ("Left-censored onset: no preceding interpretable non-event evidence.",))
    return Event(identifier, f.system_id, f.id, asset, category, "ongoing", first, first, onset, bounds,
                 first, None, "available", None, "unavailable" if category == "pump_failure" else "not_applicable",
                 None, None, None, None, None,
                 "compare_responses" if category == "pump_failure" else "field_inspection", tuple(evidence), (), limitations)


def _frame(view, channels, timestamp):
    # Detection walks only series returned by causal View methods. No inspection
    # of the archive's hidden future rows, source-now or wall clock occurs here.
    readings = []
    for key in channels:
        sample = channels[key].get(timestamp)
        if sample is None:
            return None
        readings.append(sample)
    return tuple(readings)


def _series(view, keys):
    return {key: {sample.unix: sample for sample in view.series(*key)} for key in keys}


def _keys(f, *, wells=False, receipt=False):
    result = [(f.pump_id, "status"), (f.pump_id, "flow_bpd")]
    if receipt:
        result.append((f.pipeline_id, "receive_flow_bpd"))
    if wells:
        result.extend(((f.id, "inflow_bpd"), (f.tank_id, "level_pct")))
        for well in f.wells:
            result.extend(((well.id, "status"), (well.id, "water_bpd")))
    return result


def _retain(*blocks):
    result = {}
    for block in blocks:
        for sample in block:
            result[(sample.unix, *sample.key)] = sample
    return tuple(sorted(result.values(), key=lambda s: (s.unix, s.asset_id, s.metric)))


def _normal_recovery(f, values, zero, rules):
    status, flow = values[(f.pump_id, "status")].value, values[(f.pump_id, "flow_bpd")].value
    if status == 1 and flow > zero and flow / f.pump_flow_bpd <= 1 + rules.running_upper_fraction:
        return True
    if status == 0 and abs(flow) <= zero:
        level = values.get((f.tank_id, "level_pct"))
        return (level is not None and 0 <= level.value < f.start_pct - rules.level_tolerance_pct and
                all(values.get((w.id, "status")) is not None and values[(w.id, "status")].value in (0, 1)
                    for w in f.wells))
    return False


def _failure_events(model, f, view, rules):
    keys = _keys(f, wells=True)
    data = _series(view, keys)
    status_key, flow_key = keys[:2]
    zero = max(rules.zero_flow_bpd, f.pump_flow_bpd * rules.zero_flow_fraction)
    events, current, last_nonfault = [], None, None
    last_nonfault_evidence = ()
    baseline_productive = None
    baseline_evidence = ()
    event_baselines = {}
    for timestamp, status_sample in data[status_key].items():
        values = {key: samples[timestamp] for key, samples in data.items() if timestamp in samples}
        status = status_sample.value
        if status not in (0, 1, 2):
            continue
        if status == 2:
            if current is None:
                current = _event(f, "pump_failure", timestamp, last_nonfault, (status_sample,))
                # Opportunity exposure may be established by pre-fault states, or
                # by still-producing wells observed before automatic shutdown.
                active_now = tuple(w.id for w in f.wells if (w.id, "status") in values and
                                   values[(w.id, "status")].value == 1)
                all_known = all((w.id, "status") in values and values[(w.id, "status")].value in (0, 1)
                                for w in f.wells)
                event_baselines[current.id] = (baseline_productive if baseline_productive is not None else
                                               active_now if all_known and active_now else None)
                current = replace(current, evidence=_retain(current.evidence, last_nonfault_evidence,
                                  baseline_evidence if baseline_productive is not None else
                                  tuple(values[(w.id, "status")] for w in f.wells if (w.id, "status") in values)))
            current = replace(current, updated_unix=timestamp)
        elif current is not None:
            if flow_key in values and _normal_recovery(f, values, zero, rules):
                evidence = tuple(values[key] for key in (status_key, flow_key))
                current = replace(current, state="resolved", end_unix=timestamp, updated_unix=timestamp,
                                  countdown_state="not_applicable", evidence=_retain(current.evidence, evidence),
                                  assumptions=("Resolution is first coherent observed delivery/normal-off recovery, not a scheduled repair label.",))
                events.append(current)
                current = None
                last_nonfault = timestamp
                last_nonfault_evidence = tuple(values.values())
            # A stopped pump above start or contradictory flow cannot repair a fault.
        else:
            if flow_key in values and _normal_recovery(f, values, zero, rules):
                last_nonfault = timestamp
                last_nonfault_evidence = tuple(values.values())
        if status != 2 and current is None and all((w.id, "status") in values and
                values[(w.id, "status")].value in (0, 1) for w in f.wells):
            baseline_productive = tuple(w.id for w in f.wells if values[(w.id, "status")].value == 1)
            baseline_evidence = tuple(values[(w.id, "status")] for w in f.wells)
    if current is not None:
        current = _forecast_failure(model, f, current, view, rules, event_baselines[current.id])
        events.append(current)
    state = view.cohort(keys).state
    coverage = Coverage(f.id, "pump_failure", "available" if state == "available" else "unavailable",
                        "Current coherent failure-state channels." if state == "available" else
                        "Missing, stale, unsupported or incoherent failure-state channels.")
    if events and events[-1].state == "ongoing" and events[-1].coverage != "available":
        coverage = replace(coverage, state=events[-1].coverage, reason="Ongoing failure has incomplete/contradictory current state.")
    elif state == "available" and not any(e.state == "ongoing" for e in events):
        current = {r.sample.key: r.sample for r in view.cohort(keys).readings}
        if not _normal_recovery(f, current, zero, rules) or not 0 <= current[(f.tank_id, "level_pct")].value <= 100:
            coverage = replace(coverage, state="ambiguous", reason="Non-fault feedback contradicts normal delivery/hysteresis or supported inventory.")
    return events, coverage


def _forecast_failure(model, f, event, view, rules, productive):
    assumptions = ["Onset estimate uses the first observed fault feedback; it is not a hidden event label.",
                   "Pump delivery is assumed zero after compatible fault feedback; raw measured flow is retained."]
    limitations = list(event.limitations)
    exposure = None if productive is None else math.fsum(w.revenue_per_hour for w in f.wells if w.id in productive)
    remaining = None
    if event.onset_unix is not None:
        remaining = model.repair_hours - (view.cutoff_unix - event.onset_unix) / 3600
        if remaining <= 0:
            remaining = None
            limitations.append("Configured repair duration has elapsed while the fault persists; no zero-hour repair is invented.")
    if exposure is None:
        limitations.append("Pre-trip productive wells are unknown; lost revenue is unavailable, not zero.")
    if event.onset_unix is None:
        limitations.append("Failure predates interpretable onset evidence; remaining repair time is unavailable.")
    cohort = view.cohort(_keys(f, wells=True))
    if cohort.state != "available":
        return replace(event, coverage="unavailable", repair_remaining_hours=remaining,
                       hourly_revenue_exposure=exposure, recommended_action="unavailable",
                       evidence=_retain(event.evidence, tuple(r.sample for r in cohort.readings if r.sample is not None)),
                       assumptions=tuple(assumptions), limitations=tuple(limitations + ["Current required channels are missing/stale/incoherent/unsupported."]))
    values = {reading.sample.key: reading.sample for reading in cohort.readings}
    # Forecasts require exact acquisition alignment even if a caller loosens the
    # display/index skew policy. Mixing timestamps is not a physical snapshot.
    timestamps = {sample.unix for sample in values.values()}
    if len(timestamps) != 1:
        return replace(event, coverage="ambiguous", repair_remaining_hours=remaining,
                       hourly_revenue_exposure=exposure, recommended_action="unavailable",
                       assumptions=tuple(assumptions), limitations=tuple(limitations + ["Failure forecast requires a common acquisition timestamp."]))
    timestamp = timestamps.pop()
    level = values[(f.tank_id, "level_pct")].value
    inflow = values[(f.id, "inflow_bpd")].value
    flow = values[(f.pump_id, "flow_bpd")].value
    status = values[(f.pump_id, "status")].value
    well_states = {w.id: values[(w.id, "status")].value for w in f.wells}
    water = [values[(w.id, "water_bpd")].value for w in f.wells]
    zero = max(rules.zero_flow_bpd, f.pump_flow_bpd * rules.zero_flow_fraction)
    invalid = (status != 2 or abs(flow) > zero or not 0 <= level <= 100 or inflow < 0 or any(q < 0 for q in water))
    try:
        water_total = math.fsum(water)
    except OverflowError:
        water_total = math.inf
    if abs(water_total - inflow) > max(rules.inflow_balance_bpd, inflow * rules.inflow_balance_fraction):
        invalid = True
    if any(well_states[w.id] == 0 and abs(values[(w.id, "water_bpd")].value) > rules.zero_flow_bpd for w in f.wells):
        invalid = True
    observed = (level / 100) * f.capacity_bbl
    threshold = (f.shutdown_pct / 100) * f.capacity_bbl
    if not all(math.isfinite(value) for value in (observed, threshold, water_total)):
        invalid = True
    if invalid:
        return replace(event, coverage="ambiguous", repair_remaining_hours=remaining,
                       hourly_revenue_exposure=exposure, observed_volume_bbl=observed if math.isfinite(observed) else None,
                       recommended_action="unavailable", evidence=_retain(event.evidence, tuple(values.values())),
                       assumptions=tuple(assumptions), limitations=tuple(limitations + ["Unsupported inventory, contradictory delivery/status or unbalanced well/inflow measurements; raw evidence is not clipped."]))
    all_shut = all(state == 0 for state in well_states.values())
    available = remaining is not None and exposure is not None
    if all_shut and level >= f.shutdown_pct - rules.level_tolerance_pct and inflow <= rules.zero_flow_bpd:
        assumptions.append("Automatic shut-in inferred from near-threshold level and all wells stopped; opportunity revenue uses observed pre-trip productive wells.")
        return replace(event, coverage="available", countdown_hours=0,
                       countdown_state="already_shutdown", repair_remaining_hours=remaining,
                       hourly_revenue_exposure=exposure, observed_volume_bbl=observed,
                       forecast_volume_at_cutoff_bbl=observed, assumed_inflow_bpd=0,
                       recommended_action="compare_responses" if available else "unavailable", evidence=_retain(event.evidence, tuple(values.values())),
                       assumptions=tuple(assumptions), limitations=tuple(limitations))
    active = tuple(w.id for w in f.wells if well_states[w.id] == 1)
    if ((all_shut and productive != ()) or level >= f.shutdown_pct or
            (productive is not None and set(active) != set(productive))):
        return replace(event, coverage="ambiguous", repair_remaining_hours=remaining,
                       hourly_revenue_exposure=exposure, observed_volume_bbl=observed,
                       recommended_action="unavailable", evidence=_retain(event.evidence, tuple(values.values())),
                       assumptions=tuple(assumptions), limitations=tuple(limitations + ["Well state and automatic threshold do not define a supported current operating state."]))
    age_h = (view.cutoff_unix - timestamp) / 3600
    rate = inflow / 24
    if inflow > 0 and rate == 0:
        return replace(event, coverage="unavailable", repair_remaining_hours=remaining,
                       hourly_revenue_exposure=exposure, recommended_action="unavailable",
                       assumptions=tuple(assumptions), limitations=tuple(limitations + ["Hourly inflow underflows binary64."]))
    if rate == 0:
        countdown, countdown_state, projected = None, "no_future_crossing", observed
    else:
        if threshold <= observed:
            return replace(event, coverage="unavailable", repair_remaining_hours=remaining,
                           hourly_revenue_exposure=exposure, recommended_action="unavailable",
                           assumptions=tuple(assumptions), limitations=tuple(limitations + ["Distinct level/threshold inventories round to an unsupported binary64 margin."]))
        sample_countdown = (threshold - observed) / rate
        if not math.isfinite(sample_countdown):
            return replace(event, coverage="unavailable", repair_remaining_hours=remaining,
                           hourly_revenue_exposure=exposure, recommended_action="unavailable",
                           assumptions=tuple(assumptions), limitations=tuple(limitations + ["Countdown is not representable in binary64."]))
        if sample_countdown <= age_h:
            countdown, countdown_state, projected = 0.0, "predicted_reached_since_sample", threshold
            assumptions.append("The held-rate forecast reaches shutdown between the last sample and cutoff; that transition is predicted, not observed.")
        else:
            countdown, countdown_state = sample_countdown - age_h, "available"
            projected = observed + rate * age_h
    if age_h:
        assumptions.append("Measured inflow is held from its actual acquisition to cutoff; raw level is not retimestamped.")
    return replace(event, coverage="available", countdown_hours=countdown,
                   countdown_state=countdown_state, repair_remaining_hours=remaining,
                   hourly_revenue_exposure=exposure, observed_volume_bbl=observed,
                   forecast_volume_at_cutoff_bbl=projected, assumed_inflow_bpd=inflow,
                   recommended_action="compare_responses" if available else "unavailable", evidence=_retain(event.evidence, tuple(values.values())),
                   assumptions=tuple(assumptions), limitations=tuple(limitations))


def _inspection_events(f, view, rules, category):
    keys = _keys(f, receipt=category == "pipeline_leak")
    data = _series(view, keys)
    status_key, flow_key = keys[:2]
    zero = max(rules.zero_flow_bpd, f.pump_flow_bpd * rules.zero_flow_fraction)
    current, pending_bad, pending_good, previous_good = None, [], [], None
    events, last_interpretable = [], None
    def confirmed(sequence):
        return (len(sequence) == rules.inspection_samples and
                sequence[-1][0] - sequence[0][0] >= (rules.inspection_samples - 1) * 300)

    maximum_gap = min(300, view.policy.max_gap_s)
    for timestamp, status in data[status_key].items():
        frame = _frame(view, data, timestamp)
        if (frame is None or status.value != 1 or frame[1].value <= zero or
                frame[1].value / f.pump_flow_bpd > 1 + rules.running_upper_fraction):
            pending_bad, pending_good = [], []
            continue
        flow = frame[1].value
        if category == "pipeline_leak":
            received = frame[2].value
            if received < 0 or received > flow + max(rules.leak_absolute_bpd, flow * rules.leak_fraction):
                pending_bad, pending_good = [], []
                continue
            bad = flow - received > max(rules.leak_absolute_bpd, flow * rules.leak_fraction)
        else:
            bad = (f.pump_flow_bpd - flow) / f.pump_flow_bpd > rules.degradation_fraction
        last_interpretable = timestamp
        if bad:
            if pending_bad and timestamp - pending_bad[-1][0] > maximum_gap:
                pending_bad = []
            pending_bad.append((timestamp, frame))
            pending_bad = pending_bad[-rules.inspection_samples:]
            pending_good = []
            if current is None and confirmed(pending_bad):
                evidence = tuple(sample for _, frame_samples in pending_bad for sample in frame_samples)
                current = _event(f, category, pending_bad[0][0], previous_good, evidence)
                current = replace(current, detected_unix=timestamp, updated_unix=timestamp, assumptions=(
                    "Onset is the first sample of a sustained evidence-threshold breach, not an authenticated physical fault onset.",))
            elif current is not None:
                initial = current.evidence[:rules.inspection_samples * len(keys)]
                current = replace(current, updated_unix=timestamp, evidence=_retain(initial, frame))
        else:
            if pending_good and timestamp - pending_good[-1][0] > maximum_gap:
                pending_good = []
            pending_good.append((timestamp, frame))
            pending_good = pending_good[-rules.inspection_samples:]
            pending_bad = []
            if current is not None:
                initial = current.evidence[:rules.inspection_samples * len(keys)]
                current = replace(current, updated_unix=timestamp, evidence=_retain(initial, frame))
            if current is not None and confirmed(pending_good):
                current = replace(current, state="resolved", end_unix=pending_good[0][0], updated_unix=timestamp,
                                  evidence=current.evidence + tuple(s for _, readings in pending_good for s in readings),
                                  limitations=current.limitations + ("Recovery is sustained observed flow agreement; it is not an externally confirmed repair endpoint.",))
                events.append(current)
                current = None
            previous_good = timestamp
    cohort = view.cohort(keys)
    latest_stamps = [r.sample.unix for r in cohort.readings if r.sample is not None]
    recent = (cohort.state == "available" and len(set(latest_stamps)) == 1 and
              last_interpretable == latest_stamps[0])
    enough = confirmed(pending_bad) or confirmed(pending_good)
    state = "available" if recent and enough else "ambiguous" if cohort.state == "available" else "unavailable"
    reason = ("Sustained running-flow evidence available." if state == "available" else
              "Normal off/fault/missing/short/noncontemporaneous evidence cannot establish inspection health or repair.")
    if current is not None:
        if not recent:
            current = replace(current, coverage=state, limitations=current.limitations + (
                "The inspection condition is currently masked/unobservable; stopping or failure does not establish repair.",))
        elif not enough:
            current = replace(current, coverage="ambiguous", limitations=current.limitations + (
                "Recovery is not yet sustained for the declared confirmation window.",))
        if category == "pump_degradation" and view.latest(f.pump_id, "status").sample is not None and view.latest(f.pump_id, "status").sample.value == 2:
            current = replace(current, recommended_action="unavailable")
        events.append(current)
    return events, Coverage(f.id, category, state, reason)


def detect(model: Model, view: View, rules: Rules = Rules()) -> Detection:
    if type(model) is not Model or type(view) is not View or type(rules) is not Rules:
        raise ModelError("invalid_input", "Detection needs a compiled Model, causal View and Rules.")
    for channel in model.channels():
        if view.latest(*channel.key).channel != channel:
            raise ModelError("unit_mismatch", "Observation catalog differs from the compiled model's units/status meanings.")
    events, coverage = [], []
    for facility in model.facilities:
        for operation, category in ((_failure_events, "pump_failure"),
                                    (_inspection_events, "pipeline_leak"), (_inspection_events, "pump_degradation")):
            try:
                found, support = (operation(model, facility, view, rules) if category == "pump_failure" else
                                  operation(facility, view, rules, category))
            except ObservationError as error:
                if error.code != "limit_exceeded":
                    raise
                raise ModelError("limit_exceeded", "Detection needs a bounded retained observation horizon.") from error
            events.extend(found)
            coverage.append(support)
            if len(events) > rules.max_events:
                raise ModelError("limit_exceeded", "Detection event budget exceeded; no successful truncated output.")
    events.sort(key=lambda e: (e.first_evidence_unix, e.system_id, e.facility_id, e.category))
    rules_hash = hashlib.sha256(json.dumps(asdict(rules), sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return Detection(METHOD, view.cutoff_unix, model.fingerprint(), view.evidence_fingerprint(), rules_hash,
                     tuple(events), tuple(coverage))


def ongoing_alerts(result: Detection):
    """Failure urgency, then exposure; explicit unknowns follow known countdowns."""
    def priority(event):
        if event.category != "pump_failure":
            return (1, 0, 0, event.id)
        countdown = event.countdown_hours
        return (0, math.inf if countdown is None else countdown,
                0 if event.hourly_revenue_exposure is None else -event.hourly_revenue_exposure, event.id)
    return tuple(sorted((event for event in result.events if event.state == "ongoing"), key=priority))
