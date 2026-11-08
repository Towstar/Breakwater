"""Offline EOG contracts; these tests do not execute a numerical provider."""
import copy
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("eog_validate", ROOT / "validate.py")
v = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(v)


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
        for directory in ("../capture", "\\\\server\\capture", "//server/capture", "C:/bad\x00dir"):
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
        value["result"]["events"][0]["asset_id"] = "🚰" * 128
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
        trajectory["times_unix"] = [value["cutoff_unix"]] * 3
        trajectory["volumes_bbl"] = [800, 650, 500]
        self.reject(value, code="trajectory_shape")
        trajectory["times_unix"] = [value["cutoff_unix"]] * 2
        trajectory["volumes_bbl"] = [800, 650]
        v.validate_document(value)
        trajectory["volumes_bbl"].append(1)
        self.reject(value, code="trajectory_shape")

    def test_overflowing_negative_trajectories_are_retained(self):
        value = read("examples/evaluate-plan-response.json")
        evaluation = value["result"]["evaluation"]
        evaluation["trajectories"][0]["volumes_bbl"] = [800, 1001, -1, 850]
        evaluation["feasible"] = False
        evaluation["violations"] = [{"code": "inventory_violation", "message": "Out of range.", "asset_id": "tank:A", "time_unix": 1000005400}]
        v.validate_document(value)

    def test_evaluation_cost_occupancy_and_feasibility_consistency(self):
        value = read("examples/evaluate-plan-response.json")
        value["result"]["evaluation"]["cost"]["total"] += 1
        self.reject(value, code="cost_mismatch")
        value = read("examples/evaluate-plan-response.json")
        value["result"]["evaluation"]["fleet_occupancy"][0]["end_unix"] += 1
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

    def test_shared_corpus(self):
        corpus = read("fixtures/validation-cases.json")
        for case in corpus["cases"]:
            with self.subTest(case=case["name"]):
                context = corpus["contexts"].get(case.get("context_id"), {})
                if case["valid"]:
                    v.validate_document(case["document"], case["kind"], context)
                else:
                    self.reject(case["document"], context)


if __name__ == "__main__":
    unittest.main()
