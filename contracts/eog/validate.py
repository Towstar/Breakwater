"""Strict EOG interchange validation; no detector, optimizer or control authority.

The public functions validate content and binding. Even a well-formed independent
validation record is a claim by its publisher, not execution of a numerical proof.
"""
from __future__ import annotations

import argparse
from collections import Counter
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import math
from pathlib import Path, PureWindowsPath
import re
import struct
import sys

ROOT = Path(__file__).resolve().parent
FILENAMES = ("network.json", "ops.json", "telemetry.csv")
MAX_DOCUMENT_BYTES = 4 * 1024 * 1024
MAX_REQUEST_BYTES = 1024 * 1024
MAX_CSV_BYTES = 128 * 1024 * 1024
MAX_ROWS = 2_000_000
MAX_DEPTH = 64
MAX_UTC = 253402300799
SCHEMAS = {"capture_manifest": "capture-manifest-v1.schema.json",
           "analysis_request": "analysis-request-v1.schema.json",
           "analysis_response": "analysis-response-v1.schema.json"}
METRIC_UNITS = {"inflow_bpd": "bpd", "water_bpd": "bpd", "status": "state",
                "level_pct": "%", "flow_bpd": "bpd", "discharge_psi": "psi",
                "receive_flow_bpd": "bpd", "pressure_psi": "psi"}
METRICS_BY_KIND = {"facility": {"inflow_bpd"}, "well": {"water_bpd", "status"},
                   "tank": {"level_pct"}, "pump": {"status", "flow_bpd", "discharge_psi"},
                   "pipeline": {"receive_flow_bpd", "pressure_psi"}, "disposal": set()}
BINDINGS = ("request_id", "operation", "method_version", "dataset_identity",
            "capture_id", "cutoff_unix")
_VALIDATORS = None


class ContractError(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


def require(condition, code, message):
    if not condition:
        raise ContractError(code, message)


def _pairs(items):
    value = {}
    for key, item in items:
        require(key not in value, "duplicate_key", "Duplicate JSON object key.")
        value[key] = item
    return value


def _finite(value, depth=0):
    require(depth <= MAX_DEPTH, "limit_exceeded", "JSON nesting exceeds 64 levels.")
    if isinstance(value, str):
        try:
            value.encode("utf-8", "strict")
        except UnicodeError as error:
            raise ContractError("invalid_utf8", "Unpaired Unicode surrogate.") from error
    elif isinstance(value, (int, float)) and not isinstance(value, bool):
        try:
            require(math.isfinite(float(value)), "nonfinite", "Numbers must be finite binary64.")
        except OverflowError as error:
            raise ContractError("nonfinite", "Number exceeds finite binary64.") from error
    elif isinstance(value, list):
        for item in value:
            _finite(item, depth + 1)
    elif isinstance(value, dict):
        for key, item in value.items():
            require(isinstance(key, str), "invalid_shape", "JSON object keys must be text.")
            _finite(key, depth + 1)
            _finite(item, depth + 1)
    elif value is not None and not isinstance(value, bool):
        raise ContractError("invalid_shape", "Unsupported JSON value type.")


def decode_document(data):
    if isinstance(data, str):
        try:
            data = data.encode("utf-8", "strict")
        except UnicodeError as error:
            raise ContractError("invalid_utf8", "Document is not valid UTF-8.") from error
    require(isinstance(data, bytes), "invalid_shape", "Expected bytes or text.")
    require(len(data) <= MAX_DOCUMENT_BYTES, "limit_exceeded", "Document exceeds 4 MiB.")
    try:
        value = json.loads(data.decode("utf-8", "strict"), object_pairs_hook=_pairs,
                           parse_constant=lambda _: (_ for _ in ()).throw(
                               ContractError("nonfinite", "Nonfinite JSON is forbidden.")))
    except ContractError:
        raise
    except (ValueError, UnicodeError, RecursionError) as error:
        raise ContractError("invalid_json", "Expected one bounded UTF-8 JSON object.") from error
    require(isinstance(value, dict), "invalid_shape", "Document must be an object.")
    _finite(value)
    if value.get("kind") == "analysis_request":
        require(len(data) <= MAX_REQUEST_BYTES, "limit_exceeded", "Request exceeds 1 MiB.")
    return value


def _shape(value, kind):
    global _VALIDATORS
    if _VALIDATORS is None:
        from jsonschema import Draft202012Validator
        from referencing import Registry, Resource
        from referencing.exceptions import NoSuchResource

        def local_only(uri):
            raise NoSuchResource(ref=uri)

        documents = {path.name: json.loads(path.read_text(encoding="utf-8"))
                     for path in ROOT.glob("*.schema.json")}
        for document in documents.values():
            Draft202012Validator.check_schema(document)
        registry = Registry(retrieve=local_only).with_resources(
            (document["$id"], Resource.from_contents(document))
            for document in documents.values())
        _VALIDATORS = {key: Draft202012Validator(documents[name], registry=registry)
                       for key, name in SCHEMAS.items()}
    require(isinstance(kind, str) and kind in _VALIDATORS, "unsupported_kind", "Unsupported EOG document kind.")
    require(next(_VALIDATORS[kind].iter_errors(value), None) is None,
            "invalid_shape", "Document violates its strict local EOG schema.")


def _unique(items, key, code="duplicate_id"):
    values = [item[key] for item in items]
    require(len(values) == len(set(values)), code, "Duplicate " + key + ".")


def _ids(value):
    # Imported references retain the reader's UTF-16 limit, including astral text.
    if isinstance(value, dict):
        for key, item in value.items():
            if (key.endswith("_id") or key in ("evaluator_method", "validator_method", "threshold_version")) and isinstance(item, str):
                require(0 < len(item.encode("utf-16-le")) // 2 <= 256,
                        "invalid_id", "ID exceeds 256 UTF-16 code units.")
            if key in ("system_ids", "evidence_ids", "inspection_assets"):
                require(len(item) == len(set(item)), "duplicate_id", "Repeated reference.")
                for identifier in item:
                    require(len(identifier.encode("utf-16-le")) // 2 <= 256,
                            "invalid_id", "Reference exceeds 256 UTF-16 code units.")
            _ids(item)
    elif isinstance(value, list):
        for item in value:
            _ids(item)


def canonical_bytes(value):
    _finite(value)
    return json.dumps(value, ensure_ascii=False, allow_nan=False, sort_keys=True,
                      separators=(",", ":")).encode("utf-8")


def semantic_bytes(value):
    """G1-style float64 projection avoids Python/Qt decimal-format differences."""
    _finite(value)
    def project(item):
        if isinstance(item, bool):
            return item
        if isinstance(item, (int, float)):
            number = float(item)
            return {"$f64": struct.pack(">d", 0.0 if number == 0 else number).hex()}
        if isinstance(item, dict):
            return {key: project(child) for key, child in item.items()}
        if isinstance(item, list):
            return [project(child) for child in item]
        return item
    return canonical_bytes(project(value))


def dataset_identity(source_hashes):
    require(isinstance(source_hashes, dict) and set(source_hashes) == set(FILENAMES),
            "invalid_hashes", "Exactly the three source hashes are required.")
    require(all(isinstance(h, str) and re.fullmatch(r"[0-9a-f]{64}", h)
                for h in source_hashes.values()), "invalid_hashes", "Expected lowercase SHA-256.")
    payload = "breakwater-eog-inputs-v1\n" + "".join(
        name + ":" + source_hashes[name] + "\n" for name in FILENAMES)
    return hashlib.sha256(payload.encode("ascii")).hexdigest()


def capture_identity(manifest):
    # Manifest fields are strings, exact integers and fixed-key objects.
    projection = {key: value for key, value in manifest.items() if key != "capture_id"}
    return hashlib.sha256(b"breakwater-eog-capture-v1\n" + semantic_bytes(projection)).hexdigest()


def plan_identity(plan):
    projection = {key: value for key, value in plan.items() if key != "plan_hash"}
    return hashlib.sha256(b"breakwater-eog-plan-v1\n" + semantic_bytes(projection)).hexdigest()


def _manifest(value):
    require(value["dataset_identity"] == dataset_identity(value["source_hashes"]),
            "dataset_mismatch", "Original dataset identity does not match its hashes.")
    require(value["source_first_unix"] <= value["cutoff_unix"] <= value["source_now_unix"],
            "invalid_cutoff", "Cutoff lies outside the source window.")
    if value["context"] == "snapshot_now":
        require(value["cutoff_unix"] == value["source_now_unix"], "invalid_cutoff",
                "Snapshot-now cutoff must equal source now.")
    copied = FILENAMES if value["cutoff_unix"] == value["source_now_unix"] else FILENAMES[:2]
    require(all(value["captured_files"][name]["sha256"] == value["source_hashes"][name]
                for name in copied), "source_mismatch", "Untruncated captured files must match original hashes.")
    require(all(info["size_bytes"] <= (MAX_CSV_BYTES if name == "telemetry.csv" else MAX_DOCUMENT_BYTES)
                for name, info in value["captured_files"].items()), "limit_exceeded",
            "Captured file exceeds its bound.")
    require(value["capture_id"] == capture_identity(value), "capture_mismatch",
            "Capture identity does not match its canonical manifest.")


def _close(left, right):
    return math.isclose(left, right, rel_tol=1e-12, abs_tol=1e-9)


def _cost(value):
    require(_close(value["total"], value["truck_cost"] + value["lost_revenue"]),
            "cost_mismatch", "Total must equal the declared cost components.")


def _plan(plan, cutoff, manifest=None):
    require(plan["plan_hash"] == plan_identity(plan), "plan_mismatch", "Plan content hash differs.")
    require(plan["start_unix"] == cutoff, "stale_plan", "Plan must start at the capture cutoff.")
    if manifest:
        require(plan["context"] == manifest["context"], "context_mismatch", "Plan context differs from capture.")
    _unique(plan["repair_deadlines"], "facility_id")
    _unique(plan["truck_jobs"], "job_id")
    _unique(plan["well_shut_ins"], "well_id")
    for shutdown in plan["well_shut_ins"]:
        require(0 <= shutdown["start_elapsed_s"] <= shutdown["end_elapsed_s"], "invalid_time",
                "Shut-in interval precedes the plan or is reversed.")
    for job in plan["truck_jobs"]:
        arrivals = [trip["arrival_elapsed_s"] for trip in job["trips"]]
        require(0 <= job["called_elapsed_s"] <= arrivals[0] <= arrivals[-1] <= job["available_again_elapsed_s"],
                "invalid_time", "Truck job timestamps are reversed or precede the plan.")
        require(all(a < b for a, b in zip(arrivals, arrivals[1:])), "invalid_time",
                "A truck cannot have simultaneous or reversed trips.")
    # Infeasible candidates (late/partial/overbooked trips etc.) remain representable.
    # Only Ethan's evaluator/independent validator decides physical feasibility.


def _evidence(samples, cutoff, assets=None):
    _unique(samples, "evidence_id")
    if assets is not None:
        require(isinstance(assets, dict) and all(isinstance(key, str) and kind in METRICS_BY_KIND
                for key, kind in assets.items()), "invalid_context", "Asset context must map IDs to supported kinds.")
    for sample in samples:
        require(sample["timestamp_unix"] <= cutoff, "future_evidence", "Evidence is later than cutoff.")
        require(sample["unit"] == METRIC_UNITS[sample["metric"]], "unit_mismatch",
                "Evidence metric/unit pair is incompatible.")
        require((sample["quality"] == "missing") == (sample["value"] is None), "invalid_quality",
                "Only a missing observation has a null value.")
        if sample["quality"] == "good":
            require(cutoff - sample["timestamp_unix"] <= 300, "stale_evidence",
                    "A good observation exceeds the capture maximum age.")
        if assets is not None:
            require(sample["asset_id"] in assets, "unknown_asset", "Unknown evidence asset.")
            kind = assets[sample["asset_id"]]
            require(sample["metric"] in METRICS_BY_KIND[kind], "metric_mismatch",
                    "Metric does not belong to this asset kind.")
            if sample["metric"] == "status" and sample["value"] is not None:
                allowed = (0, 1, 2) if kind == "pump" else (0, 1)
                require(sample["value"] in allowed, "invalid_state", "Unsupported typed equipment state.")


def _events(events, evidence, cutoff):
    _unique(events, "event_id")
    known = {sample["evidence_id"] for sample in evidence}
    for event in events:
        require(set(event["evidence_ids"]) <= known, "unknown_evidence", "Event references missing evidence.")
        require(event["updated_unix"] <= cutoff, "future_evidence", "Event update exceeds cutoff.")
        onset = event["onset_unix"]
        require(onset is None or onset <= event["updated_unix"], "invalid_time", "Onset is after event update.")
        bounds = event["onset_bounds"]
        if bounds:
            require(bounds["earliest_unix"] <= bounds["latest_unix"] <= event["updated_unix"],
                    "invalid_time", "Onset uncertainty bounds are reversed or future.")
            require(onset is None or bounds["earliest_unix"] <= onset <= bounds["latest_unix"],
                    "invalid_time", "Point onset lies outside onset bounds.")
        if event["state"] == "resolved":
            require(event["end_unix"] is not None and event["end_unix"] <= event["updated_unix"]
                    and (onset is None or onset <= event["end_unix"]), "invalid_time",
                    "Resolved event needs a causal ordered end time.")
            require(event["countdown_hours"] is None, "invalid_countdown",
                    "Resolved events have no current countdown.")
        else:
            require(event["end_unix"] is None, "invalid_time", "Ongoing event cannot have a known end.")
        if event["category"] in ("pipeline_leak", "pump_degradation"):
            require(event["countdown_hours"] is None, "invalid_countdown",
                    "Leaks/degradation have no shutdown countdown.")
            require(event["recommended_action"] in ("field_inspection", "unavailable"),
                    "invalid_action", "Leaks/degradation recommend inspection.")
        if event["coverage"] != "available":
            require(event["countdown_hours"] is None, "unavailable_countdown",
                    "Unavailable/ambiguous evidence cannot claim an available countdown.")


def _horizons(values):
    _unique(values, "facility_id")
    return {item["facility_id"]: item["repair_elapsed_s"] for item in values}


def _assessment(value):
    if value["cost"] is not None:
        _cost(value["cost"])
    if value["feasible"]:
        require(value["coverage"] == "available" and value["cost"] is not None
                and value["outcome"] != "unavailable", "incomplete_assessment",
                "Admissible actions need available forecast coverage, cost and outcome.")
        require(not value["violations"], "contradictory_result",
                "Admissible actions cannot list violations.")
    else:
        require(bool(value["violations"]), "missing_violation",
                "An assessed invalid action needs a violation; unknown physics is top-level unavailable.")
    require(value["coverage"] == "available" or value["outcome"] == "unavailable",
            "incomplete_assessment", "Partial/unavailable coverage has no complete outcome.")
    _unique(value["shutdowns"], "facility_id")
    if value["outcome"] == "avoids_shutdown":
        require(not value["shutdowns"], "contradictory_result", "Avoidance cannot list an automatic shutdown.")
    if value["outcome"] == "automatic_shutdown":
        require(bool(value["shutdowns"]), "missing_shutdown", "Automatic-shutdown outcome needs its crossing.")
    if value["coverage"] != "available":
        require(value["cost"] is None, "unavailable_cost",
                "Incomplete assessment cannot invent a computable cost.")


def _shutdowns(value, horizons):
    for shutdown in value["shutdowns"]:
        require(shutdown["facility_id"] in horizons and
                shutdown["elapsed_s"] <= horizons[shutdown["facility_id"]],
                "invalid_shutdown", "Shutdown lies outside its modeled facility/repair horizon.")


def _evaluation(value, cutoff, manifest=None):
    _plan(value["plan"], cutoff, manifest)
    _assessment(value)
    required = _horizons(value["plan"]["repair_deadlines"])
    _unique(value["trajectories"], "tank_id")
    _unique(value["trajectories"], "facility_id")
    _unique(value["fleet_occupancy"], "job_id")
    actual = {item["facility_id"] for item in value["trajectories"]}
    require(actual <= set(required), "forecast_coverage", "Trajectory references an undeclared repair facility.")
    if value["coverage"] == "available":
        require(bool(required) and actual == set(required), "forecast_coverage",
                "Available forecast must cover every declared repaired facility.")
    for trajectory in value["trajectories"]:
        times = trajectory["elapsed_s"]
        require(len(times) == len(trajectory["volumes_bbl"]), "trajectory_shape",
                "Trajectory times and volumes must have equal lengths.")
        require(times[0] == 0 and all(a <= b for a, b in zip(times, times[1:])),
                "invalid_time", "Trajectory must start at elapsed zero and remain ordered.")
        horizon = required[trajectory["facility_id"]]
        require(times[-1] <= horizon and (value["coverage"] != "available" or times[-1] == horizon),
                "forecast_horizon", "Available forecast must include its exact repair endpoint.")
        require(all(a < b or (index == 0 or times[index-1] < a) for index, (a, b) in enumerate(zip(times, times[1:]))),
                "trajectory_shape", "At most two samples at a timestamp represent before/after a discrete action.")
        require(trajectory["shutdown_volume_bbl"] <= trajectory["capacity_bbl"],
                "invalid_range", "Shutdown inventory exceeds tank capacity.")
    _shutdowns(value, required)
    if value["feasible"]:
        points = {item["facility_id"]: Counter(item["elapsed_s"]) for item in value["trajectories"]}
        for job in value["plan"]["truck_jobs"]:
            require(job["facility_id"] in points and all(points[job["facility_id"]][trip["arrival_elapsed_s"]] == 2
                    for trip in job["trips"]), "missing_breakpoint",
                    "Each facility truck-arrival time needs an exact combined before/after pair.")
        for action in value["plan"]["well_shut_ins"]:
            require(action["facility_id"] in points and
                    points[action["facility_id"]][action["start_elapsed_s"]] > 0 and
                    points[action["facility_id"]][action["end_elapsed_s"]] > 0, "missing_breakpoint",
                    "Well-action breakpoints must be retained in the facility trajectory.")
        for shutdown in value["shutdowns"]:
            require(points[shutdown["facility_id"]][shutdown["elapsed_s"]] > 0, "missing_breakpoint",
                    "Automatic shutdown crossing must be retained in the facility trajectory.")
    jobs = {job["job_id"]: job for job in value["plan"]["truck_jobs"]}
    require({item["job_id"] for item in value["fleet_occupancy"]} == set(jobs),
            "occupancy_mismatch", "Every declared truck job needs an occupancy record.")
    for item in value["fleet_occupancy"]:
        job = jobs[item["job_id"]]
        require(item["truck_id"] == job["truck_id"] and item["start_elapsed_s"] == job["called_elapsed_s"]
                and item["end_elapsed_s"] == job["available_again_elapsed_s"], "occupancy_mismatch",
                "Occupancy must bind the exact half-open job interval.")


def validate_document(document, kind=None, context=None):
    context = {} if context is None else context
    require(isinstance(document, dict) and isinstance(context, dict), "invalid_shape", "Expected object/context.")
    _finite(document)
    require(len(canonical_bytes(document)) <= (MAX_REQUEST_BYTES if document.get("kind") == "analysis_request" else MAX_DOCUMENT_BYTES),
            "limit_exceeded", "Serialized document exceeds its byte budget.")
    kind = document.get("kind") if kind is None else kind
    try:
        _shape(document, kind)
    except RecursionError as error:
        raise ContractError("limit_exceeded", "Schema recursion exceeded its bound.") from error
    _ids(document)
    if kind == "capture_manifest":
        _manifest(document)
        return
    if kind == "analysis_request":
        manifest = document["capture_manifest"]
        validate_document(manifest, "capture_manifest")
        for key in ("dataset_identity", "capture_id", "cutoff_unix"):
            require(document[key] == manifest[key], "binding_mismatch", "Request and capture binding differ.")
        path = document["capture_directory"]
        require(Path(path).is_absolute() or PureWindowsPath(path).is_absolute(), "invalid_path",
                "Capture directory must be an absolute local path.")
        require(not path.startswith(("\\\\", "//")) and "\x00" not in path, "invalid_path",
                "Network paths and NUL are forbidden.")
        if document["operation"] in ("evaluate_plan", "validate_plan"):
            _plan(document["arguments"]["plan"], document["cutoff_unix"], manifest)
        return
    request = context.get("request")
    manifest = None
    if request is not None:
        validate_document(request, "analysis_request")
        for key in BINDINGS:
            require(document[key] == request[key], "binding_mismatch", "Response and request binding differ.")
        manifest = request["capture_manifest"]
    if document["status"] != "completed":
        return
    result, operation, cutoff = document["result"], document["operation"], document["cutoff_unix"]
    if operation == "detect":
        _evidence(result["evidence"], cutoff, context.get("assets"))
        _events(result["events"], result["evidence"], cutoff)
        require(result["coverage"] != "unavailable" or bool(result["limitations"]), "missing_limitation",
                "Unavailable detection must explain its missing coverage.")
        require(result["coverage"] != "available" or bool(result["evidence"]), "missing_evidence",
                "Available detection must retain source evidence even when no fault is found.")
    elif operation == "evaluate_plan":
        _evaluation(result["evaluation"], cutoff, manifest)
        if request:
            require(result["evaluation"]["plan"] == request["arguments"]["plan"], "plan_mismatch",
                    "Evaluation must retain the exact proposed plan.")
    elif operation == "search_plans":
        evaluations = result["evaluations"]
        _unique([item["plan"] for item in evaluations], "plan_id")
        for evaluation in evaluations:
            _evaluation(evaluation, cutoff, manifest)
        search = result["search"]
        require(search["evaluated_candidates"] <= search["candidate_limit"], "search_limit",
                "Search exceeds its declared candidate bound.")
        require(len(evaluations) <= search["evaluated_candidates"], "search_limit",
                "Retained candidates exceed evaluated candidates.")
        require(search["optimality"] != "proven_within_space" or
                (search["complete"] and search["scope"] == "enumerated_space"),
                "invalid_optimality", "Optimality requires complete declared enumeration.")
        selected = result["selected_plan_id"]
        require(selected is None or any(item["plan"]["plan_id"] == selected and item["feasible"]
                                       for item in evaluations), "unknown_plan",
                "Selected plan must reference a retained feasible candidate.")
        if selected is not None:
            chosen = next(item for item in evaluations if item["plan"]["plan_id"] == selected)
            require(chosen["cost"]["total"] == min(item["cost"]["total"] for item in evaluations if item["feasible"]),
                    "nonminimal_selection", "Selected plan must have minimum declared cost among retained feasible candidates.")
        if request:
            require(search["candidate_limit"] == request["arguments"]["candidate_limit"] and
                    search["time_limit_s"] == request["arguments"]["time_limit_s"],
                    "search_limit", "Search bounds differ from the request.")
    elif operation == "validate_plan":
        validation = result["validation"]
        _evidence(validation["evidence"], cutoff, context.get("assets"))
        require(set(validation["evidence_ids"]) <= {item["evidence_id"] for item in validation["evidence"]},
                "unknown_evidence", "Validation references missing source evidence.")
        _assessment(validation)
        verified = _horizons(validation["validated_horizons"])
        require(validation["coverage"] != "available" or bool(verified), "forecast_coverage",
                "Available validation must declare its modeled repair horizons.")
        _shutdowns(validation, verified)
        require(validation["validator_method"] != validation["evaluator_method"],
                "not_independent", "Validator must declare a different implementation.")
        if request:
            required = _horizons(request["arguments"]["plan"]["repair_deadlines"])
            require(set(verified) <= set(required) and all(verified[key] == required[key] for key in verified),
                    "forecast_horizon", "Validation changed a supplied repair horizon.")
            require(validation["coverage"] != "available" or verified == required, "forecast_coverage",
                    "Available validation must cover every requested repair horizon.")
            require(validation["plan_id"] == request["arguments"]["plan"]["plan_id"] and
                    validation["plan_hash"] == request["arguments"]["plan"]["plan_hash"] and
                    validation["evaluator_method"] == request["arguments"]["evaluator_method"],
                    "plan_mismatch", "Validation must bind the exact proposed plan/evaluator.")


def verify_capture(directory, manifest):
    """Check only fixed captured files, hashes, sizes and causal CSV; no physics."""
    validate_document(manifest, "capture_manifest")
    folder = Path(directory)
    require(folder.is_dir() and not folder.is_symlink() and
            not (hasattr(folder, "is_junction") and folder.is_junction()),
            "invalid_path", "Capture directory must be a plain local directory.")
    root = folder.resolve()
    telemetry = None
    for name in FILENAMES:
        path = folder / name
        require(path.is_file() and not path.is_symlink() and path.resolve().parent == root,
                "invalid_path", "Captured input must be a regular local child file.")
        info = manifest["captured_files"][name]
        limit = MAX_CSV_BYTES if name == "telemetry.csv" else MAX_DOCUMENT_BYTES
        require(path.stat().st_size == info["size_bytes"] <= limit, "size_mismatch",
                "Captured input size differs from manifest.")
        with path.open("rb") as handle:
            raw = handle.read(limit + 1)
        require(len(raw) == info["size_bytes"] and hashlib.sha256(raw).hexdigest() == info["sha256"],
                "hash_mismatch", "Captured input differs from manifest.")
        if name == "telemetry.csv":
            telemetry = raw
        else:
            decode_document(raw)
    try:
        rows = csv.reader(io.StringIO(telemetry.decode("utf-8-sig", "strict"), newline=""), strict=True)
        require(next(rows, None) == ["timestamp", "ts_unix", "asset_id", "metric", "value"],
                "invalid_csv", "Unexpected telemetry header.")
        count, first, previous, latest = 0, None, None, {}
        for row in rows:
            require(len(row) == 5 and sum(len(item.encode("utf-8")) for item in row) <= 16 * 1024,
                    "invalid_csv", "Malformed or oversized telemetry row.")
            stamp = int(row[1])
            require(0 <= stamp <= manifest["cutoff_unix"], "future_evidence", "Captured CSV exceeds cutoff.")
            require(row[0] == datetime.fromtimestamp(stamp, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                    "invalid_time", "UTC text and Unix timestamp disagree.")
            require(previous is None or previous <= stamp, "invalid_time", "CSV rows are not ordered.")
            require(previous is None or (stamp - previous) % 300 == 0, "invalid_time", "CSV cadence is not five-minute based.")
            key = row[2], row[3]
            require(row[2] and len(row[2].encode("utf-16-le")) // 2 <= 256 and row[3] in METRIC_UNITS,
                    "invalid_csv", "Unknown metric or invalid asset reference.")
            require(latest.get(key) != stamp, "duplicate_sample", "Duplicate asset/metric/time sample.")
            require(math.isfinite(float(row[4])), "nonfinite", "CSV values must be finite.")
            latest[key], previous = stamp, stamp
            first = stamp if first is None else first
            count += 1
            require(count <= MAX_ROWS, "limit_exceeded", "CSV exceeds row budget.")
        require(count > 0 and first == manifest["source_first_unix"], "invalid_time",
                "Captured CSV must retain the original first timestamp.")
        require(manifest["cutoff_unix"] != manifest["source_now_unix"] or previous == manifest["source_now_unix"],
                "invalid_time", "Untruncated CSV must retain the original final timestamp.")
        return {"rows": count, "first_unix": first, "last_unix": previous,
                "capture_id": manifest["capture_id"]}
    except ContractError:
        raise
    except (ValueError, OverflowError, OSError, UnicodeError, csv.Error) as error:
        raise ContractError("invalid_csv", "Invalid captured telemetry.") from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("document", type=Path)
    parser.add_argument("--context", type=Path)
    parser.add_argument("--verify-capture", type=Path)
    args = parser.parse_args()
    try:
        document = decode_document(args.document.read_bytes())
        context = decode_document(args.context.read_bytes()) if args.context else {}
        validate_document(document, context=context)
        report = {"valid": True, "bound": "request" in context,
                  "scope": "Content/binding only; no numerical execution or operating authority."}
        if args.verify_capture:
            report["capture"] = verify_capture(args.verify_capture, document)
        print(json.dumps(report))
        return 0
    except (ContractError, OSError) as error:
        print(json.dumps({"valid": False, "code": getattr(error, "code", "read_error"),
                          "message": str(error)[:300]}))
        return 1


if __name__ == "__main__":
    sys.exit(main())
