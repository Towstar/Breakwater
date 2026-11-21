"""Fabricated OFFLINE auditor regression fixtures, never execution evidence."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nemotron_evaluation_audit import MODEL, audit_case, retained_tool_metrics


def stamp_sandbox_events(events):
    for sequence, row in enumerate(events, 1):
        row.update(session_id="session-1", snapshot_id="snapshot-1", sequence=sequence,
                   elapsed_ms=sequence, timestamp=f"2026-11-21T00:00:00.{sequence:03d}Z")


def sandbox(*, hosted=True, no_plan=False):
    snapshot = {"snapshot_id": "snapshot-1", "physical_hash": "a" * 64, "horizon_h": 3}
    run = {**snapshot, "run_id": "run-1", "validation_id": "validation-1", "validation_source": "validate_run",
           "validation_status": "validated", "cost": 12, "controls": [],
           "validation": {"feasible": not no_plan, "violations": [{"code": "storage_capacity"}] if no_plan else []}}
    recommendation = ({**snapshot, "status": "no_validated_plan", "selected_run_id": None,
                       "source_mode": "hosted" if hosted else "local_native"} if no_plan else
                      {**run, "status": "validated_recommendation", "selected_run_id": "run-1", "ok": True,
                       "verified_feasible": True, "verified_cost": 12, "approval_status": "pending_operator",
                       "actionable": True, "source_mode": "hosted" if hosted else "local_native"})
    events = []
    if hosted:
        events.extend([{"type": "model_request_started"}, {"type": "model_request_completed", "request": {
            "configured_model": MODEL, "response_model": MODEL, "model_request_id": "model-1", "duration_ms": 8,
            "error_code": "", "http_status": 200, "outcome": "succeeded", "retry_count": 1}}])
    events.extend([{"type": "tool_requested", "tool_call_id": "call-1", "tool_name": "validate_run"},
                   {"type": "tool_started", "tool_call_id": "call-1", "tool_name": "validate_run",
                    "input_summary": {"snapshot_id": "snapshot-1", "run_id": "run-1"}},
                   {"type": "tool_completed", "tool_call_id": "call-1", "tool_name": "validate_run",
                    "result": {"ok": True, "run": deepcopy(run)}},
                   {"type": "recommendation_ready", "recommendation": deepcopy(recommendation)}])
    stamp_sandbox_events(events)
    archive = {"archive_version": 1, "session_id": "session-1", "status": "completed", "snapshot": snapshot,
               "model": {"configured_model": MODEL if hosted else "local-native-finite-search"}, "events": events,
               "runs": [{"ok": True, "run": run}], "recommendation": recommendation, "approvals": [], "current": True}
    summary = {"session_id": "session-1", "exit_code": 3 if no_plan else 0, "status": "incomplete" if no_plan else "passed",
               "model_requests": int(hosted), "archive_written": True, "controls_unchanged": True, "inputs_unchanged": True,
               "controls_applied": False, "physical_hash": "a" * 64,
               "selected_run_id": "" if no_plan else "run-1", "validation_id": "" if no_plan else "validation-1"}
    return archive, summary


def sandbox_predispatch(*, code="refinement_limit", repeats=1, no_plan=False):
    """Fabricate the real nextTool/completeTool boundary, not execution evidence."""
    archive, summary = sandbox(no_plan=no_plan)
    inserted = []
    name = "simulate_candidates" if code == "refinement_limit" else "get_network_state"
    if code == "refinement_limit":
        for index in range(3):
            identity = f"candidate-{index}"
            error = {"code": "invalid_arguments", "message": "candidates must be an array."}
            inserted.extend([
                {"type": "tool_requested", "tool_call_id": identity, "tool_name": name},
                {"type": "tool_started", "tool_call_id": identity, "tool_name": name,
                 "input_summary": {"snapshot_id": "snapshot-1", "candidates": "not-an-array"}},
                {"type": "tool_failed", "tool_call_id": identity, "tool_name": name,
                 "duration_ms": 0, "status": "failed", "error": deepcopy(error), "result": {"ok": False, "error": error}}])
    message = ("Candidate refinement limit reached; inspect existing evidence or finish." if code == "refinement_limit"
               else "Tool arguments must be a JSON object.")
    for index in range(repeats):
        identity = f"predispatch-{index}"
        inserted.extend([{"type": "tool_requested", "tool_call_id": identity, "tool_name": name},
                         {"type": "tool_failed", "tool_call_id": identity, "tool_name": name,
                          "error": {"code": code, "message": message}}])
    archive["events"][2:2] = inserted
    stamp_sandbox_events(archive["events"])
    return archive, summary


def eog(*, refusal=None, hosted=True):
    capture = {"capture_id": "capture-1", "dataset_identity": "dataset-1", "cutoff_unix": 1000}
    plan = {"plan_id": "plan-1", "plan_hash": "b" * 64}
    evaluation = {"plan": deepcopy(plan), "feasible": True, "coverage": "available", "cost": {"total": 12}}
    validation = {"validation_id": "validation-1", **plan, "independent": True, "feasible": True,
                  "coverage": "available", "violations": []}
    analyses = []
    def analysis(identity, operation, arguments, result, status="completed", code=None):
        request = {**capture, "request_id": identity, "operation": operation, "arguments": arguments}
        response = {**capture, "request_id": identity, "operation": operation, "status": status, "result": result,
                    "diagnostics": [{"code": code}] if code else []}
        analyses.append({"request": request, "response": response})
    if refusal:
        analysis("request-1", "detect", {"system_ids": []}, {"coverage": "partial", "events": [], "evidence": []})
        analysis("request-2", "search_plans", {"system_ids": []}, None, "unavailable", refusal)
    else:
        analysis("request-1", "evaluate_plan", {"plan": deepcopy(plan)}, {"evaluation": evaluation})
        analysis("request-2", "validate_plan", {"plan": deepcopy(plan), "evaluator_method": "native-1"}, {"validation": validation})
    calls = []
    for index, row in enumerate(analyses, 1):
        arguments = row["request"]["arguments"] if row["request"]["operation"] != "validate_plan" else {
            "plan_id": "plan-1", "evaluator_method": "native-1"}
        calls.extend([{"type": "tool_call", "call_id": f"call-{index}", "name": row["request"]["operation"],
                       "arguments": deepcopy(arguments)},
                      {"type": "tool_result", "call_id": f"call-{index}", "name": row["request"]["operation"],
                       "analysis_request_id": row["request"]["request_id"], "ok": row["response"]["status"] == "completed",
                       "summary": "{}"}])
    events = []
    if hosted:
        events = [{"type": "model_request", "model_identity": MODEL, "http_status": 200, "duration_ms": 11, "retry_count": 0},
                  *calls, {"type": "final", "selected_plan_id": None if refusal else "plan-1",
                           "validation_id": None if refusal else "validation-1", "event_ids": [], "summary": "Synthetic audit fixture."}]
    archive = {"eog_session_version": 1, "session_id": "session-1", "status": "completed", "historical": True,
               "actionable": False, "operator_records": [], "capture_manifest": capture,
               "agent_events": [{"sequence": index, "event": row} for index, row in enumerate(events, 1)], "analyses": analyses}
    summary = {**capture, "session_id": "session-1", "model_requests": int(hosted), "archive_written": True,
               "tool_order": [row["name"] for row in calls if row["type"] == "tool_call"] if hosted else [],
               "exit_code": 3 if refusal and hosted else 0, "status": "incomplete" if refusal and hosted else "passed",
               "grounded_review_eligible_final": not refusal, "selected_plan_id": "" if refusal else "plan-1",
               "validation_id": "" if refusal else "validation-1"}
    if not hosted:
        del summary["exit_code"]
        summary.update(preflight=True, provider_verified=not bool(refusal), provider_exchange_verified=True, zero_model_requests=True,
                       inputs_unchanged=True, history_reopened_nonactionable=True, actionable=False,
                       analysis_request_ids=[row["request"]["request_id"] for row in analyses])
    return archive, summary


class EvaluationAuditTests(unittest.TestCase):
    def test_public_retained_metrics_use_actual_request_retries_and_only_known_arguments(self):
        archive, _ = sandbox_predispatch(code="invalid_arguments")
        events = archive["events"]
        requests = [row["request"] for row in events if row["type"] == "model_request_completed"]
        starts = {row["tool_call_id"]: row["input_summary"] for row in events if row["type"] == "tool_started"}
        calls = [{**row, **({"arguments": starts[row["tool_call_id"]]} if row["tool_call_id"] in starts else {})}
                 for row in events if row["type"] == "tool_requested"]
        results = [row for row in events if row["type"] in {"tool_completed", "tool_failed"}]
        metrics = retained_tool_metrics(requests, calls, results, name="tool_name", identity="tool_call_id")
        self.assertEqual(metrics["http_attempts"], 2)
        self.assertEqual(metrics["retries"], 1)
        self.assertEqual(metrics["actual_tool_executions"], 1)
        self.assertEqual(metrics["unknown_argument_calls"], 1)
        self.assertEqual(metrics["duplicate_calls"], 0)
        self.assertFalse(metrics["duplicate_counts_complete"])

    def test_predispatch_refinement_rejection_preserves_validated_completion_and_metrics(self):
        archive, summary = sandbox_predispatch()
        result = audit_case("sandbox", archive, summary, "advisory_plan")
        self.assertTrue(result["passed"])
        self.assertEqual(result["predispatch_rejections"], 1)
        self.assertEqual(result["unknown_argument_calls"], 1)
        self.assertEqual(result["dispatched_tool_calls"], 4)
        self.assertEqual(result["tool_calls"], 5)
        self.assertEqual(result["duplicate_calls"], 2)  # Three known identical dispatched attempts.
        self.assertFalse(result["duplicate_counts_complete"])

    def test_decode_rejections_have_unknown_arguments_instead_of_empty_object_duplicates(self):
        archive, summary = sandbox_predispatch(code="invalid_arguments", repeats=2)
        archive["events"][2:2] = [
            {"type": "tool_requested", "tool_call_id": "known-empty", "tool_name": "get_network_state"},
            {"type": "tool_started", "tool_call_id": "known-empty", "tool_name": "get_network_state", "input_summary": {}},
            {"type": "tool_completed", "tool_call_id": "known-empty", "tool_name": "get_network_state", "result": {"ok": True}}]
        stamp_sandbox_events(archive["events"])
        result = audit_case("sandbox", archive, summary, "advisory_plan")
        self.assertTrue(result["passed"])
        self.assertEqual(result["predispatch_rejections"], 2)
        self.assertEqual(result["unknown_argument_calls"], 2)
        self.assertEqual(result["duplicate_calls"], 0)
        self.assertEqual(result["read_repeats"], 0)

    def test_predispatch_rejection_cannot_promote_a_null_planning_miss(self):
        archive, summary = sandbox_predispatch(no_plan=True)
        result = audit_case("sandbox", archive, summary, "advisory_plan")
        self.assertFalse(result["passed"])
        self.assertEqual(result["classification"], "unsupported_abstention")
        self.assertEqual(result["tool_calls"], 5)
        self.assertEqual(result["predispatch_rejections"], 1)

    def test_refinement_rejection_requires_candidate_tool_and_fourth_candidate_request(self):
        for mutation in ("early", "read-tool", "wrong-message", "wrong-code", "decode-over-limit"):
            archive, summary = sandbox_predispatch()
            if mutation == "early":
                del archive["events"][2:5]  # Now only two candidate attempts precede the rejection.
            for row in archive["events"]:
                if row.get("tool_call_id") != "predispatch-0":
                    continue
                if mutation == "read-tool":
                    row["tool_name"] = "get_network_state"
                if row["type"] == "tool_failed":
                    if mutation == "wrong-message":
                        row["error"]["message"] = "Another rejection."
                    elif mutation == "wrong-code":
                        row["error"]["code"] = "tool_failure"
                    elif mutation == "decode-over-limit":
                        row["error"] = {"code": "invalid_arguments", "message": "Tool arguments must be a JSON object."}
            stamp_sandbox_events(archive["events"])
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                audit_case("sandbox", archive, summary, "advisory_plan")

    def test_predispatch_exception_rejects_dispatch_payloads_extra_fields_and_added_start(self):
        for field in ("result", "status", "duration_ms", "run_id", "validation_id", "unexpected"):
            archive, summary = sandbox_predispatch()
            failed = next(row for row in archive["events"] if row.get("tool_call_id") == "predispatch-0" and row["type"] == "tool_failed")
            failed[field] = {} if field == "result" else "not-predispatch"
            with self.subTest(field=field), self.assertRaises(ValueError):
                audit_case("sandbox", archive, summary, "advisory_plan")
        archive, summary = sandbox_predispatch()
        index = next(i for i, row in enumerate(archive["events"]) if row.get("tool_call_id") == "predispatch-0" and row["type"] == "tool_failed")
        archive["events"].insert(index, {"type": "tool_started", "tool_call_id": "predispatch-0", "tool_name": "simulate_candidates", "input_summary": {}})
        stamp_sandbox_events(archive["events"])
        with self.assertRaises(ValueError):
            audit_case("sandbox", archive, summary, "advisory_plan")

    def test_paired_results_still_require_starts_for_success_and_dispatched_failure(self):
        for complete in (False, True):
            for failure in (False, True):
                archive, summary = sandbox_predispatch()
                if not complete:
                    archive["status"] = "failed"
                    summary.update(exit_code=3, status="incomplete")
                identity = "candidate-0" if failure else "call-1"
                # A real completeTool failure still carries result/status/duration.
                archive["events"] = [row for row in archive["events"]
                                     if not (row["type"] == "tool_started" and row.get("tool_call_id") == identity)]
                stamp_sandbox_events(archive["events"])
                with self.subTest(complete=complete, failure=failure), self.assertRaises(ValueError):
                    audit_case("sandbox", archive, summary, "advisory_plan")

    def test_failed_investigation_may_retain_an_unfinished_requested_tail_without_a_result(self):
        for after_rejection in (False, True):
            archive, summary = sandbox_predispatch() if after_rejection else sandbox()
            archive.update(status="failed", recommendation={})
            summary.update(exit_code=5, status="failed", selected_run_id="", validation_id="")
            archive["events"].pop()  # No recommendation_ready was published before this deadline.
            archive["events"].extend([
                {"type": "tool_requested", "tool_call_id": "deadline-tail", "tool_name": "get_network_state"},
                {"type": "agent_failed", "error": {"code": "deadline_exceeded", "message": "Investigation reached its time limit."}}])
            stamp_sandbox_events(archive["events"])
            result = audit_case("sandbox", archive, summary, "advisory_plan")
            with self.subTest(after_rejection=after_rejection):
                self.assertFalse(result["passed"])
                self.assertEqual(result["classification"], "failed_investigation")
                self.assertEqual(result["predispatch_rejections"], int(after_rejection))
                self.assertEqual(result["unknown_argument_calls"], 1 + int(after_rejection))

    def test_predispatch_event_metadata_and_all_call_result_pairing_stay_strict(self):
        for mutation in ("sequence", "elapsed", "negative-elapsed", "timestamp", "dropped-result", "duplicate-result", "execution-event"):
            archive, summary = sandbox_predispatch()
            index = next(i for i, row in enumerate(archive["events"]) if row.get("tool_call_id") == "predispatch-0" and row["type"] == "tool_failed")
            failed = archive["events"][index]
            if mutation == "sequence":
                failed["sequence"] = 1
            elif mutation == "elapsed":
                failed["elapsed_ms"] = True
            elif mutation == "negative-elapsed":
                failed["elapsed_ms"] = -1
            elif mutation == "timestamp":
                failed["timestamp"] = "not-a-timestamp"
            elif mutation == "dropped-result":
                del archive["events"][index]
                stamp_sandbox_events(archive["events"])
            elif mutation == "duplicate-result":
                archive["events"].insert(index, deepcopy(failed))
                stamp_sandbox_events(archive["events"])
            elif mutation == "execution-event":
                archive["events"].insert(index, {"type": "search_started", "tool_call_id": "predispatch-0", "tool_name": "simulate_candidates"})
                stamp_sandbox_events(archive["events"])
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                audit_case("sandbox", archive, summary, "advisory_plan")

    def test_start_result_interleaving_cannot_be_repaired_by_separate_identity_lists(self):
        archive, summary = sandbox()
        archive["events"][2], archive["events"][3] = archive["events"][3], archive["events"][2]
        stamp_sandbox_events(archive["events"])
        with self.assertRaises(ValueError):
            audit_case("sandbox", archive, summary, "advisory_plan")

    def test_wall_clock_steps_do_not_override_monotonic_sequence_and_elapsed_time(self):
        archive, summary = sandbox_predispatch()
        for row in archive["events"]:
            if row["type"] in {"tool_started", "tool_completed", "tool_failed"}:
                row["timestamp"] = "2026-11-20T00:00:00.000Z"
        self.assertTrue(audit_case("sandbox", archive, summary, "advisory_plan")["passed"])

    def test_hosted_sandbox_pending_review_is_allowed_without_approval(self):
        archive, summary = sandbox()
        value = audit_case("sandbox", archive, summary, "advisory_plan")
        self.assertTrue(value["passed"])
        self.assertEqual(value["logical_requests"], 1)
        self.assertEqual(value["http_attempts"], 2)
        self.assertEqual(value["retries"], 1)
        self.assertEqual(value["selected_cost"], 12)

    def test_local_native_evidence_is_explicitly_not_hosted_qualification(self):
        archive, summary = sandbox(hosted=False)
        self.assertTrue(audit_case("sandbox", archive, summary, "advisory_plan", hosted=False)["passed"])
        with self.assertRaises(ValueError):
            audit_case("sandbox", archive, summary, "advisory_plan")

    def test_exact_sandbox_run_hash_horizon_controls_cost_and_validation_are_required(self):
        for field, replacement in (("physical_hash", "wrong"), ("horizon_h", 4), ("controls", [{"control_id": "x", "value": 2}]),
                                   ("verified_cost", 13), ("validation_id", "wrong"), ("selected_run_id", "unknown")):
            archive, summary = sandbox()
            archive["recommendation"][field] = replacement
            archive["events"][-1]["recommendation"] = deepcopy(archive["recommendation"])
            with self.subTest(field=field), self.assertRaises(ValueError):
                audit_case("sandbox", archive, summary, "advisory_plan")

    def test_search_screening_cannot_replace_model_issued_validation(self):
        # Renaming a validate_run execution does not authenticate search proof.
        archive, summary = sandbox()
        for row in archive["events"]:
            if row.get("tool_name") == "validate_run":
                row["tool_name"] = "search_interventions"
        with self.assertRaises(ValueError):
            audit_case("sandbox", archive, summary, "advisory_plan")
        for hosted in (True, False):
            archive, summary = sandbox(hosted=hosted)
            run = archive["runs"][0]["run"]
            run["validation_source"] = "native_search_dense_reference"
            archive["recommendation"]["validation_source"] = "native_search_dense_reference"
            for row in archive["events"]:
                if row.get("tool_name") == "validate_run":
                    row["tool_name"] = "search_interventions"
                    if row["type"] == "tool_started":
                        row["input_summary"] = {"snapshot_id": "snapshot-1", "axes": []}
                    elif row["type"] == "tool_completed":
                        row["result"] = {**archive["snapshot"], "ok": True, "runs": [deepcopy(run)], "search_status": "recommendations_ready"}
                elif row["type"] == "recommendation_ready":
                    row["recommendation"] = deepcopy(archive["recommendation"])
            value = audit_case("sandbox", archive, summary, "advisory_plan", hosted=hosted)
            with self.subTest(hosted=hosted):
                self.assertEqual(value["passed"], not hosted)
                self.assertEqual(value["classification"], "explicit_model_validation_missing" if hosted else "advisory_plan")
                self.assertEqual(value["logical_requests"], int(hosted))
                self.assertEqual(value["http_attempts"], 2 if hosted else 0)
                self.assertEqual(value["total_tool_calls"], 1)
                self.assertEqual(value["stored_runs"], 1)
                self.assertEqual(value["selected_cost"], 12)
                self.assertTrue(value["request_counts_complete"])
                self.assertTrue(value["duplicate_counts_complete"])

    def test_authority_and_secret_or_raw_reasoning_fields_are_rejected(self):
        for change in (lambda a, s: a["approvals"].append({"status": "operator_applied"}),
                       lambda a, s: s.update(controls_applied=True),
                       lambda a, s: a.update(reasoning_content="synthetic forbidden field"),
                       lambda a, s: a["events"][4]["result"].update(applied=True)):
            archive, summary = sandbox()
            change(archive, summary)
            with self.subTest(change=change), self.assertRaises(ValueError):
                audit_case("sandbox", archive, summary, "advisory_plan")

    def test_successful_wrong_model_or_capture_is_an_integrity_failure(self):
        archive, summary = sandbox()
        archive["events"][1]["request"]["response_model"] = "wrong-model"
        with self.assertRaises(ValueError):
            audit_case("sandbox", archive, summary, "advisory_plan")
        archive, summary = eog()
        archive["analyses"][1]["response"]["capture_id"] = "other"
        with self.assertRaises(ValueError):
            audit_case("eog", archive, summary, "advisory_plan")

    def test_completed_native_refusal_needs_actual_rejected_numerics(self):
        archive, summary = sandbox(no_plan=True)
        self.assertTrue(audit_case("sandbox", archive, summary, "no_supported_plan")["passed"])
        archive["runs"] = []
        value = audit_case("sandbox", archive, summary, "no_supported_plan")
        self.assertFalse(value["passed"])
        self.assertEqual(value["classification"], "unsupported_abstention")

    def test_unselected_feasible_run_cannot_become_supported_refusal(self):
        archive, summary = sandbox(no_plan=True)
        archive["runs"][0]["run"]["validation"]["feasible"] = True
        archive["runs"][0]["run"]["validation"]["violations"] = []
        self.assertFalse(audit_case("sandbox", archive, summary, "no_supported_plan")["passed"])

    def test_numerical_rejection_and_unexecuted_validation_are_not_physical_refusals(self):
        archive, summary = sandbox(no_plan=True)
        archive["runs"][0]["run"]["validation"]["violations"] = [{"code": "nonfinite_result"}]
        archive["events"][4]["result"]["run"] = deepcopy(archive["runs"][0]["run"])
        self.assertFalse(audit_case("sandbox", archive, summary, "no_supported_plan")["passed"])
        archive, summary = sandbox(no_plan=True)
        for row in archive["events"]:
            if row.get("tool_name") == "validate_run":
                row["tool_name"] = "forecast_baseline"
        self.assertFalse(audit_case("sandbox", archive, summary, "no_supported_plan")["passed"])

    def test_eog_hosted_validated_success_and_manual_preflight(self):
        for hosted in (True, False):
            archive, summary = eog(hosted=hosted)
            with self.subTest(hosted=hosted):
                value = audit_case("eog", archive, summary, "advisory_plan", hosted=hosted)
                self.assertTrue(value["passed"])
                self.assertEqual(value["provider_exchanges"], 2)

    def test_manual_preflight_flags_cannot_substitute_for_bound_provider_records(self):
        for key in ("preflight", "provider_verified", "provider_exchange_verified", "zero_model_requests", "inputs_unchanged", "history_reopened_nonactionable"):
            archive, summary = eog(hosted=False)
            summary[key] = False
            with self.subTest(key=key), self.assertRaises(ValueError):
                audit_case("eog", archive, summary, "advisory_plan", hosted=False)
        archive, summary = eog(refusal="failure_context_incomplete", hosted=False)
        self.assertTrue(audit_case("eog", archive, summary, "missing_coverage", hosted=False)["passed"])
        summary["analysis_request_ids"] = ["forged"]
        with self.assertRaises(ValueError):
            audit_case("eog", archive, summary, "missing_coverage", hosted=False)

    def test_truthful_unavailable_exchange_is_distinct_from_numerical_execution(self):
        archive, summary = eog(refusal="failure_context_incomplete", hosted=False)
        self.assertIs(summary["provider_verified"], False)
        result = audit_case("eog", archive, summary, "missing_coverage", hosted=False)
        self.assertTrue(result["passed"])
        self.assertIs(result["provider_verified"], False)
        self.assertIs(result["provider_exchange_verified"], True)
        archive, summary = eog(refusal="no_active_failure", hosted=False)
        self.assertTrue(audit_case("eog", archive, summary, "no_supported_plan", hosted=False)["passed"])
        archive, summary = eog(hosted=False)
        summary["provider_verified"] = False
        with self.assertRaises(ValueError):
            audit_case("eog", archive, summary, "advisory_plan", hosted=False)
        archive, summary = eog(refusal="provider_failed", hosted=False)
        self.assertFalse(audit_case("eog", archive, summary, "missing_coverage", hosted=False)["passed"])

    def test_raw_exit_three_can_pass_grounded_coverage_refusal(self):
        for expected in ("missing_coverage", "stale_coverage"):
            archive, summary = eog(refusal="failure_context_incomplete")
            value = audit_case("eog", archive, summary, expected)
            self.assertTrue(value["passed"])
            self.assertEqual(value["classification"], expected)
            self.assertEqual(summary["exit_code"], 3)

    def test_partial_detection_or_unavailable_method_is_not_coverage_grounding(self):
        for code in ("native_unavailable", "provider_failed", "operation_unavailable", "numerical_unavailable", "validation_time_limit"):
            archive, summary = eog(refusal=code)
            with self.subTest(code=code):
                value = audit_case("eog", archive, summary, "missing_coverage")
                self.assertFalse(value["passed"])
                self.assertEqual(value["classification"], "unsupported_abstention")

    def test_final_event_reference_and_validation_operation_cannot_be_forged(self):
        for change in (lambda a: a["agent_events"][-1]["event"].update(event_ids=["fabricated"]),
                       lambda a: a["analyses"][1]["request"].update(operation="evaluate_plan"),
                       lambda a: a["analyses"][1]["response"]["result"]["validation"].update(plan_hash="wrong")):
            archive, summary = eog()
            change(archive)
            with self.subTest(change=change), self.assertRaises(ValueError):
                audit_case("eog", archive, summary, "advisory_plan")

    def test_duplicate_metrics_canonicalize_keys_and_distinguish_validation_reuse(self):
        archive, summary = eog()
        rows = [row["event"] for row in archive["agent_events"]]
        final = rows.pop()
        rows.extend([{"type": "tool_call", "call_id": "call-3", "name": "validate_plan",
                      "arguments": {"evaluator_method": "native-1", "plan_id": "plan-1"}},
                     {"type": "tool_result", "call_id": "call-3", "name": "validate_plan", "ok": True,
                      "analysis_request_id": "request-2", "summary": '{"reused_validation":true}'}, final])
        archive["agent_events"] = [{"sequence": i, "event": row} for i, row in enumerate(rows, 1)]
        summary["tool_order"].append("validate_plan")
        value = audit_case("eog", archive, summary, "advisory_plan")
        self.assertEqual(value["duplicate_calls"], 1)
        self.assertEqual(value["duplicate_expensive_calls"], 1)
        self.assertEqual(value["duplicate_expensive_executions"], 0)
        self.assertEqual(value["validation_reuse"], 1)
        self.assertEqual(value["actual_tool_executions"], 2)

    def test_valid_failed_archive_is_retained_without_promoting_a_refusal(self):
        archive, summary = eog(refusal="failure_context_incomplete")
        archive["status"] = "failed"
        archive["agent_events"].append({"sequence": 99, "event": {"type": "failure", "code": "transport_failed"}})
        value = audit_case("eog", archive, summary, "missing_coverage")
        self.assertFalse(value["passed"])
        self.assertEqual(value["classification"], "transport_failure")
        summary.update(status="passed", exit_code=0)
        with self.assertRaises(ValueError):
            audit_case("eog", archive, summary, "missing_coverage")

    def test_call_identity_order_and_request_counts_are_not_repairable(self):
        archive, summary = eog()
        archive["agent_events"][2]["event"]["call_id"] = "other"
        with self.assertRaises(ValueError):
            audit_case("eog", archive, summary, "advisory_plan")
        archive, summary = sandbox()
        summary["model_requests"] = 2
        with self.assertRaises(ValueError):
            audit_case("sandbox", archive, summary, "advisory_plan")

    def test_fake_process_success_and_null_review_eligibility_are_rejected(self):
        archive, summary = sandbox()
        summary["status"] = "failed"
        with self.assertRaises(ValueError):
            audit_case("sandbox", archive, summary, "advisory_plan")
        archive, summary = eog(refusal="failure_context_incomplete")
        summary["grounded_review_eligible_final"] = True
        with self.assertRaises(ValueError):
            audit_case("eog", archive, summary, "missing_coverage")


if __name__ == "__main__":
    unittest.main(verbosity=2)
