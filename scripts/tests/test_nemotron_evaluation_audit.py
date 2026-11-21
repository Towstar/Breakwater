"""Fabricated OFFLINE auditor regression fixtures, never execution evidence."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nemotron_evaluation_audit import MODEL, audit_case


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
    for row in events:
        row["session_id"] = "session-1"
    archive = {"archive_version": 1, "session_id": "session-1", "status": "completed", "snapshot": snapshot,
               "model": {"configured_model": MODEL if hosted else "local-native-finite-search"}, "events": events,
               "runs": [{"ok": True, "run": run}], "recommendation": recommendation, "approvals": [], "current": True}
    summary = {"session_id": "session-1", "exit_code": 3 if no_plan else 0, "status": "incomplete" if no_plan else "passed",
               "model_requests": int(hosted), "archive_written": True, "controls_unchanged": True, "inputs_unchanged": True,
               "controls_applied": False, "physical_hash": "a" * 64,
               "selected_run_id": "" if no_plan else "run-1", "validation_id": "" if no_plan else "validation-1"}
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
        archive, summary = sandbox()
        for row in archive["events"]:
            if row.get("tool_name") == "validate_run":
                row["tool_name"] = "search_interventions"
        with self.assertRaises(ValueError):
            audit_case("sandbox", archive, summary, "advisory_plan")

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
