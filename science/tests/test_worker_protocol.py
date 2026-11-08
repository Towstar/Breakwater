"""Actual NDJSON process checks, separate from the mapper's numerical unit tests."""
from __future__ import annotations

import copy
import importlib.util
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SCIENCE = Path(__file__).resolve().parents[1]
ROOT = SCIENCE.parent
WORKER = SCIENCE / "worker.py"
MAX_LINE = 1024 * 1024


def request(method="health", request_id="health:one", payload=None):
    return {"protocol_version": 1, "request_id": request_id, "method": method,
            "payload": {} if payload is None else payload}


def wire_request(request_id="storage:one"):
    value = json.loads((ROOT / "contracts/science/examples/storage-rollout-request.json").read_text(encoding="utf-8"))
    value["request_id"] = request_id
    return value


def encoded(value):
    return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8") + b"\n"


def methods(response):
    return {item["name"]: item for item in response["result"]["methods"]}


class ProcessChecks(unittest.TestCase):
    def process(self, data, *, flags=(), worker=WORKER, startup=None, timeout=45):
        command = [sys.executable, *flags, "-u"]
        command += ["-c", startup] if startup is not None else [str(worker)]
        completed = subprocess.run(command, input=data, capture_output=True, timeout=timeout,
                                   cwd=SCIENCE, check=False)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode("utf-8", errors="replace"))
        self.assertEqual(completed.stderr, b"", "Protocol diagnostics leaked to stderr")
        self.assertFalse(completed.stdout and not completed.stdout.endswith(b"\n"), "Missing response newline")
        lines = completed.stdout.splitlines(keepends=True)
        for line in lines:
            self.assertLessEqual(len(line), MAX_LINE)
            self.assertTrue(line.endswith(b"\n"))
        responses = [json.loads(line.decode("utf-8")) for line in lines]
        for value in responses:
            self.assertIsInstance(value, dict)
            self.assertEqual(value["protocol_version"], 1)
        return responses

    def exchange(self, values, **kwargs):
        responses = self.process(b"".join(encoded(value) for value in values), **kwargs)
        self.assertEqual(len(responses), len(values))
        return responses

    def assert_error(self, response, code, identity):
        self.assertEqual(response["status"], "error", response)
        self.assertEqual(response["request_id"], identity)
        self.assertEqual(response["error"]["code"], code, response)
        self.assertLessEqual(len(response["error"]["message"]), 2000)
        self.assertNotIn("result", response)


class FramingTests(ProcessChecks):
    def test_health_works_without_site_packages_and_keeps_legacy_shape(self):
        legacy = {"protocol_version": 1, "request_id": "legacy:health", "method": "health"}
        response, = self.exchange([legacy], flags=("-S",))
        self.assertEqual(response["request_id"], "legacy:health")
        self.assertEqual(response["status"], "completed")
        result = response["result"]
        self.assertEqual(set(result), {"python_version", "methods", "torch_detected", "physicsnemo_detected", "scientific_execution"})
        self.assertEqual(result["methods"], ["health"])
        self.assertEqual(result["scientific_execution"], "not_implemented")
        self.assertFalse(result["torch_detected"])
        self.assertFalse(result["physicsnemo_detected"])

    def test_installed_single_file_still_supports_health_and_honest_capabilities(self):
        with tempfile.TemporaryDirectory(prefix="breakwater-worker-") as directory:
            installed = Path(directory) / "worker.py"
            shutil.copyfile(WORKER, installed)
            health, caps, storage = self.exchange([
                request(), request("capabilities", "caps", {"capability_version": 1}), wire_request("solo")],
                flags=("-S",), worker=installed)
        self.assertEqual(health["status"], "completed")
        self.assertFalse(methods(caps)["storage_rollout"]["implemented"])
        self.assertFalse(methods(caps)["storage_rollout"]["execution_verified"])
        self.assert_error(storage, "runtime_unavailable", "solo")

    def test_bad_json_duplicates_nonfinite_and_utf8_resynchronize_at_next_line(self):
        invalid = [b"not-json\n", b"[]\n", b"null\n", b"\xff\n",
                   b'{"protocol_version":1,"request_id":"a","request_id":"b","method":"health"}\n',
                   b'{"protocol_version":1,"request_id":"n","method":"health","payload":{"x":NaN}}\n',
                   b'{"protocol_version":1,"request_id":"n","method":"health","payload":{"x":Infinity}}\n',
                   b'{"protocol_version":1,"request_id":"n","method":"health","payload":{"x":0,"x":1}}\n']
        responses = self.process(b"".join(invalid) + encoded(request(request_id="after:errors")), flags=("-S",))
        self.assertEqual(len(responses), len(invalid) + 1)
        for response in responses[:-1]:
            self.assert_error(response, "invalid_request", None)
        self.assertEqual(responses[-1]["request_id"], "after:errors")
        self.assertEqual(responses[-1]["status"], "completed")

    def test_semantic_errors_preserve_exact_valid_request_identity(self):
        values = []
        for version in (True, False, 2, "1"):
            value = request(request_id=f"version:{version!r}")
            value["protocol_version"] = version
            values.append(value)
        values += [request("unknown", "method:unknown"), request("pipe_transport", "reserved", {})]
        responses = self.exchange(values, flags=("-S",))
        for value, response in zip(values[:4], responses[:4]):
            self.assert_error(response, "unsupported_version", value["request_id"])
        self.assert_error(responses[-2], "unsupported_method", "method:unknown")
        self.assert_error(responses[-1], "not_implemented", "reserved")

    def test_numeric_version_and_representability_errors_match_contract_semantics(self):
        numeric = request(request_id="numeric:version")
        numeric["protocol_version"] = 1.0
        numeric_caps = request("capabilities", "numeric:caps", {"capability_version": 1.0})
        overflow = b'{"protocol_version":1,"request_id":"finite:id","method":"health","payload":{"x":1e309}}\n'
        bigint = b'{"protocol_version":1,"request_id":"large:int","method":"health","payload":{"x":' + b"9" * 1000 + b'}}\n'
        responses = self.process(encoded(numeric) + encoded(numeric_caps) + overflow + bigint, flags=("-S",))
        self.assertEqual([value["status"] for value in responses[:2]], ["completed", "completed"])
        self.assert_error(responses[2], "invalid_request", "finite:id")
        self.assert_error(responses[3], "invalid_request", "large:int")

    def test_invalid_id_and_unknown_envelope_fields_fail_explicitly(self):
        values = []
        for identifier in (None, 1, "", "x" * 129):
            values.append(request(request_id=identifier))
        extra = request(request_id="keep:this:ID")
        extra["unexpected"] = True
        values.append(extra)
        responses = self.exchange(values, flags=("-S",))
        for response in responses[:-1]:
            self.assert_error(response, "invalid_request", None)
        self.assert_error(responses[-1], "invalid_request", "keep:this:ID")

    def test_exact_line_budget_is_accepted_and_oversize_stops_without_resync(self):
        base = encoded(request(request_id="limit"))[:-1]
        exact = base + b" " * (MAX_LINE - len(base) - 1) + b"\n"
        response, = self.process(exact, flags=("-S",))
        self.assertEqual(response["status"], "completed")
        oversized = base + b" " * (MAX_LINE - len(base)) + b"\n"
        responses = self.process(oversized + encoded(request(request_id="unread")), flags=("-S",))
        self.assertEqual(len(responses), 1)
        self.assert_error(responses[0], "invalid_request", None)

    def test_unterminated_frame_and_empty_stream(self):
        response, = self.process(encoded(request())[:-1], flags=("-S",))
        self.assert_error(response, "invalid_request", None)
        self.assertEqual(self.process(b"", flags=("-S",)), [])

    def test_missing_torch_does_not_turn_detection_into_verified_execution(self):
        caps = request("capabilities", "caps:before", {"capability_version": 1})
        after = copy.deepcopy(caps)
        after["request_id"] = "caps:after"
        before, failed, result = self.exchange([caps, wire_request("missing:torch"), after], flags=("-S",))
        self.assertTrue(methods(before)["storage_rollout"]["implemented"])
        self.assertFalse(methods(before)["storage_rollout"]["execution_verified"])
        self.assert_error(failed, "runtime_unavailable", "missing:torch")
        self.assertFalse(methods(result)["storage_rollout"]["execution_verified"])


@unittest.skipUnless(importlib.util.find_spec("torch") is not None, "PyTorch is required for actual worker science execution")
class ScientificProcessTests(ProcessChecks):
    def test_requested_horizon_grid_regressions_and_selected_derivative(self):
        values = []
        grids = [
            (.9000000000000001, .1, [0.0, *[index * .1 for index in range(1, 10)], .9000000000000001]),
            (.30000000000000004, .1, [0.0, .1, .2, .30000000000000004]),
            (math.ulp(0.0), 1e308, [0.0, math.ulp(0.0)]),
        ]
        for index, (horizon, step, _) in enumerate(grids):
            value = wire_request(f"grid:{index}")
            value["payload"].update(horizon_h=horizon, step_h=step)
            values.append(value)
        responses = self.exchange(values)
        for response, (horizon, _, expected) in zip(responses, grids):
            self.assertEqual(response["status"], "completed", response)
            output = response["result"]
            self.assertEqual(output["time_h"], expected)
            self.assertEqual(output["time_indices"], list(range(len(expected))))
            self.assertEqual(output["derivatives"]["terminal_volume_jacobian_h"], [[-horizon]])

    def test_one_ulp_terminal_tail_counts_toward_work_limit(self):
        valid = wire_request("grid:10000")
        valid["payload"].update(horizon_h=1.0, step_h=.0001)
        invalid = copy.deepcopy(valid)
        invalid["request_id"] = "grid:10001"
        invalid["payload"]["horizon_h"] = math.nextafter(1.0, math.inf)
        accepted, rejected = self.exchange([valid, invalid])
        self.assertEqual(accepted["status"], "completed", accepted)
        output = accepted["result"]
        self.assertEqual(output["time_h"][-1], 1.0)
        self.assertEqual(output["time_indices"][-1], 10000)
        self.assert_error(rejected, "work_limit", "grid:10001")

    def test_publication_rejects_mapper_that_omits_one_ulp_terminal_interval(self):
        startup = f"""import sys
sys.path.insert(0, {str(SCIENCE)!r})
import worker, worker_storage
original = worker_storage.storage_rollout
def shortened(payload):
    result = original(payload)
    result['time_h'].pop()
    result['time_indices'].pop()
    for row in result['volumes_m3']:
        row.pop()
    return result
worker_storage.storage_rollout = shortened
worker.main()
"""
        value = wire_request("grid:shortened")
        value["payload"].update(horizon_h=.9000000000000001, step_h=.1)
        before, failed, after, health = self.exchange([
            request("capabilities", "before", {"capability_version": 1}), value,
            request("capabilities", "after", {"capability_version": 1}), request()], startup=startup)
        self.assertFalse(methods(before)["storage_rollout"]["execution_verified"])
        self.assert_error(failed, "numerical_failure", "grid:shortened")
        self.assertFalse(methods(after)["storage_rollout"]["execution_verified"])
        self.assertEqual(health["status"], "completed")

    def test_capability_verification_requires_success_and_is_session_local(self):
        caps = request("capabilities", "caps:0", {"capability_version": 1})
        invalid = wire_request("bad:control")
        invalid["payload"]["controls"][0]["control_id"] = "fabricated:control"
        values = [caps, invalid, request("capabilities", "caps:1", {"capability_version": 1}),
                  wire_request("executed"), request("capabilities", "caps:2", {"capability_version": 1}),
                  invalid, request("capabilities", "caps:3", {"capability_version": 1})]
        results = self.exchange(values)
        self.assertFalse(methods(results[0])["storage_rollout"]["execution_verified"])
        self.assert_error(results[1], "invalid_controls", "bad:control")
        self.assertFalse(methods(results[2])["storage_rollout"]["execution_verified"])
        self.assertEqual(results[3]["status"], "completed")
        self.assertTrue(methods(results[4])["storage_rollout"]["execution_verified"])
        self.assertFalse(methods(results[4])["pipe_transport"]["implemented"])
        self.assertTrue(methods(results[6])["storage_rollout"]["execution_verified"])
        fresh, = self.exchange([caps])
        self.assertFalse(methods(fresh)["storage_rollout"]["execution_verified"])

    def test_partial_overrides_are_isolated_and_zero_controls_have_empty_derivatives(self):
        values = []
        for index, flow in enumerate((2, None, 3, None)):
            value = wire_request(f"isolation:{index}")
            value["payload"]["controls"] = [] if flow is None else [{"control_id": "route:west-transfer:flow", "value": flow}]
            values.append(value)
        results = self.exchange(values)
        for result, final in zip(results, (48, 49, 47, 49)):
            self.assertEqual(result["status"], "completed", result)
            self.assertEqual(result["result"]["volumes_m3"][0][-1], final)
            self.assertNotIn("feasible", result["result"])
            self.assertNotIn("artifact", result["result"])
        empty = results[1]["result"]["derivatives"]
        self.assertEqual(empty, {"control_ids": [], "terminal_volume_jacobian_h": [[]], "cost_gradient": []})

    def test_selected_control_order_unicode_ids_and_derivative_units(self):
        value = wire_request("選択:order")
        payload = value["payload"]
        payload["network"]["routes"][0]["id"] = "pump:α:one"
        payload["network"]["routes"][0]["cost_per_m3"] = 2
        second = copy.deepcopy(payload["network"]["routes"][0])
        second["id"] = "pump:β:two"
        second["cost_per_m3"] = 5
        payload["network"]["routes"].append(second)
        payload["controls"] = [{"control_id": "route:pump:β:two:flow", "value": 3},
                               {"control_id": "route:pump:α:one:flow", "value": 2}]
        payload["horizon_h"], payload["step_h"] = .7, .2
        result, = self.exchange([value])
        self.assertEqual(result["request_id"], value["request_id"])
        self.assertEqual(result["status"], "completed", result)
        output = result["result"]
        self.assertEqual(output["identity"], payload["identity"])
        self.assertEqual(output["tank_ids"], ["west"])
        self.assertAlmostEqual(output["volumes_m3"][0][-1], 46.5)
        self.assertAlmostEqual(output["cost"], 13.3)
        self.assertEqual(output["derivatives"]["control_ids"], [c["control_id"] for c in payload["controls"]])
        for value in output["derivatives"]["terminal_volume_jacobian_h"][0]:
            self.assertAlmostEqual(value, -.7)
        for actual, expected in zip(output["derivatives"]["cost_gradient"], (3.5, 1.4)):
            self.assertAlmostEqual(actual, expected)
        self.assertEqual(output["provenance"]["dtype"], "float64")
        self.assertEqual(output["provenance"]["device"], "cpu")

    def test_preview_keeps_original_indices_endpoints_and_does_not_clip_predictions(self):
        value = wire_request("preview")
        payload = value["payload"]
        payload["network"]["assets"][0]["initial_m3"] = .1
        payload["step_h"] = .007
        payload["derivatives"] = False
        payload["preview"]["max_times"] = 7
        response, = self.exchange([value])
        self.assertEqual(response["status"], "completed", response)
        result = response["result"]
        indices = result["time_indices"]
        self.assertEqual(len(indices), 7)
        self.assertEqual(indices, sorted(set(indices)))
        self.assertEqual((indices[0], indices[-1]), (0, math.ceil(1 / .007)))
        self.assertEqual((result["time_h"][0], result["time_h"][-1]), (0, 1))
        for index, time, volume in zip(indices, result["time_h"], result["volumes_m3"][0]):
            self.assertAlmostEqual(time, min(index * .007, 1))
            self.assertAlmostEqual(volume, .1 - time)
        self.assertLess(result["volumes_m3"][0][-1], 0)
        self.assertNotIn("derivatives", result)

    def test_invalid_network_artifact_and_boundary_derivatives_are_explicit(self):
        draft = wire_request("invalid:network")
        draft["payload"]["network"]["routes"][0]["target_id"] = "deleted:plant"
        artifact = wire_request("artifact")
        artifact["payload"]["artifact"] = {"relative_path": "../escape.json"}
        boundary = wire_request("boundary")
        boundary["payload"]["controls"][0]["value"] = 0
        responses = self.exchange([draft, artifact, boundary])
        self.assert_error(responses[0], "invalid_network", "invalid:network")
        # Artifacts are not part of this request schema; reject the extra field.
        self.assert_error(responses[1], "invalid_request", "artifact")
        self.assertEqual(responses[2]["status"], "error")
        self.assertEqual(responses[2]["request_id"], "boundary")
        self.assertIn(responses[2]["error"]["code"], {"invalid_controls", "invalid_parameters"})

    def test_output_budget_failure_never_marks_execution_verified(self):
        startup = f"import sys; sys.path.insert(0, {str(SCIENCE)!r}); import worker; worker.MAX_LINE=1024; worker.main()"
        caps = request("capabilities", "caps:before", {"capability_version": 1})
        after = copy.deepcopy(caps)
        after["request_id"] = "caps:after"
        before, failed, final = self.exchange([caps, wire_request("too:large"), after], startup=startup)
        self.assertFalse(methods(before)["storage_rollout"]["execution_verified"])
        self.assert_error(failed, "output_too_large", "too:large")
        self.assertFalse(methods(final)["storage_rollout"]["execution_verified"])
        self.assertEqual(final["result"]["max_line_bytes"], 1024)

    def test_output_budget_reduces_only_preview_and_preserves_derivatives(self):
        startup = f"import sys; sys.path.insert(0, {str(SCIENCE)!r}); import worker; worker.MAX_LINE=1400; worker.main()"
        value = wire_request("fit:preview")
        value["payload"]["step_h"] = .01
        value["payload"]["preview"]["max_times"] = 100
        response, = self.exchange([value], startup=startup)
        self.assertEqual(response["status"], "completed", response)
        result = response["result"]
        self.assertLess(len(result["time_h"]), 100)
        self.assertEqual((result["time_indices"][0], result["time_indices"][-1]), (0, 100))
        self.assertEqual((result["time_h"][0], result["time_h"][-1]), (0, 1))
        self.assertEqual(result["derivatives"]["control_ids"], ["route:west-transfer:flow"])
        self.assertEqual(result["derivatives"]["terminal_volume_jacobian_h"], [[-1.0]])
        self.assertEqual(result["derivatives"]["cost_gradient"], [0.0])


if __name__ == "__main__":
    unittest.main()
