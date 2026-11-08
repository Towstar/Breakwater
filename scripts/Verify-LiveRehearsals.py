"""Audit supplied live rehearsal evidence; never run a model or apply controls.

Checks internal consistency of actual application archives. JSON/database evidence
is not cryptographically authenticated, so this is not an independent execution
attestation or a replacement for observing the operator-review action.
"""

import argparse
from contextlib import closing
from datetime import datetime
import json
import math
from pathlib import Path
import re
import sqlite3
import sys
from uuid import UUID


MAX_ARCHIVE_BYTES = 512 * 1024 * 1024
DEFAULT_MODEL = "nvidia/nemotron-3.5-lightning-30b-a3b"
TOOLS = {
    "get_network_state", "get_control_catalog", "forecast_baseline", "simulate_candidates",
    "validate_run", "compare_runs", "request_approval", "search_interventions",
    "get_sensitivity_catalog", "analyze_sensitivities",
}


class AuditError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise AuditError(message)


def number(value):
    require(isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value),
            "A numeric evidence field is missing or nonfinite.")
    return value


def integer(value, minimum=0, maximum=9007199254740991):
    value = number(value)
    require(minimum <= value <= maximum and int(value) == value, "An integer evidence field is invalid.")
    return int(value)


def text(value, maximum=256):
    require(isinstance(value, str) and value.strip() and len(value) <= maximum,
            "A text evidence field is missing or oversized.")
    return value


def identifier(value):
    value = text(value)
    try:
        require(UUID(value).int != 0, "An evidence identity is not a nonzero UUID.")
    except (ValueError, AttributeError):
        raise AuditError("An evidence identity is not a nonzero UUID.") from None
    return value


def stamp(value):
    value = text(value, 64)
    try:
        result = datetime.fromisoformat(value.replace("Z", "+00:00"))
        require(result.utcoffset() is not None, "An evidence timestamp needs an explicit offset.")
        return result
    except ValueError:
        raise AuditError("An evidence timestamp is malformed.") from None


def array(value, minimum=0, maximum=1024):
    require(isinstance(value, list) and minimum <= len(value) <= maximum,
            "An evidence array is missing or outside its bounds.")
    return value


def object_(value):
    require(isinstance(value, dict), "An evidence object is missing.")
    return value


def same_number(left, right):
    return number(left) == number(right)


def inspect(value, depth=0):
    require(depth <= 64, "Evidence nesting exceeds its supported bound.")
    if isinstance(value, float):
        number(value)
    elif isinstance(value, list):
        for item in value:
            inspect(item, depth + 1)
    elif isinstance(value, dict):
        for key, item in value.items():
            normalized = key.lower().replace("_", "").replace("-", "")
            require(normalized not in {"apikey", "nvidiaapikey", "authorization", "reasoning", "reasoningcontent"},
                    "Credential or raw-reasoning fields are prohibited.")
            inspect(item, depth + 1)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON object fields are not accepted.")
        result[key] = value
    return result


def decode(bytes_):
    require(len(bytes_) <= MAX_ARCHIVE_BYTES, "The archive exceeds the 512 MiB session limit.")
    try:
        result = json.loads(bytes_, object_pairs_hook=unique_object,
                            parse_constant=lambda _: (_ for _ in ()).throw(AuditError("Nonfinite JSON is prohibited.")))
    except (json.JSONDecodeError, UnicodeDecodeError, RecursionError):
        raise AuditError("The supplied archive is not supported JSON.") from None
    object_(result)
    inspect(result)
    return result


def read_archive(path):
    with Path(path).open("rb") as source:
        return decode(source.read(MAX_ARCHIVE_BYTES + 1))


def controls(value):
    result = {}
    for item in array(value, 0, 384):
        item = object_(item)
        control = text(item.get("control_id"), 512)
        require(control not in result, "Control identities must be unique.")
        result[control] = number(item.get("value"))
    return result


def topology(network):
    assets = {}
    for value in array(network.get("assets"), 1, 128):
        asset = object_(value)
        key = text(asset.get("id"))
        require(key not in assets and asset.get("kind") in {"tank", "source", "treatment", "trucking"},
                "Asset identities must be unique and asset kinds recognized.")
        assets[key] = asset
    incoming = {key: [] for key in assets}
    outgoing = {key: [] for key in assets}
    routes = array(network.get("routes"), 0, 256)
    route_ids = set()
    for route in routes:
        route = object_(route)
        route_id = text(route.get("id"))
        require(route_id not in route_ids, "Route identities must be unique.")
        route_ids.add(route_id)
        source, target = route.get("source_id"), route.get("target_id")
        require(source in assets and target in assets, "Topology contains an unknown route endpoint.")
        outgoing[source].append(assets[target].get("kind"))
        incoming[target].append(assets[source].get("kind"))
    # A changed typed-degree signature proves more than a label/ID change. It is
    # intentionally sufficient rather than a general graph-isomorphism solver.
    signature = sorted((asset.get("kind"), tuple(sorted(incoming[key])), tuple(sorted(outgoing[key])))
                       for key, asset in assets.items())
    return assets, routes, signature


def canonical(network):
    assets, routes, _ = topology(network)
    expected_assets = {
        "tank_a": {"kind": "tank", "capacity_m3": 120, "initial_m3": 80},
        "tank_b": {"kind": "tank", "capacity_m3": 130, "initial_m3": 114},
        "source_a": {"kind": "source", "inflow_m3h": 52, "curtailment_m3h": 0, "curtailment_max_m3h": 15, "cost_per_m3": 250},
        "source_b": {"kind": "source", "inflow_m3h": 16, "curtailment_m3h": 0, "curtailment_max_m3h": 0, "cost_per_m3": 250},
        "treatment": {"kind": "treatment", "capacity_m3h": 60, "cost_per_m3": 0},
        "trucking": {"kind": "trucking", "capacity_m3h": 22, "cost_per_m3": 80},
    }
    expected_routes = {
        "feed_a": ("source_a", "tank_a", 52, 52, False, True),
        "feed_b": ("source_b", "tank_b", 16, 16, False, True),
        "main_pump": ("tank_a", "treatment", 55, 0, True, False),
        "backup_pump": ("tank_b", "treatment", 35, 16, True, True),
        "bypass": ("tank_a", "tank_b", 25, 0, False, True),
        "truck_route": ("tank_a", "trucking", 22, 0, False, True),
    }
    require(set(assets) == set(expected_assets), "A canonical rehearsal needs the unmodified canonical incident assets.")
    for key, expected in expected_assets.items():
        require(all(assets[key].get(field) == value for field, value in expected.items()),
                "Canonical asset state differs from the declared incident fixture.")
    by_id = {text(route.get("id")): route for route in routes}
    require(set(by_id) == set(expected_routes), "Canonical route identities differ from the incident fixture.")
    fields = ("source_id", "target_id", "capacity_m3h", "setpoint_m3h", "has_pump", "available")
    for key, expected in expected_routes.items():
        require(tuple(by_id[key].get(field) for field in fields) == expected and by_id[key].get("cost_per_m3") == 0,
                "Canonical route state differs from a clean incident reset.")


def contains_validation(value, selected, validation_id):
    if isinstance(value, dict):
        if value.get("run_id") == selected and value.get("validation_id") == validation_id:
            report = object_(value.get("validation", {}))
            if value.get("validation_status") == "validated" and report.get("feasible") is True:
                return True
        return any(contains_validation(item, selected, validation_id) for item in value.values())
    if isinstance(value, list):
        return any(contains_validation(item, selected, validation_id) for item in value)
    return False


def audit_archive(path, model, role):
    archive = read_archive(path)
    require(archive.get("archive_version") == 1 and archive.get("status") == "completed",
            "A clean rehearsal requires a completed session-v1 archive.")
    session = identifier(archive.get("session_id"))
    integer(archive.get("checkpoint_sequence"), 1)
    created, updated = stamp(archive.get("created_at")), stamp(archive.get("updated_at"))
    require(created <= updated, "Session timestamps are reversed.")
    snapshot = object_(archive.get("snapshot"))
    snapshot_id = identifier(snapshot.get("snapshot_id"))
    physical_hash = text(snapshot.get("physical_hash"), 64)
    require(re.fullmatch(r"[0-9a-f]{64}", physical_hash) is not None, "The snapshot hash is malformed.")
    require(same_number(snapshot.get("horizon_h"), 3), "Rehearsals retain the declared three-hour horizon.")
    network = object_(snapshot.get("network"))
    assets, routes, signature = topology(network)
    if role == "canonical":
        canonical(network)
    tanks = [key for key, value in assets.items() if value.get("kind") == "tank"]
    runs = {}
    for detail in array(archive.get("runs"), 1, 128):
        detail = object_(detail)
        run = object_(detail.get("run"))
        run_id = identifier(run.get("run_id"))
        require(detail.get("ok") is True and run_id not in runs, "Native runs must be successful and uniquely identified.")
        require(run.get("snapshot_id") == snapshot_id and run.get("physical_hash") == physical_hash and
                same_number(run.get("horizon_h"), 3), "A numerical run is detached from its immutable incident.")
        require(number(run.get("cost")) >= 0, "Native run cost is invalid.")
        controls(run.get("controls"))
        times = array(detail.get("time_h"), 2, 10001)
        require(times[0] == 0 and times[-1] == 3 and all(number(b) > number(a) for a, b in zip(times, times[1:])),
                "Complete native trajectories must preserve a strictly increasing original horizon.")
        native_tanks = array(detail.get("tank_ids"), len(tanks), len(tanks))
        require(len(set(native_tanks)) == len(native_tanks) and set(native_tanks) == set(tanks),
                "Native tank identities differ from the incident.")
        for row in array(detail.get("volumes_m3"), len(tanks), len(tanks)):
            for value in array(row, len(times), len(times)):
                number(value)
        runs[run_id] = detail
    recommendation = object_(archive.get("recommendation"))
    selected = identifier(recommendation.get("selected_run_id"))
    validation_id = identifier(recommendation.get("validation_id"))
    require(selected in runs and recommendation.get("status") == "validated_recommendation" and
            recommendation.get("approval_status") == "operator_applied" and recommendation.get("actionable") is False and
            recommendation.get("ok") is True and recommendation.get("verified_feasible") is True,
            "A clean rehearsal requires an applied, independently validated model recommendation.")
    selected_run = runs[selected]["run"]
    require(selected_run.get("validation_status") == "validated" and selected_run.get("validation_id") == validation_id and
            object_(selected_run.get("validation")).get("feasible") is True and
            same_number(recommendation.get("verified_cost"), selected_run.get("cost")) and
            same_number(recommendation.get("cost"), selected_run.get("cost")) and
            controls(recommendation.get("controls")) == controls(selected_run.get("controls")),
            "The selected recommendation differs from the stored native validation or controls.")
    approvals = [object_(item) for item in array(archive.get("approvals"), 1, 128)]
    matching = [item for item in approvals if item.get("status") == "operator_applied" and item.get("run_id") == selected and
                item.get("validation_id") == validation_id and item.get("physical_hash") == physical_hash and
                controls(item.get("controls")) == controls(selected_run.get("controls"))]
    require(len(matching) == 1, "Exactly one matching explicit operator application must be retained.")
    approval_time = stamp(matching[0].get("timestamp"))
    require(created <= approval_time <= updated, "Operator application lies outside the session timestamps.")
    events = array(archive.get("events"), 1, 1024)
    requests, started, terminal, successful_requests, request_ids = {}, {}, {}, [], set()
    ready_events, native_validation_seen, staging_seen = [], False, False
    maximum_elapsed = 0
    previous_sequence = 0
    for event in events:
        event = object_(event)
        sequence = integer(event.get("sequence"), previous_sequence + 1)
        previous_sequence = sequence
        require(event.get("session_id") == session and created <= stamp(event.get("timestamp")) <= updated,
                "An event is detached from the session identity or timestamps.")
        if "snapshot_id" in event:
            require(event["snapshot_id"] == snapshot_id, "An event has the wrong incident snapshot.")
        maximum_elapsed = max(maximum_elapsed, integer(event.get("elapsed_ms")))
        kind = event.get("type")
        require(kind not in {"agent_failed", "agent_cancelled"}, "A failed/cancelled attempt is retained evidence, not a clean rehearsal.")
        if kind == "model_request_completed":
            metadata = object_(event.get("request"))
            request_id = identifier(metadata.get("model_request_id"))
            require(request_id not in request_ids, "Model request identities must be unique.")
            request_ids.add(request_id)
            require(metadata.get("configured_model") == model and metadata.get("response_model") == model and
                    metadata.get("outcome") == "succeeded" and 200 <= integer(metadata.get("http_status"), 100, 599) < 300 and
                    metadata.get("error_code") == "", "Hosted model identity or successful HTTP completion is unverified.")
            integer(metadata.get("duration_ms"))
            integer(metadata.get("retry_count"), 0, 2)
            successful_requests.append((sequence, metadata))
        elif kind in {"tool_requested", "tool_started", "tool_completed", "tool_failed"}:
            call_id, name = text(event.get("tool_call_id")), text(event.get("tool_name"), 128)
            require(name in TOOLS, "The trace contains an unrecognized tool.")
            if kind == "tool_requested":
                require(call_id not in requests, "Tool-call identities were reused.")
                requests[call_id] = (name, sequence)
            else:
                require(call_id in requests and requests[call_id][0] == name and requests[call_id][1] < sequence,
                        "A tool event has no matching model-issued call identity.")
                if kind == "tool_started":
                    require(call_id not in started, "A model tool call was executed more than once.")
                    arguments = object_(event.get("input_summary"))
                    if name not in {"get_network_state", "get_control_catalog"}:
                        require(arguments.get("snapshot_id") == snapshot_id, "Tool arguments refer to a foreign snapshot.")
                    started[call_id] = event
                else:
                    require(call_id not in terminal, "Tool-call results were duplicated.")
                    if kind == "tool_completed":
                        require(call_id in started and object_(event.get("result")).get("ok") is True,
                                "A successful tool result has no executed call or successful outcome.")
                    terminal[call_id] = event
                    if kind == "tool_completed" and name in {"validate_run", "search_interventions"}:
                        native_validation_seen |= contains_validation(event["result"], selected, validation_id)
                    if kind == "tool_completed" and name == "request_approval":
                        result = event["result"]
                        plan = object_(result.get("plan"))
                        if result.get("run_id") == selected:
                            require(result.get("applied") is False and result.get("approval_status") == "pending_operator" and
                                    plan.get("selected_run_id") == selected and plan.get("validation_id") == validation_id and
                                    controls(plan.get("controls")) == controls(selected_run.get("controls")),
                                    "The staged plan does not match the later operator application.")
                            staging_seen = True
        elif kind == "recommendation_ready":
            ready_events.append(event)
    require(1 <= len(requests) <= 12 and set(requests) == set(terminal), "Tool-call completion or the bounded call count is invalid.")
    require(len(successful_requests) >= 2, "A real tool-result continuation requires multiple successful hosted requests.")
    for event in terminal.values():
        require(any(sequence > event["sequence"] for sequence, _ in successful_requests),
                "A tool result has no subsequent successful model continuation.")
    completed_names = {event.get("tool_name") for event in terminal.values() if event.get("type") == "tool_completed"}
    require({"get_network_state", "get_control_catalog"} <= completed_names, "The model did not inspect actual network and controls.")
    require(native_validation_seen and staging_seen, "The selected run lacks matched native validation or model-requested staging.")
    require(len(ready_events) == 1 and ready_events[0].get("recommendation", {}).get("selected_run_id") == selected and
            ready_events[0].get("recommendation", {}).get("approval_status") == "pending_operator" and
            stamp(ready_events[0].get("timestamp")) <= approval_time,
            "The operator application lacks the matching completed model recommendation.")
    science = [(call_id, event["result"], started[call_id]["input_summary"])
               for call_id, event in terminal.items()
               if event.get("type") == "tool_completed" and event.get("tool_name") == "analyze_sensitivities"]
    return dict(archive=archive, session_id=session, snapshot_id=snapshot_id, physical_hash=physical_hash,
                selected_run_id=selected, validation_id=validation_id, runs=runs, science=science,
                topology=signature, asset_ids=set(assets), route_ids={route["id"] for route in routes},
                hosted_requests=[metadata for _, metadata in successful_requests],
                tool_calls=len(requests), elapsed_ms=maximum_elapsed, selected_cost=selected_run["cost"])


def retained_science(result_id, database_paths):
    found = None
    for database in database_paths:
        require(database.is_file(), "A supplied scientific evidence database is missing.")
        with closing(sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True, timeout=5)) as connection:
            connection.execute("PRAGMA query_only=ON")
            row = connection.execute("SELECT length(archive), CASE WHEN length(archive) <= ? THEN archive END "
                                     "FROM sessions WHERE session_id = ?", (MAX_ARCHIVE_BYTES, result_id)).fetchone()
        if row is None:
            continue
        require(row[0] <= MAX_ARCHIVE_BYTES, "A stored scientific archive exceeds the session bound.")
        payload = row[1].encode("utf-8") if isinstance(row[1], str) else row[1]
        archive = decode(payload)
        require(found is None or found == archive, "The same scientific identity names conflicting retained evidence.")
        found = archive
    require(found is not None, "A successful scientific result was not found in the supplied durable databases.")
    return found


def audit_science(record, databases):
    retained_ids = []
    for call_id, outcome, arguments in record["science"]:
        require(outcome.get("advisory") is True and outcome.get("validation_status") == "not_performed",
                "Scientific analysis must remain advisory and separate from native validation.")
        result_id, run_id = identifier(outcome.get("result_id")), identifier(outcome.get("run_id"))
        require(run_id in record["runs"] and arguments.get("run_id") == run_id, "Scientific output refers to a foreign numerical run.")
        origin = object_(outcome.get("origin"))
        require(set(origin) == {"incident_snapshot_id", "incident_physical_hash", "run_id", "effective_physical_hash", "agent_session_id", "tool_call_id"} and
                origin.get("agent_session_id") == record["session_id"] and origin.get("tool_call_id") == call_id and
                origin.get("incident_snapshot_id") == record["snapshot_id"] and origin.get("incident_physical_hash") == record["physical_hash"] and
                origin.get("run_id") == run_id, "Scientific origin is detached from the actual investigation/tool call.")
        analysis = object_(outcome.get("analysis"))
        require(analysis.get("analysis_mode") == "sensitivities" and analysis.get("snapshot_id") == run_id and
                analysis.get("effective_physical_hash") == origin.get("effective_physical_hash") and
                same_number(analysis.get("horizon_h"), 3) and analysis.get("within_tolerance") is True,
                "Scientific evidence lacks a matching captured-run comparison.")
        selected = array(analysis.get("selected_controls"), 1, 8)
        ids = [text(object_(item).get("control_id"), 512) for item in selected]
        require(len(set(ids)) == len(ids) and arguments.get("control_ids") == ids,
                "Scientific columns differ from the model's explicit ordered selection.")
        for column in selected:
            require(column.get("unit") == "m3/h", "Captured scientific control units are invalid.")
            number(column.get("captured_value"))
        derivative = object_(analysis.get("derivatives"))
        require(derivative.get("control_ids") == ids, "Derivative identities/order differ from the captured controls.")
        tank_count = len(record["runs"][run_id]["tank_ids"])
        for row in array(derivative.get("terminal_volume_jacobian_h"), tank_count, tank_count):
            for value in array(row, len(ids), len(ids)):
                number(value)
        for value in array(derivative.get("cost_gradient"), len(ids), len(ids)):
            number(value)
        require(analysis.get("units") == {"captured_controls": "m3/h", "terminal_volume_jacobian_h": "h",
                "cost_gradient": "synthetic units / (m3/h)", "cost": "synthetic units"}, "Scientific units are missing or inconsistent.")
        archive = retained_science(result_id, databases)
        require(archive.get("session_id") == result_id and archive.get("session_kind") == "scientific" and
                archive.get("status") == "completed" and archive.get("actionable") is False and archive.get("approvals") == [] and
                archive.get("agent_origin") == origin and archive.get("scientific_evidence_version") == 1,
                "The retained science record has inconsistent provenance or authority flags.")
        evidence = object_(array(archive.get("scientific_evidence"), 1, 1)[0])
        for field in ("analysis_mode", "snapshot_id", "horizon_h", "step_h", "native_cost", "worker_cost", "within_tolerance",
                      "tanks", "selected_controls", "derivatives", "provenance", "comparison_scope", "native_solver_version"):
            require(evidence.get(field) == analysis.get(field), "The model result differs from retained scientific evidence.")
        require(evidence.get("result_id") == result_id and evidence.get("run_id") == run_id and
                evidence.get("physical_hash") == origin.get("effective_physical_hash"), "Retained scientific identity is inconsistent.")
        native = record["runs"][run_id]
        captured = object_(array(archive.get("runs"), 1, 1)[0])
        require(all(captured.get(field) == native.get(field) for field in ("time_h", "tank_ids", "volumes_m3")) and
                same_number(evidence.get("native_cost"), native["run"].get("cost")), "Retained science lost its original native trajectory/cost.")
        retained_ids.append(result_id)
    return retained_ids


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--canonical", type=Path, action="append", required=True, help="A complete canonical rehearsal archive; provide exactly three.")
    parser.add_argument("--edited", type=Path, required=True, help="A complete rehearsal on a renamed, changed topology.")
    parser.add_argument("--science-db", type=Path, action="append", required=True, help="Read-only application history database(s) retaining the scientific results.")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--output", type=Path, help="Optional new JSON summary file; existing files are never replaced.")
    args = parser.parse_args()
    if len(args.canonical) != 3:
        parser.error("Provide --canonical exactly three times.")
    report = {"verification_version": 1, "passed": False,
              "scope": "Internal consistency of retained live application evidence; not cryptographic execution authentication.",
              "model": args.model, "rehearsals": [], "errors": []}
    records = []
    for role, path in [("canonical", path) for path in args.canonical] + [("edited", args.edited)]:
        item = {"role": role, "archive": str(path), "passed": False}
        try:
            record = audit_archive(path, args.model, role)
            science_ids = audit_science(record, args.science_db)
            item.update(passed=True, session_id=record["session_id"], selected_run_id=record["selected_run_id"],
                        validation_id=record["validation_id"], native_runs=len(record["runs"]), selected_cost=record["selected_cost"],
                        tool_calls=record["tool_calls"], hosted_requests=len(record["hosted_requests"]),
                        retries=sum(value["retry_count"] for value in record["hosted_requests"]),
                        elapsed_ms=record["elapsed_ms"], retained_scientific_results=science_ids)
            records.append(record)
        except (AuditError, OSError, sqlite3.Error, TypeError, KeyError) as error:
            item["error"] = str(error) if isinstance(error, AuditError) else "Evidence could not be read or audited."
        report["rehearsals"].append(item)
    if len(records) == 4:
        try:
            require(len({record["session_id"] for record in records}) == 4, "Four distinct live session identities are required.")
            require(len({record["physical_hash"] for record in records[:3]}) == 1, "Canonical rehearsals must share the same clean incident state.")
            require(records[3]["topology"] != records[0]["topology"] and
                    records[3]["asset_ids"].isdisjoint(records[0]["asset_ids"]) and
                    records[3]["route_ids"].isdisjoint(records[0]["route_ids"]),
                    "The edited rehearsal must have renamed assets/routes and a structurally changed topology.")
            require(any(record["science"] for record in records), "At least one model-issued sensitivity analysis must execute and be durably retained.")
            report["passed"] = True
        except AuditError as error:
            report["errors"].append(str(error))
    else:
        report["errors"].append("All three canonical and the edited-network rehearsal must pass.")
    output = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        try:
            with args.output.open("x", encoding="utf-8") as destination:
                destination.write(output)
        except OSError:
            print("The audit summary destination must be a new writable local file.", file=sys.stderr)
            return 2
    print(output, end="")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
