"""Offline integrity/authority audit of the published Nov19 synthetic evidence.

Uses only the standard library. A passing audit does not make failed hosted
investigations successful, prove all model prose, or grant release approval.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re

from prepare_release_handoff import digest, read_json, relative_name

FORBIDDEN = {"authorization", "api_key", "nvidia_api_key", "reasoning_content",
             "raw_reasoning", "reasoning", "request_headers", "headers"}
MODEL = "nvidia/nemotron-3.5-lightning-30b-a3b"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def request_fields_match(request, required):
    """Accept legacy metadata or the closed, optional scheduled-retry pair.

    Delays describe scheduled waits, so cancellation can leave more scheduled
    waits than actual posted retries. Raw Retry-After values are never allowed.
    """
    if not isinstance(request, dict) or not required <= set(request):
        return False
    extra = set(request) - required
    if not extra:
        return True
    if extra != {"retry_wait_policy", "retry_delays_ms"}:
        return False
    if not isinstance(request["retry_wait_policy"], str) or request["retry_wait_policy"] not in {
            "bounded_backoff", "invalid_retry_after_backoff", "retry_after_seconds",
            "retry_after_date", "retry_after_exceeds_budget"}:
        return False
    delays = request["retry_delays_ms"]
    return (isinstance(delays, list) and len(delays) <= 3
            and all(type(delay) in (int, float) and 0 <= delay <= 1215000
                    and math.isfinite(delay) and int(delay) == delay for delay in delays))


def inspect(value):
    if isinstance(value, dict):
        require(not (set(map(str.lower, value)) & FORBIDDEN), "Disallowed retained field.")
        for child in value.values():
            inspect(child)
    elif isinstance(value, list):
        for child in value:
            inspect(child)
    elif isinstance(value, str):
        require(not re.search(r"nvapi-[A-Za-z0-9_-]{16,}|Bearer\s+[A-Za-z0-9_-]{16,}", value),
                "Credential pattern in evidence.")
    elif isinstance(value, float):
        require(math.isfinite(value), "Nonfinite evidence number.")


def archive(root, row):
    published = row["published"]
    path = root / relative_name(published["path"])
    require(path.resolve().is_relative_to(root.resolve()), "Archive escapes the checkout.")
    require(path.stat().st_size == row["bytes"] == published["bytes"], "Archive size mismatch.")
    require(digest(path) == row["sha256"] == published["sha256"], "Archive hash mismatch.")
    value = read_json(path)
    inspect(value)
    require(value["session_id"] == row["session_id"] and value["actionable"] is False,
            "Archive identity or authority mismatch.")
    return value


def eog_run(root, row):
    value = archive(root, row)
    require(value["historical"] is True and not value["operator_records"], "EOG history gained authority.")
    events = [x["event"] for x in value["agent_events"]]
    calls = [x for x in events if x["type"] == "tool_call"]
    results = [x for x in events if x["type"] == "tool_result"]
    require([(x["call_id"], x["name"]) for x in calls] == [(x["call_id"], x["name"]) for x in results],
            "Tool call/result continuation mismatch.")
    require(len({x["call_id"] for x in calls}) == len(calls), "Duplicate call identity.")
    require([x["name"] for x in calls] == row["tool_order"], "Tool order mismatch.")
    requests = [x for x in events if x["type"] == "model_request"]
    require(len(requests) == row["model_requests"], "Request count mismatch.")
    require(all(request_fields_match(x, {"duration_ms", "http_status", "model_identity", "retry_count", "type"})
                and x["model_identity"] == MODEL and x["http_status"] == 200
                and x["duration_ms"] >= 0 and x["retry_count"] >= 0 for x in requests),
            "Unexpected request metadata.")
    finals = [x for x in events if x["type"] == "final"]
    evaluations, validations = [], []
    for analysis in value["analyses"]:
        request, response = analysis["request"], analysis["response"]
        require(request["request_id"] == response["request_id"], "Provider request identity mismatch.")
        for field in ("capture_id", "dataset_identity"):
            require(request[field] == response[field] == value["capture_manifest"][field], "Capture identity mismatch.")
        result = response.get("result", {})
        if "evaluation" in result:
            evaluations.append(result["evaluation"])
        evaluations.extend(result.get("evaluations", []))
        if "validation" in result:
            validations.append(result["validation"])
    if row["grounded_review_eligible_final"]:
        require(row["exit_code"] == 0 and len(finals) == 1, "Passing attempt lacks one final.")
        final = finals[0]
        matching = [x for x in validations if x["validation_id"] == final["validation_id"]]
        require(len(matching) == 1, "Final lacks its exact validation.")
        validation = matching[0]
        require(validation["plan_id"] == final["selected_plan_id"] and validation["feasible"] is True
                and validation["independent"] is True and validation["coverage"] == "available"
                and not validation["violations"], "Final validation is not independently feasible.")
        require(any(x.get("plan", {}).get("plan_hash") == validation["plan_hash"] for x in evaluations),
                "Validated plan hash lacks evaluation evidence.")
    else:
        require(row["exit_code"] != 0 and not finals, "Failed attempt was promoted to success.")


def native_run(root, row):
    value = archive(root, row)
    require(value["archive_version"] == 2 and value["session_kind"] == "observed_origin", "Wrong observed format.")
    origins = [x["payload"] for x in value["evidence_envelopes"] if x["kind"] == "observed_origin"]
    require(len(origins) == 1 and origins[0]["snapshot"]["snapshot_id"] == row["capture_id"], "Observed origin mismatch.")
    require(origins[0]["snapshot"]["availability"] == row["coverage"] and value["status"] == row["status"],
            "Observed coverage or completion mismatch.")
    recommendation = value.get("recommendation", {})
    mode = recommendation.get("source_mode") or value["events"][0]["source_mode"]
    require(mode == row["source_mode"], "Native search cannot be relabeled hosted.")
    requests = [x["request"] for x in value["events"] if x["type"] == "model_request_completed"]
    require(len(requests) == row["model_requests"] and all(
        request_fields_match(x, {"configured_model", "duration_ms", "error_code", "http_status", "model_request_id",
                                "outcome", "response_model", "retry_count"}) and x["configured_model"] == MODEL
        and x["duration_ms"] >= 0 and x["retry_count"] >= 0 for x in requests),
        "Unexpected observed-request metadata.")
    require(len(value["runs"]) == row["stored_runs"] and len(value["approvals"]) == row["advisory_acknowledgements"],
            "Retained run/review count mismatch.")
    if row["coverage"] == "unavailable":
        require(not value["runs"] and not value["approvals"], "Unavailable coverage produced numerical authority.")
    selected = recommendation.get("selected_run_id")
    require(selected == row["selected_run_id"], "Selected run mismatch.")
    if selected:
        require(row["source_mode"] == "local_native" and recommendation["verified_feasible"] is True
                and recommendation["validation_source"] == "validate_run", "Unexpected selected response.")
        matching = [x["run"] for x in value["runs"] if x["run"]["run_id"] == selected]
        require(len(matching) == 1 and matching[0]["validation_id"] == recommendation["validation_id"],
                "Selected response lacks exact retained validation.")
        require(matching[0]["validation"]["feasible"] is True and matching[0]["cost"] == row["cost"]
                and matching[0]["validation"]["minimum_headroom_m3"] == row["minimum_headroom_m3"],
                "Retained response metrics mismatch.")
        require(all(x["status"] == "operator_acknowledged_advisory" and x["run_id"] == selected
                    and x["validation_id"] == recommendation["validation_id"] for x in value["approvals"]),
                "Observed review gained operating authority.")
    else:
        require(not value["approvals"], "No-plan response has a review record.")


def verify(root):
    evidence = root / "docs/evidence/v03"
    eog = read_json(evidence / "live-20261119/eog-archive-audit.json")
    native = read_json(evidence / "live-20261119/observed-archive-audit.json")
    for row in eog["runs"]:
        eog_run(root, row)
    for row in native["runs"]:
        native_run(root, row)
    require(len(eog["runs"]) == 13 and len(native["runs"]) == 6, "Attempt inventory changed.")
    require(len({x["session_id"] for x in eog["runs"] + native["runs"]}) == 19,
            "Repeated session counted as a separate attempt.")
    counts = Counter((x["revision"][:7], bool(x["grounded_review_eligible_final"])) for x in eog["runs"])
    require(counts == Counter({("92119fb", True): 4, ("62052b9", True): 3, ("62052b9", False): 1,
                              ("c71249e", True): 3, ("c71249e", False): 2}), "Hosted outcomes changed.")
    record = read_json(evidence / "final-verification.json")
    inspect(record)
    require(record["public_release_approved"] is False and record["hosted"]["all_case_qualification"] == "incomplete",
            "Audit cannot grant release qualification.")
    return {"audit_status": "passed", "eog_archives": 13, "observed_archives": 6,
            "hosted_all_case_qualification": "incomplete", "public_release_approved": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.root.resolve())))
    except (OSError, ValueError, KeyError, TypeError, IndexError) as error:
        print(json.dumps({"audit_status": "failed", "error": str(error)}))
        raise SystemExit(1)
