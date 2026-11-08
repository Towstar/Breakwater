"""Compile a causal failed-pump state and exact candidate to native flat indices.

No filesystem, public-schema ownership, optimization, validation claim or model
API. The public worker validates the request/capture before calling this module.
"""
from __future__ import annotations

import math

from eog_detection import detect

METHOD = "eog-response-0.1.0"


class ResponseUnavailable(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


def compile_context(model, view):
    detection = detect(model, view)
    if any(item.category == "pump_failure" and item.state != "available" for item in detection.coverage):
        raise ResponseUnavailable("failure_context_incomplete", "Current failure coverage is incomplete; the provider cannot establish the complete shared-fleet problem.")
    active = {event.facility_id: event for event in detection.events
              if event.category == "pump_failure" and event.state == "ongoing"}
    if not active:
        raise ResponseUnavailable("no_active_failure", "Response evaluation needs at least one current pump failure.")
    # First E3 provider evaluates the complete current failure set. This prevents
    # silently omitting a concurrent failure or granting each facility a fresh fleet.
    facilities = tuple(f for f in model.facilities if f.id in active)
    system_indices = {system.id: i for i, system in enumerate(model.systems)}
    snapshot = {"oil_per_bbl": model.oil_per_bbl, "gas_per_mcf": model.gas_per_mcf,
                "truck_capacity_bbl": model.truck_capacity_bbl, "cost_per_trip": model.cost_per_trip,
                "fleets": [{"trucks": system.fleet_size} for system in model.systems], "facilities": [], "wells": []}
    well_indices = {}
    assumptions = ["Synthetic fixed-rate failed-pump forecast ends at each causal expected repair deadline.",
                   "All current pump failures share each system's whole snapshot fleet; prior scenario commitments are not modeled.",
                   "The facility inflow meter is authoritative for total future inflow. Per-well contributions are allocated in proportion to current measured well water; this explicit reconciliation retains the raw captured evidence.",
                   "Automatic shutdown never restarts wells before repair, including after a later pickup."]
    for fi, f in enumerate(facilities):
        event = active[f.id]
        if (event.coverage != "available" or event.onset_unix is None or event.productive_well_ids is None or
                event.forecast_volume_at_cutoff_bbl is None or event.recommended_action == "unavailable"):
            raise ResponseUnavailable("failure_state_unavailable", "A current failure lacks coherent inventory, causal onset or known pre-trip well economics.")
        # Direct seconds from captured ops and integer times, never hours -> seconds
        # through a rounded countdown or remaining-hours display field.
        repair = model.repair_hours * 3600 - (view.cutoff_unix - event.onset_unix)
        if not math.isfinite(repair) or not 0 < repair <= 604800:
            raise ResponseUnavailable("repair_unavailable", "The causal repair deadline is unknown, elapsed or unrepresentable.")
        already = event.countdown_state in ("already_shutdown", "predicted_reached_since_sample")
        first_well = len(snapshot["wells"])
        measured_water = []
        for well in f.wells:
            well_indices[well.id] = (fi, len(snapshot["wells"]))
            reading = view.latest(well.id, "water_bpd")
            if reading.state != "available" or reading.sample is None:
                raise ResponseUnavailable("well_rate_unavailable", "A current per-well rate is missing or stale.")
            productive = well.id in event.productive_well_ids
            water = 0 if already else reading.sample.value
            if not productive and water != 0:
                raise ResponseUnavailable("well_rate_ambiguous", "A pre-trip inactive well has nonzero measured water; it is not silently clipped.")
            measured_water.append(water)
            snapshot["wells"].append({"water_bpd": water, "oil_bpd": well.oil_bpd,
                                       "gas_mcfd": well.gas_mcfd, "productive": int(productive)})
        if not already:
            measured_total = math.fsum(measured_water)
            authoritative = event.assumed_inflow_bpd
            if authoritative is None or ((authoritative == 0) != (measured_total == 0)):
                raise ResponseUnavailable("inflow_ambiguous", "A zero/nonzero meter disagreement cannot be reconciled into a response forecast.")
            if measured_total > 0:
                # Use the largest contributor for the residual, avoiding loss of
                # a small positive contribution from subtraction cancellation.
                residual_index = max(range(len(measured_water)), key=measured_water.__getitem__)
                allocated = [authoritative * (q / measured_total) for q in measured_water]
                allocated[residual_index] = authoritative - math.fsum(q for i, q in enumerate(allocated) if i != residual_index)
                if any(not math.isfinite(q) or q < 0 or (raw > 0 and q == 0) for raw, q in zip(measured_water, allocated)):
                    raise ResponseUnavailable("inflow_unrepresentable", "Per-well inflow allocation cannot be represented; no rates are clipped.")
                for i, q in enumerate(allocated):
                    snapshot["wells"][first_well + i]["water_bpd"] = q
        snapshot["facilities"].append({"system": system_indices[f.system_id], "first_well": first_well,
            "well_count": len(f.wells), "already_shut_in": int(already), "capacity_bbl": f.capacity_bbl,
            "initial_bbl": event.forecast_volume_at_cutoff_bbl, "shutdown_bbl": (f.shutdown_pct / 100) * f.capacity_bbl,
            "repair_s": repair, "response_min": f.response_min, "cycle_min": f.cycle_min})
        assumptions.extend(event.assumptions)
    assets = {s.disposal_id for s in model.systems}
    for f in model.facilities:
        assets.update((f.id, f.tank_id, f.pump_id, f.pipeline_id, *(w.id for w in f.wells)))
    return {"snapshot": snapshot, "facilities": facilities, "facility_indices": {f.id: i for i, f in enumerate(facilities)},
            "system_indices": system_indices, "well_indices": well_indices,
            "known_assets": assets,
            "assumptions": list(dict.fromkeys(assumptions)), "detection": detection}


def native_plan(context, plan):
    """Retain every claim; map opaque IDs without changing candidate actions."""
    facilities, systems, wells = context["facility_indices"], context["system_indices"], context["well_indices"]
    invalid = []
    def index(mapping, asset, kind):
        if asset not in mapping:
            invalid.append({"code": "unknown_" + kind, "message": "Candidate references an asset outside the current response context.", "asset_id": asset})
            return 0xffffffff
        return mapping[asset]
    result = {"horizons": [], "stops": [], "jobs": [], "trips": []}
    for horizon in plan["repair_deadlines"]:
        result["horizons"].append({"facility": index(facilities, horizon["facility_id"], "facility"),
                                    "repair_s": horizon["repair_elapsed_s"]})
    for stop in plan["well_shut_ins"]:
        well = index(wells, stop["well_id"], "well")
        result["stops"].append({"facility": index(facilities, stop["facility_id"], "facility"),
                                "well": well[1] if isinstance(well, tuple) else well,
                                "start_s": stop["start_elapsed_s"], "end_s": stop["end_elapsed_s"]})
    # Truck IDs are opaque labels for the shared physical slots. Counting unique
    # labels per system prevents inventing extra trucks without imposing ID syntax.
    trucks = {}
    for job in plan["truck_jobs"]:
        system = index(systems, job["system_id"], "system")
        labels = trucks.setdefault(system, {})
        truck = labels.setdefault(job["truck_id"], len(labels))
        result["jobs"].append({"facility": index(facilities, job["facility_id"], "facility"),
            "system": system, "truck": truck, "first_trip": len(result["trips"]), "trip_count": len(job["trips"]),
            "called_s": job["called_elapsed_s"], "response_min": job["response_min"], "cycle_min": job["cycle_min"],
            "available_again_s": job["available_again_elapsed_s"]})
        result["trips"].extend({"arrival_s": trip["arrival_elapsed_s"], "volume_bbl": trip["volume_bbl"], "cost": trip["cost"]}
                               for trip in job["trips"])
    # Inspection recommendations have no simulated repair/production effect.
    for asset in plan["inspection_assets"]:
        if asset not in context["known_assets"]:
            invalid.append({"code": "unknown_inspection_asset", "message": "Inspection asset is outside the response context.", "asset_id": asset})
    return result, invalid


def evaluate(context, plan, native):
    indexed, violations = native_plan(context, plan)
    if violations:
        computed = None
    else:
        computed = native.evaluate(context["snapshot"], indexed)
        if computed["status"] == "unavailable":
            raise ResponseUnavailable("numerical_unavailable", "Native evaluation could not represent a complete trajectory or cost; no feasibility verdict is issued.")
        for violation in computed["violations"]:
            item = {"code": violation["code"], "message": "Native evaluator rejected the supplied candidate claim or action."}
            if violation["facility"] is not None and violation["facility"] < len(context["facilities"]):
                item["asset_id"] = context["facilities"][violation["facility"]].id
            if violation["elapsed_s"] is not None:
                item["elapsed_s"] = violation["elapsed_s"]
            violations.append(item)
    valid = computed is not None and computed["status"] == "available"
    for violation in violations:
        violation.setdefault("asset_id", None)
        violation.setdefault("elapsed_s", None)
    trajectories, shutdowns = [], []
    if valid:
        for result in computed["facilities"]:
            f = context["facilities"][result["facility"]]
            trajectories.append({"facility_id": f.id, "tank_id": f.tank_id, "capacity_bbl": f.capacity_bbl,
                "shutdown_volume_bbl": (f.shutdown_pct / 100) * f.capacity_bbl,
                "elapsed_s": [p["elapsed_s"] for p in result["points"]], "volumes_bbl": [p["volume_bbl"] for p in result["points"]]})
            if result["shutdown_s"] is not None:
                shutdowns.append({"facility_id": f.id, "elapsed_s": result["shutdown_s"]})
    return {"plan": plan, "feasible": valid, "coverage": "available" if valid else "unavailable",
            "outcome": ("automatic_shutdown" if shutdowns else "avoids_shutdown") if valid else "unavailable",
            "shutdowns": shutdowns, "trajectories": trajectories, "cost": computed["cost"] if valid else None,
            "fleet_occupancy": [{"truck_id": j["truck_id"], "job_id": j["job_id"], "start_elapsed_s": j["called_elapsed_s"],
                                 "end_elapsed_s": j["available_again_elapsed_s"]} for j in plan["truck_jobs"]],
            "violations": violations, "assumptions": context["assumptions"],
            "limitations": ["Method " + METHOD + "; native " + native.method + ".",
                            "Fixed-rate response evaluation is not hydraulic simulation, independent validation, search or a calibrated uncertainty estimate.",
                            "All current failures must be declared; subset response contexts and evolving prior fleet commitments are not yet supported."]}
