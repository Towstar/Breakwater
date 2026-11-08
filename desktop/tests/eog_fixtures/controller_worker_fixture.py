"""Controller integration fixture only: synthetic protocol records, not a
numerical detector, optimizer, independent validator or operational provider."""
import csv
import hashlib
import json
from pathlib import Path
import struct
import sys
import time


def identity(plan):
    def project(value):
        if isinstance(value, bool):
            return value
        if isinstance(value, (int, float)):
            return {"$f64": struct.pack(">d", 0.0 if value == 0 else float(value)).hex()}
        if isinstance(value, list):
            return [project(item) for item in value]
        if isinstance(value, dict):
            return {key: project(item) for key, item in value.items()}
        return value
    body = {key: value for key, value in plan.items() if key != "plan_hash"}
    return hashlib.sha256(b"breakwater-eog-plan-v1\n" + json.dumps(
        project(body), ensure_ascii=False, allow_nan=False, sort_keys=True,
        separators=(",", ":")).encode("utf-8")).hexdigest()


def main():
    mode, log_path = sys.argv[1:3]
    gate = Path(sys.argv[3]) if len(sys.argv) > 3 else None
    raw = sys.stdin.buffer.readline(1024 * 1024 + 1)
    if not raw.endswith(b"\n") or len(raw) > 1024 * 1024 or sys.stdin.buffer.read(1):
        return 3
    request = json.loads(raw)
    with Path(log_path).open("a", encoding="utf-8") as stream:
        stream.write(json.dumps({key: request[key] for key in
                                ("request_id", "operation", "cutoff_unix")}) + "\n")
    if mode == "gated" and gate is not None:
        Path(str(gate) + ".ready").write_text(request["request_id"], encoding="utf-8")
        while not gate.exists():
            time.sleep(0.005)
    directory = Path(request["capture_directory"])
    network = json.loads((directory / "network.json").read_text(encoding="utf-8-sig"))
    ops = json.loads((directory / "ops.json").read_text(encoding="utf-8-sig"))
    system = network["systems"][0]
    facility = system["facilities"][0]
    operational = ops["systems"][system["id"]]["facilities"][facility["id"]]
    with (directory / "telemetry.csv").open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    sample = next(row for row in reversed(rows) if row["asset_id"] == facility["tank"]["id"])
    capacity = facility["tank"]["capacity_bbl"]
    shutdown = network["defaults"]["tank_limits_pct"]["shutdown"] / 100 * capacity
    volume = float(sample["value"]) / 100 * capacity
    horizon = ops["expected_repair_hours"]["pump_failure"] * 3600
    evidence = {"evidence_id": "protocol-only-tank-sample", "asset_id": sample["asset_id"],
                "metric": "level_pct", "timestamp_unix": int(sample["ts_unix"]),
                "value": float(sample["value"]), "unit": "%", "quality": "good"}
    response = {key: request[key] for key in ("eog_version", "request_id", "operation",
                "method_version", "dataset_identity", "capture_id", "cutoff_unix")}
    response.update(kind="analysis_response", status="completed", diagnostics=[])

    def plan(plan_id, truck=False):
        result = {"plan_id": plan_id, "context": request["capture_manifest"]["context"],
                  "start_unix": request["cutoff_unix"],
                  "repair_deadlines": [{"facility_id": facility["id"], "repair_elapsed_s": horizon}],
                  "truck_jobs": [], "well_shut_ins": [], "inspection_assets": []}
        if truck:
            arrival = operational["response_min"] * 60
            cycle = operational["cycle_min"] * 60
            result["truck_jobs"] = [{"job_id": "protocol-only-job", "truck_id": "protocol-only-truck",
                "system_id": system["id"], "facility_id": facility["id"],
                "called_elapsed_s": 0, "available_again_elapsed_s": arrival + cycle,
                "response_min": operational["response_min"], "cycle_min": operational["cycle_min"],
                "trips": [{"arrival_elapsed_s": arrival, "volume_bbl": ops["emergency_trucking"]["truck_capacity_bbl"],
                           "cost": ops["emergency_trucking"]["cost_per_trip"]}]}]
        result["plan_hash"] = identity(result)
        return result

    def evaluation(proposed):
        deadline = proposed["repair_deadlines"][0]["repair_elapsed_s"]
        has_truck = bool(proposed["truck_jobs"])
        if has_truck:
            job = proposed["truck_jobs"][0]
            arrival = job["trips"][0]["arrival_elapsed_s"]
            load = job["trips"][0]["volume_bbl"]
            before = volume + 1
            times = [0, arrival, arrival, deadline]
            volumes = [volume, before, before - load, before - load + 6]
            lost = 0
            truck = sum(trip["cost"] for action in proposed["truck_jobs"] for trip in action["trips"])
            shutdowns = []
            occupancy = [{"truck_id": action["truck_id"], "job_id": action["job_id"],
                          "start_elapsed_s": action["called_elapsed_s"],
                          "end_elapsed_s": action["available_again_elapsed_s"]}
                         for action in proposed["truck_jobs"]]
        else:
            times = [0, deadline / 2, deadline]
            volumes = [volume, shutdown, shutdown]
            lost = 2000 * deadline / horizon
            truck = 0
            shutdowns = [{"facility_id": facility["id"], "elapsed_s": deadline / 2}]
            occupancy = []
        return {"plan": proposed, "feasible": True, "coverage": "available",
                "outcome": "avoids_shutdown" if has_truck else "automatic_shutdown",
                "shutdowns": shutdowns, "cost": {"truck_cost": truck, "lost_revenue": lost, "total": truck + lost},
                "trajectories": [{"facility_id": facility["id"], "tank_id": facility["tank"]["id"],
                    "capacity_bbl": capacity, "shutdown_volume_bbl": shutdown,
                    "elapsed_s": times, "volumes_bbl": volumes}],
                "fleet_occupancy": occupancy, "violations": [],
                "assumptions": ["Protocol-only synthetic response; no numerical implementation."],
                "limitations": ["Exercise desktop contracts and retention only."]}

    operation = request["operation"]
    if operation == "detect":
        event = {"event_id": "protocol-only-event", "system_id": system["id"], "facility_id": facility["id"],
                 "asset_id": facility["pump"]["id"], "category": "pump_failure", "state": "ongoing",
                 "onset_unix": request["cutoff_unix"], "onset_bounds": None,
                 "updated_unix": request["cutoff_unix"], "end_unix": None,
                 "evidence_ids": [evidence["evidence_id"]], "countdown_hours": 1,
                 "lost_revenue_per_hour": 10, "coverage": "available",
                 "threshold_version": "protocol-only-thresholds", "recommended_action": "plan_required",
                 "limitations": ["Protocol-only event; no detector implementation."]}
        response["result"] = {"analysis_id": "protocol-only-detect", "events": [event],
            "evidence": [evidence], "assumptions": [], "coverage": "available",
            "limitations": ["Protocol-only fixture."]}
        if mode == "partial_empty_detect":
            response["result"].update(events=[], coverage="partial",
                assumptions=["Protocol-only assumption: recorded samples do not establish complete detector coverage."],
                limitations=["Protocol-only limitation: missing coverage is not evidence of healthy equipment."])
        elif mode == "unavailable_detect":
            response.update(status="unavailable", result=None, diagnostics=[{
                "code": "protocol_only_detection_unavailable",
                "message": "Protocol-only fixture: detection is unavailable; no operational provider is exercised."}])
    elif operation == "search_plans":
        response["result"] = {"analysis_id": "protocol-only-search",
            "evaluations": [evaluation(plan("protocol-only-no-action")), evaluation(plan("protocol-only-truck-plan", True))],
            "selected_plan_id": "protocol-only-truck-plan",
            "search": {"scope": "bounded_search", "optimality": "best_found",
                "evaluated_candidates": 2, "candidate_limit": request["arguments"]["candidate_limit"],
                "time_limit_s": request["arguments"]["time_limit_s"], "elapsed_s": 0.001,
                "complete": False, "action_time_resolution_s": None,
                "space_description": "Protocol-only retained synthetic candidates; no real search."},
            "limitations": ["No optimization implementation."]}
    elif operation == "evaluate_plan":
        response["result"] = {"analysis_id": "protocol-only-evaluate",
                              "evaluation": evaluation(request["arguments"]["plan"])}
    elif operation == "validate_plan":
        proposed = request["arguments"]["plan"]
        evaluated = evaluation(proposed)
        validation = {"validation_id": "protocol-only-validation:" + proposed["plan_id"],
            "plan_id": proposed["plan_id"], "plan_hash": proposed["plan_hash"],
            "evaluator_method": request["arguments"]["evaluator_method"],
            "validator_method": "protocol-only-validator", "independent": True,
            "feasible": True, "coverage": "available", "outcome": evaluated["outcome"],
            "shutdowns": evaluated["shutdowns"], "cost": evaluated["cost"], "violations": [],
            "minimum_shutdown_margin_bbl": 0 if evaluated["shutdowns"] else 100,
            "validated_horizons": proposed["repair_deadlines"],
            "evidence_ids": [evidence["evidence_id"]], "evidence": [evidence]}
        if mode == "mismatched_validation":
            validation["plan_hash"] = "0" * 64
        response["result"] = {"analysis_id": "protocol-only-validate", "validation": validation}
    else:
        return 4
    if mode == "wrong_source_value" and operation == "detect":
        # Shape, IDs, timestamp and units still match the contract. Only a lookup
        # against the actual captured observation can reject this invented value.
        response["result"]["evidence"][0]["value"] += 1
    sys.stdout.buffer.write(json.dumps(response, ensure_ascii=False, allow_nan=False,
                                     separators=(",", ":")).encode("utf-8") + b"\n")
    sys.stdout.buffer.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
