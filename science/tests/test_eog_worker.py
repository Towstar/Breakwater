import copy
from dataclasses import replace
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import eog_worker as worker
from eog_test_fixtures import frame, inputs, model
from eog_response import compile_context
from eog_detection import detect
from eog_model import compile_model
from eog_test_fixtures import view
from test_eog_response import proposal

SCIENCE = Path(__file__).resolve().parents[1]


def capture(folder, contract, *, historical=False, multiple_systems=False):
    network, ops = inputs()
    if multiple_systems:
        other_network, other_ops = inputs(prefix="other/system/\u03b2")
        network["systems"].extend(other_network["systems"])
        ops["systems"].update(other_ops["systems"])
    m = compile_model(network, ops)
    rows = sorted((s for i in range(len(m.facilities)) for s in frame(m, 0, index=i, status=0) + frame(m, 300, index=i, status=2)),
                  key=lambda s: (s.unix, s.asset_id, s.metric))
    content = {"network.json": json.dumps(network, ensure_ascii=False).encode(),
               "ops.json": json.dumps(ops, ensure_ascii=False).encode()}
    text = "timestamp,ts_unix,asset_id,metric,value\n" + "".join(
        f"{datetime.fromtimestamp(s.unix, timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')},{s.unix},{s.asset_id},{s.metric},{s.value}\n"
        for s in rows)
    content["telemetry.csv"] = text.encode()
    for name, raw in content.items():
        (folder / name).write_bytes(raw)
    files = {name: {"sha256": hashlib.sha256(raw).hexdigest(), "size_bytes": len(raw)} for name, raw in content.items()}
    hashes = {name: item["sha256"] for name, item in files.items()}
    if historical:
        hashes["telemetry.csv"] = hashlib.sha256(b"unread-original-future").hexdigest()
    manifest = {"eog_version": 1, "kind": "capture_manifest", "dataset_identity": contract.dataset_identity(hashes),
                "source_hashes": hashes, "captured_files": files, "source_first_unix": 0,
                "source_now_unix": 600 if historical else 300, "cutoff_unix": 300,
                "context": "historical_cursor" if historical else "snapshot_now", "sample_period_s": 300, "max_age_s": 300}
    manifest["capture_id"] = contract.capture_identity(manifest)
    request = {"eog_version": 1, "kind": "analysis_request", "request_id": "opaque/request-alpha", "operation": "detect",
               "method_version": 1, "dataset_identity": manifest["dataset_identity"], "capture_id": manifest["capture_id"],
               "cutoff_unix": 300, "capture_directory": str(folder), "capture_manifest": manifest, "arguments": {"system_ids": []}}
    return request, m, rows


class EogWorkerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = os.environ.get("BREAKWATER_EOG_CONTRACTS")
        if not root:
            raise unittest.SkipTest("Set BREAKWATER_EOG_CONTRACTS to the adopted contract root.")
        cls.root = Path(root)
        cls.contract = worker.contract_module(cls.root)
        cls.native = os.environ.get("BREAKWATER_EOG_NATIVE")
        # Materialize local schema dependencies before patching input reads.
        cls.contract.validate_document(cls.contract.decode_document((cls.root / "examples/detect-request.json").read_bytes()))

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.request, self.model, self.rows = capture(self.folder, self.contract)

    def call(self, request=None, native=None):
        request = self.request if request is None else request
        result = worker.handle(request, self.contract, native)
        self.contract.validate_document(result, context={"request": request})
        self.assertLessEqual(len(worker.encode_response(result)), worker.MAX_RESPONSE)
        return result

    def evaluate_request(self):
        request = copy.deepcopy(self.request)
        request["operation"] = "evaluate_plan"
        plan = proposal(compile_context(self.model, view(self.model, self.rows)))
        plan["plan_hash"] = self.contract.plan_identity(plan)
        request["arguments"] = {"plan": plan}
        return request

    def test_protected_causal_detection_and_exact_binding(self):
        before = copy.deepcopy(self.request)
        result = self.call()
        self.assertEqual(result["status"], "completed", result["diagnostics"])
        self.assertEqual(len(result["result"]["events"]), 1)
        event = result["result"]["events"][0]
        self.assertEqual(event["recommended_action"], "plan_required")
        self.assertAlmostEqual(event["countdown_hours"], 4.48)
        self.assertEqual(event["lost_revenue_per_hour"], 3300)
        self.assertTrue(all(s["timestamp_unix"] <= 300 for s in result["result"]["evidence"]))
        self.assertEqual({k: result[k] for k in worker.BINDINGS}, {k: before[k] for k in worker.BINDINGS})
        self.assertEqual(self.request, before)

    def test_runtime_only_reads_fixed_inputs_and_trusted_schemas(self):
        (self.folder / "answer_key.json").write_text("NOT VALID JSON; MUST NOT READ", encoding="utf-8")
        original = Path.open
        opened = []
        def guarded(path, *args, **kwargs):
            opened.append(path)
            if path.parent == self.folder and path.name not in ("network.json", "ops.json", "telemetry.csv"):
                self.fail("Runtime opened a file outside the three allowed capture names.")
            return original(path, *args, **kwargs)
        with patch.object(Path, "open", guarded):
            self.assertEqual(self.call()["status"], "completed")
        self.assertEqual({p.name for p in opened if p.parent == self.folder}, {"network.json", "ops.json", "telemetry.csv"})

    def test_capture_hash_tamper_is_not_an_empty_healthy_result(self):
        with (self.folder / "telemetry.csv").open("ab") as stream:
            stream.write(b"\n")
        result = self.call()
        self.assertEqual(result["status"], "invalid_input"); self.assertIsNone(result["result"])

    def test_repeated_private_evidence_maps_to_unique_public_references(self):
        v = view(self.model, self.rows)
        detection = detect(self.model, v)
        event = detection.events[0]
        repeated = replace(event, evidence=event.evidence + event.evidence)
        with patch.object(worker, "detect", return_value=replace(detection, events=(repeated,))):
            result = self.call()
        self.assertEqual(result["status"], "completed", result["diagnostics"])
        mapped = result["result"]["events"][0]
        self.assertEqual(len(mapped["evidence_ids"]), len(set(mapped["evidence_ids"])))
        self.assertEqual(len(mapped["evidence_ids"]), len(event.evidence))

    def test_change_between_verifier_and_loader_is_rejected(self):
        original = worker.load_inputs
        def changed(*args, **kwargs):
            path = self.folder / "ops.json"
            value = json.loads(path.read_text(encoding="utf-8")); value["prices"]["oil_per_bbl"] += 1
            path.write_text(json.dumps(value), encoding="utf-8")
            return original(*args, **kwargs)
        with patch.object(worker, "load_inputs", changed):
            result = self.call()
        self.assertEqual(result["status"], "invalid_input")
        self.assertEqual(result["diagnostics"][0]["code"], "capture_changed")

    def test_historical_future_identity_does_not_change_analysis_evidence(self):
        request, _, _ = capture(self.folder, self.contract, historical=True)
        a = self.call(request)
        request["capture_manifest"]["source_hashes"]["telemetry.csv"] = hashlib.sha256(b"different-unread-future").hexdigest()
        request["dataset_identity"] = request["capture_manifest"]["dataset_identity"] = self.contract.dataset_identity(request["capture_manifest"]["source_hashes"])
        request["capture_id"] = request["capture_manifest"]["capture_id"] = self.contract.capture_identity(request["capture_manifest"])
        b = self.call(request)
        self.assertEqual(a["status"], "completed"); self.assertEqual(b["status"], "completed")
        for field in ("events", "evidence", "coverage", "assumptions", "limitations"):
            self.assertEqual(a["result"][field], b["result"][field])

    def test_unknown_system_and_missing_search_native_are_explicit(self):
        self.request["arguments"]["system_ids"] = ["unknown"]
        self.assertEqual(self.call()["status"], "invalid_input")
        self.request["operation"] = "search_plans"
        self.request["arguments"] = {"system_ids": [], "candidate_limit": 100, "time_limit_s": 5}
        result = self.call()
        self.assertEqual(result["status"], "unavailable"); self.assertIsNone(result["result"])
        self.assertEqual(result["diagnostics"][0]["code"], "native_unavailable")

    def test_actual_joint_search_process_and_independent_selected_record(self):
        if not self.native:
            self.skipTest("Set BREAKWATER_EOG_NATIVE for actual joint-search process qualification.")
        request = copy.deepcopy(self.request); request["operation"] = "search_plans"
        request["arguments"] = {"system_ids": [], "candidate_limit": 300, "time_limit_s": 5}
        run = self.process(worker.encode_response(request), native=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        result = self.contract.decode_document(run.stdout)
        self.contract.validate_document(result, context={"request": request})
        self.assertEqual(result["status"], "completed", result["diagnostics"])
        selected = next(e for e in result["result"]["evaluations"] if e["plan"]["plan_id"] == result["result"]["selected_plan_id"])
        self.assertEqual(selected["cost"]["total"], 905)
        self.assertLessEqual(result["result"]["search"]["evaluated_candidates"], 300)
        self.assertTrue(any("independently recomputed" in s for s in result["result"]["limitations"]))
        request["request_id"] += ":validate"; request["operation"] = "validate_plan"
        request["arguments"] = {"plan": selected["plan"], "evaluator_method": "eog-native-seconds-0.1.1"}
        checked = self.call(request)
        self.assertEqual(checked["status"], "completed", checked["diagnostics"])
        self.assertEqual(checked["result"]["validation"]["cost"], selected["cost"])
        self.assertEqual(checked["result"]["validation"]["plan_hash"], selected["plan"]["plan_hash"])

    def test_search_system_scope_never_silently_drops_other_failures(self):
        request, captured_model, _ = capture(self.folder, self.contract, multiple_systems=True)
        request["operation"] = "search_plans"
        systems = [s.id for s in captured_model.systems]
        self.assertEqual(len(systems), 2)
        for selected, code in (([systems[0]], "system_scope_unavailable"), (["unknown"], "unknown_system"),
                               ([], "native_unavailable"), (systems, "native_unavailable")):
            request["arguments"] = {"system_ids": selected, "candidate_limit": 100, "time_limit_s": 5}
            response = self.call(request)
            self.assertEqual(response["diagnostics"][0]["code"], code)
            self.assertIsNone(response["result"])

    def test_search_tiny_budget_returns_no_selection_not_empty_healthy_data(self):
        if not self.native:
            self.skipTest("Set BREAKWATER_EOG_NATIVE for actual bounded-search qualification.")
        request = copy.deepcopy(self.request); request["operation"] = "search_plans"
        request["arguments"] = {"system_ids": [], "candidate_limit": 1, "time_limit_s": 1e-300}
        result = self.call(request, self.native)
        self.assertEqual(result["status"], "completed", result["diagnostics"])
        self.assertIsNone(result["result"]["selected_plan_id"])
        self.assertFalse(result["result"]["search"]["complete"])
        self.assertEqual(result["result"]["search"]["optimality"], "unavailable")

    def test_missing_native_capability_is_explicit(self):
        result = self.call(self.evaluate_request())
        self.assertEqual(result["status"], "unavailable")
        self.assertEqual(result["diagnostics"][0]["code"], "native_unavailable")

    def test_independent_validation_needs_no_native_or_detector_execution(self):
        request = self.evaluate_request(); request["operation"] = "validate_plan"
        request["arguments"]["evaluator_method"] = "eog-native-seconds-0.1.0"
        with patch.object(worker, "detect", side_effect=AssertionError("detector called")), \
             patch.object(worker, "compile_context", side_effect=AssertionError("compiler called")), \
             patch.object(worker, "evaluate", side_effect=AssertionError("evaluator called")), \
             patch.object(worker, "NativeEvaluator", side_effect=AssertionError("native loaded")):
            result = self.call(request)
        self.assertEqual(result["status"], "completed", result["diagnostics"])
        validation = result["result"]["validation"]
        self.assertTrue(validation["independent"]); self.assertEqual(validation["cost"]["total"], 5016)
        self.assertEqual(validation["plan_hash"], request["arguments"]["plan"]["plan_hash"])
        self.assertEqual(validation["validated_horizons"], request["arguments"]["plan"]["repair_deadlines"])

    def test_actual_independent_validation_process_and_forged_deadline(self):
        request = self.evaluate_request(); request["operation"] = "validate_plan"
        request["arguments"]["evaluator_method"] = "eog-native-seconds-0.1.0"
        run = self.process(worker.encode_response(request))
        self.assertEqual(run.returncode, 0, run.stderr)
        result = self.contract.decode_document(run.stdout)
        self.contract.validate_document(result, context={"request": request})
        self.assertEqual(result["status"], "completed", result["diagnostics"])
        self.assertEqual(result["result"]["validation"]["cost"]["total"], 5016)
        request["arguments"]["plan"]["repair_deadlines"][0]["repair_elapsed_s"] += 1
        request["arguments"]["plan"]["plan_hash"] = self.contract.plan_identity(request["arguments"]["plan"])
        invalid = self.call(request)
        self.assertEqual(invalid["status"], "completed", invalid["diagnostics"])
        self.assertFalse(invalid["result"]["validation"]["feasible"])
        self.assertIsNone(invalid["result"]["validation"]["cost"])
        self.assertNotEqual(invalid["result"]["analysis_id"], result["result"]["analysis_id"])

    def test_unknown_evaluator_cannot_be_relabeled_as_independent_validation(self):
        request = self.evaluate_request(); request["operation"] = "validate_plan"
        request["arguments"]["evaluator_method"] = "unknown-evaluator"
        result = self.call(request)
        self.assertEqual(result["status"], "unavailable")
        self.assertEqual(result["diagnostics"][0]["code"], "unsupported_evaluator")

    def test_positive_cost_underflow_is_unavailable_in_both_real_processes(self):
        if not self.native:
            self.skipTest("Set BREAKWATER_EOG_NATIVE for the native underflow process regression.")
        ops_path = self.folder / "ops.json"
        ops = json.loads(ops_path.read_text(encoding="utf-8"))
        ops["expected_repair_hours"]["pump_failure"] = 1e-300
        ops["prices"] = {"oil_per_bbl": 1e-300, "gas_per_mcf": 0}
        raw = json.dumps(ops).encode(); ops_path.write_bytes(raw)
        request = self.evaluate_request(); manifest = request["capture_manifest"]
        digest = hashlib.sha256(raw).hexdigest()
        manifest["source_hashes"]["ops.json"] = digest
        manifest["captured_files"]["ops.json"] = {"sha256": digest, "size_bytes": len(raw)}
        request["dataset_identity"] = manifest["dataset_identity"] = self.contract.dataset_identity(manifest["source_hashes"])
        request["capture_id"] = manifest["capture_id"] = self.contract.capture_identity(manifest)
        plan = request["arguments"]["plan"]; horizon = 1e-300 * 3600; f = self.model.facilities[0]
        plan["repair_deadlines"][0]["repair_elapsed_s"] = horizon
        plan["well_shut_ins"] = [{"facility_id": f.id, "well_id": w.id, "start_elapsed_s": 0, "end_elapsed_s": horizon} for w in f.wells]
        plan["plan_hash"] = self.contract.plan_identity(plan)
        for operation in ("evaluate_plan", "validate_plan"):
            request["operation"] = operation
            request["arguments"] = {"plan": plan}
            if operation == "validate_plan": request["arguments"]["evaluator_method"] = "eog-native-seconds-0.1.1"
            run = self.process(worker.encode_response(request), native=operation == "evaluate_plan")
            self.assertEqual(run.returncode, 0, run.stderr)
            result = self.contract.decode_document(run.stdout)
            self.contract.validate_document(result, context={"request": request})
            self.assertEqual(result["status"], "unavailable", result["diagnostics"])
            self.assertIsNone(result["result"])
            self.assertEqual(result["diagnostics"][0]["code"], "numerical_unavailable")

    def test_actual_native_noaction_and_forged_horizon(self):
        if not self.native:
            self.skipTest("Set BREAKWATER_EOG_NATIVE for actual native process qualification.")
        request = self.evaluate_request(); result = self.call(request, self.native)
        self.assertEqual(result["status"], "completed", result["diagnostics"])
        evaluation = result["result"]["evaluation"]
        self.assertTrue(evaluation["feasible"]); self.assertEqual(evaluation["outcome"], "automatic_shutdown")
        self.assertAlmostEqual(evaluation["cost"]["total"], 5016)
        self.assertEqual(evaluation["plan"], request["arguments"]["plan"])
        request["arguments"]["plan"]["repair_deadlines"][0]["repair_elapsed_s"] += 1
        request["arguments"]["plan"]["plan_hash"] = self.contract.plan_identity(request["arguments"]["plan"])
        bad = self.call(request, self.native)
        self.assertEqual(bad["status"], "completed", bad["diagnostics"])
        self.assertFalse(bad["result"]["evaluation"]["feasible"])
        self.assertIsNone(bad["result"]["evaluation"]["cost"])

    def process(self, raw, native=False):
        args = [sys.executable, str(SCIENCE / "eog_worker.py"), "--contracts-root", str(self.root)]
        if native:
            args += ["--native-library", self.native]
        return subprocess.run(args, input=raw, capture_output=True, timeout=30)

    def test_actual_one_shot_process_stdout_is_one_bound_line(self):
        result = self.process(worker.encode_response(self.request))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, b""); self.assertEqual(result.stdout.count(b"\n"), 1)
        response = self.contract.decode_document(result.stdout)
        self.contract.validate_document(response, context={"request": self.request})
        self.assertEqual(response["status"], "completed")

    def test_actual_native_process_and_full_trip_contract(self):
        if not self.native:
            self.skipTest("Set BREAKWATER_EOG_NATIVE for native process qualification.")
        request = self.evaluate_request(); plan = request["arguments"]["plan"]
        f = self.model.facilities[0]
        plan["truck_jobs"] = [{"job_id": "job-alpha", "truck_id": "truck/opaque", "facility_id": f.id,
            "system_id": f.system_id, "called_elapsed_s": 0, "response_min": 75, "cycle_min": 230,
            "available_again_elapsed_s": 18300, "trips": [{"arrival_elapsed_s": 4500, "volume_bbl": 150, "cost": 905}]}]
        plan["plan_hash"] = self.contract.plan_identity(plan)
        result = self.process(worker.encode_response(request), native=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        response = self.contract.decode_document(result.stdout)
        self.contract.validate_document(response, context={"request": request})
        self.assertEqual(response["status"], "completed", response["diagnostics"])
        evaluation = response["result"]["evaluation"]
        self.assertTrue(evaluation["feasible"]); self.assertEqual(evaluation["cost"]["total"], 905)
        self.assertEqual(evaluation["outcome"], "avoids_shutdown")
        self.assertEqual(evaluation["trajectories"][0]["elapsed_s"].count(4500), 2)
        plan["truck_jobs"][0]["trips"][0]["volume_bbl"] = 149
        plan["plan_hash"] = self.contract.plan_identity(plan)
        invalid = self.call(request, self.native)
        self.assertEqual(invalid["status"], "completed", invalid["diagnostics"])
        self.assertFalse(invalid["result"]["evaluation"]["feasible"])
        self.assertIsNone(invalid["result"]["evaluation"]["cost"])

    def test_actual_process_malformed_oversized_and_unterminated_requests(self):
        for raw in (b"{}\n", b"{broken\n", b" " * (worker.MAX_REQUEST + 1) + b"\n", worker.encode_response(self.request)[:-1]):
            with self.subTest(size=len(raw)):
                result = self.process(raw)
                self.assertEqual(result.returncode, 2); self.assertEqual(result.stdout, b"")
                self.assertLess(len(result.stderr), 200)

    def test_oversized_result_becomes_explicit_unavailable(self):
        original = worker._detection_result
        def large(*args, **kwargs):
            value = original(*args, **kwargs); value["limitations"] += ["x" * worker.MAX_RESPONSE]
            return value
        with patch.object(worker, "_detection_result", large):
            result = self.call()
        self.assertEqual(result["status"], "unavailable"); self.assertIsNone(result["result"])
        self.assertEqual(result["diagnostics"][0]["code"], "complete_result_unavailable")

    def test_contract_pin_rejects_unreviewed_runtime(self):
        with tempfile.TemporaryDirectory() as folder:
            target = Path(folder)
            for name in worker.CONTRACT_HASHES:
                (target / name).write_bytes((self.root / name).read_bytes())
            with (target / "validate.py").open("ab") as stream:
                stream.write(b"# unreviewed\n")
            with self.assertRaises(RuntimeError):
                worker.contract_module(target)


if __name__ == "__main__":
    unittest.main()
