"""Synthetic offline-auditor fixtures; no model, application or VM is executed."""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "scripts"))
import audit_candidate_evidence as auditor
from verify_v03_evidence import request_fields_match


def operations(case):
    dataset = "renamed" if case == "renamed_topology" else "canonical"
    validation = {"validation_id": "v1", "plan_id": "p1", "plan_hash": "h1", "feasible": True,
                  "independent": True, "coverage": "available", "violations": []}
    final = {"type": "final", "selected_plan_id": "p1", "validation_id": "v1"}
    events = [{"type": "model_request", "model_identity": auditor.MODEL, "http_status": 200,
               "duration_ms": 12, "retry_count": 0},
              {"type": "tool_call", "call_id": "c1", "name": "validate_plan",
               "arguments": {"plan_id": "p1", "evaluator_method": "method1"}},
              {"type": "tool_result", "call_id": "c1", "name": "validate_plan", "ok": True,
               "analysis_request_id": "r1"}, final]
    request = {"request_id": "r1", "capture_id": "capture", "dataset_identity": dataset,
               "operation": "validate_plan", "arguments": {"plan": {"plan_id": "p1", "plan_hash": "h1"},
                                                            "evaluator_method": "method1"}}
    response = {**request, "status": "completed", "result": {"evaluation": {"plan": {"plan_id": "p1", "plan_hash": "h1"}},
                                     "validation": validation}}
    archive = {"session_id": case, "historical": True, "actionable": False, "operator_records": [],
               "capture_manifest": {"capture_id": "capture", "dataset_identity": dataset},
               "agent_events": [{"event": row} for row in events],
               "analyses": [{"request": request, "response": response}]}
    summary = {"session_id": case, "model_requests": 1, "tool_order": ["validate_plan"],
               "capture_id": "capture", "dataset_identity": dataset, "exit_code": 0,
               "status": "passed", "grounded_review_eligible_final": True,
               "selected_plan_id": "p1", "validation_id": "v1"}
    return archive, summary


def observed(missing=False):
    case = "observed_missing_coverage" if missing else "observed_pump"
    snapshot = {"snapshot_id": "capture", "physical_hash": "design", "configuration_hash": "configuration",
                "actionable": False, "availability": "unavailable" if missing else "available",
                "captured_event_time_s": 70, "expires_event_time_s": 70 if missing else 75}
    mapping = {"actionable": False, "design_physical_hash": "design", "effective_physical_hash": "effective",
               "detector": {"configuration_hash": "configuration"},
               "mapping_status": "unavailable" if missing else "available"}
    origin = {"snapshot": snapshot, "mapping": mapping, "effective_network": {"fixture": True},
              "context": {"physical_hash": "design"}}
    recommendation = {"source_mode": "hosted", "actionable": False,
                      "status": "no_validated_plan" if missing else "validated_recommendation",
                      "selected_run_id": None if missing else "run1"}
    run = {"run_id": "run1", "validation_id": "v1", "validation_status": "validated", "cost": 12,
           "snapshot_id": "investigation", "physical_hash": "effective", "horizon_h": 3,
           "validation": {"feasible": True, "violations": []}}
    if not missing:
        recommendation.update({"validation_id": "v1", "validation_source": "validate_run",
                               "snapshot_id": "investigation", "physical_hash": "effective", "horizon_h": 3,
                               "verified_feasible": True, "verified_cost": 12})
    tool = "get_observations" if missing else "validate_run"
    events = [{"type": "model_request_completed", "request": {"configured_model": auditor.MODEL,
               "response_model": auditor.MODEL, "model_request_id": "m1", "duration_ms": 12,
               "error_code": "", "http_status": 200, "outcome": "succeeded", "retry_count": 0}},
              {"type": "tool_requested", "tool_call_id": "c1", "tool_name": tool},
              {"type": "tool_started", "tool_call_id": "c1", "tool_name": tool},
              {"type": "tool_completed", "tool_call_id": "c1", "tool_name": tool,
               "result": {"ok": True, "run": deepcopy(run)}},
              {"type": "recommendation_ready", "recommendation": deepcopy(recommendation)}]
    for row in events:
        row["session_id"] = case
    return {"session_id": case, "archive_version": 2, "session_kind": "observed_origin", "actionable": False,
            "model": {"configured_model": auditor.MODEL}, "evidence_envelopes": [{"kind": "observed_origin", "payload": origin}],
            "events": events, "status": "completed", "recommendation": recommendation,
            "runs": [] if missing else [{"ok": True, "run": run}], "approvals": [],
            "snapshot": {"snapshot_id": "investigation", "physical_hash": "effective", "horizon_h": 3,
                         "network": {"fixture": True}}}


class CandidateEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="breakwater-evidence-fixture-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.path = self.directory / "manifest.json"
        self.manifest = {"manifest_version": 1, "source_commit": "a" * 40, "dirty": False,
                         "status": "passed", "model_identity": auditor.MODEL,
                         "executables": {key: "b" * 64 for key in set(auditor.CASES.values())}, "cases": []}
        for case, executable in auditor.CASES.items():
            row = {"case": case, "source_commit": "a" * 40, "executable": executable,
                   "executable_sha256": "b" * 64, "status": "passed"}
            if case == "protocol_smoke":
                row.update({"exit_code": 0, "evidence": self.write(case + ".log",
                    f"Executed actual get_network_state; snapshot test\nPASS: model {auditor.MODEL} "
                    "issued a structured call, received its real result and continued.\n")})
            elif executable == "eog_live_check":
                archive, summary = operations(case)
                row.update({"exit_code": 0, "evidence": self.write(case + ".json", archive),
                            "summary": self.write(case + "-summary.json", summary)})
            else:
                row["evidence"] = self.write(case + ".json", observed(case == "observed_missing_coverage"))
            self.manifest["cases"].append(row)

    def write(self, name, value):
        raw = (json.dumps(value) if isinstance(value, dict) else value).encode()
        (self.directory / name).write_bytes(raw)
        return {"path": name, "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}

    def row(self, case):
        return next(row for row in self.manifest["cases"] if row["case"] == case)

    def change(self, case, mutator, kind="evidence"):
        row = self.row(case)
        name = row[kind]["path"]
        value = json.loads((self.directory / name).read_text())
        mutator(value)
        row[kind] = self.write(name, value)

    def audit(self):
        self.path.write_text(json.dumps(self.manifest))
        return auditor.audit(self.path)

    def reject(self):
        with self.assertRaises((ValueError, KeyError, TypeError)):
            self.audit()

    def test_complete_declared_round_passes_without_release_authority(self):
        result = self.audit()
        self.assertEqual(result["qualification_status"], "passed")
        self.assertFalse(result["public_release_approved"])
        self.assertEqual(len(result["cases"]), 7)

    def test_closed_retry_wait_metadata_supports_legacy_and_both_archive_kinds(self):
        for observed_mode in (False, True):
            request = (observed()["events"][0]["request"] if observed_mode else
                       operations("canonical_1")[0]["agent_events"][0]["event"])
            required = set(request)
            auditor.request_metadata([request], observed=observed_mode)
            for policy, delays in (("retry_after_seconds", [6000]), ("retry_after_date", [0]),
                                   ("bounded_backoff", [250, 500, 1000]),
                                   ("retry_after_exceeds_budget", []),
                                   ("invalid_retry_after_backoff", [250])):
                with self.subTest(observed=observed_mode, policy=policy):
                    row = {**request, "retry_wait_policy": policy, "retry_delays_ms": delays}
                    self.assertTrue(request_fields_match(row, required))
                    auditor.request_metadata([row], observed=observed_mode)

    def test_retry_wait_metadata_rejects_tampering_without_relaxing_primary_fields(self):
        original = operations("canonical_1")[0]["agent_events"][0]["event"]
        valid = {**original, "retry_wait_policy": "retry_after_seconds", "retry_delays_ms": [6000]}
        for update in ({"retry_wait_policy": "raw-private-header"}, {"retry_wait_policy": {}},
                       {"retry_delays_ms": [True]}, {"retry_delays_ms": [-1]},
                       {"retry_delays_ms": [0.5]}, {"retry_delays_ms": [1215001]},
                       {"retry_delays_ms": [10 ** 400]}, {"retry_delays_ms": [float("inf")]},
                       {"retry_delays_ms": [float("nan")]}, {"retry_delays_ms": [0] * 4},
                       {"retry_delays_ms": "6000"}, {"headers": {"Retry-After": "private"}}):
            with self.subTest(update=update):
                row = {**valid, **update}
                self.assertFalse(request_fields_match(row, set(original)))
                with self.assertRaises(ValueError):
                    auditor.request_metadata([row])
        for missing in ("retry_wait_policy", "retry_delays_ms", "model_identity"):
            row = dict(valid)
            del row[missing]
            self.assertFalse(request_fields_match(row, set(original)))
            with self.assertRaises(ValueError):
                auditor.request_metadata([row])

    def test_pending_round_is_honestly_incomplete(self):
        self.manifest["status"] = "pending"
        row = self.row("observed_pump")
        row["status"] = "pending"
        del row["evidence"]
        self.assertEqual(self.audit()["qualification_status"], "incomplete")

    def test_failed_attempt_is_retained_and_not_promoted(self):
        self.manifest["status"] = "failed"
        self.row("canonical_1")["status"] = "failed"
        self.assertEqual(self.audit()["qualification_status"], "incomplete")

    def test_failed_wrong_model_evidence_is_preserved_without_becoming_passed(self):
        self.manifest["status"] = "failed"
        self.row("canonical_1")["status"] = "failed"
        self.change("canonical_1", lambda a: a["agent_events"][0]["event"].update(model_identity="wrong-model"))
        self.assertEqual(self.audit()["qualification_status"], "incomplete")
        self.row("canonical_1")["status"] = "passed"
        self.reject()

    def test_interrupted_tool_prefix_is_allowed_only_for_failed_attempt(self):
        self.manifest["status"] = "failed"
        self.row("canonical_1")["status"] = "failed"
        self.change("canonical_1", lambda a: a["agent_events"].__delitem__(slice(2, None)))
        self.assertEqual(self.audit()["qualification_status"], "incomplete")
        self.row("canonical_1")["status"] = "passed"
        self.reject()

    def test_all_seven_required_once(self):
        for mutation in (lambda rows: rows.pop(), lambda rows: rows.__setitem__(6, deepcopy(rows[5]))):
            original = deepcopy(self.manifest)
            mutation(self.manifest["cases"])
            self.reject()
            self.manifest = original

    def test_mixed_source_and_binary_rejected(self):
        for field, value in (("source_commit", "c" * 40), ("executable_sha256", "c" * 64),
                             ("executable", "desktop")):
            row = self.row("canonical_1")
            previous = row[field]
            row[field] = value
            self.reject()
            row[field] = previous

    def test_dirty_and_false_pass_rejected(self):
        self.manifest["dirty"] = True
        self.reject()
        self.manifest["dirty"] = False
        self.row("protocol_smoke")["status"] = "pending"
        self.reject()

    def test_tampering_size_path_escape_and_oversize_rejected(self):
        row = self.row("protocol_smoke")
        original = deepcopy(row["evidence"])
        for field, value in (("sha256", "0" * 64), ("bytes", 1), ("path", "../outside.log"),
                             ("bytes", auditor.MAX_EVIDENCE_BYTES + 1)):
            row["evidence"] = {**original, field: value}
            self.reject()
        row["evidence"] = original

    def test_duplicate_keys_rejected(self):
        self.path.write_text('{"manifest_version":1,"manifest_version":1}')
        with self.assertRaises(ValueError):
            auditor.audit(self.path)

    def test_secret_fields_nonfinite_and_credential_text_rejected(self):
        for key, value in (("authorization", "redacted"), ("fixture", float("nan")),
                           ("fixture", "nvapi-" + "x" * 40)):
            self.manifest[key] = value
            self.reject()
            del self.manifest[key]

    def test_smoke_needs_actual_execution_continuation_and_matching_model(self):
        row = self.row("protocol_smoke")
        row["evidence"] = self.write("protocol_smoke.log", "PASS: everything worked")
        self.reject()

    def test_call_result_mismatch_rejected(self):
        self.change("canonical_1", lambda a: a["agent_events"][2]["event"].update(call_id="wrong"))
        self.reject()

    def test_duplicate_session_cannot_count_as_a_second_rehearsal(self):
        self.change("canonical_2", lambda a: a.update(session_id="canonical_1"))
        self.change("canonical_2", lambda a: a.update(session_id="canonical_1"), kind="summary")
        self.reject()

    def test_json_parser_consumes_the_verified_bytes(self):
        path = self.directory / "changed.json"
        path.write_text('{"changed":true}')
        with patch.object(auditor, "reference", return_value=(path, b'{"verified":true}')):
            self.assertEqual(auditor.json_reference(self.directory, {}), {"verified": True})

    def test_final_plan_and_validation_hash_must_match(self):
        for change in (lambda a: a["agent_events"][-1]["event"].update(validation_id="wrong"),
                       lambda a: a["analyses"][0]["response"]["result"]["validation"].update(plan_hash="wrong"),
                       lambda a: a["analyses"][0]["response"]["result"]["validation"].update(independent=False)):
            original = deepcopy(self.row("canonical_1"))
            archive, summary = operations("canonical_1")
            change(archive)
            self.row("canonical_1")["evidence"] = self.write("canonical_1.json", archive)
            self.reject()
            self.row("canonical_1").update(original)

    def test_orphan_or_different_model_validation_cannot_ground_the_final(self):
        for change in (lambda a: a["agent_events"][2]["event"].update(analysis_request_id="unrelated"),
                       lambda a: a["agent_events"][1]["event"]["arguments"].update(plan_id="different-plan"),
                       lambda a: a["agent_events"][1]["event"]["arguments"].update(evaluator_method="different-method"),
                       lambda a: a["analyses"][0]["request"].update(operation="evaluate_plan")):
            archive, _ = operations("canonical_1")
            change(archive)
            self.row("canonical_1")["evidence"] = self.write("canonical_1.json", archive)
            self.reject()

    def test_wrong_model_request_or_capture_rejected(self):
        self.change("canonical_1", lambda a: a["agent_events"][0]["event"].update(model_identity="different"))
        self.reject()

    def test_recovered_requests_and_bounded_retries_are_retained(self):
        def recovered(archive):
            failed = deepcopy(archive["agent_events"][0])
            failed["event"].update(http_status=503, retry_count=1)
            archive["agent_events"].insert(0, failed)
        self.change("canonical_1", recovered)
        self.change("canonical_1", lambda summary: summary.update(model_requests=2), kind="summary")
        self.assertEqual(self.audit()["qualification_status"], "passed")

    def test_renamed_cannot_reuse_canonical_dataset(self):
        archive, summary = operations("canonical_1")
        archive["session_id"] = "renamed_topology"
        summary["session_id"] = "renamed_topology"
        row = self.row("renamed_topology")
        row["evidence"] = self.write("renamed_topology.json", archive)
        row["summary"] = self.write("renamed_topology-summary.json", summary)
        self.reject()

    def test_observed_must_be_hosted_and_use_frozen_effective_network(self):
        self.change("observed_pump", lambda a: a["snapshot"].update(network={"wrong": True}))
        self.reject()

    def test_observed_selected_run_needs_own_exact_validation(self):
        self.change("observed_pump", lambda a: a["events"][3]["result"]["run"].update(validation_id="wrong"))
        self.reject()

    def test_matching_run_and_validation_cannot_belong_to_another_snapshot(self):
        def forged(archive):
            for row in (archive["runs"][0]["run"], archive["events"][3]["result"]["run"],
                        archive["recommendation"], archive["events"][-1]["recommendation"]):
                row["snapshot_id"] = "different-frozen-state"
        self.change("observed_pump", forged)
        self.reject()

    def test_missing_coverage_cannot_produce_numerical_tools_runs_or_approval(self):
        self.change("observed_missing_coverage", lambda a: a.update(runs=[{"run": {"run_id": "x"}}]))
        self.reject()

    def test_missing_coverage_permits_a_rejected_model_numerical_request(self):
        def blocked(archive):
            for row in archive["events"]:
                if row["type"] in {"tool_requested", "tool_started", "tool_completed"}:
                    row["tool_name"] = "forecast_baseline"
                if row["type"] == "tool_completed":
                    row["result"] = {"ok": False, "error": {"code": "coverage_unavailable"}}
        self.change("observed_missing_coverage", blocked)
        self.assertEqual(self.audit()["qualification_status"], "passed")
        self.change("observed_missing_coverage", lambda a: a["events"][3]["result"].update(ok=True))
        self.reject()

    def test_missing_coverage_rejects_success_from_plural_simulator_and_science(self):
        for tool in ("simulate_candidates", "get_sensitivity_catalog", "analyze_sensitivities", "compare_runs"):
            archive = observed(True)
            for row in archive["events"]:
                if row["type"] in {"tool_requested", "tool_started", "tool_completed"}:
                    row["tool_name"] = tool
            self.row("observed_missing_coverage")["evidence"] = self.write("observed_missing_coverage.json", archive)
            self.reject()

    def test_observed_local_search_is_not_hosted_qualification(self):
        def local(archive):
            archive["recommendation"]["source_mode"] = "local_native"
            archive["events"][-1]["recommendation"]["source_mode"] = "local_native"
        self.change("observed_pump", local)
        self.reject()

    def test_acknowledgement_is_advisory_only_and_apply_rejected(self):
        approval = {"status": "operator_acknowledged_advisory", "run_id": "run1", "validation_id": "v1"}
        self.change("observed_pump", lambda a: a["approvals"].append(approval))
        self.assertEqual(self.audit()["qualification_status"], "passed")
        self.change("observed_pump", lambda a: a["events"][3]["result"].update(applied=True))
        self.reject()

    def test_cli_incomplete_returns_two_without_printing_untrusted_errors(self):
        self.manifest["status"] = "pending"
        self.audit()
        run = subprocess.run([sys.executable, str(ROOT / "scripts/audit_candidate_evidence.py"), str(self.path)],
                             text=True, capture_output=True, check=False)
        self.assertEqual(run.returncode, 2)
        self.assertEqual(json.loads(run.stdout)["qualification_status"], "incomplete")
        self.path.write_text('{"authorization":"do not echo"}')
        run = subprocess.run([sys.executable, str(ROOT / "scripts/audit_candidate_evidence.py"), str(self.path)],
                             text=True, capture_output=True, check=False)
        self.assertEqual(run.returncode, 1)
        self.assertNotIn("do not echo", run.stdout + run.stderr)

    def test_cli_handles_deep_or_malformed_nested_containers_without_traceback(self):
        invalid = ["[" * 1800 + "0" + "]" * 1800,
                   json.dumps({**self.manifest, "executables": ["desktop", "eog_live_check", "nemotron_smoke"]})]
        for raw in invalid:
            self.path.write_text(raw)
            run = subprocess.run([sys.executable, str(ROOT / "scripts/audit_candidate_evidence.py"), str(self.path)],
                                 text=True, capture_output=True, check=False)
            self.assertEqual(run.returncode, 1)
            self.assertEqual(json.loads(run.stdout)["code"], "invalid_candidate_evidence")
            self.assertEqual(run.stderr, "")


if __name__ == "__main__":
    unittest.main()
