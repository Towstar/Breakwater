"""Published worker mapping, independent control-domain and derivative checks."""
import copy
import math
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import worker_storage
from worker_storage import StorageError, storage_rollout


def wire_network():
    assets = [
        {"id": "a:one", "label": "West", "kind": "tank", "capacity_m3": 200, "initial_m3": 80},
        {"id": "b:two", "label": "East", "kind": "tank", "capacity_m3": 200, "initial_m3": 30},
        {"id": "prod:west", "label": "Source", "kind": "source", "inflow_m3h": 20,
         "curtailment_m3h": 3, "curtailment_max_m3h": 10, "cost_per_m3": 7},
        {"id": "shared:plant", "label": "Treatment", "kind": "treatment", "capacity_m3h": 50, "cost_per_m3": 5},
        {"id": "truck", "label": "Truck", "kind": "trucking", "capacity_m3h": 10, "cost_per_m3": 11},
    ]
    specifications = [
        ("feed:west", "prod:west", "a:one", 20, 20, 2, False, True),
        ("transfer:ab", "a:one", "b:two", 12, 4, 1.5, False, True),
        ("treat:a", "a:one", "shared:plant", 15, 6, 3, False, True),
        ("treat:b", "b:two", "shared:plant", 20, 5, 4, False, True),
        ("haul", "a:one", "truck", 10, 1, 6, False, True),
        ("failed:pump", "a:one", "shared:plant", 30, 0, 0, True, False),
    ]
    routes = [dict(zip(("id", "source_id", "target_id", "capacity_m3h", "setpoint_m3h",
                       "cost_per_m3", "has_pump", "available"), values)) for values in specifications]
    return {"assets": assets, "routes": routes}


def payload(*, derivatives=False):
    return {"method_version": 1,
        "identity": {"snapshot_id": "snapshot:β", "volume_run_id": "native:captured",
            "numerical_revision": 7, "physical_hash": "a" * 64},
        "network": wire_network(), "controls": [], "horizon_h": 3.0, "step_h": .025,
        "derivatives": derivatives, "preview": {"max_times": 256}}


def private_network(wire):
    # Deliberately independent conversion used only by the AD reference in tests.
    normalized = copy.deepcopy(wire)
    for asset in normalized["assets"]:
        if asset["kind"] in ("treatment", "trucking"):
            asset["capacity_m3"] = asset.pop("capacity_m3h")
    return normalized


class ValidationTests(unittest.TestCase):
    def assertRejectedBeforeRuntime(self, request, code):
        with patch.object(worker_storage, "_load_backend") as loader:
            with self.assertRaises(StorageError) as caught:
                storage_rollout(request)
            self.assertEqual(caught.exception.code, code)
            self.assertLessEqual(len(caught.exception.message), 2000)
            loader.assert_not_called()

    def test_payload_and_identity_are_strict_and_opaque(self):
        for key, value, code in (("method_version", 3, "unsupported_version"),
                                 ("method_version", True, "invalid_request"),
                                 ("derivatives", 1, "invalid_request"),
                                 ("undocumented", 1, "invalid_request")):
            request = payload(); request[key] = value
            self.assertRejectedBeforeRuntime(request, code)
        for key, value in (("physical_hash", "A" * 64), ("numerical_revision", True),
                           ("numerical_revision", 2**53), ("numerical_revision", .5),
                           ("snapshot_id", ""), ("snapshot_id", "\ud800"), ("unknown", 1)):
            request = payload(); request["identity"][key] = value
            self.assertRejectedBeforeRuntime(request, "invalid_request")

    def test_horizon_preview_and_work_bounds(self):
        for key, value, code in (("horizon_h", 0, "invalid_parameters"),
                ("horizon_h", 25, "invalid_parameters"), ("step_h", 0, "invalid_parameters"),
                ("step_h", True, "invalid_parameters"), ("step_h", float("nan"), "invalid_parameters"),
                ("step_h", 1e-320, "work_limit")):
            request = payload(); request[key] = value
            self.assertRejectedBeforeRuntime(request, code)
        request = payload(); request["horizon_h"] = math.nextafter(1.0, math.inf); request["step_h"] = .0001
        self.assertRejectedBeforeRuntime(request, "work_limit")
        for preview in ({"max_times": 1}, {"max_times": True}, {"max_times": 257}, {"max_times": 2, "extra": 0}):
            request = payload(); request["preview"] = preview
            self.assertRejectedBeforeRuntime(request, "invalid_parameters")
        request = payload(); request["network"]["assets"] *= 26
        self.assertRejectedBeforeRuntime(request, "work_limit")
        request = payload(); request["controls"] = [{}] * 385
        self.assertRejectedBeforeRuntime(request, "work_limit")

    def test_project_units_and_draft_checks_precede_overrides(self):
        request = payload(); plant = request["network"]["assets"][3]
        plant["capacity_m3"] = plant.pop("capacity_m3h")
        self.assertRejectedBeforeRuntime(request, "invalid_network")

        mutations = [lambda n: n["assets"][0].update(initial_m3=201),
            lambda n: n["assets"][0].update(capacity_m3=True),
            lambda n: n["assets"][2].update(curtailment_max_m3h=21),
            lambda n: n["assets"][0].update(id="truck"),
            lambda n: n["assets"][0].update(label="😀" * 129),
            lambda n: n["routes"][0].update(target_id="absent"),
            lambda n: n["routes"][1].update(target_id="a:one"),
            lambda n: n["routes"][0].update(has_pump=True),
            lambda n: n["routes"][0].update(capacity_m3h=16),
            lambda n: n["routes"][1].pop("available"),
            lambda n: n["routes"][1].update(has_pump=0),
            lambda n: n["routes"][5].update(setpoint_m3h=1)]
        for mutation in mutations:
            request = payload(); mutation(request["network"])
            # Even an override that could repair the draft must not bypass it.
            request["controls"] = [{"control_id": "source:prod:west:curtail", "value": 10}]
            self.assertRejectedBeforeRuntime(request, "invalid_network")
        request = payload(); request["network"]["routes"].pop(0)
        self.assertRejectedBeforeRuntime(request, "invalid_network")

    def test_combined_control_price_overflow_fails_before_runtime(self):
        for kind in ("treatment", "trucking"):
            for flow in (0.0, .25):
                with self.subTest(kind=kind, flow=flow):
                    request = payload()
                    request["network"] = {"assets": [
                        {"id": "tank", "label": "Tank", "kind": "tank", "capacity_m3": 100, "initial_m3": 50},
                        {"id": "sink", "label": "Sink", "kind": kind, "capacity_m3h": 10,
                         "cost_per_m3": sys.float_info.max}],
                        "routes": [{"id": "out", "source_id": "tank", "target_id": "sink",
                            "capacity_m3h": 10, "setpoint_m3h": flow, "cost_per_m3": sys.float_info.max,
                            "has_pump": False, "available": True}]}
                    self.assertRejectedBeforeRuntime(request, "invalid_network")

    def test_explicit_controls_are_exact_catalog_entries_and_not_clipped(self):
        for identifier, value in (("unknown", 1), ("route:failed:pump:flow", 1),
                ("route:transfer:ab:flow", -1), ("route:transfer:ab:flow", 13),
                ("source:prod:west:curtail", float("inf")), ("route:transfer:ab:flow", True)):
            request = payload(); request["controls"] = [{"control_id": identifier, "value": value}]
            self.assertRejectedBeforeRuntime(request, "invalid_controls")
        request = payload(); request["controls"] = [{"control_id": "route:haul:flow", "value": 1}] * 2
        self.assertRejectedBeforeRuntime(request, "invalid_controls")
        request = payload(); request["network"]["routes"][0]["capacity_m3h"] = 17
        request["controls"] = [{"control_id": "source:prod:west:curtail", "value": 2}]
        self.assertRejectedBeforeRuntime(request, "invalid_controls")

    def test_derivative_domain_excludes_fixed_bounds_and_feed_capacity_boundary(self):
        for identifier, value in (("route:transfer:ab:flow", 0), ("route:transfer:ab:flow", 12),
                                  ("route:failed:pump:flow", 0), ("source:prod:west:curtail", 10)):
            request = payload(derivatives=True); request["controls"] = [{"control_id": identifier, "value": value}]
            self.assertRejectedBeforeRuntime(request, "invalid_controls")
        request = payload(derivatives=True); request["network"]["routes"][0]["capacity_m3h"] = 17
        request["controls"] = [{"control_id": "source:prod:west:curtail", "value": 3}]
        self.assertRejectedBeforeRuntime(request, "invalid_controls")

    def test_valid_optional_project_defaults_and_unknown_properties_reach_runtime(self):
        request = payload()
        request["network"]["assets"][0]["additional_project_note"] = {"value": "ignored like desktop"}
        request["network"]["routes"][0]["additional_project_note"] = 1
        with patch.object(worker_storage, "_load_backend", side_effect=StorageError("runtime_unavailable", "No runtime")) as loader:
            with self.assertRaises(StorageError) as caught:
                storage_rollout(request)
            self.assertEqual(caught.exception.code, "runtime_unavailable")
            loader.assert_called_once()


class StorageExecutionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            cls.torch, cls.StorageModel, _ = worker_storage._load_backend()
        except StorageError as error:
            raise unittest.SkipTest(error.message)

    def assertNear(self, actual, expected, atol=1e-10, rtol=1e-10):
        self.torch.testing.assert_close(self.torch.tensor(actual, dtype=self.torch.float64),
            self.torch.tensor(expected, dtype=self.torch.float64), atol=atol, rtol=rtol)

    def test_cost_overflow_does_not_preclude_finite_control_override(self):
        request = payload()
        request.update(horizon_h=1)
        request["network"] = {"assets": [
            {"id": "tank", "label": "Tank", "kind": "tank", "capacity_m3": 100, "initial_m3": 50},
            {"id": "sink", "label": "Sink", "kind": "treatment", "capacity_m3h": 10}],
            "routes": [{"id": "out", "source_id": "tank", "target_id": "sink", "capacity_m3h": 10,
                        "setpoint_m3h": 10, "cost_per_m3": sys.float_info.max,
                        "has_pump": False, "available": True}]}
        with self.assertRaises(StorageError) as caught:
            storage_rollout(request)
        self.assertEqual(caught.exception.code, "numerical_failure")
        for flow in (0.0, .25):
            request["controls"] = [{"control_id": "route:out:flow", "value": flow}]
            result = storage_rollout(request)
            self.assertEqual(result["cost"], flow * sys.float_info.max)
        request["horizon_h"] = 24
        with self.assertRaises(StorageError) as caught:
            storage_rollout(request)
        self.assertEqual(caught.exception.code, "numerical_failure")

    def test_wire_units_ids_default_controls_and_provenance(self):
        request = payload(); request["identity"].update(route_id="opaque:route", parameter_hash="b" * 64)
        original = copy.deepcopy(request)
        result = storage_rollout(request)
        self.assertEqual(request, original)
        self.assertEqual(result["identity"], request["identity"])
        self.assertEqual(result["tank_ids"], ["a:one", "b:two"])
        self.assertNear([row[0] for row in result["volumes_m3"]], [80, 30])
        self.assertNear([row[-1] for row in result["volumes_m3"]], [98, 27])
        self.assertNear(result["cost"], 513)
        self.assertEqual(result["provenance"]["implementation"], "torch_storage_float64")
        self.assertEqual(result["provenance"]["dtype"], "float64")
        self.assertEqual(result["provenance"]["device"], "cpu")
        self.assertNotIn("derivatives", result); self.assertNotIn("feasible", result)

    def test_partial_overrides_selected_derivative_order_and_units(self):
        request = payload(derivatives=True)
        request["controls"] = [{"control_id": "route:transfer:ab:flow", "value": 5},
                               {"control_id": "source:prod:west:curtail", "value": 4}]
        result = storage_rollout(request)
        self.assertNear([row[-1] for row in result["volumes_m3"]], [92, 30])
        self.assertNear(result["cost"], 532.5)
        derivative = result["derivatives"]
        self.assertEqual(derivative["control_ids"], [item["control_id"] for item in request["controls"]])
        self.assertNear(derivative["terminal_volume_jacobian_h"], [[-3, -3], [3, 0]])
        self.assertNear(derivative["cost_gradient"], [4.5, 15])
        self.assertNear(storage_rollout(payload())["cost"], 513)

    def test_roundoff_and_subnormal_grids_keep_exact_requested_horizon(self):
        cases = [
            (.225, .075, [0.0, .075, .15, .22499999999999998, .225]),
            (math.nextafter(.9, math.inf), .1, [0.0] + [index * .1 for index in range(1, 10)] + [math.nextafter(.9, math.inf)]),
            (.30000000000000004, .1, [0.0, .1, .2, .30000000000000004]),
            (1.1, .3, [0.0, .3, .6, .8999999999999999, 1.1]),
            (math.ulp(0.0), 24, [0.0, math.ulp(0.0)]),
            (.0082, 8.2e-7, [index * 8.2e-7 for index in range(10000)] + [.0082]),
            (1, .0001, [index * .0001 for index in range(10000)] + [1.0]),
        ]
        for horizon, step, times in cases:
            for maximum in (2, 256):
                with self.subTest(horizon=horizon, step=step, maximum=maximum):
                    request = payload(); request.update(horizon_h=horizon, step_h=step)
                    request["preview"]["max_times"] = maximum
                    result = storage_rollout(request)
                    count = min(maximum, len(times))
                    indices = [i * (len(times) - 1) // (count - 1) for i in range(count)]
                    self.assertEqual(result["time_h"], [times[i] for i in indices])
                    self.assertEqual(result["time_indices"], indices)
                    self.assertEqual(result["time_h"][-1], horizon)
                    self.assertEqual(result["time_indices"][-1], len(times) - 1)
                    self.assertEqual(result["time_h"].count(horizon), 1)
                    self.assertTrue(all(left < right for left, right in zip(result["time_h"], result["time_h"][1:])))
                    self.assertEqual(result["identity"], request["identity"])

    def test_one_ulp_tail_forward_and_derivatives_use_same_horizon(self):
        horizon = math.nextafter(.9, math.inf)
        request = payload(derivatives=True)
        request.update(horizon_h=horizon, step_h=.1)
        request["controls"] = [{"control_id": "route:transfer:ab:flow", "value": 5}]
        result = storage_rollout(request)
        self.assertEqual(result["time_h"][-1], horizon)
        self.assertEqual(result["derivatives"]["terminal_volume_jacobian_h"], [[-horizon], [horizon]])
        self.assertNear([row[-1] for row in result["volumes_m3"]], [80 + 5 * horizon, 30])

    def test_large_finite_source_retains_requested_endpoint_overflow(self):
        flow = 2.0 ** 50
        horizon = math.nextafter(.9, math.inf)
        request = payload(); request.update(horizon_h=horizon, step_h=.1)
        request["network"] = {"assets": [
            {"id": "tank", "label": "Tank", "kind": "tank", "capacity_m3": .9 * flow, "initial_m3": 0},
            {"id": "source", "label": "Source", "kind": "source", "inflow_m3h": flow}],
            "routes": [{"id": "feed", "source_id": "source", "target_id": "tank", "capacity_m3h": flow,
                        "setpoint_m3h": 0, "cost_per_m3": 0, "has_pump": False, "available": True}]}
        result = storage_rollout(request)
        self.assertEqual(result["time_h"][-1], horizon)
        self.assertEqual(result["volumes_m3"][0][-1] - .9 * flow, .125)
        self.assertNotIn("feasible", result)

    def test_preview_uses_real_frames_and_exact_shortened_horizon(self):
        request = payload(); request.update(horizon_h=1.1, step_h=.03)
        request["preview"]["max_times"] = 7
        result = storage_rollout(request)
        full = self.StorageModel(private_network(request["network"])).rollout(horizon_h=1.1, step_h=.03)
        self.assertEqual(result["time_indices"], [0, 6, 12, 18, 24, 30, 37])
        self.assertEqual(result["time_h"], full.time_h[result["time_indices"]].tolist())
        self.assertEqual(result["time_h"][-1], 1.1)
        self.assertNear(result["volumes_m3"], full.volumes_m3[:, result["time_indices"]].tolist())

    def test_zero_selected_derivatives_are_empty_not_full_catalog(self):
        result = storage_rollout(payload(derivatives=True))
        self.assertEqual(result["derivatives"], {"control_ids": [], "terminal_volume_jacobian_h": [[], []], "cost_gradient": []})

    def test_one_step_derivatives_match_full_autograd_and_central_differences(self):
        torch = self.torch
        request = payload(derivatives=True); request.update(horizon_h=1.13, step_h=.07)
        request["controls"] = [{"control_id": "route:haul:flow", "value": 2},
                               {"control_id": "source:prod:west:curtail", "value": 4}]
        result = storage_rollout(request)
        identifiers = [item["control_id"] for item in request["controls"]]
        values = torch.tensor([item["value"] for item in request["controls"]], dtype=torch.float64, requires_grad=True)
        model = self.StorageModel(private_network(request["network"]))

        def full(vector):
            run = model.rollout(dict(zip(identifiers, vector.unbind())), horizon_h=1.13, step_h=.07)
            return torch.cat((run.volumes_m3[:, -1], run.cost.reshape(1)))

        actual = torch.tensor(result["derivatives"]["terminal_volume_jacobian_h"] +
            [result["derivatives"]["cost_gradient"]], dtype=torch.float64)
        torch.testing.assert_close(actual, torch.autograd.functional.jacobian(full, values), atol=1e-12, rtol=1e-12)
        for epsilon in (1e-3, 1e-4, 1e-5):
            for column in range(len(identifiers)):
                plus, minus = values.detach().clone(), values.detach().clone()
                plus[column] += epsilon; minus[column] -= epsilon
                torch.testing.assert_close(actual[:, column], (full(plus) - full(minus)) / (2 * epsilon), atol=1e-7, rtol=1e-7)

    def test_long_forward_grid_does_not_create_long_backward_graph(self):
        original_rollout = self.StorageModel.rollout
        calls = []

        def observed(model, overrides=None, **kwargs):
            calls.append((kwargs["step_h"], self.torch.is_grad_enabled()))
            return original_rollout(model, overrides, **kwargs)

        request = payload(derivatives=True); request.update(horizon_h=1.0, step_h=.0001)
        request["controls"] = [{"control_id": "route:haul:flow", "value": 2}]
        with patch.object(self.StorageModel, "rollout", observed):
            result = storage_rollout(request)
        self.assertEqual(calls, [(.0001, False), (1.0, True)])
        self.assertEqual(result["time_indices"][-1], 10000)
        self.assertEqual(len(result["time_h"]), 256)
        self.assertNear(result["derivatives"]["terminal_volume_jacobian_h"], [[-1], [0]])

    def test_negative_and_capacity_overflow_evidence_is_not_clipped(self):
        request = payload(); request["network"]["assets"][0]["initial_m3"] = 200
        request["network"]["assets"][1]["initial_m3"] = 0
        result = storage_rollout(request)
        self.assertNear([row[-1] for row in result["volumes_m3"]], [218, -3])
        self.assertNotIn("feasible", result)

    def test_nonfinite_forecast_or_derivative_is_explicit_failure(self):
        request = payload(); request["network"]["routes"][0]["cost_per_m3"] = 1e308
        with self.assertRaises(StorageError) as caught:
            storage_rollout(request)
        self.assertEqual(caught.exception.code, "numerical_failure")
        request = payload(derivatives=True)
        request["network"] = {"assets": [
            {"id": "a", "label": "A", "kind": "tank", "capacity_m3": 100, "initial_m3": 50},
            {"id": "d", "label": "D", "kind": "treatment", "capacity_m3h": 10}],
            "routes": [{"id": "r", "source_id": "a", "target_id": "d", "capacity_m3h": 10,
                "setpoint_m3h": 1e-308, "has_pump": False, "available": True, "cost_per_m3": 1e308}]}
        request["controls"] = [{"control_id": "route:r:flow", "value": 1e-308}]
        with self.assertRaises(StorageError) as caught:
            storage_rollout(request)
        self.assertEqual(caught.exception.code, "numerical_failure")


if __name__ == "__main__":
    unittest.main()
