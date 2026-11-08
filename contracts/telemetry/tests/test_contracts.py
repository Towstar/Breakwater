"""Portable G1 regressions; --native makes cross-language agreement mandatory."""
import argparse
import base64
import copy
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("telemetry_validate", ROOT / "validate.py")
v = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(v)
NATIVE = None


def read(name):
    return json.loads((ROOT / name).read_text(encoding="utf-8"))


def fixture_requests():
    corpus = read("fixtures/validation-cases.json")
    for case in corpus["cases"]:
        context = {**corpus["contexts"].get(case.get("context_id"), {}), **case.get("context", {})}
        yield case, {"kind": case["kind"], "document": case["document"], "context": context}


def encoded(value):
    return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")


def raw_requests():
    frame = read("examples/readings.json")
    payload = encoded(frame)
    exact = payload + b" " * (v.MAX_FRAME_BYTES - len(payload) - 1) + b"\n"
    rows = [
        ("duplicate-key", b'{"kind":"stream_frame","kind":"stream_frame"}', False),
        ("escaped-duplicate-key", b'{"kind":"stream_frame","ki\\u006ed":"stream_frame"}', False),
        ("invalid-utf8", b'{"label":"\xff"}', False),
        ("unpaired-surrogate", b'{"label":"\\ud800"}', False),
        ("unpaired-surrogate-key", b'{"\\ud800":0}', False),
        ("nonfinite-nan", b'{"value":NaN}', False),
        ("nonfinite-infinity", b'{"value":Infinity}', False),
        ("overflow-number", b'{"value":1e9999}', False),
        ("nesting-over-limit", b'{"x":' + b'[' * 65 + b'0' + b']' * 65 + b'}', False),
        ("exact-frame-byte-bound", exact, True),
        ("frame-byte-bound-overrun", exact + b' ', False),
    ]
    for name, data, valid in rows:
        yield {"name": name, "valid": valid}, {"kind": "stream_frame", "raw_base64": base64.b64encode(data).decode("ascii")}


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.configuration = read("examples/canonical-sensors.json")

    def assert_invalid(self, document, kind=None, context=None):
        with self.assertRaises(v.ContractError):
            v.validate_document(document, kind, context)

    def test_shared_semantic_fixtures(self):
        for case, request in fixture_requests():
            with self.subTest(case=case["name"]):
                if not case["valid"]:
                    self.assert_invalid(request["document"], request["kind"], request["context"])
                    continue
                v.validate_document(request["document"], request["kind"], request["context"])
                if "disposition" in case:
                    self.assertEqual(v.frame_disposition(request["document"], request["context"]), case["disposition"])
                if "snapshot_disposition" in case:
                    self.assertEqual(v.snapshot_disposition(request["document"], request["context"]), case["snapshot_disposition"])

    def test_published_hash_vectors(self):
        vectors = read("fixtures/hash-vectors.json")["vectors"]
        for vector in vectors:
            with self.subTest(vector=vector["name"]):
                self.assertEqual(v.sensor_semantic_bytes(vector["configuration"]).decode(), vector["semantic_utf8"])
                self.assertEqual(v.sensor_semantic_hash(vector["configuration"]), vector["sha256"])

    def test_identity_ignores_display_labels_document_revision_and_sensor_order(self):
        other = copy.deepcopy(self.configuration)
        other["configuration_id"] = "renamed-document"
        other["revision"] += 1
        other["sensors"][0]["label"] = "Display \"label\" \u00e9 \U0001f30a"
        other["sensors"].reverse()
        self.assertEqual(v.sensor_semantic_hash(other), v.sensor_semantic_hash(self.configuration))

    def test_mapping_and_policy_change_semantic_identity(self):
        before = v.sensor_semantic_hash(self.configuration)
        for path, value in [(("policy", "max_skew_s"), 0.5), (("sensors", 0, "attachment", "id"), "tank_b"),
                            (("sensors", 0, "calibration", "version"), 2), (("sensors", 0, "enabled"), False)]:
            with self.subTest(path=path):
                other = copy.deepcopy(self.configuration)
                target = other
                for key in path[:-1]:
                    target = target[key]
                target[path[-1]] = value
                self.assertNotEqual(v.sensor_semantic_hash(other), before)

    def test_negative_zero_and_numeric_spelling_normalize(self):
        other = copy.deepcopy(self.configuration)
        other["sensors"][0]["physical_support"]["delay_s"] = -0.0
        other["sensors"][0]["expected_period_s"] = 1.0
        self.assertEqual(v.sensor_semantic_hash(other), v.sensor_semantic_hash(self.configuration))

    def test_boolean_is_not_a_numeric_value(self):
        for value in (True, False):
            with self.subTest(value=value):
                other = copy.deepcopy(self.configuration)
                other["sensors"][0]["expected_period_s"] = value
                self.assert_invalid(other)

    def test_sensor_count_limit(self):
        other = copy.deepcopy(self.configuration)
        other["sensors"] = []
        for index in range(256):
            sensor = copy.deepcopy(self.configuration["sensors"][0])
            sensor.update(sensor_id=f"volume-{index}", channel_id=f"volume-{index}", group_id=f"volume-{index}")
            other["sensors"].append(sensor)
        v.validate_document(other)
        extra = copy.deepcopy(other["sensors"][-1])
        extra.update(sensor_id="extra", channel_id="extra", group_id="extra")
        other["sensors"].append(extra)
        self.assert_invalid(other)

    def test_batch_count_limit(self):
        frame = read("examples/readings.json")
        template = frame["payload"]["readings"][0]
        frame["payload"]["readings"] = [{**template, "sensor_id": f"volume-{i}"} for i in range(256)]
        v.validate_document(frame)
        frame["payload"]["readings"].append({**template, "sensor_id": "extra"})
        self.assert_invalid(frame)

    def test_strict_raw_json_and_exact_frame_size(self):
        for case, request in raw_requests():
            with self.subTest(case=case["name"]):
                raw = base64.b64decode(request["raw_base64"])
                if case["valid"]:
                    v.validate_document(v.decode_document(raw), "stream_frame")
                else:
                    with self.assertRaises(v.ContractError):
                        value = v.decode_document(raw)
                        v.validate_document(value, "stream_frame")

    def test_nonfinite_values_are_rejected_even_for_in_memory_callers(self):
        for number in (float("nan"), float("inf"), -float("inf"), 10 ** 400):
            with self.subTest(number_type=type(number).__name__):
                other = copy.deepcopy(self.configuration)
                other["sensors"][0]["uncertainty_bound"] = number
                self.assert_invalid(other)

    def test_invalid_unicode_scalar_is_rejected(self):
        other = copy.deepcopy(self.configuration)
        other["sensors"][0]["label"] = "\ud800"
        self.assert_invalid(other)

    def test_schemas_resolve_without_network(self):
        with patch.object(socket, "create_connection", side_effect=AssertionError("Network use is forbidden")):
            validators = v._validators()
            validators["sensor_configuration"].validate(self.configuration)
        with self.assertRaises(v.NoSuchResource):
            v._local_only("https://example.invalid/schema.json")

    def test_command_cannot_apply_operating_controls(self):
        command = read("examples/pause-command.json")
        command["operation"] = "apply_controls"
        command["payload"] = {"route:main_pump:flow": 44}
        self.assert_invalid(command)

    def test_publisher_cannot_assign_receive_time_or_freshness(self):
        frame = read("examples/readings.json")
        frame["payload"]["readings"][0]["received_utc"] = "2026-11-08T05:00:10Z"
        self.assert_invalid(frame)

    def test_historical_expiry_classification_does_not_mutate_evidence(self):
        snapshot = read("examples/observed-snapshot.json")
        before = encoded(snapshot)
        corpus = read("fixtures/validation-cases.json")
        context = copy.deepcopy(corpus["contexts"]["clock"])
        context["clock"]["now_event_time_s"] = 16
        disposition = v.snapshot_disposition(snapshot, context)
        self.assertEqual(disposition, "expired")
        self.assertEqual(encoded(snapshot), before)
        self.assertFalse(snapshot["actionable"])

    def test_snapshot_semantics_require_registered_context(self):
        snapshot = read("examples/observed-snapshot.json")
        v._validators()["observed_snapshot"].validate(snapshot)
        self.assert_invalid(snapshot)
        corpus = read("fixtures/validation-cases.json")
        for field in ("configuration", "network", "physical_hash"):
            with self.subTest(field=field):
                context = copy.deepcopy(corpus["contexts"]["clock"])
                del context[field]
                self.assert_invalid(snapshot, context=context)
                self.assertEqual(v.snapshot_disposition(snapshot, context), "unavailable")

    def test_malformed_receiver_clocks_raise_contract_error(self):
        snapshot = read("examples/observed-snapshot.json")
        context = read("fixtures/validation-cases.json")["contexts"]["clock"]
        mutations = [None, False, [], {}, {**context["clock"], "now_event_time_s": False},
                     {**context["clock"], "now_event_time_s": -1}, {**context["clock"], "now_event_time_s": "10"},
                     {**context["clock"], "mode": "live"}, {**context["clock"], "stream_id": "plant-1\n"}]
        for clock in mutations:
            with self.subTest(clock=clock):
                altered = {**context, "clock": clock}
                self.assert_invalid(snapshot, context=altered)
                with self.assertRaises(v.ContractError):
                    v.snapshot_disposition(snapshot, altered)

    def test_disabled_primary_is_not_replaced_implicitly(self):
        other = copy.deepcopy(self.configuration)
        other["sensors"][0]["enabled"] = False
        redundant = copy.deepcopy(other["sensors"][0])
        redundant.update(sensor_id="volume-redundant", channel_id="volume-redundant", enabled=True, role="redundant")
        other["sensors"].append(redundant)
        v.validate_document(other)
        self.assertEqual(other["sensors"][0]["role"], "primary")
        self.assertEqual(other["sensors"][-1]["role"], "redundant")

    def test_native_differential_fixtures_and_hashes(self):
        if not NATIVE:
            self.skipTest("Native comparator was not supplied; CI must pass --native PATH.")
        rows = list(fixture_requests()) + list(raw_requests())
        requests = [request for _, request in rows]
        vectors = read("fixtures/hash-vectors.json")["vectors"]
        requests += [{"kind": "sensor_configuration", "document": vector["configuration"]} for vector in vectors]
        completed = subprocess.run([NATIVE], input=b"".join(encoded(request) + b"\n" for request in requests),
                                   capture_output=True, timeout=120, check=False)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode("utf-8", errors="replace"))
        self.assertEqual(completed.stderr, b"", "Native comparator must not mix diagnostics into contract output.")
        results = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual(len(results), len(requests))
        for (case, request), result in zip(rows, results):
            with self.subTest(case=case["name"]):
                self.assertEqual(result.get("valid"), case["valid"], result)
                for field in ("disposition", "snapshot_disposition"):
                    if field in case:
                        self.assertEqual(result.get(field), case[field], result)
        for vector, result in zip(vectors, results[len(rows):]):
            with self.subTest(vector=vector["name"]):
                self.assertTrue(result.get("valid"), result)
                self.assertEqual(result.get("semantic_utf8"), vector["semantic_utf8"])
                self.assertEqual(result.get("sha256"), vector["sha256"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", type=str, default=os.environ.get("BREAKWATER_TELEMETRY_NATIVE"))
    args, remaining = parser.parse_known_args()
    NATIVE = args.native
    unittest.main(argv=[sys.argv[0], *remaining])
