"""Independent behavioral/derivative checks for the standalone A3 storage model."""

from concurrent.futures import ThreadPoolExecutor
import copy
import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import torch

from breakwater_science.rollout import StorageModel


def custom_network():
    """Two tanks, one internal transfer and two shared discharge destinations."""
    return {
        "assets": [
            {"id": "a:one", "kind": "tank", "capacity_m3": 200.0, "initial_m3": 80.0},
            {"id": "b:two", "kind": "tank", "capacity_m3": 200.0, "initial_m3": 30.0},
            {"id": "prod:west", "kind": "source", "inflow_m3h": 20.0,
             "curtailment_m3h": 3.0, "curtailment_max_m3h": 10.0, "cost_per_m3": 7.0},
            {"id": "shared:plant", "kind": "treatment", "capacity_m3": 50.0,
             "cost_per_m3": 5.0},
            {"id": "truck", "kind": "trucking", "capacity_m3": 10.0,
             "cost_per_m3": 11.0},
        ],
        "routes": [
            {"id": "feed:west", "source_id": "prod:west", "target_id": "a:one",
             "capacity_m3h": 20.0, "setpoint_m3h": 20.0, "cost_per_m3": 2.0},
            {"id": "transfer:ab", "source_id": "a:one", "target_id": "b:two",
             "capacity_m3h": 12.0, "setpoint_m3h": 4.0, "cost_per_m3": 1.5},
            {"id": "treat:a", "source_id": "a:one", "target_id": "shared:plant",
             "capacity_m3h": 15.0, "setpoint_m3h": 6.0, "cost_per_m3": 3.0},
            {"id": "treat:b", "source_id": "b:two", "target_id": "shared:plant",
             "capacity_m3h": 20.0, "setpoint_m3h": 5.0, "cost_per_m3": 4.0},
            {"id": "haul", "source_id": "a:one", "target_id": "truck",
             "capacity_m3h": 10.0, "setpoint_m3h": 1.0, "cost_per_m3": 6.0},
            {"id": "failed:pump", "source_id": "a:one", "target_id": "shared:plant",
             "capacity_m3h": 30.0, "setpoint_m3h": 0.0,
             "has_pump": True, "available": False},
        ],
    }


def active_model():
    network = custom_network()
    network["routes"].pop()  # Remove the fixed zero-bound failed pump for central differences.
    return StorageModel(network)


class RolloutTests(unittest.TestCase):
    def assertClose(self, actual, expected, *, atol=1e-10, rtol=1e-10):
        expected = torch.as_tensor(expected, dtype=torch.float64)
        torch.testing.assert_close(actual, expected, atol=atol, rtol=rtol)

    def test_custom_ids_inventory_cost_and_catalog_order(self):
        model = StorageModel(custom_network())
        self.assertEqual(model.control_ids, (
            "source:prod:west:curtail", "route:transfer:ab:flow", "route:treat:a:flow",
            "route:treat:b:flow", "route:haul:flow", "route:failed:pump:flow"))
        run = model.rollout()
        self.assertEqual(run.tank_ids, ("a:one", "b:two"))
        self.assertEqual(tuple(run.volumes_m3.shape), (2, 121))
        self.assertEqual(run.volumes_m3.dtype, torch.float64)
        self.assertEqual(run.volumes_m3.device.type, "cpu")
        self.assertClose(run.volumes_m3[:, 0], [80.0, 30.0])
        self.assertClose(run.rates_m3h, [6.0, -1.0])
        self.assertClose(run.volumes_m3[:, -1], [98.0, 27.0])
        self.assertClose(run.cost, 513.0)

    def test_overrides_keep_every_unspecified_control_fixed(self):
        model = StorageModel(custom_network())
        run = model.rollout({"source:prod:west:curtail": 4.0, "route:transfer:ab:flow": 5.0})
        self.assertClose(run.control_values, [4.0, 5.0, 6.0, 5.0, 1.0, 0.0])
        self.assertClose(run.volumes_m3[:, -1], [92.0, 30.0])
        self.assertClose(run.cost, 532.5)
        self.assertClose(model.rollout().volumes_m3[:, -1], [98.0, 27.0])

    def test_combined_control_price_overflow_rejects_even_zero_flow(self):
        price = sys.float_info.max
        for kind in ("treatment", "trucking"):
            for flow in (0.0, .25):
                with self.subTest(kind=kind, flow=flow):
                    network = {"assets": [
                        {"id": "tank", "kind": "tank", "capacity_m3": 100, "initial_m3": 50},
                        {"id": "sink", "kind": kind, "capacity_m3": 10, "cost_per_m3": price}],
                        "routes": [{"id": "out", "source_id": "tank", "target_id": "sink",
                                    "capacity_m3h": 10, "setpoint_m3h": flow, "cost_per_m3": price}]}
                    with self.assertRaisesRegex(ValueError, "combined.*price"):
                        StorageModel(network)

    def test_finite_unit_price_retains_zero_and_repaired_cost_candidates(self):
        price = sys.float_info.max
        model = StorageModel({"assets": [
            {"id": "tank", "kind": "tank", "capacity_m3": 100, "initial_m3": 50},
            {"id": "sink", "kind": "treatment", "capacity_m3": 10}],
            "routes": [{"id": "out", "source_id": "tank", "target_id": "sink",
                        "capacity_m3h": 10, "setpoint_m3h": 10, "cost_per_m3": price}]})
        # A nonfinite baseline cost must not prevent a finite explicit intervention.
        with self.assertRaisesRegex(ValueError, "Nonfinite rollout result"):
            model.rollout(horizon_h=1)
        zero = model.rollout({"route:out:flow": 0}, horizon_h=1)
        self.assertEqual(zero.cost.item(), 0)
        self.assertEqual(zero.volumes_m3[0, -1].item(), 50)
        finite = model.rollout({"route:out:flow": .25}, horizon_h=1)
        self.assertEqual(finite.cost.item(), .25 * price)
        with self.assertRaisesRegex(ValueError, "Nonfinite rollout result"):
            model.rollout({"route:out:flow": .25}, horizon_h=24)

    def test_initial_sample_and_exact_nondividing_final_time(self):
        run = StorageModel(custom_network()).rollout(horizon_h=1.1, step_h=0.3)
        self.assertClose(run.time_h, [0.0, 0.3, 0.6, 0.9, 1.1], atol=1e-15)
        self.assertEqual(run.time_h[-1].item(), 1.1)
        self.assertTrue(bool(torch.all(run.time_h[1:] > run.time_h[:-1])))
        self.assertClose(run.volumes_m3[:, 0], [80.0, 30.0])
        self.assertClose(run.volumes_m3[:, -1], [86.6, 28.9])

    def test_step_larger_than_horizon_has_two_samples(self):
        run = StorageModel(custom_network()).rollout(horizon_h=0.25, step_h=1.0)
        self.assertClose(run.time_h, [0.0, 0.25])
        self.assertClose(run.volumes_m3[:, -1], [81.5, 29.75])

    def test_one_ulp_tail_preserves_regular_samples_and_exact_horizon(self):
        horizon = math.nextafter(.9, math.inf)
        run = StorageModel(custom_network()).rollout(horizon_h=horizon, step_h=.1)
        self.assertEqual(run.time_h.tolist(), [0.0] + [index * .1 for index in range(1, 10)] + [horizon])
        self.assertEqual(run.time_h[-2].item(), .9)
        self.assertTrue(bool(torch.all(run.time_h[1:] > run.time_h[:-1])))

    def test_rounding_up_the_quotient_does_not_duplicate_final_sample(self):
        horizon = .30000000000000004
        run = StorageModel(custom_network()).rollout(horizon_h=horizon, step_h=.1)
        self.assertEqual(run.time_h.tolist(), [0.0, .1, .2, horizon])
        self.assertTrue(bool(torch.all(run.time_h[1:] > run.time_h[:-1])))

    def test_subnormal_horizon_and_overflowing_step_multiple_are_supported(self):
        horizon = math.ulp(0.0)
        for step in (24.0, 1e308):
            with self.subTest(step=step):
                run = StorageModel(custom_network()).rollout(horizon_h=horizon, step_h=step)
                self.assertEqual(run.time_h.tolist(), [0.0, horizon])
                self.assertEqual(run.volumes_m3.shape[1], 2)

    def test_requested_horizon_excess_is_retained_at_large_finite_flow(self):
        flow = 2.0 ** 50
        network = {"assets": [
            {"id": "tank", "kind": "tank", "capacity_m3": .9 * flow, "initial_m3": 0},
            {"id": "source", "kind": "source", "inflow_m3h": flow}],
            "routes": [{"id": "feed", "source_id": "source", "target_id": "tank", "capacity_m3h": flow}]}
        horizon = math.nextafter(.9, math.inf)
        run = StorageModel(network).rollout(horizon_h=horizon, step_h=.1)
        self.assertEqual(run.time_h[-1].item(), horizon)
        self.assertEqual(run.volumes_m3[0, -1].item() - .9 * flow, .125)

    def test_zero_control_static_network(self):
        model = StorageModel({"assets": [{"id": "alone", "kind": "tank",
                                         "capacity_m3": 5.0, "initial_m3": 2.0}], "routes": []})
        self.assertEqual(model.control_ids, ())
        self.assertEqual(tuple(model.default_controls().shape), (0,))
        run = model.rollout_vector(torch.empty(0, dtype=torch.float64), horizon_h=0.5, step_h=0.2)
        self.assertClose(run.volumes_m3, [[2.0, 2.0, 2.0, 2.0]])
        self.assertClose(run.cost, 0.0)

    def test_zero_control_network_stays_on_cpu_under_non_cpu_default_device(self):
        network = {"assets": [{"id": "isolated", "kind": "tank",
                               "capacity_m3": 5.0, "initial_m3": 2.0}], "routes": []}
        # The context restores the caller's device preference automatically. Empty
        # flow vectors and generated times must not inherit this ambient default.
        with torch.device("meta"):
            model = StorageModel(network)
            defaults = model.default_controls()
            run = model.rollout(horizon_h=0.5, step_h=0.2)
        for tensor in (defaults, run.time_h, run.volumes_m3, run.rates_m3h, run.cost):
            self.assertEqual(tensor.device.type, "cpu")
            self.assertEqual(tensor.dtype, torch.float64)
        self.assertClose(run.time_h, [0.0, 0.2, 0.4, 0.5])
        self.assertClose(run.volumes_m3, [[2.0, 2.0, 2.0, 2.0]])
        self.assertClose(run.rates_m3h, [0.0])
        self.assertClose(run.cost, 0.0)

    def test_negative_and_overflowing_storage_are_not_clipped(self):
        negative = {"assets": [
            {"id": "tank", "kind": "tank", "capacity_m3": 10.0, "initial_m3": 1.0},
            {"id": "sink", "kind": "treatment", "capacity_m3": 20.0}],
            "routes": [{"id": "drain", "source_id": "tank", "target_id": "sink",
                        "capacity_m3h": 10.0, "setpoint_m3h": 10.0}]}
        run = StorageModel(negative).rollout()
        self.assertClose(run.volumes_m3[:, -1], [-29.0])
        overflow = StorageModel(custom_network()).rollout(horizon_h=24.0, step_h=0.5)
        self.assertGreater(overflow.volumes_m3[0, -1].item(), 200.0)

    def test_shared_destination_excess_is_a_prediction_not_a_torch_verdict(self):
        network = custom_network()
        network["assets"][3]["capacity_m3"] = 1.0
        run = StorageModel(network).rollout()
        self.assertClose(run.volumes_m3[:, -1], [98.0, 27.0])
        self.assertFalse(hasattr(run, "feasible"))

    def test_capacity_equality_is_permitted(self):
        network = {"assets": [
            {"id": "tank", "kind": "tank", "capacity_m3": 100.0, "initial_m3": 70.0},
            {"id": "source", "kind": "source", "inflow_m3h": 10.0}],
            "routes": [{"id": "feed", "source_id": "source", "target_id": "tank",
                        "capacity_m3h": 10.0}]}
        self.assertClose(StorageModel(network).rollout().volumes_m3[:, -1], [100.0])

    def test_all_control_gradients_have_physical_signs_and_source_feed_savings(self):
        model = active_model()
        controls = model.default_controls().requires_grad_()
        run = model.rollout_vector(controls, horizon_h=2.0, step_h=0.3)
        a_grad = torch.autograd.grad(run.volumes_m3[0, -1], controls, retain_graph=True)[0]
        b_grad = torch.autograd.grad(run.volumes_m3[1, -1], controls, retain_graph=True)[0]
        cost_grad = torch.autograd.grad(run.cost, controls)[0]
        self.assertClose(a_grad, [-2.0, -2.0, -2.0, 0.0, -2.0])
        self.assertClose(b_grad, [0.0, 2.0, 0.0, -2.0, 0.0])
        # Curtailment costs 7/m3, but saves 2/m3 on the source feed route.
        self.assertClose(cost_grad, [10.0, 3.0, 16.0, 18.0, 34.0])

    def test_mapping_scalar_tensor_preserves_its_autograd_history(self):
        value = torch.tensor(4.0, dtype=torch.float64, requires_grad=True)
        run = StorageModel(custom_network()).rollout({"route:transfer:ab:flow": value})
        weighted_endpoint = run.volumes_m3[0, -1] + 2.0 * run.volumes_m3[1, -1]
        self.assertClose(torch.autograd.grad(weighted_endpoint, value)[0], 3.0)

    def test_torch_gradcheck_for_vector_endpoint_and_cost(self):
        model = active_model()
        values = model.default_controls().requires_grad_()

        def objectives(candidate):
            run = model.rollout_vector(candidate, horizon_h=2.375, step_h=0.6)
            return torch.cat((run.volumes_m3[:, -1], run.cost.reshape(1)))

        self.assertTrue(torch.autograd.gradcheck(objectives, (values,), eps=1e-6,
                                               atol=1e-6, rtol=1e-4))

    def test_multiple_central_difference_scales_on_smooth_objective(self):
        model = active_model()
        values = model.default_controls().requires_grad_()

        def objective(candidate):
            run = model.rollout_vector(candidate, horizon_h=1.7, step_h=0.35)
            # A smooth nonlinear objective catches incorrect autograd wiring beyond
            # the linear storage equations. Volume and synthetic-cost scales explicit.
            return (run.volumes_m3[:, -1] / 100.0).square().sum() + run.cost / 1000.0

        actual = torch.autograd.grad(objective(values), values)[0]
        for epsilon in (1e-3, 1e-4, 1e-5):
            estimates = []
            for index in range(len(values)):
                plus, minus = values.detach().clone(), values.detach().clone()
                plus[index] += epsilon
                minus[index] -= epsilon
                estimates.append((objective(plus) - objective(minus)) / (2.0 * epsilon))
            self.assertClose(actual, torch.stack(estimates), atol=1e-6, rtol=1e-4)

    def test_capture_defaults_and_returned_results_are_isolated(self):
        network = custom_network()
        model = StorageModel(network)
        network["assets"][0]["initial_m3"] = 0.0
        network["routes"][1]["setpoint_m3h"] = 12.0
        defaults = model.default_controls()
        defaults[0] = 0.0
        first = model.rollout()
        first.volumes_m3.zero_()
        first.control_values.zero_()
        self.assertClose(model.default_controls(), [3.0, 4.0, 6.0, 5.0, 1.0, 0.0])
        self.assertClose(model.rollout().volumes_m3[:, -1], [98.0, 27.0])

    def test_concurrent_calls_do_not_share_candidate_state(self):
        model = StorageModel(custom_network())

        def run(value):
            return model.rollout({"route:transfer:ab:flow": value}).volumes_m3[:, -1]

        with ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(run, [2.0, 3.0, 4.0, 5.0]))
        for value, result in zip([2.0, 3.0, 4.0, 5.0], results):
            self.assertClose(result, [110.0 - 3.0 * value, 15.0 + 3.0 * value])

    def test_invalid_controls_fail_without_changing_defaults(self):
        model = StorageModel(custom_network())
        bad = [
            {"unknown": 1.0}, {"route:feed:west:flow": 1.0},
            {"route:transfer:ab:flow": -1.0}, {"route:transfer:ab:flow": 12.0001},
            {"route:transfer:ab:flow": float("nan")},
            {"route:transfer:ab:flow": float("inf")},
            {"route:failed:pump:flow": 0.001},
            {"source:prod:west:curtail": 10.0001},
        ]
        for overrides in bad:
            with self.subTest(overrides=overrides), self.assertRaises((ValueError, TypeError)):
                model.rollout(overrides)
        self.assertClose(model.rollout().volumes_m3[:, -1], [98.0, 27.0])
        self.assertClose(model.rollout({"route:failed:pump:flow": 0.0}).cost, 513.0)

    def test_source_curtailment_rechecks_actual_feed_route_capacity(self):
        network = custom_network()
        # Native structural route comparisons permit a 1e-9 m3/h roundoff margin.
        network["routes"][0]["capacity_m3h"] = 17.0 - 5e-10
        model = StorageModel(network)
        self.assertClose(model.rollout().rates_m3h, [6.0, -1.0])
        # This curtailment is within its own catalog bound, but its resulting source
        # inflow would exceed the actual feed route. It must fail, not be clipped.
        with self.assertRaises((ValueError, TypeError)):
            model.rollout({"source:prod:west:curtail": 2.99})

    def test_omitted_stored_roundoff_default_differs_from_explicit_override(self):
        network = custom_network()
        stored = 12.0 + 5e-10
        network["routes"][1]["setpoint_m3h"] = stored
        model = StorageModel(network)
        # Native structural validation allows this stored roundoff-sized excess.
        # Omitting the control preserves it exactly rather than silently clipping it.
        run = model.rollout()
        self.assertEqual(run.control_values[1].item(), stored)
        self.assertClose(run.volumes_m3[:, -1], [110.0 - 3.0 * stored,
                                               15.0 + 3.0 * stored])
        # Explicit controls use strict catalog bounds, including a full vector.
        with self.assertRaises((ValueError, TypeError)):
            model.rollout({"route:transfer:ab:flow": stored})
        with self.assertRaises((ValueError, TypeError)):
            model.rollout_vector(model.default_controls())

    def test_tensor_dtype_shape_and_nonfinite_values_are_explicit_errors(self):
        model = StorageModel(custom_network())
        valid = model.default_controls()
        for values in (valid.float(), valid.reshape(2, 3), valid[:-1], valid.to(dtype=torch.int64)):
            with self.subTest(dtype=values.dtype, shape=tuple(values.shape)):
                with self.assertRaises((ValueError, TypeError)):
                    model.rollout_vector(values)
        for value in (torch.tensor(4.0, dtype=torch.float32),
                      torch.tensor([4.0], dtype=torch.float64)):
            with self.assertRaises((ValueError, TypeError)):
                model.rollout({"route:transfer:ab:flow": value})
        for bad in (float("nan"), float("inf"), float("-inf")):
            values = valid.clone()
            values[1] = bad
            with self.assertRaises((ValueError, TypeError)):
                model.rollout_vector(values)

    def test_non_cpu_tensors_are_explicit_errors(self):
        # Meta requires no GPU and ensures device errors precede scalar extraction.
        model = StorageModel(custom_network())
        with self.assertRaises((ValueError, TypeError)):
            model.rollout_vector(torch.empty(len(model.control_ids), dtype=torch.float64, device="meta"))
        with self.assertRaises((ValueError, TypeError)):
            model.rollout({"route:transfer:ab:flow": torch.empty((), dtype=torch.float64, device="meta")})

    def test_invalid_forecast_configuration_is_rejected(self):
        model = StorageModel(custom_network())
        configs = [
            {"horizon_h": 0.0}, {"horizon_h": -1.0}, {"horizon_h": 24.0001},
            {"horizon_h": float("nan")}, {"horizon_h": float("inf")},
            {"step_h": 0.0}, {"step_h": -0.1}, {"step_h": float("nan")},
            {"step_h": float("inf")}, {"horizon_h": 1.0, "step_h": 1.0 / 10001.0},
            {"horizon_h": math.nextafter(1.0, math.inf), "step_h": .0001},
        ]
        for config in configs:
            with self.subTest(config=config), self.assertRaises((ValueError, TypeError)):
                model.rollout(**config)

    def test_maximum_step_count_remains_supported(self):
        run = StorageModel(custom_network()).rollout(horizon_h=1.0, step_h=0.0001)
        self.assertEqual(run.time_h.numel(), 10001)
        self.assertEqual(run.time_h[-1].item(), 1.0)
        self.assertClose(run.volumes_m3[:, -1], [86.0, 29.0])

    def test_invalid_network_structure_is_rejected(self):
        invalid = []
        invalid.append({"assets": [], "routes": []})
        for mutate in (
            lambda n: n["assets"].append(copy.deepcopy(n["assets"][0])),
            lambda n: n["assets"][0].update(id=""),
            lambda n: n["assets"][0].update(kind="unknown"),
            lambda n: n["assets"][0].update(initial_m3=201.0),
            lambda n: n["assets"][0].update(capacity_m3=0.0),
            lambda n: n["assets"][2].update(curtailment_max_m3h=21.0),
            lambda n: n["assets"][3].update(capacity_m3=0.0),
            lambda n: n["routes"][1].update(target_id="missing"),
            lambda n: n["routes"][1].update(target_id="a:one"),
            lambda n: n["routes"][1].update(source_id="shared:plant"),
            lambda n: n["routes"][1].update(id="a:one"),
            lambda n: n["routes"][1].update(setpoint_m3h=13.0),
            lambda n: n["routes"][0].update(capacity_m3h=16.0),
            lambda n: n["routes"][0].update(has_pump=True),
            lambda n: n["routes"].pop(0),
            lambda n: n["routes"].append({**n["routes"][0], "id": "second_feed"}),
            lambda n: n["routes"][-1].update(has_pump=False),
            lambda n: n["routes"][-1].update(setpoint_m3h=1.0),
        ):
            network = custom_network()
            mutate(network)
            invalid.append(network)
        for index, network in enumerate(invalid):
            with self.subTest(case=index), self.assertRaises((ValueError, TypeError)):
                StorageModel(network)

    def test_invalid_parameter_numbers_and_size_limits_are_rejected(self):
        for location, field in (("assets", "initial_m3"), ("assets", "cost_per_m3"),
                                ("routes", "capacity_m3h"), ("routes", "cost_per_m3")):
            for bad in (-1.0, float("nan"), float("inf")):
                network = custom_network()
                network[location][0][field] = bad
                with self.subTest(location=location, field=field, bad=bad):
                    with self.assertRaises((ValueError, TypeError)):
                        StorageModel(network)
        too_many_assets = {"assets": [{"id": f"t{i}", "kind": "tank", "capacity_m3": 1.0}
                                       for i in range(129)], "routes": []}
        with self.assertRaises((ValueError, TypeError)):
            StorageModel(too_many_assets)
        too_many_routes = custom_network()
        too_many_routes["routes"] = [{**too_many_routes["routes"][1], "id": f"r{i}"}
                                      for i in range(257)]
        with self.assertRaises((ValueError, TypeError)):
            StorageModel(too_many_routes)


if __name__ == "__main__":
    unittest.main()
