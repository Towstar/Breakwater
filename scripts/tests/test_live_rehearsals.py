"""Synthetic auditor regression fixtures ONLY. Never live rehearsal evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("audit", ROOT / "scripts/Verify-LiveRehearsals.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def fixture():
    session, snapshot, run_id, validation = (str(uuid4()) for _ in range(4))
    stamp = "2026-11-07T20:00:00.000Z"
    network = {"assets": [
        dict(id="tank_a", kind="tank", capacity_m3=120, initial_m3=80),
        dict(id="tank_b", kind="tank", capacity_m3=130, initial_m3=114),
        dict(id="source_a", kind="source", inflow_m3h=52, curtailment_m3h=0, curtailment_max_m3h=15, cost_per_m3=250),
        dict(id="source_b", kind="source", inflow_m3h=16, curtailment_m3h=0, curtailment_max_m3h=0, cost_per_m3=250),
        dict(id="treatment", kind="treatment", capacity_m3h=60, cost_per_m3=0),
        dict(id="trucking", kind="trucking", capacity_m3h=22, cost_per_m3=80)], "routes": []}
    for key, source, target, capacity, setpoint, pump, available in [
        ("feed_a", "source_a", "tank_a", 52, 52, False, True),
        ("feed_b", "source_b", "tank_b", 16, 16, False, True),
        ("main_pump", "tank_a", "treatment", 55, 0, True, False),
        ("backup_pump", "tank_b", "treatment", 35, 16, True, True),
        ("bypass", "tank_a", "tank_b", 25, 0, False, True),
        ("truck_route", "tank_a", "trucking", 22, 0, False, True)]:
        network["routes"].append(dict(id=key, source_id=source, target_id=target, capacity_m3h=capacity,
                                      setpoint_m3h=setpoint, has_pump=pump, available=available, cost_per_m3=0))
    controls = [{"control_id": "route:bypass:flow", "value": 20}]
    run = dict(run_id=run_id, snapshot_id=snapshot, physical_hash="a"*64, horizon_h=3, cost=100,
               controls=controls, validation_status="validated", validation_id=validation, validation={"feasible": True})
    recommendation = dict(run, selected_run_id=run_id, status="validated_recommendation", approval_status="operator_applied",
                          actionable=False, ok=True, verified_feasible=True, verified_cost=100, session_id=session)
    events = []
    def event(kind, **values):
        events.append(dict(type=kind, session_id=session, snapshot_id=snapshot, sequence=len(events)+1,
                           timestamp=stamp, elapsed_ms=len(events), **values))
    def metadata():
        event("model_request_completed", request=dict(model_request_id=str(uuid4()), configured_model=audit.DEFAULT_MODEL,
              response_model=audit.DEFAULT_MODEL, outcome="succeeded", http_status=200, error_code="", duration_ms=1, retry_count=0))
    def tool(name, arguments, result):
        call = str(uuid4())
        event("tool_requested", tool_call_id=call, tool_name=name)
        event("tool_started", tool_call_id=call, tool_name=name, input_summary=arguments)
        event("tool_completed", tool_call_id=call, tool_name=name, result=result)
    event("agent_started")
    metadata()
    tool("get_network_state", {}, {"ok": True})
    tool("get_control_catalog", {}, {"ok": True})
    metadata()
    tool("forecast_baseline", {"snapshot_id": snapshot}, {"ok": True})
    metadata()
    tool("validate_run", {"snapshot_id": snapshot, "run_id": run_id}, {"ok": True, "run": run})
    metadata()
    tool("request_approval", {"snapshot_id": snapshot, "run_id": run_id},
         {"ok": True, "run_id": run_id, "applied": False, "approval_status": "pending_operator", "plan": recommendation})
    metadata()
    event("recommendation_ready", recommendation=dict(recommendation, approval_status="pending_operator"))
    return dict(archive_version=1, status="completed", session_id=session, checkpoint_sequence=1, created_at=stamp, updated_at=stamp,
                snapshot=dict(snapshot_id=snapshot, physical_hash="a"*64, horizon_h=3, network=network),
                runs=[dict(ok=True, run=run, time_h=[0, 3], tank_ids=["tank_a", "tank_b"], volumes_m3=[[80, 100], [114, 120]])],
                recommendation=recommendation,
                approvals=[dict(timestamp=stamp, status="operator_applied", run_id=run_id, validation_id=validation,
                                physical_hash="a"*64, controls=controls)], events=events)


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="breakwater-synthetic-audit-fixture-")
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "synthetic-only.json"
        self.value = fixture()

    def check(self, value):
        self.path.write_text(json.dumps(value), encoding="utf-8")
        return audit.audit_archive(self.path, audit.DEFAULT_MODEL, "canonical")

    def test_positive_internal_consistency_fixture(self):
        self.assertEqual(self.check(self.value)["tool_calls"], 5)

    def test_missing_operator_approval(self):
        self.value["approvals"] = []
        with self.assertRaises(audit.AuditError): self.check(self.value)

    def test_mismatched_tool_call_identity(self):
        next(event for event in self.value["events"] if event["type"] == "tool_completed")["tool_call_id"] = "foreign"
        with self.assertRaises(audit.AuditError): self.check(self.value)

    def test_missing_hosted_metadata(self):
        self.value["events"] = [event for event in self.value["events"] if event["type"] != "model_request_completed"]
        with self.assertRaises(audit.AuditError): self.check(self.value)

    def test_wrong_hosted_identity(self):
        next(event for event in self.value["events"] if event["type"] == "model_request_completed")["request"]["response_model"] = "another-model"
        with self.assertRaises(audit.AuditError): self.check(self.value)

    def test_no_successful_tool_result_continuation(self):
        self.value["events"].pop(-2)
        with self.assertRaises(audit.AuditError): self.check(self.value)

    def test_malformed_duplicate_nonfinite_json_and_forbidden_fields(self):
        for payload in [b"{", b'{"a":1,"a":2}', b'{"value":NaN}', b'{"authorization":"synthetic-only"}']:
            with self.assertRaises(audit.AuditError): audit.decode(payload)

    def test_missing_retained_scientific_result(self):
        database = Path(self.temp.name) / "synthetic-only.sqlite"
        with sqlite3.connect(database) as connection:
            connection.execute("CREATE TABLE sessions(session_id TEXT PRIMARY KEY, archive BLOB)")
        connection.close()
        with self.assertRaises(audit.AuditError): audit.retained_science(str(uuid4()), [database])

    def test_duplicate_topology_identity(self):
        self.value["snapshot"]["network"]["assets"].append(self.value["snapshot"]["network"]["assets"][0])
        with self.assertRaises(audit.AuditError): self.check(self.value)

    def test_staging_or_native_validation_mismatch(self):
        for kind in ["validate_run", "request_approval"]:
            value = copy.deepcopy(self.value)
            event = next(event for event in value["events"] if event.get("tool_name") == kind and event["type"] == "tool_completed")
            event["result"]["run" if kind == "validate_run" else "plan"]["validation_id"] = str(uuid4())
            with self.assertRaises(audit.AuditError): self.check(value)


if __name__ == "__main__":
    print("SYNTHETIC AUDITOR FIXTURE REGRESSION ONLY - NOT LIVE REHEARSAL EVIDENCE")
    unittest.main(verbosity=2)
