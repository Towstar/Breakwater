"""Read-only semantic checks for one bounded evaluation attempt.

This consumes retained application evidence; it never invokes a model, repairs
an archive, certifies free text, or grants approval. Hash/source/process binding
belongs to the enclosing runner manifest. Synthetic unit fixtures are not runs.
"""
from collections import Counter
from datetime import datetime
import json

from audit_candidate_evidence import audit_eog, pairs, request_metadata
from prepare_release_handoff import unique_object
from nemotron_evaluation_fixtures import POLICY
from verify_v03_evidence import MODEL, inspect, require

EXPECTED = {"advisory_plan", "no_supported_plan", "missing_coverage", "stale_coverage"}
READS = {"get_network_state", "get_control_catalog", "get_observations", "get_detection_events",
         "get_sensitivity_catalog", "read_topology", "read_observations"}
COVERAGE_CODES = {"failure_context_incomplete", "failure_state_unavailable", "well_rate_unavailable",
                  "well_rate_ambiguous", "inflow_ambiguous"}
CORRECTIONS = {"final_format_rejected_" + reason for reason in (
    "content_type", "encoding_or_size", "json_document", "field_set", "summary", "selected_plan_reference",
    "validation_reference", "event_ids", "event_reference", "duplicate_event_ids", "response_truncated")}
TRANSPORT_CODES = {"transport_failed", "rate_limited", "authentication_failed", "model_unavailable",
                   "timeout", "response_invalid_json", "response_invalid_envelope", "response_truncated",
                   "request_budget_exceeded", "rate_limit_wait_exceeds_budget", "retry_wait_exceeds_budget"}
PHYSICAL_VIOLATIONS = {"storage_capacity", "negative_inventory", "destination_throughput", "route_capacity"}
EVENT_FIELDS = {"type", "session_id", "snapshot_id", "sequence", "timestamp", "elapsed_ms"}
PREDISPATCH_ERRORS = {
    "invalid_arguments": "Tool arguments must be a JSON object.",
    "refinement_limit": "Candidate refinement limit reached; inspect existing evidence or finish."}
CANDIDATE_TOOLS = {"simulate_candidates", "search_interventions"}


def _object(value):
    require(isinstance(value, dict), "Expected an evidence object.")
    return value


def _array(value):
    require(isinstance(value, list), "Expected an evidence array.")
    return value


def _identity(value):
    require(isinstance(value, str) and bool(value) and len(value) <= 512, "Invalid evidence identity.")
    return value


def _authority(value):
    if isinstance(value, dict):
        require(value.get("applied") is not True and value.get("controls_applied") is not True
                and value.get("approval_status") != "operator_applied", "Evaluation evidence contains applied controls.")
        for child in value.values():
            _authority(child)
    elif isinstance(value, list):
        for child in value:
            _authority(child)


def _reference(value):
    return None if value in (None, "") else _identity(value)


def _brief(row):
    value = row.get("summary", {})
    if isinstance(value, str):
        require(len(value) <= 4000, "Oversized tool summary.")
        try:
            value = json.loads(value, object_pairs_hook=unique_object)
        except (ValueError, TypeError):
            raise ValueError("Malformed retained tool summary.") from None
    inspect(value)
    return _object(value)


def retained_tool_metrics(requests, calls, results, *, logical=None, arguments="arguments", name="name", identity="call_id"):
    """Count retained requests/tools; absent arguments never imply equality."""
    counters, successes, reused = Counter(), 0, 0
    read_repeats = expensive = executions = unknown_arguments = 0
    by_id = {row[identity]: row for row in results}
    for call in calls:
        result = by_id.get(call[identity], {})
        payload = result.get("result", {})
        ok = result.get("ok") is True if name == "name" else isinstance(payload, dict) and payload.get("ok") is True
        cached = (_brief(result).get("reused_validation") is True if name == "name" else
                  isinstance(payload, dict) and payload.get("reused_validation") is True)
        successes += int(ok)
        reused += int(cached and ok)
        if arguments not in call:
            # Predispatch rejection records contain no raw argument object.
            # Absence cannot establish equality with another call's {}.
            unknown_arguments += 1
            continue
        args = _object(call[arguments])
        key = (call[name], json.dumps(args, sort_keys=True, ensure_ascii=False, allow_nan=False, separators=(",", ":")))
        if counters[key]:
            if call[name] in READS:
                read_repeats += 1
            else:
                expensive += 1
                executions += int(ok and not cached)
        counters[key] += 1
    retry_count = sum(row["retry_count"] for row in requests)
    has_attempts = ["transport_diagnostics" in row for row in requests]
    http_attempts = sum(len(row["transport_diagnostics"]["attempts"]) if "transport_diagnostics" in row
                        else 1 + row["retry_count"] for row in requests)
    basis = ("per_attempt_diagnostics" if has_attempts and all(has_attempts) else
             "mixed_attempt_and_legacy_metadata" if any(has_attempts) else "completed_request_metadata")
    logical_count = len(requests) if logical is None else logical
    return {"logical_requests": logical_count, "model_requests": logical_count,
            "completed_model_requests": len(requests), "http_attempts": http_attempts,
            "http_attempts_basis": basis, "request_counts_complete": logical_count == len(requests),
            "retries": retry_count, "retry_count": retry_count, "total_tool_calls": len(calls), "tool_calls": len(calls),
            "duplicate_calls": sum(count - 1 for count in counters.values()), "read_repeats": read_repeats,
            "duplicate_expensive_calls": expensive, "duplicate_expensive_executions": executions,
            "unknown_argument_calls": unknown_arguments, "duplicate_counts_complete": unknown_arguments == 0,
            "successful_tools": successes, "validation_reuse": reused, "actual_tool_executions": successes - reused,
            "model_duration_ms": sum(row["duration_ms"] for row in requests)}


def _process(summary, archive, session, *, preflight=False):
    require(summary["session_id"] == session, "Process/session identity differs.")
    if preflight:
        require(summary.get("preflight") is True and summary.get("provider_exchange_verified") is True
                and type(summary.get("provider_verified")) is bool
                and summary.get("zero_model_requests") is True and summary.get("inputs_unchanged") is True
                and summary.get("history_reopened_nonactionable") is True and summary.get("actionable") is False
                and summary["status"] == "passed", "Local provider preflight is not technically verified.")
        if "exit_code" not in summary:
            require(archive["status"] == "completed", "Local preflight did not retain completed evidence.")
            return 0
    require(type(summary["exit_code"]) is int and summary["exit_code"] in {0, 2, 3, 4, 5, 6}, "Invalid process exit code.")
    expected_status = "passed" if summary["exit_code"] == 0 else "incomplete" if summary["exit_code"] == 3 else "failed"
    require(summary["status"] == expected_status, "Process status/exit code differs.")
    require(summary.get("archive_written", True) is True, "Summary does not retain an archive.")
    if archive["status"] != "completed":
        require(summary["exit_code"] != 0 and summary["status"] != "passed", "Process summary promotes a noncompleted archive.")
    return summary["exit_code"]


def _model(requests, hosted, *, sandbox=False, completed=True):
    if not hosted:
        require(not requests, "Local evidence contains hosted requests.")
        return
    if requests:
        require(sandbox or not completed or all("http_status" in row for row in requests),
                "Completed Operations evidence omits an HTTP status.")
        request_metadata(requests, observed=sandbox, success=False)
    for row in requests:
        if sandbox:
            require(row["configured_model"] == MODEL, "Configured hosted model differs.")
            if row["outcome"] == "succeeded":
                require(row["response_model"] == MODEL, "Successful response model differs.")
        elif row.get("http_status", 0) == 200:
            require(row["model_identity"] == MODEL, "Successful response model differs.")


def _failed(events, status, *, sandbox=False):
    if sandbox:
        failures = [row.get("error", {}).get("code", "failed_investigation") for row in events if row["type"] == "agent_failed"]
        cancelled = any(row["type"] == "agent_cancelled" for row in events)
    else:
        failures = [row.get("code", "failed_investigation") for row in events
                    if row["type"] == "failure" and row.get("code") not in CORRECTIONS]
        cancelled = any(row["type"] == "cancel" for row in events)
    if any(code in TRANSPORT_CODES or code.startswith("response_invalid_") for code in failures):
        return "transport_failure"
    if cancelled or status in {"cancelled", "interrupted"}:
        return "cancelled_investigation"
    if failures or status != "completed":
        return "failed_investigation"
    return None


def _sandbox_starts(events, calls, starts, results, *, complete):
    """Match dispatches without inventing starts for the two gateway bypasses."""
    elapsed = 0
    for sequence, row in enumerate(events, 1):
        require(type(row["sequence"]) is int and row["sequence"] == sequence,
                "Sandbox event sequence differs from its retained order.")
        require(type(row["elapsed_ms"]) is int and row["elapsed_ms"] >= elapsed,
                "Sandbox event elapsed time is invalid or moves backwards.")
        elapsed = row["elapsed_ms"]
        stamp = row["timestamp"]
        require(isinstance(stamp, str) and len(stamp) == 24 and stamp.endswith("Z"), "Sandbox event timestamp must be UTC milliseconds.")
        try:
            parsed = datetime.strptime(stamp, "%Y-%m-%dT%H:%M:%S.%fZ")
        except ValueError:
            raise ValueError("Invalid Sandbox event timestamp.") from None
        require(parsed.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z" == stamp, "Sandbox event timestamp is not canonical UTC milliseconds.")
    start_by_id = {row["tool_call_id"]: row for row in starts}
    require(len(start_by_id) == len(starts), "A tool dispatch identity was reused.")
    result_by_id = {row["tool_call_id"]: row for row in results}
    rejected, refinements = set(), 0
    for call in calls:
        identity, name = call["tool_call_id"], call["tool_name"]
        refinements += int(name in CANDIDATE_TOOLS)
        result, start = result_by_id.get(identity), start_by_id.get(identity)
        if start is not None:
            _object(start["input_summary"])
            require(start["tool_name"] == name and call["sequence"] < start["sequence"],
                    "Tool dispatch precedes or differs from its request.")
        if result is None:
            continue  # Only an unfinished tail is allowed by call/result pairing.
        require(call["sequence"] < result["sequence"], "Tool result precedes its request.")
        if start is not None:
            require(start["sequence"] < result["sequence"] and
                    set(result) != EVENT_FIELDS | {"tool_name", "tool_call_id", "error"},
                    "Tool result precedes dispatch or claims both dispatch and predispatch rejection.")
            continue
        require(result["type"] == "tool_failed" and
                set(result) == EVENT_FIELDS | {"tool_name", "tool_call_id", "error"},
                "A non-predispatch tool result has no start.")
        error = _object(result["error"])
        code = error.get("code")
        require(code in PREDISPATCH_ERRORS and error == {"code": code, "message": PREDISPATCH_ERRORS[code]},
                "Unknown predispatch tool rejection.")
        over_limit = name in CANDIDATE_TOOLS and refinements > POLICY["refinement_rounds"]
        require((code == "refinement_limit") == over_limit,
                "Predispatch rejection differs from the candidate refinement boundary.")
        require([row["type"] for row in events if row.get("tool_call_id") == identity] == ["tool_requested", "tool_failed"],
                "Predispatch rejection also contains execution evidence.")
        rejected.add(identity)
    expected = [(row["tool_call_id"], row["tool_name"]) for row in calls if row["tool_call_id"] not in rejected]
    dispatched = [(row["tool_call_id"], row["tool_name"]) for row in starts]
    require(dispatched == expected[:len(dispatched)] and (not complete or dispatched == expected),
            "Tool dispatch identities or order differ.")
    return rejected


def _sandbox(archive, summary, expected, hosted):
    require(archive["archive_version"] == 1 and not archive.get("session_kind"), "Expected a Sandbox session-v1 archive.")
    require(archive["status"] in {"running", "completed", "failed", "cancelled", "stale"}, "Unknown Sandbox archive status.")
    session = _identity(archive["session_id"])
    _process(summary, archive, session)
    require(archive["approvals"] == [] and summary["controls_unchanged"] is True
            and summary["inputs_unchanged"] is True and summary["controls_applied"] is False,
            "Sandbox input or operating authority changed.")
    events = _array(archive["events"])
    require(all(row["session_id"] == session for row in events), "Event/session identity differs.")
    snapshot = _object(archive["snapshot"])
    require(summary["physical_hash"] == snapshot["physical_hash"], "Snapshot/process physical hash differs.")
    calls = [row for row in events if row["type"] == "tool_requested"]
    starts = [row for row in events if row["type"] == "tool_started"]
    results = [row for row in events if row["type"] in {"tool_completed", "tool_failed"}]
    complete = archive["status"] == "completed"
    pairs(calls, results, "tool_call_id", "tool_name", complete=complete)
    rejected = _sandbox_starts(events, calls, starts, results, complete=complete)
    require(all(row.get("snapshot_id", snapshot["snapshot_id"]) == snapshot["snapshot_id"] for row in events),
            "Event/frozen snapshot identity differs.")
    requests = [row["request"] for row in events if row["type"] == "model_request_completed"]
    _model(requests, hosted, sandbox=True, completed=archive["status"] == "completed")
    if hosted:
        require(archive["model"]["configured_model"] == MODEL, "Archive hosted model differs.")
    logical = sum(row["type"] == "model_request_started" for row in events)
    require(summary["model_requests"] == logical, "Logical request count differs.")
    if complete:
        require(logical == len(requests), "Completed investigation has unfinished request metadata.")
    start_by_id = {row["tool_call_id"]: row for row in starts}
    metric_calls = [{**call, **({"arguments": start_by_id[call["tool_call_id"]]["input_summary"]}
                              if call["tool_call_id"] in start_by_id else {})} for call in calls]
    metrics = retained_tool_metrics(requests, metric_calls, results, logical=logical, name="tool_name", identity="tool_call_id")
    metrics.update(predispatch_rejections=len(rejected), dispatched_tool_calls=len(starts))
    runs = {}
    for detail in _array(archive["runs"]):
        run = _object(detail["run"])
        require(detail["ok"] is True and run["run_id"] not in runs, "Duplicate or unsuccessful stored run.")
        for field in ("snapshot_id", "physical_hash", "horizon_h"):
            require(run[field] == snapshot[field], "Stored run is detached from the frozen snapshot.")
        runs[run["run_id"]] = run
    recommendation = _object(archive["recommendation"])
    selected = _reference(recommendation.get("selected_run_id"))
    validation_id = _reference(recommendation.get("validation_id"))
    require(summary.get("grounded_validated_final") is not True or selected is not None, "Null selection claims a validated final.")
    require(_reference(summary.get("selected_run_id")) == selected and _reference(summary.get("validation_id")) == validation_id,
            "Process and retained recommendation references differ.")
    finals = [row for row in events if row["type"] == "recommendation_ready"]
    if complete:
        require(len(finals) == 1 and finals[0]["recommendation"] == recommendation, "Final differs from retained recommendation.")
        require(recommendation["source_mode"] == ("hosted" if hosted else "local_native"), "Wrong recommendation origin.")
    def bound_validation(run):
        return any(row["tool_name"] == "validate_run" and row.get("result", {}).get("ok") is True
                   and row["result"].get("run") == run
                   and start_by_id[row["tool_call_id"]].get("input_summary", {}).get("run_id") == run["run_id"]
                   and start_by_id[row["tool_call_id"]].get("input_summary", {}).get("snapshot_id") == snapshot["snapshot_id"]
                   for row in results)
    explicit_validation_missing = False
    if selected:
        require(selected in runs and validation_id, "Selected run/validation is missing.")
        run = runs[selected]
        for field in ("snapshot_id", "physical_hash", "horizon_h"):
            require(recommendation[field] == run[field], "Recommendation/frozen run identity differs.")
        require(recommendation["status"] == "validated_recommendation" and recommendation["ok"] is True
                and recommendation["verified_feasible"] is True and run["validation_status"] == "validated"
                and run["validation_id"] == validation_id and run["validation"]["feasible"] is True
                and run["validation"]["violations"] == [] and recommendation["controls"] == run["controls"]
                and recommendation["verified_cost"] == run["cost"] == recommendation["cost"]
                and type(run["cost"]) in (int, float) and run["cost"] >= 0, "Selected native evidence differs.")
        require(recommendation["validation_source"] == run["validation_source"], "Validation source differs.")
        if hosted:
            source = run["validation_source"]
            require(source in {"validate_run", "native_search_dense_reference"}, "Unsupported selected validation source.")
            for row in results:
                if row["tool_name"] != "validate_run" or row.get("result", {}).get("ok") is not True:
                    continue
                args = start_by_id[row["tool_call_id"]]["input_summary"]
                checked = row["result"].get("run", {})
                if args.get("run_id") == selected or checked.get("run_id") == selected:
                    require(source == "validate_run" and checked == run and args.get("run_id") == selected and
                            args.get("snapshot_id") == snapshot["snapshot_id"], "Selected explicit validation event is mismatched.")
            if source == "validate_run":
                require(bound_validation(run), "Selected run lacks its actual independent validation operation.")
            else:
                require(any(row["type"] == "tool_completed" and row["tool_name"] == "search_interventions" and
                            row.get("result", {}).get("ok") is True and run in row["result"].get("runs", []) and
                            all(row["result"].get(field) == snapshot[field] for field in ("snapshot_id", "physical_hash", "horizon_h")) and
                            start_by_id[row["tool_call_id"]]["input_summary"].get("snapshot_id") == snapshot["snapshot_id"]
                            for row in results), "Selected native-search proof has no matching completed search.")
                explicit_validation_missing = True
        else:
            require(run["validation_source"] in {"validate_run", "native_search_dense_reference"}, "Unsupported selected validation source.")
    else:
        require(validation_id is None, "A null selected run has a validation reference.")
    metrics.update(stored_runs=len(runs), selected_cost=runs[selected]["cost"] if selected else None, provider_exchanges=0)
    failure = _failed(events, archive["status"], sandbox=True)
    if failure:
        return failure, metrics
    if hosted and not any(row["outcome"] == "succeeded" for row in requests):
        return "no_successful_hosted_reply", metrics
    if summary["exit_code"] not in {0, 3}:
        return "incomplete_execution", metrics
    if explicit_validation_missing:
        return "explicit_model_validation_missing", metrics
    if selected:
        return ("advisory_plan" if expected == "advisory_plan" and summary["exit_code"] == 0 else "unexpected_selected_plan"), metrics
    search = [row.get("result", {}) for row in results if row["tool_name"] == "search_interventions"
              and row.get("result", {}).get("ok") is True]
    rejected = any(run.get("validation_status") == "validated" and run.get("validation", {}).get("feasible") is False
                   and run["validation"].get("violations")
                   and {item["code"] for item in run["validation"]["violations"]} <= PHYSICAL_VIOLATIONS
                   and bound_validation(run) for run in runs.values())
    supported_search = any(row.get("search_status") == "no_feasible_plan_found"
                           and row.get("diagnostics", {}).get("candidates_evaluated", 0) > 0
                           and row.get("diagnostics", {}).get("numerical_screen_rejections", 0) == 0
                           and row.get("diagnostics", {}).get("reference_rejections", 0) == 0 for row in search)
    feasible_retained = any(run.get("validation_status") == "validated" and run.get("validation", {}).get("feasible") is True
                            for run in runs.values())
    bounded = recommendation.get("status") == "no_validated_plan" and not feasible_retained and (supported_search or rejected)
    return ("no_supported_plan" if expected == "no_supported_plan" and bounded else "unsupported_abstention"), metrics


def _eog(archive, summary, expected, hosted):
    require(archive["eog_session_version"] == 1, "Expected an EOG session-v1 archive.")
    require(archive["status"] in {"running", "completed", "failed", "cancelled", "interrupted"}, "Unknown EOG archive status.")
    session = _identity(archive["session_id"])
    process_code = _process(summary, archive, session, preflight=not hosted)
    rows = _array(archive["agent_events"])
    events = [row["event"] for row in rows]
    previous = 0
    for row in rows:
        require(type(row["sequence"]) is int and row["sequence"] > previous, "Invalid EOG event ordering.")
        previous = row["sequence"]
    requests = [row for row in events if row["type"] == "model_request"]
    _model(requests, hosted, completed=archive["status"] == "completed")
    require(summary["model_requests"] == len(requests), "EOG request count differs.")
    calls = [row for row in events if row["type"] == "tool_call"]
    results = [row for row in events if row["type"] == "tool_result"]
    pairs(calls, results, "call_id", "name", complete=archive["status"] == "completed" and hosted)
    # The original success auditor assumes every response has a result object;
    # unavailable provider responses legitimately retain result=null. Preserve
    # their bytes and apply common pairing/binding checks here, using the
    # unchanged strict auditor for actual hosted validated success below.
    require(archive["historical"] is True and archive["actionable"] is False and archive["operator_records"] == [],
            "EOG evidence gained operating authority.")
    if hosted:
        require(summary["tool_order"] == [row["name"] for row in calls], "EOG process tool order differs.")
    metrics = retained_tool_metrics(requests, calls, results)
    capture = archive["capture_manifest"]
    require(summary["capture_id"] == capture["capture_id"] and summary["dataset_identity"] == capture["dataset_identity"],
            "Process/captured input identity differs.")
    if not hosted:
        require(summary["cutoff_unix"] == capture["cutoff_unix"], "Local preflight/capture cutoff differs.")
    analyses = _array(archive["analyses"])
    by_request, evaluations, validations = {}, [], []
    for analysis in analyses:
        request, response = _object(analysis["request"]), _object(analysis["response"])
        identity = _identity(request["request_id"])
        require(identity not in by_request, "A numerical request identity was counted twice.")
        by_request[identity] = analysis
        require(response["status"] in {"completed", "unavailable", "invalid_input", "failed"}, "Unknown provider result status.")
        for field in ("request_id", "operation", "capture_id", "dataset_identity", "cutoff_unix"):
            require(request[field] == response[field], "Numerical operation/result binding differs.")
        for field in ("capture_id", "dataset_identity", "cutoff_unix"):
            require(request[field] == capture[field], "Numerical operation/capture binding differs.")
        result = response.get("result") or {}
        if "evaluation" in result:
            evaluations.append(result["evaluation"])
        evaluations.extend(result.get("evaluations", []))
        if "validation" in result:
            validations.append((result["validation"], analysis))
    if not hosted:
        require(summary["analysis_request_ids"] == [row["request"]["request_id"] for row in analyses],
                "Local preflight/numerical request identities differ.")
    linked = set()
    for row in results:
        identity = row.get("analysis_request_id")
        if identity:
            require(identity in by_request and by_request[identity]["request"]["operation"] == row["name"],
                    "Model tool result/numerical operation binding differs.")
            linked.add(identity)
    if hosted:
        require(set(by_request) <= linked, "Numerical evidence has no model-requested execution.")
    finals = [row for row in events if row["type"] == "final"]
    if hosted and archive["status"] == "completed":
        require(len(finals) == 1, "Completed hosted EOG evidence lacks one grounded final.")
    final = finals[0] if finals else summary
    selected = _reference(final.get("selected_plan_id"))
    validation_id = _reference(final.get("validation_id"))
    require(summary.get("grounded_review_eligible_final") is not True or selected is not None,
            "Null selection claims a review-eligible final.")
    require(_reference(summary.get("selected_plan_id")) == selected and _reference(summary.get("validation_id")) == validation_id,
            "Process/final EOG references differ.")
    if selected:
        require(hosted or summary["provider_verified"] is True,
                "Selected advice lacks verified numerical provider execution.")
        matches = [(value, analysis) for value, analysis in validations if value["validation_id"] == validation_id]
        require(len(matches) == 1, "Selected EOG plan lacks one exact validation.")
        checked, analysis = matches[0]
        request, response = analysis["request"], analysis["response"]
        require(checked["plan_id"] == selected and checked["independent"] is True and checked["feasible"] is True
                and checked["coverage"] == "available" and checked["violations"] == []
                and request["operation"] == response["operation"] == "validate_plan" and response["status"] == "completed"
                and request["arguments"]["plan"]["plan_id"] == selected
                and request["arguments"]["plan"]["plan_hash"] == checked["plan_hash"], "EOG selected validation is detached.")
        selected_evaluations = [value for value in evaluations if value["plan"]["plan_id"] == selected
                                and value["plan"]["plan_hash"] == checked["plan_hash"]]
        require(selected_evaluations, "Selected EOG plan hash has no evaluation.")
        if hosted and process_code == 0:
            audit_eog(archive, summary, passed=True)
        cost = selected_evaluations[0].get("cost", {}).get("total")
    else:
        require(validation_id is None, "Null EOG selection has a validation reference.")
        cost = None
    known_events = {event["event_id"] for analysis in analyses for event in (analysis["response"].get("result") or {}).get("events", [])}
    for final_row in finals:
        event_ids = final_row.get("event_ids", [])
        require(isinstance(event_ids, list) and len(set(event_ids)) == len(event_ids) and set(event_ids) <= known_events,
                "Final refers to unknown or repeated detector evidence.")
    metrics.update(stored_runs=len({value["plan"]["plan_id"] for value in evaluations}), selected_cost=cost, provider_exchanges=len(analyses),
                   provider_verified=summary.get("provider_verified"), provider_exchange_verified=summary.get("provider_exchange_verified"))
    failure = _failed(events, archive["status"])
    if failure:
        return failure, metrics
    if hosted and not any(row.get("http_status", 0) == 200 for row in requests):
        return "no_successful_hosted_reply", metrics
    if process_code not in {0, 3}:
        return "incomplete_execution", metrics
    if selected:
        return ("advisory_plan" if expected == "advisory_plan" and process_code == 0 else "unexpected_selected_plan"), metrics
    completed_detection = any(analysis["request"]["operation"] == "detect" and analysis["response"]["status"] == "completed"
                              for analysis in analyses)
    unavailable = {diagnostic["code"] for analysis in analyses
                   if analysis["request"]["operation"] in {"search_plans", "evaluate_plan", "validate_plan"}
                   and analysis["response"]["status"] == "unavailable"
                   for diagnostic in analysis["response"].get("diagnostics", [])}
    if completed_detection and expected in {"missing_coverage", "stale_coverage"} and unavailable & COVERAGE_CODES:
        require(not any(value.get("feasible") is True and value.get("coverage") == "available" for value in evaluations),
                "Missing-coverage refusal retained a feasible response.")
        return expected, metrics
    no_feasible = any(analysis["request"]["operation"] == "search_plans" and analysis["response"]["status"] == "completed"
                      and (analysis["response"].get("result") or {}).get("selected_plan_id") is None
                      and (analysis["response"].get("result") or {}).get("search", {}).get("evaluated_candidates", 0) > 0
                      and (analysis["response"].get("result") or {}).get("search", {}).get("complete") is True
                      for analysis in analyses)
    available_evaluation = any(value.get("feasible") is True and value.get("coverage") == "available" for value in evaluations)
    if expected == "no_supported_plan" and completed_detection and not available_evaluation and ("no_active_failure" in unavailable or no_feasible):
        return "no_supported_plan", metrics
    return "unsupported_abstention", metrics


def audit_case(mode, archive, summary, expected, *, hosted=True):
    """Return semantic classification/metrics; raise ValueError for corrupt evidence.

    A passed refusal is bounded to the captured evidence and expected fixture
    category. It never establishes global infeasibility. Missing versus stale is
    declared by the independently bound fixture; provider refusal proves the
    shared unavailable-coverage behavior, not that fixture's causal diagnosis.
    """
    try:
        require(mode in {"sandbox", "eog"} and expected in EXPECTED and type(hosted) is bool, "Unknown evaluation contract.")
        _object(archive); _object(summary); inspect(archive); inspect(summary); _authority(archive); _authority(summary)
        classification, metrics = (_sandbox if mode == "sandbox" else _eog)(archive, summary, expected, hosted)
        return {"classification": classification, "passed": classification == expected,
                "mode": mode, "hosted": hosted, "expected": expected, **metrics,
                "limits": "Retained synthetic functional evidence; no execution attestation, prose certification, global infeasibility or approval."}
    except (KeyError, TypeError, IndexError, AttributeError, RecursionError, OverflowError):
        raise ValueError("Malformed evaluation evidence.") from None
