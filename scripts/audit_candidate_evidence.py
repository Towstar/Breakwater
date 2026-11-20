"""Read-only integrity/grounding audit of one declared seven-case hosted round.

The manifest binds witnessed process/source provenance to immutable evidence; it
is not execution attestation, numerical qualification, or release approval.
Paths are relative to the manifest directory. Existing dated audits are unchanged.
Exit codes: 0 = all seven passed, 2 = honest pending/incomplete round, 1 = invalid.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

from prepare_release_handoff import read_json, relative_name, unique_object
from verify_v03_evidence import MODEL, inspect, require


CASES = {
    "protocol_smoke": "nemotron_smoke",
    "canonical_1": "eog_live_check", "canonical_2": "eog_live_check",
    "canonical_3": "eog_live_check", "renamed_topology": "eog_live_check",
    "observed_pump": "desktop", "observed_missing_coverage": "desktop",
}
STATUSES = {"pending", "failed", "passed"}
MAX_EVIDENCE_BYTES = 8 * 1024 * 1024


def checksum(value, length=64):
    return isinstance(value, str) and bool(re.fullmatch("[0-9a-f]{%d}" % length, value))


def reference(root, entry):
    require(isinstance(entry, dict), "Evidence reference must be an object.")
    name = relative_name(entry["path"])
    path = (root / name).resolve(strict=True)
    require(path.is_relative_to(root) and path.is_file(), "Evidence escapes its manifest directory.")
    size = path.stat().st_size
    require(type(entry["bytes"]) is int and 0 < size == entry["bytes"] <= MAX_EVIDENCE_BYTES,
            "Evidence size differs or exceeds its bound.")
    require(checksum(entry["sha256"]), "Evidence needs a SHA-256 checksum.")
    raw = path.read_bytes()
    require(len(raw) == entry["bytes"] and hashlib.sha256(raw).hexdigest() == entry["sha256"],
            "Evidence checksum differs.")
    return path, raw


def json_reference(root, entry):
    _, raw = reference(root, entry)
    # Parse the bytes just hashed, not a second read that could have changed.
    value = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=unique_object)
    require(isinstance(value, dict), "Evidence must contain a JSON object.")
    inspect(value)
    return value


def number(value):
    return type(value) in (float, int)


def request_metadata(requests, observed=False, success=True):
    require(requests, "No retained hosted request metadata.")
    successful_requests = 0
    for request in requests:
        if observed:
            fields = {"configured_model", "response_model", "model_request_id", "duration_ms",
                      "error_code", "http_status", "outcome", "retry_count"}
            require(set(request) == fields and request["configured_model"] == MODEL,
                    "Unexpected observed request metadata.")
            require(request["outcome"] in {"succeeded", "failed", "cancelled", "rejected"}, "Unknown observed request outcome.")
            if request["outcome"] == "succeeded":
                require(request["response_model"] == MODEL and request["error_code"] == ""
                        and request["http_status"] == 200, "Successful observed reply differs from the configured model.")
                successful_requests += 1
        else:
            require(set(request) == {"duration_ms", "http_status", "model_identity", "retry_count", "type"}
                    and request["model_identity"] == MODEL, "Unexpected Operations request metadata.")
            successful_requests += int(request["http_status"] == 200)
        require(number(request["duration_ms"]) and request["duration_ms"] >= 0
                and type(request["retry_count"]) is int and request["retry_count"] >= 0,
                "Invalid request timing or retry count.")
        require(type(request["http_status"]) is int and 0 <= request["http_status"] <= 599,
                "Invalid retained HTTP status.")
    if success:
        require(successful_requests > 0, "Passing case lacks a successful matching-model reply.")


def pairs(calls, results, identity, name, complete=True):
    requested = [(x[identity], x[name]) for x in calls]
    completed = [(x[identity], x[name]) for x in results]
    require((bool(calls) if complete else True) and completed == requested[:len(completed)]
            and (not complete or len(requested) == len(completed)), "Tool call/result identities or order differ.")
    require(len({x[identity] for x in calls}) == len(calls), "A tool call identity was reused.")


def audit_eog(value, summary, passed):
    require(value["historical"] is True and value["actionable"] is False
            and value["operator_records"] == [], "Operations archive gained operating authority.")
    events = [row["event"] for row in value["agent_events"]]
    calls = [row for row in events if row["type"] == "tool_call"]
    results = [row for row in events if row["type"] == "tool_result"]
    if calls or results or passed:
        pairs(calls, results, "call_id", "name", complete=passed)
    requests = [row for row in events if row["type"] == "model_request"]
    if requests or passed:
        request_metadata(requests, success=passed)
    require(summary["session_id"] == value["session_id"] and
            summary["model_requests"] == len(requests), "Operations summary identity/count differs.")
    require(summary["tool_order"] == [row["name"] for row in calls], "Operations summary tool order differs.")
    capture = value["capture_manifest"]
    require(summary["capture_id"] == capture["capture_id"] and
            summary["dataset_identity"] == capture["dataset_identity"], "Operations summary capture differs.")
    evaluations, validations, validation_analyses = [], [], []
    for analysis in value["analyses"]:
        req, res = analysis["request"], analysis["response"]
        require(req["request_id"] == res["request_id"], "Provider request/result IDs differ.")
        for field in ("capture_id", "dataset_identity"):
            require(req[field] == res[field] == capture[field], "Provider capture identity differs.")
        result = res.get("result", {})
        if "evaluation" in result:
            evaluations.append(result["evaluation"])
        evaluations.extend(result.get("evaluations", []))
        if "validation" in result:
            validations.append(result["validation"])
            validation_analyses.append(analysis)
    if passed:
        require(summary["exit_code"] == 0 and summary["status"] == "passed"
                and summary["grounded_review_eligible_final"] is True,
                "Operations case is not a production-controller pass.")
        finals = [row for row in events if row["type"] == "final"]
        require(len(finals) == 1, "Passing Operations case needs one grounded final.")
        final = finals[0]
        require(final["selected_plan_id"] == summary["selected_plan_id"] and
                final["validation_id"] == summary["validation_id"], "Final and summary references differ.")
        matches = [item for item in validations if item["validation_id"] == final["validation_id"]]
        require(len(matches) == 1, "Final lacks its unique explicit validation.")
        validation = matches[0]
        require(validation["plan_id"] == final["selected_plan_id"] and validation["independent"] is True
                and validation["feasible"] is True and validation["coverage"] == "available"
                and validation["violations"] == [], "Selected plan lacks independent feasible coverage.")
        require(any(item.get("plan", {}).get("plan_id") == validation["plan_id"] and
                    item["plan"]["plan_hash"] == validation["plan_hash"] for item in evaluations),
                "Selected plan hash has no matching retained evaluation.")
        analysis = next(item for item in validation_analyses
                        if item["response"]["result"]["validation"]["validation_id"] == validation["validation_id"])
        request, response = analysis["request"], analysis["response"]
        require(request["operation"] == response["operation"] == "validate_plan" and response["status"] == "completed"
                and request["arguments"]["plan"]["plan_id"] == validation["plan_id"]
                and request["arguments"]["plan"]["plan_hash"] == validation["plan_hash"],
                "Final validation belongs to a different provider operation or plan.")
        linked = [row for row in results if row["name"] == "validate_plan" and row.get("ok") is True
                  and row.get("analysis_request_id") == request["request_id"]]
        call_by_id = {row["call_id"]: row for row in calls}
        require(linked and all(
            call_by_id[row["call_id"]]["arguments"]["plan_id"] == validation["plan_id"] and
            call_by_id[row["call_id"]]["arguments"]["evaluator_method"] == request["arguments"]["evaluator_method"]
            for row in linked), "Final validation has no matching model-issued validate_plan execution.")
    return {"session_id": value["session_id"], "dataset_identity": capture["dataset_identity"],
            "model_requests": len(requests), "tool_calls": len(calls)}


def audit_observed(value, missing, passed):
    require(value["archive_version"] == 2 and value["session_kind"] == "observed_origin"
            and value["actionable"] is False, "Wrong observed format or operating authority.")
    require(value["model"]["configured_model"] == MODEL, "Wrong observed model configuration.")
    origins = [row["payload"] for row in value["evidence_envelopes"] if row["kind"] == "observed_origin"]
    require(len(origins) == 1, "Observed case needs one immutable origin.")
    origin = origins[0]
    snapshot = origin["snapshot"]
    require(snapshot["actionable"] is False and origin["mapping"]["actionable"] is False,
            "Observed origin gained operating authority.")
    require(snapshot["physical_hash"] == origin["context"]["physical_hash"] and
            snapshot["configuration_hash"] == origin["mapping"]["detector"]["configuration_hash"],
            "Observed design/configuration identity differs.")
    events = value["events"]
    require(all(row["session_id"] == value["session_id"] for row in events), "Observed event session differs.")
    requests = [row["request"] for row in events if row["type"] == "model_request_completed"]
    if requests or passed:
        request_metadata(requests, observed=True, success=passed)
    calls = [row for row in events if row["type"] == "tool_requested"]
    starts = [row for row in events if row["type"] == "tool_started"]
    results = [row for row in events if row["type"] == "tool_completed"]
    if calls or results or passed:
        pairs(calls, starts, "tool_call_id", "tool_name", complete=passed)
        pairs(calls, results, "tool_call_id", "tool_name", complete=passed)
    recommendation = value.get("recommendation", {})
    if passed:
        require(value["status"] == "completed" and recommendation["source_mode"] == "hosted"
                and recommendation["actionable"] is False, "Observed completion is not hosted advisory evidence.")
        finals = [row for row in events if row["type"] == "recommendation_ready"]
        require(len(finals) == 1 and finals[0]["recommendation"] == recommendation,
                "Observed final differs from retained recommendation.")
        if missing:
            require(snapshot["availability"] == "unavailable" and origin["mapping"]["mapping_status"] == "unavailable"
                    and snapshot["expires_event_time_s"] <= snapshot["captured_event_time_s"],
                    "Missing-coverage case has available initial state.")
            require(recommendation["status"] == "no_validated_plan" and recommendation.get("selected_run_id") is None
                    and value["runs"] == [] and value["approvals"] == [], "Missing coverage produced a plan or approval.")
            require(not {row["tool_name"] for row in results if row.get("result", {}).get("ok") is True} &
                    {"forecast_baseline", "simulate_candidates", "search_interventions", "validate_run", "compare_runs",
                     "request_approval", "get_sensitivity_catalog", "analyze_sensitivities"},
                    "Missing coverage successfully executed numerical/approval tools.")
        else:
            require(snapshot["availability"] == "available" and origin["mapping"]["mapping_status"] == "available"
                    and snapshot["expires_event_time_s"] > snapshot["captured_event_time_s"],
                    "Pump case lacks available unexpired captured state.")
            require(value["snapshot"]["network"] == origin["effective_network"] and
                    value["snapshot"]["physical_hash"] == origin["mapping"]["effective_physical_hash"] and
                    origin["mapping"]["design_physical_hash"] == snapshot["physical_hash"],
                    "Investigation does not use the frozen measured network.")
            require(recommendation["status"] == "validated_recommendation" and recommendation["verified_feasible"] is True
                    and recommendation["validation_source"] == "validate_run", "No explicit validated observed recommendation.")
            selected = recommendation["selected_run_id"]
            runs = [row["run"] for row in value["runs"] if row["run"]["run_id"] == selected]
            require(len(runs) == 1, "Observed selected run is missing or ambiguous.")
            run = runs[0]
            for field in ("snapshot_id", "physical_hash", "horizon_h"):
                require(run[field] == recommendation[field] == value["snapshot"][field],
                        "Selected observed run or recommendation belongs to a different frozen state.")
            require(run["validation_id"] == recommendation["validation_id"] and run["validation_status"] == "validated"
                    and run["validation"]["feasible"] is True and run["validation"]["violations"] == []
                    and run["cost"] == recommendation["verified_cost"], "Selected observed run/validation/cost differs.")
            checked = [row["result"]["run"] for row in results if row["tool_name"] == "validate_run"
                       and row.get("result", {}).get("ok") is True and row["result"].get("run", {}).get("run_id") == selected
                       and row["result"]["run"].get("validation_id") == recommendation["validation_id"]]
            require(checked and all(item == run for item in checked), "Selected run lacks its exact model-requested validation result.")
    for approval in value["approvals"]:
        require(approval["status"] == "operator_acknowledged_advisory" and
                approval["run_id"] == recommendation.get("selected_run_id") and
                approval["validation_id"] == recommendation.get("validation_id"), "Observed history gained operating approval.")
    require(not any(row.get("applied") is True or row.get("result", {}).get("applied") is True for row in events),
            "Observed investigation applied controls.")
    return {"session_id": value["session_id"], "capture_id": snapshot["snapshot_id"],
            "model_requests": len(requests), "tool_calls": len(calls)}


def audit(path):
    path = path.resolve(strict=True)
    root = path.parent
    manifest = read_json(path)
    inspect(manifest)
    require(manifest["manifest_version"] == 1 and checksum(manifest["source_commit"], 40)
            and manifest["dirty"] is False and manifest["model_identity"] == MODEL,
            "A clean, source-qualified declared hosted round is required.")
    require(manifest["status"] in {"passed", "incomplete", "pending", "failed"}, "Unknown round status.")
    executables = manifest["executables"]
    require(set(executables) == set(CASES.values()) and all(checksum(value) for value in executables.values()),
            "Each candidate executable requires one SHA-256.")
    rows = manifest["cases"]
    require(isinstance(rows, list) and len(rows) == len(CASES) and {row["case"] for row in rows} == set(CASES),
            "Declare each of the seven cases exactly once.")
    output, sessions, datasets = [], set(), {}
    for row in rows:
        case = row["case"]
        require(row["source_commit"] == manifest["source_commit"] and row["executable"] == CASES[case]
                and row["executable_sha256"] == executables[CASES[case]], "Mixed source or executable provenance.")
        require(row["status"] in STATUSES, "Unknown case status.")
        result = {"case": case, "status": row["status"]}
        passed = row["status"] == "passed"
        if "evidence" in row:
            if case == "protocol_smoke":
                _, raw = reference(root, row["evidence"])
                text = raw.decode("utf-8-sig")
                inspect(text)
                if passed:
                    require(row["exit_code"] == 0 and "Executed actual get_network_state; snapshot " in text and
                            f"PASS: model {MODEL} issued a structured call, received its real result and continued." in text,
                            "Protocol smoke lacks actual structured-call continuation success.")
            elif CASES[case] == "eog_live_check":
                value = json_reference(root, row["evidence"])
                summary = json_reference(root, row["summary"])
                require(row["exit_code"] == summary["exit_code"], "Process exit and summary differ.")
                result.update(audit_eog(value, summary, passed))
                datasets[case] = result["dataset_identity"]
            else:
                value = json_reference(root, row["evidence"])
                result.update(audit_observed(value, case == "observed_missing_coverage", passed))
        else:
            require(not passed, "Passing case has no immutable evidence.")
        if "session_id" in result:
            require(result["session_id"] not in sessions, "A retained session was counted twice.")
            sessions.add(result["session_id"])
        output.append(result)
    canonical = [datasets[key] for key in ("canonical_1", "canonical_2", "canonical_3") if key in datasets]
    require(len(set(canonical)) <= 1, "Canonical rehearsals use different datasets.")
    if canonical and "renamed_topology" in datasets:
        require(datasets["renamed_topology"] != canonical[0], "Renamed rehearsal reused the canonical dataset.")
    complete = manifest["status"] == "passed" and all(row["status"] == "passed" for row in output)
    require(manifest["status"] != "passed" or complete, "Manifest promotes an incomplete round to passed.")
    return {"audit_version": 1, "audit_status": "passed", "qualification_status": "passed" if complete else "incomplete",
            "source_commit": manifest["source_commit"], "cases": output, "public_release_approved": False,
            "limits": "Hash/source bindings are witnessed manifest provenance, not execution attestation. "
                      "This audit checks retained integrity/grounding, not numerical accuracy, model prose or external release gates."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    try:
        report = audit(args.manifest)
        print(json.dumps(report, indent=2))
        raise SystemExit(0 if report["qualification_status"] == "passed" else 2)
    except (OSError, ValueError, KeyError, TypeError, IndexError, UnicodeError, AttributeError, RecursionError):
        # Never echo untrusted payloads, paths or exception text into shared logs.
        print(json.dumps({"audit_version": 1, "audit_status": "failed", "code": "invalid_candidate_evidence",
                          "public_release_approved": False}))
        raise SystemExit(1)
