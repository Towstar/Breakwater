"""Offline EOG contracts; these tests do not execute a numerical provider."""
import argparse
import base64
import copy
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("eog_validate", ROOT / "validate.py")
v = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(v)
NATIVE = None


def read(name):
    return json.loads((ROOT / name).read_text(encoding="utf-8"))


def encoded(value):
    return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode()


def rehash_plan(plan):
    plan["plan_hash"] = v.plan_identity(plan)


def rehash_manifest(manifest):
    manifest["dataset_identity"] = v.dataset_identity(manifest["source_hashes"])
    manifest["capture_id"] = v.capture_identity(manifest)


def utc(stamp):
    return datetime.fromtimestamp(stamp, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


class ContractTests(unittest.TestCase):
    def reject(self, document, context=None, code=None):
        with self.assertRaises(v.ContractError) as caught:
            v.validate_document(document, context=context)
        if code:
            self.assertEqual(caught.exception.code, code)

    def test_all_examples(self):
        for path in sorted((ROOT / "examples").glob("*.json")):
            with self.subTest(example=path.name):
                document = v.decode_document(path.read_bytes())
                v.validate_document(document)

    def test_operation_responses_bind_exact_requests(self):
        for operation in ("detect", "evaluate-plan", "search-plans", "validate-plan"):
            with self.subTest(operation=operation):
                request = read("examples/" + operation + "-request.json")
                response = read("examples/" + operation + "-response.json")
                v.validate_document(response, context={"request": request})

    def test_hash_vectors(self):
        vectors = read("fixtures/hash-vectors.json")
        for vector in vectors["dataset_vectors"]:
            self.assertEqual(v.dataset_identity(vector["source_hashes"]), vector["sha256"])
            self.assertEqual(hashlib.sha256(vector["identity_utf8"].encode()).hexdigest(), vector["sha256"])
        for vector in vectors["capture_vectors"]:
            self.assertEqual(v.capture_identity(vector["manifest"]), vector["sha256"])
            self.assertEqual(v.semantic_bytes({k: x for k, x in vector["manifest"].items() if k != "capture_id"}).decode(), vector["canonical_utf8"])
        for vector in vectors["plan_vectors"]:
            self.assertEqual(v.plan_identity(vector["plan"]), vector["sha256"])

    def test_capture_identity_includes_clock_and_context(self):
        manifest = read("examples/capture-manifest.json")
        changed = copy.deepcopy(manifest)
        changed["context"] = "historical_cursor"
        self.assertNotEqual(v.capture_identity(changed), manifest["capture_id"])
        changed["cutoff_unix"] -= 300
        self.assertNotEqual(v.capture_identity(changed), v.capture_identity(manifest))

    def test_input_order_does_not_change_dataset_identity(self):
        hashes = read("examples/capture-manifest.json")["source_hashes"]
        self.assertEqual(v.dataset_identity(hashes), v.dataset_identity(dict(reversed(list(hashes.items())))))

    def test_binary64_identity_projection_is_formatter_independent(self):
        self.assertEqual(v.semantic_bytes({"a": 1, "b": -0.0}),
                         v.semantic_bytes({"b": 0, "a": 1.0}))
        self.assertEqual(v.semantic_bytes({"a": 1}).decode(), '{"a":{"$f64":"3ff0000000000000"}}')

    def test_unchanged_copies_match_original_hashes(self):
        manifest = read("examples/capture-manifest.json")
        manifest["captured_files"]["ops.json"]["sha256"] = "0" * 64
        rehash_manifest(manifest)
        self.reject(manifest, code="source_mismatch")

    def test_healthy_available_result_still_needs_source_evidence(self):
        response = read("examples/detect-response.json")
        response["result"].update(events=[], evidence=[])
        self.reject(response, code="missing_evidence")

    def test_validation_references_are_causal_and_resolved(self):
        response = read("examples/validate-plan-response.json")
        response["result"]["validation"]["evidence_ids"] = ["missing"]
        self.reject(response, code="unknown_evidence")

    def test_manifest_hash_and_size_mismatch(self):
        for key, value in (("dataset_identity", "1"*64), ("capture_id", "1"*64)):
            manifest = read("examples/capture-manifest.json")
            manifest[key] = value
            self.reject(manifest)
        manifest = read("examples/capture-manifest.json")
        manifest["captured_files"]["network.json"]["size_bytes"] = v.MAX_DOCUMENT_BYTES + 1
        rehash_manifest(manifest)
        self.reject(manifest, code="limit_exceeded")

    def test_no_extra_files_or_answer_keys(self):
        manifest = read("examples/capture-manifest.json")
        manifest["captured_files"]["answer_key.json"] = {"sha256": "0"*64, "size_bytes": 10}
        rehash_manifest(manifest)
        self.reject(manifest)

    def test_snapshot_cutoff_and_historical_cursor(self):
        manifest = read("examples/capture-manifest.json")
        manifest["cutoff_unix"] -= 300
        rehash_manifest(manifest)
        self.reject(manifest, code="invalid_cutoff")
        manifest["context"] = "historical_cursor"
        rehash_manifest(manifest)
        v.validate_document(manifest)
        manifest["cutoff_unix"] = manifest["source_now_unix"] + 1
        rehash_manifest(manifest)
        self.reject(manifest, code="invalid_cutoff")

    def test_request_binding_and_absolute_local_directory(self):
        for key, replacement in (("capture_id", "0"*64), ("dataset_identity", "0"*64), ("cutoff_unix", 1)):
            request = read("examples/detect-request.json")
            request[key] = replacement
            self.reject(request, code="binding_mismatch")
        invalid_paths = ["../capture", "\\\\server\\capture", "//server/capture", "C:/bad\x00dir", "C:relative", "\\tmp\\capture"]
        if os.name == "nt":
            invalid_paths.append("/tmp/capture")
        for directory in invalid_paths:
            request = read("examples/detect-request.json")
            request["capture_directory"] = directory
            self.reject(request, code="invalid_path")

    def test_response_request_binding(self):
        request = read("examples/detect-request.json")
        for key, replacement in (("request_id", "wrong"), ("operation", "evaluate_plan"),
                                 ("dataset_identity", "0"*64), ("capture_id", "0"*64), ("cutoff_unix", 1)):
            response = read("examples/unavailable-response.json")
            response[key] = replacement
            self.reject(response, context={"request": request}, code="binding_mismatch")

    def test_invalid_versions_unknown_fields_and_bool_integers(self):
        for key, value in (("eog_version", 2), ("method_version", True), ("unknown", 1)):
            request = read("examples/detect-request.json")
            request[key] = value
            self.reject(request)

    def test_unicode_reference_limit_and_renamed_inputs(self):
        value = read("examples/renamed-detect-response.json")
        v.validate_document(value)
        value["result"]["events"][0]["asset_id"] = "\U0001f6b0" * 128
        v.validate_document(value)
        value["result"]["events"][0]["asset_id"] += "x"
        self.reject(value, code="invalid_id")

    def test_duplicate_ids_and_evidence_references(self):
        value = read("examples/detect-response.json")
        value["result"]["events"].append(copy.deepcopy(value["result"]["events"][0]))
        self.reject(value, code="duplicate_id")
        value = read("examples/detect-response.json")
        value["result"]["events"][0]["evidence_ids"] = ["unknown"]
        self.reject(value, code="unknown_evidence")

    def test_causal_onset_update_end_and_evidence(self):
        for location, key in (("evidence", "timestamp_unix"), ("events", "updated_unix"),
                              ("events", "onset_unix")):
            value = read("examples/detect-response.json")
            value["result"][location][0][key] = value["cutoff_unix"] + 1
            self.reject(value)
        value = read("examples/detect-response.json")
        event = value["result"]["events"][0]
        event.update(state="resolved", end_unix=value["cutoff_unix"], countdown_hours=None)
        v.validate_document(value)
        event["end_unix"] = None
        self.reject(value)

    def test_stale_missing_and_nonfinite_evidence(self):
        value = read("examples/detect-response.json")
        sample = value["result"]["evidence"][0]
        sample["timestamp_unix"] -= 301
        self.reject(value, code="stale_evidence")
        sample["quality"] = "stale"
        v.validate_document(value)
        sample.update(quality="missing", value=None)
        v.validate_document(value)
        sample["quality"] = "good"
        self.reject(value)
        sample["value"] = float("nan")
        self.reject(value, code="nonfinite")

    def test_finite_negative_and_out_of_range_values_remain_evidence(self):
        value = read("examples/detect-response.json")
        for reading in (-3, 130):
            value["result"]["evidence"][0]["value"] = reading
            v.validate_document(value)

    def test_metric_unit_attachment_and_status(self):
        value = read("examples/detect-response.json")
        value["result"]["evidence"][0]["unit"] = "psi"
        self.reject(value, code="unit_mismatch")
        value = read("examples/detect-response.json")
        self.reject(value, context={"assets": {"tank:A": "pump"}}, code="metric_mismatch")
        self.reject(value, context={"assets": {}}, code="unknown_asset")
        sample = value["result"]["evidence"][0]
        sample.update(asset_id="pump:A", metric="status", unit="state", value=2)
        v.validate_document(value, context={"assets": {"pump:A": "pump"}})
        sample["value"] = 3
        self.reject(value, context={"assets": {"pump:A": "pump"}}, code="invalid_state")

    def test_leak_and_degradation_have_no_countdown(self):
        for category in ("pipeline_leak", "pump_degradation"):
            value = read("examples/detect-response.json")
            event = value["result"]["events"][0]
            event.update(category=category, recommended_action="field_inspection", countdown_hours=None)
            v.validate_document(value)
            event["countdown_hours"] = 1
            self.reject(value, code="invalid_countdown")

    def test_ambiguous_or_missing_coverage_never_claims_countdown(self):
        value = read("examples/detect-response.json")
        value["result"]["events"][0]["coverage"] = "ambiguous"
        self.reject(value, code="unavailable_countdown")
        value["result"]["events"][0]["countdown_hours"] = None
        v.validate_document(value)
        value["result"]["coverage"] = "unavailable"
        self.reject(value, code="missing_limitation")

    def test_unavailable_errors_are_not_successful_empty_results(self):
        value = read("examples/unavailable-response.json")
        for status in ("unavailable", "invalid_input", "cancelled", "failed"):
            value["status"] = status
            v.validate_document(value)
        value["diagnostics"] = []
        self.reject(value)
        value["result"] = {}
        self.reject(value)

    def test_plan_hash_retires_numerical_edits(self):
        request = read("examples/evaluate-plan-request.json")
        request["arguments"]["plan"]["truck_jobs"][0]["trips"][0]["volume_bbl"] += 1
        self.reject(request, code="plan_mismatch")
        rehash_plan(request["arguments"]["plan"])
        v.validate_document(request)

    def test_validation_binds_plan_hash_and_distinct_implementation(self):
        request = read("examples/validate-plan-request.json")
        value = read("examples/validate-plan-response.json")
        value["result"]["validation"]["plan_hash"] = "0"*64
        self.reject(value, {"request": request}, "plan_mismatch")
        value = read("examples/validate-plan-response.json")
        value["result"]["validation"]["validator_method"] = value["result"]["validation"]["evaluator_method"]
        self.reject(value, code="not_independent")

    def test_stale_plan_and_evaluation_substitution(self):
        request = read("examples/evaluate-plan-request.json")
        value = read("examples/evaluate-plan-response.json")
        plan = value["result"]["evaluation"]["plan"]
        plan["start_unix"] -= 1
        rehash_plan(plan)
        self.reject(value, code="stale_plan")
        value = read("examples/evaluate-plan-response.json")
        value["result"]["evaluation"]["plan"]["inspection_assets"] = ["pump:A"]
        rehash_plan(value["result"]["evaluation"]["plan"])
        self.reject(value, {"request": request}, "plan_mismatch")

    def test_trajectory_shape_and_discrete_steps(self):
        value = read("examples/evaluate-plan-response.json")
        trajectory = value["result"]["evaluation"]["trajectories"][0]
        v.validate_document(value)
        trajectory["elapsed_s"] = [0, 1800, 1800, 1800, 7200]
        trajectory["volumes_bbl"] = [800, 850, 700, 600, 850]
        self.reject(value, code="trajectory_shape")
        trajectory["elapsed_s"] = [0, 1800, 1800, 7200]
        trajectory["volumes_bbl"] = [800, 850, 700, 850]
        v.validate_document(value)
        trajectory["volumes_bbl"].append(1)
        self.reject(value, code="trajectory_shape")

    def test_overflowing_negative_trajectories_are_retained(self):
        value = read("examples/evaluate-plan-response.json")
        evaluation = value["result"]["evaluation"]
        evaluation["trajectories"][0]["volumes_bbl"] = [800, 1001, -1, 850]
        evaluation["feasible"] = False
        evaluation["violations"] = [{"code": "inventory_violation", "message": "Out of range.", "asset_id": "tank:A", "elapsed_s": 1800}]
        v.validate_document(value)

    def test_evaluation_cost_occupancy_and_feasibility_consistency(self):
        value = read("examples/evaluate-plan-response.json")
        value["result"]["evaluation"]["cost"]["total"] += 1
        self.reject(value, code="cost_mismatch")
        value = read("examples/evaluate-plan-response.json")
        value["result"]["evaluation"]["fleet_occupancy"][0]["end_elapsed_s"] += 1
        self.reject(value, code="occupancy_mismatch")
        value = read("examples/evaluate-plan-response.json")
        value["result"]["evaluation"]["feasible"] = False
        self.reject(value, code="missing_violation")

    def test_search_bounds_selection_and_optimality(self):
        value = read("examples/search-plans-response.json")
        value["result"]["search"]["optimality"] = "proven_within_space"
        self.reject(value, code="invalid_optimality")
        value["result"]["search"].update(scope="enumerated_space", complete=True)
        v.validate_document(value)
        value["result"]["selected_plan_id"] = "missing"
        self.reject(value, code="unknown_plan")
        value = read("examples/search-plans-response.json")
        value["result"]["search"]["evaluated_candidates"] = 10001
        self.reject(value)

    def test_search_selects_lowest_retained_feasible_cost_and_allows_ties(self):
        value = read("examples/search-plans-response.json")
        cheaper = copy.deepcopy(value["result"]["evaluations"][0])
        cheaper["plan"]["plan_id"] = "cheaper"
        rehash_plan(cheaper["plan"])
        cheaper["cost"] = {"truck_cost": 50, "lost_revenue": 0, "total": 50}
        value["result"]["evaluations"].append(cheaper)
        value["result"]["search"]["evaluated_candidates"] = 2
        self.reject(value, code="nonminimal_selection")
        value["result"]["selected_plan_id"] = "cheaper"
        v.validate_document(value)
        cheaper["cost"] = {"truck_cost": 100, "lost_revenue": 0, "total": 100}
        v.validate_document(value)

    def test_json_parser_errors_and_limits(self):
        bad = [b'{"a":1,"a":2}', b'{"a":1,"\\u0061":2}', b'{"v":NaN}',
               b'{"v":Infinity}', b'{"v":1e9999}', b'{"v":"\xff"}',
               b'{"v":"\\ud800"}', b'[]', b'{}{}',
               b'{"v":' + b'[' * 65 + b'0' + b']' * 65 + b'}',
               b' ' * (v.MAX_DOCUMENT_BYTES + 1)]
        for raw in bad:
            with self.subTest(raw=raw[:30]):
                with self.assertRaises(v.ContractError):
                    v.decode_document(raw)
        raw = encoded(read("examples/detect-request.json"))
        v.decode_document(raw + b" " * (v.MAX_REQUEST_BYTES - len(raw)))
        with self.assertRaises(v.ContractError):
            v.decode_document(raw + b" " * (v.MAX_REQUEST_BYTES - len(raw) + 1))

    def test_schema_references_never_require_network(self):
        def inspect(value):
            if isinstance(value, dict):
                for key, item in value.items():
                    if key == "$ref":
                        self.assertFalse("://" in item, item)
                    inspect(item)
            elif isinstance(value, list):
                for item in value:
                    inspect(item)
        for path in ROOT.glob("*.schema.json"):
            inspect(json.loads(path.read_text(encoding="utf-8")))
        with patch("socket.socket", side_effect=AssertionError("Network forbidden")):
            v.validate_document(read("examples/detect-request.json"))

    def capture(self, folder, timestamps=(1000000000, 1000000300)):
        contents = {"network.json": b'{"systems":[]}', "ops.json": b'{}'}
        contents["telemetry.csv"] = ("timestamp,ts_unix,asset_id,metric,value\n" + "".join(
            f"{utc(t)},{t},tank:A,level_pct,-1\n" for t in timestamps)).encode()
        manifest = read("examples/capture-manifest.json")
        manifest.update(source_first_unix=timestamps[0], source_now_unix=timestamps[-1],
                        cutoff_unix=timestamps[-1])
        for name, raw in contents.items():
            (folder / name).write_bytes(raw)
            manifest["captured_files"][name] = {"size_bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
            manifest["source_hashes"][name] = hashlib.sha256(raw).hexdigest()
        rehash_manifest(manifest)
        return manifest

    def test_actual_capture_checks_only_three_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            manifest = self.capture(folder)
            (folder / "answer_key.json").write_text("DO NOT READ", encoding="utf-8")
            v.validate_document(manifest)
            original = Path.open
            opened = []
            def guarded(path, *args, **kwargs):
                opened.append(path.name)
                self.assertNotEqual(path.name, "answer_key.json")
                return original(path, *args, **kwargs)
            with patch.object(Path, "open", guarded):
                report = v.verify_capture(folder, manifest)
            self.assertEqual(opened, list(v.FILENAMES))
            self.assertEqual(report["rows"], 2)

    def test_actual_capture_tamper_and_future_rows(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            manifest = self.capture(folder)
            (folder / "ops.json").write_bytes(b'[]')
            with self.assertRaises(v.ContractError):
                v.verify_capture(folder, manifest)
            manifest = self.capture(folder)
            manifest["context"] = "historical_cursor"
            manifest["cutoff_unix"] -= 300
            rehash_manifest(manifest)
            with self.assertRaises(v.ContractError) as error:
                v.verify_capture(folder, manifest)
            self.assertEqual(error.exception.code, "future_evidence")

    def test_full_capture_requires_its_declared_final_timestamp(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            manifest = self.capture(folder, timestamps=(1000000000,))
            manifest["source_now_unix"] += 300
            manifest["cutoff_unix"] += 300
            rehash_manifest(manifest)
            with self.assertRaises(v.ContractError) as error:
                v.verify_capture(folder, manifest)
            self.assertEqual(error.exception.code, "invalid_time")

    def test_capture_preserves_utf8_bom_hash_and_reader_compatibility(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            manifest = self.capture(folder)
            raw = b"\xef\xbb\xbf" + (folder / "telemetry.csv").read_bytes()
            (folder / "telemetry.csv").write_bytes(raw)
            digest = hashlib.sha256(raw).hexdigest()
            manifest["captured_files"]["telemetry.csv"] = {"size_bytes": len(raw), "sha256": digest}
            manifest["source_hashes"]["telemetry.csv"] = digest
            rehash_manifest(manifest)
            self.assertEqual(v.verify_capture(folder, manifest)["rows"], 2)

    def test_actual_capture_malformed_duplicate_and_bad_clock(self):
        for suffix in ("duplicate", "timestamp", "nonfinite"):
            with self.subTest(case=suffix), tempfile.TemporaryDirectory() as tmp:
                folder = Path(tmp)
                manifest = self.capture(folder)
                raw = (folder / "telemetry.csv").read_bytes()
                if suffix == "duplicate":
                    raw += raw.splitlines(keepends=True)[-1]
                elif suffix == "timestamp":
                    raw = raw.replace(utc(1000000000).encode(), b"2026-01-01T00:00:00Z")
                else:
                    raw = raw.replace(b",-1", b",NaN")
                (folder / "telemetry.csv").write_bytes(raw)
                manifest["captured_files"]["telemetry.csv"] = {"size_bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
                manifest["source_hashes"]["telemetry.csv"] = hashlib.sha256(raw).hexdigest()
                rehash_manifest(manifest)
                with self.assertRaises(v.ContractError):
                    v.verify_capture(folder, manifest)

    def test_available_forecasts_require_all_facilities_and_exact_repair_endpoints(self):
        for change in ("empty", "short", "missing"):
            value = read("examples/evaluate-plan-response.json")
            evaluation = value["result"]["evaluation"]
            if change == "empty":
                evaluation["trajectories"] = []
            elif change == "short":
                evaluation["trajectories"][0].update(elapsed_s=[0], volumes_bbl=[800])
            else:
                evaluation["plan"]["repair_deadlines"].append({"facility_id": "field:B", "repair_elapsed_s": 7200})
                rehash_plan(evaluation["plan"])
            self.reject(value)
        value = read("examples/evaluate-plan-response.json")
        value["result"]["evaluation"]["trajectories"][0]["elapsed_s"][-1] = 7200.25
        self.reject(value, code="forecast_horizon")

    def test_available_validation_covers_exact_requested_horizons(self):
        request = read("examples/validate-plan-request.json")
        value = read("examples/validate-plan-response.json")
        value["result"]["validation"]["validated_horizons"] = []
        self.reject(value, {"request": request}, "forecast_coverage")
        value = read("examples/validate-plan-response.json")
        value["result"]["validation"]["validated_horizons"][0]["repair_elapsed_s"] += 0.25
        self.reject(value, {"request": request}, "forecast_horizon")

    def test_complete_forecast_retains_action_and_shutdown_breakpoints(self):
        value = read("examples/evaluate-plan-response.json")
        trajectory = value["result"]["evaluation"]["trajectories"][0]
        trajectory.update(elapsed_s=[0, 7200], volumes_bbl=[800, 850])
        self.reject(value, code="missing_breakpoint")
        trajectory.update(elapsed_s=[0, 1800, 7200], volumes_bbl=[800, 700, 850])
        self.reject(value, code="missing_breakpoint")
        value = read("examples/well-only-evaluation-response.json")
        value["result"]["evaluation"]["trajectories"][0].update(elapsed_s=[0, 7200], volumes_bbl=[800, 915])
        self.reject(value, code="missing_breakpoint")
        value = read("examples/do-nothing-evaluation-response.json")
        value["result"]["evaluation"]["trajectories"][0].update(elapsed_s=[0, 7200], volumes_bbl=[800, 920])
        self.reject(value, code="missing_breakpoint")

    def test_fractional_counterfactual_clocks_preserve_binary64_without_integer_rounding(self):
        value = read("examples/fractional-crossing-response.json")
        crossing = (1.0 / 7.0) * 3600.0
        self.assertNotEqual(crossing, round(crossing))
        self.assertEqual(value["result"]["evaluation"]["shutdowns"][0]["elapsed_s"], crossing)
        self.assertEqual(value["result"]["evaluation"]["trajectories"][0]["elapsed_s"][1], crossing)
        v.validate_document(value)
        request = read("examples/evaluate-plan-request.json")
        job = request["arguments"]["plan"]["truck_jobs"][0]
        job["trips"][0]["arrival_elapsed_s"] = crossing
        rehash_plan(request["arguments"]["plan"])
        v.validate_document(request)

    def test_do_nothing_automatic_shutdown_is_admissible_and_search_selectable(self):
        response = read("examples/do-nothing-selected-search-response.json")
        v.validate_document(response, context={"request": read("examples/search-plans-request.json")})
        chosen = next(item for item in response["result"]["evaluations"]
                      if item["plan"]["plan_id"] == response["result"]["selected_plan_id"])
        self.assertTrue(chosen["feasible"])
        self.assertEqual(chosen["outcome"], "automatic_shutdown")
        self.assertEqual(chosen["cost"]["lost_revenue"], 80)
        self.assertEqual(chosen["shutdowns"][0]["elapsed_s"], 4320)
        chosen["shutdowns"] = []
        self.reject(response, code="missing_shutdown")

    def test_incomplete_assessment_cannot_be_feasible_or_invent_zero_cost(self):
        value = read("examples/shared-fleet-infeasible-response.json")
        v.validate_document(value)
        evaluation = value["result"]["evaluation"]
        self.assertIsNone(evaluation["cost"])
        evaluation["cost"] = {"truck_cost": 0, "lost_revenue": 0, "total": 0}
        self.reject(value, code="unavailable_cost")
        evaluation["cost"] = None
        evaluation["feasible"] = True
        self.reject(value, code="incomplete_assessment")
        unknown = read("examples/unknown-horizon-response.json")
        self.assertEqual(unknown["status"], "unavailable")
        self.assertIsNone(unknown["result"])
        v.validate_document(unknown)

    def test_search_declares_action_time_resolution(self):
        value = read("examples/search-plans-response.json")
        self.assertIsNone(value["result"]["search"]["action_time_resolution_s"])
        v.validate_document(value)
        value["result"]["search"]["action_time_resolution_s"] = 0.25
        v.validate_document(value)
        value["result"]["search"]["action_time_resolution_s"] = 0
        self.reject(value)

    def test_shared_corpus(self):
        corpus = read("fixtures/validation-cases.json")
        for case in corpus["cases"]:
            with self.subTest(case=case["name"]):
                context = corpus["contexts"].get(case.get("context_id"), {})
                if case["valid"]:
                    v.validate_document(case["document"], case["kind"], context)
                else:
                    self.reject(case["document"], context)

    def test_native_comparator_executes_shared_cases_hashes_and_mutations(self):
        if not NATIVE:
            self.skipTest("Native comparator not supplied; desktop CI requires --native PATH.")
        corpus = read("fixtures/validation-cases.json")
        requests, expected = [], []
        for case in corpus["cases"]:
            requests.append({"kind": case["kind"], "document": case["document"],
                             "context": corpus["contexts"].get(case.get("context_id"), {})})
            expected.append((case["name"], {"valid": case["valid"]}))
        vectors = read("fixtures/hash-vectors.json")
        for item in vectors["dataset_vectors"]:
            requests.append({"kind": "dataset_identity", "source_hashes": item["source_hashes"]})
            expected.append(("dataset-vector", {"valid": True, "sha256": item["sha256"]}))
        for item in vectors["capture_vectors"]:
            requests.append({"kind": "capture_manifest", "document": item["manifest"]})
            expected.append(("capture-vector", {"valid": True, "sha256": item["sha256"], "canonical_utf8": item["canonical_utf8"]}))
        for item in vectors["plan_vectors"]:
            requests.append({"kind": "plan_identity", "plan": item["plan"]})
            expected.append(("plan-vector", {"valid": True, "sha256": item["sha256"], "canonical_utf8": item["canonical_utf8"]}))
        for name, raw in (("duplicate-key", b'{"a":1,"a":2}'),
                          ("escaped-duplicate", b'{"a":1,"\\u0061":2}'),
                          ("nonfinite", b'{"value":1e9999}'),
                          ("invalid-utf8", b'{"value":"\xff"}'),
                          ("unpaired-surrogate", b'{"value":"\\ud800"}')):
            requests.append({"kind": "analysis_response", "raw_base64": base64.b64encode(raw).decode()})
            expected.append((name, {"valid": False}))
        def leaves(value, prefix=()):
            if isinstance(value, dict):
                return [path for key, child in value.items() for path in leaves(child, prefix + (key,))]
            if isinstance(value, list):
                return [path for index, child in enumerate(value) for path in leaves(child, prefix + (index,))]
            return [prefix]
        for operation in ("detect", "evaluate-plan", "search-plans", "validate-plan"):
            original = read("examples/" + operation + "-response.json")
            request = read("examples/" + operation + "-request.json")
            paths = leaves(original)
            for path in paths[::max(1, len(paths)//20)][:20]:
                changed = copy.deepcopy(original)
                parent = changed
                for key in path[:-1]:
                    parent = parent[key]
                parent[path[-1]] = []
                context = {"request": request}
                try:
                    v.validate_document(changed, context=context)
                    valid = True
                except v.ContractError:
                    valid = False
                requests.append({"kind": "analysis_response", "document": changed, "context": context})
                expected.append((operation + ":" + repr(path), {"valid": valid}))
        completed = subprocess.run([NATIVE, "--contract-check"], input=b"".join(encoded(item) + b"\n" for item in requests),
                                   capture_output=True, timeout=120, check=False)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode("utf-8", errors="replace"))
        self.assertEqual(completed.stderr, b"", "Native checker must keep diagnostics out of the protocol.")
        results = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual(len(results), len(expected))
        for (name, fields), result in zip(expected, results):
            with self.subTest(case=name):
                for key, value in fields.items():
                    self.assertEqual(result.get(key), value, result)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", default=os.environ.get("BREAKWATER_EOG_NATIVE"))
    args, remaining = parser.parse_known_args()
    NATIVE = args.native
    unittest.main(argv=[sys.argv[0], *remaining])
