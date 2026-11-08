"""A4 equation, averaging and genuine PhysicsNeMo checkpoint regressions.

The bounded training/evaluation command is exercised separately by the A4 smoke.
Only tests requiring the optional model package are skipped in an A3-only environment.
"""
from dataclasses import FrozenInstanceError
import importlib.util
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from breakwater_science.transport_pinn import (
    CASE, Case, exact_cell_averages, exact_concentration, exact_ledger,
    make_model, pde_residual, predict, predicted_cell_averages,
)


def coordinates(values):
    return torch.tensor(values, dtype=torch.float64, device="cpu")


def independent_concentration(x, t, *, length=1.0, diffusion=0.1, decay=0.1):
    """Scalar closed form, independently evaluated with Python math."""
    beta = 3 * math.pi / (4 * length)
    rate = decay + 2 * diffusion * beta * beta
    return math.exp(beta * (x-length) - rate*t) * math.sin(beta*x) / math.sin(beta*length)


class TransportMathematicsTests(unittest.TestCase):
    def assertClose(self, actual, expected, atol=1e-12, rtol=1e-12):
        torch.testing.assert_close(
            actual, torch.as_tensor(expected, dtype=torch.float64, device="cpu"),
            atol=atol, rtol=rtol)

    def test_case_is_immutable_and_has_consistent_physical_rates(self):
        beta = 3 * math.pi / 4
        self.assertAlmostEqual(CASE.flow_m3h / CASE.area_m2, 0.2*beta, places=14)
        self.assertAlmostEqual(CASE.eigenvalue_per_h, 0.1+0.2*beta*beta, places=14)
        with self.assertRaises(FrozenInstanceError):
            CASE.length_m = 2.0

    def test_exact_values_initial_data_and_boundary_conditions(self):
        points = coordinates([[0, 0], [.2, .1], [.7, .6], [1, 0], [1, 1]])
        expected = [[independent_concentration(x, t)] for x, t in points.tolist()]
        actual = exact_concentration(points)
        self.assertClose(actual, expected)
        self.assertEqual(actual[0, 0].item(), 0.0)
        self.assertAlmostEqual(actual[3, 0].item(), 1.0, places=14)
        self.assertTrue(bool((actual[1:] > 0).all()))
        outlet = coordinates([[1, .2], [1, .8]]).requires_grad_(True)
        value = exact_concentration(outlet)
        derivative, = torch.autograd.grad(value, outlet, torch.ones_like(value))
        self.assertClose(derivative[:, 0], [0, 0])

    def test_exact_eigenmode_cancels_all_pde_terms(self):
        points = coordinates([[.11, .07], [.36, .41], [.78, .83], [.94, .97]])
        residual = pde_residual(exact_concentration, points)
        self.assertClose(residual, torch.zeros_like(residual), atol=2e-12)

    def test_constant_fields_have_zero_coordinate_derivatives(self):
        points = coordinates([[.2, .3], [.7, .9]])
        for value in (0.0, 1.0, -.4):
            for decay in (0.0, .1):
                with self.subTest(value=value, decay=decay):
                    case = Case(decay_per_h=decay)
                    residual = pde_residual(
                        lambda xt: torch.full_like(xt[:, :1], value), points, case)
                    self.assertClose(residual, [[decay*value], [decay*value]])

    def test_linear_fields_include_first_derivatives_and_zero_curvature(self):
        points = coordinates([[.2, .3], [.7, .9]])
        # Independently differentiate a*x+b*t+d, including fields independent
        # of one coordinate. Diffusion contributes zero for every affine field.
        for a, b, d in ((1.0, 1.0, 0.0), (1.0, 0.0, 0.0),
                        (0.0, 1.0, 0.0), (2.5, -.4, 1.2)):
            with self.subTest(a=a, b=b, d=d):
                residual = pde_residual(
                    lambda xt: a*xt[:, :1] + b*xt[:, 1:2] + d, points)
                velocity = 2*.1*(3*math.pi/4)
                expected = [[b + velocity*a + .1*(a*x+b*t+d)]
                            for x, t in points.tolist()]
                self.assertClose(residual, expected)

    def test_coordinate_independent_parameter_retains_decay_gradient(self):
        points = coordinates([[.2, .3], [.7, .9]])
        for decay in (0.0, .37):
            with self.subTest(decay=decay):
                amplitude = torch.nn.Parameter(torch.tensor(1.3, dtype=torch.float64))
                residual = pde_residual(
                    lambda xt: amplitude.expand(xt.shape[0], 1), points,
                    Case(decay_per_h=decay))
                self.assertClose(residual, [[decay*1.3], [decay*1.3]])
                gradient, = torch.autograd.grad(residual.square().mean(), amplitude)
                self.assertClose(gradient, 2*decay*decay*1.3)

    def test_parameterized_affine_field_has_analytic_parameter_gradients(self):
        points = coordinates([[.2, .3], [.7, .9], [1.2, .5]])
        a, b, d = 1.4, -.6, .8
        velocity = 2*.25*(3*math.pi/8)
        for decay in (0.0, .37):
            with self.subTest(decay=decay):
                parameters = torch.tensor([a, b, d], dtype=torch.float64, requires_grad=True)

                def field(xt):
                    return parameters[0]*xt[:, :1] + parameters[1]*xt[:, 1:2] + parameters[2]

                residual = pde_residual(field, points,
                    Case(length_m=2.0, dispersion_m2_h=.25, decay_per_h=decay))
                expected = [b + velocity*a + decay*(a*x+b*t+d)
                            for x, t in points.tolist()]
                self.assertClose(residual, [[value] for value in expected])
                gradient, = torch.autograd.grad(residual.square().mean(), parameters)
                # For each point: dR/da=v+k*x, dR/db=1+k*t, dR/dd=k.
                expected_gradient = [0.0, 0.0, 0.0]
                for (x, t), value in zip(points.tolist(), expected):
                    for column, derivative in enumerate((velocity+decay*x, 1+decay*t, decay)):
                        expected_gradient[column] += 2*value*derivative/len(expected)
                self.assertClose(gradient, expected_gradient)

    def test_residual_does_not_swallow_field_errors(self):
        def failing_field(xt):
            raise RuntimeError("deliberate field failure")

        with self.assertRaisesRegex(RuntimeError, "deliberate field failure"):
            pde_residual(failing_field, coordinates([[.2, .3]]))

    def test_zero_parameter_value_does_not_remove_derivative_dependency(self):
        points = coordinates([[.2, .3], [.8, .7]])
        theta = torch.tensor(0.0, dtype=torch.float64, requires_grad=True)
        residual = pde_residual(lambda xt: theta*xt[:, :1].square(), points,
                                Case(decay_per_h=0.0))
        self.assertClose(residual, [[0.0], [0.0]])
        # At theta=0 the residual vanishes, but dR/dtheta does not. A squared
        # loss would hide a detached zero because its legitimate gradient is zero.
        gradient, = torch.autograd.grad(residual.mean(), theta)
        velocity = 2*.1*(3*math.pi/4)
        self.assertClose(gradient, 2*velocity*(.2+.8)/2 - 2*.1)
        self.assertGreater(abs(float(gradient)), 1e-6)

    def test_residual_does_not_swallow_derivative_errors(self):
        class FailingDerivative(torch.autograd.Function):
            @staticmethod
            def forward(ctx, values):
                return values.clone()

            @staticmethod
            def backward(ctx, upstream):
                raise RuntimeError("deliberate derivative failure")

        with self.assertRaisesRegex(RuntimeError, "deliberate derivative failure"):
            pde_residual(lambda xt: FailingDerivative.apply(xt[:, :1]),
                         coordinates([[.2, .3]]))

    def test_residual_enables_derivatives_inside_no_grad_and_retains_parameters(self):
        points = coordinates([[.2, .3], [.8, .7]])
        amplitude = torch.tensor(1.3, dtype=torch.float64, requires_grad=True)

        def field(xt):
            return amplitude*(xt[:, :1].square() + xt[:, 1:2].square() + 1)

        # An outer inference/no-backward utility must not erase the derivatives
        # defining the PDE. Parameter differentiation remains usable afterwards.
        with torch.no_grad():
            residual = pde_residual(field, points)
        velocity = 2*.1*(3*math.pi/4)
        base = [2*t + velocity*2*x - .1*2 + .1*(x*x+t*t+1)
                for x, t in points.tolist()]
        self.assertClose(residual, [[1.3*value] for value in base])
        self.assertClose(residual, pde_residual(field, points))
        gradient, = torch.autograd.grad(residual.square().mean(), amplitude)
        self.assertClose(gradient, 2*1.3*sum(value*value for value in base)/len(base))

    def test_residual_rejects_inference_mode_explicitly(self):
        with torch.inference_mode(), self.assertRaisesRegex(RuntimeError, "inference mode"):
            pde_residual(lambda xt: xt[:, :1].square() + xt[:, 1:2],
                         coordinates([[.2, .3]]))

    def test_residual_on_polynomial_has_correct_signs_and_physical_units(self):
        # Independent derivatives rule out a vacuous implementation returning zero.
        case = Case(length_m=2.0, area_m2=.3, dispersion_m2_h=.2,
                    decay_per_h=.4, horizon_h=1.5)
        points = coordinates([[.2, .1], [.7, .5], [1.7, 1.1]])

        def field(xt):
            x, t = xt[:, :1], xt[:, 1:2]
            return x.square() + 3*x*t + 2*t.square() + 1

        expected = []
        velocity = 2 * .2 * (3 * math.pi / 8)
        for x, t in points.tolist():
            c = x*x + 3*x*t + 2*t*t + 1
            expected.append([3*x + 4*t + velocity*(2*x+3*t) - .2*2 + .4*c])
        actual = pde_residual(field, points, case)
        self.assertClose(actual, expected)
        self.assertGreater(float(actual.detach().abs().max()), 1.0)

    def test_residual_retains_trainable_parameter_graph(self):
        scale = torch.tensor(1.3, dtype=torch.float64, requires_grad=True)
        points = coordinates([[.2, .3], [.8, .7]])

        def field(xt):
            return scale*(xt[:, :1].square() + xt[:, 1:2].square() + 1)

        loss = pde_residual(field, points).square().mean()
        gradient, = torch.autograd.grad(loss, scale)
        self.assertTrue(bool(torch.isfinite(gradient)))
        self.assertGreater(abs(float(gradient)), 1e-6)

    def test_exact_cell_averages_match_independent_high_order_quadrature(self):
        times = coordinates([0, .17, .63, 1])
        nodes, weights = np.polynomial.legendre.leggauss(32)
        for cells in (2, 7, 32):
            expected = []
            for t in times.tolist():
                values = []
                for cell in range(cells):
                    center = (cell+.5)/cells
                    samples = [independent_concentration(center+node/(2*cells), t)
                               for node in nodes]
                    values.append(float(np.dot(weights, samples)/2))
                expected.append(values)
            with self.subTest(cells=cells):
                self.assertClose(exact_cell_averages(cells, times), expected, atol=3e-12)

    def test_cell_averages_are_not_point_samples(self):
        time = coordinates([.2])
        averaged = exact_cell_averages(2, time)[0]
        centered = exact_concentration(coordinates([[.25, .2], [.75, .2]]))[:, 0]
        self.assertGreater(float((averaged-centered).abs().max()), 1e-3)

    def test_signed_ledger_matches_closed_form_and_mass_balance(self):
        times = coordinates([0, .1, .55, 1])
        ledger = exact_ledger(times)
        required = {"inventory_mg", "cumulative_inlet_mg", "cumulative_outlet_mg",
                    "cumulative_source_mg", "cumulative_decay_mg"}
        self.assertEqual(set(ledger), required)
        beta, diffusion, decay = 3*math.pi/4, .1, .1
        velocity = 2*diffusion*beta
        rate = decay + 2*diffusion*beta*beta
        normalization = math.exp(-beta)/math.sin(beta)
        # Integral of exp(beta*x) sin(beta*x), including unit-amplitude outlet scaling.
        primitive_at_one = math.exp(beta)*(beta*math.sin(beta)-beta*math.cos(beta))/(2*beta*beta)
        primitive_at_zero = -1/(2*beta)
        mass0 = 1000*normalization*(primitive_at_one-primitive_at_zero)
        factor = torch.tensor([(1-math.exp(-rate*t))/rate for t in times.tolist()],
                              dtype=torch.float64, device="cpu")
        self.assertClose(ledger["inventory_mg"], [mass0*math.exp(-rate*t) for t in times.tolist()])
        self.assertClose(ledger["cumulative_inlet_mg"], -1000*diffusion*normalization*beta*factor)
        self.assertClose(ledger["cumulative_outlet_mg"], 1000*velocity*factor)
        self.assertClose(ledger["cumulative_decay_mg"], decay*mass0*factor)
        self.assertClose(ledger["cumulative_source_mg"], torch.zeros_like(times))
        balance = (ledger["inventory_mg"]-ledger["inventory_mg"][0]
                   - ledger["cumulative_inlet_mg"] + ledger["cumulative_outlet_mg"]
                   - ledger["cumulative_source_mg"] + ledger["cumulative_decay_mg"])
        self.assertClose(balance, torch.zeros_like(times), atol=4e-12)
        self.assertLess(float(ledger["cumulative_inlet_mg"][-1]), 0)
        self.assertGreater(float(ledger["cumulative_outlet_mg"][-1]), 0)

    def test_cell_average_inventory_reproduces_analytic_ledger(self):
        times = coordinates([0, .21, .79, 1])
        cells = 13
        field = exact_cell_averages(cells, times)
        inventory = field.sum(dim=1) * (1000*CASE.area_m2*CASE.length_m/cells)
        self.assertClose(inventory, exact_ledger(times)["inventory_mg"], atol=3e-12)

    def test_invalid_mathematical_input_is_explicit(self):
        for points in (torch.ones(2, 2, dtype=torch.float32), coordinates([1, 2]),
                       coordinates([[1, 2, 3]]), coordinates([[float("nan"), 0]])):
            with self.subTest(shape=tuple(points.shape)), self.assertRaises(ValueError):
                exact_concentration(points)
        for cells in (1, 4097, True, 3.5):
            with self.subTest(cells=cells), self.assertRaises(ValueError):
                exact_cell_averages(cells, coordinates([0, 1]))
        with self.assertRaises(ValueError):
            pde_residual(lambda xt: xt[:, 0], coordinates([[.2, .3]]))


@unittest.skipUnless(importlib.util.find_spec("physicsnemo") is not None,
                     "PhysicsNeMo is optional in the A3-only environment")
class ActualPhysicsNeMoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        # PhysicsNeMo reads its supported cache setting on first import. Keep it
        # writable, isolated and alive for the whole class, then restore the env.
        cls._runtime_directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls._runtime_directory.cleanup)
        cache = Path(cls._runtime_directory.name)/"physicsnemo-cache"
        cache.mkdir()
        cls._cache_environment = patch.dict(os.environ, {"LOCAL_CACHE": str(cache)})
        cls._cache_environment.start()
        cls.addClassCleanup(cls._cache_environment.stop)

    def test_actual_model_and_smooth_boundary_ansatz(self):
        from physicsnemo.models.mlp import FullyConnected

        with torch.random.fork_rng(devices=[]):
            torch.manual_seed(123)
            model = make_model()
        self.assertIsInstance(model, FullyConnected)
        for parameter in model.parameters():
            self.assertEqual(parameter.dtype, torch.float64)
            self.assertEqual(parameter.device.type, "cpu")
        points = coordinates([[0, .1], [1, .2], [.4, .3]]).requires_grad_(True)
        value = predict(model, points)
        first, = torch.autograd.grad(value, points, torch.ones_like(value), create_graph=True)
        second, = torch.autograd.grad(first[:, :1], points, torch.ones_like(first[:, :1]))
        self.assertEqual(value[0, 0].item(), 0.0)
        self.assertLess(abs(float(first[1, 0].detach())), 1e-12)
        self.assertGreater(abs(float(second[2, 0].detach())), 1e-8)

    def test_actual_optimizer_step_and_float64_mdlus_round_trip(self):
        with torch.random.fork_rng(devices=[]):
            torch.manual_seed(456)
            model = make_model()
        optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
        points = coordinates([[.1, .12], [.3, .27], [.65, .53], [.91, .87]])
        initial_points = points.clone()
        initial_points[:, 1] = 0
        before = torch.cat([p.detach().reshape(-1).clone() for p in model.parameters()])
        residual = pde_residual(lambda xt: predict(model, xt), points)
        initial_error = predict(model, initial_points)-exact_concentration(initial_points)
        loss = residual.square().mean() + initial_error.square().mean()
        self.assertTrue(bool(torch.isfinite(loss)))
        optimizer.zero_grad(set_to_none=True)
        loss.backward()
        for parameter in model.parameters():
            self.assertIsNotNone(parameter.grad)
            self.assertTrue(bool(torch.isfinite(parameter.grad).all()))
        optimizer.step()
        after = torch.cat([p.detach().reshape(-1) for p in model.parameters()])
        self.assertTrue(bool(torch.any(before != after)))
        model.eval()
        expected = predict(model, points).detach()
        with tempfile.TemporaryDirectory() as directory:
            checkpoint = Path(directory)/"pipe.mdlus"
            model.save(checkpoint)
            self.assertTrue(checkpoint.is_file())
            # Convert before load so a default-float32 constructor cannot round weights.
            loaded = make_model()
            loaded.load(checkpoint, map_location="cpu", strict=True)
            loaded.eval()
            actual = predict(loaded, points).detach()
            torch.testing.assert_close(actual, expected, atol=0, rtol=0)

    def test_residual_only_neural_gradients_match_finite_difference_and_reduce_loss(self):
        with torch.random.fork_rng(devices=[]):
            torch.manual_seed(20261107)
            model = make_model()
        points = coordinates([[.12, .08], [.31, .26], [.59, .57], [.88, .91]])

        def residual_loss():
            return pde_residual(lambda xt: predict(model, xt), points).square().mean()

        # No initial-data loss is present: it cannot mask a detached PDE residual.
        optimizer = torch.optim.SGD(model.parameters(), lr=1e-4)
        optimizer.zero_grad(set_to_none=True)
        loss = residual_loss()
        self.assertTrue(bool(torch.isfinite(loss)))
        self.assertGreater(loss.item(), 1e-8)
        loss.backward()
        parameters = list(model.parameters())
        for parameter in parameters:
            self.assertIsNotNone(parameter.grad)
            self.assertTrue(bool(torch.isfinite(parameter.grad).all()))
        parameter = max(parameters, key=lambda p: p.grad.detach().abs().max().item())
        index = int(parameter.grad.detach().abs().argmax())
        analytic = parameter.grad.detach().reshape(-1)[index].clone()
        self.assertGreater(analytic.abs().item(), 1e-6)

        # Perturb an actual model parameter and reevaluate the full residual;
        # coordinate autograd remains enabled during both finite-difference calls.
        value = parameter.detach().reshape(-1)[index].item()
        epsilon = 1e-6 * max(1.0, abs(value))
        try:
            with torch.no_grad():
                parameter.reshape(-1)[index] = value + epsilon
            plus = residual_loss().detach().item()
            with torch.no_grad():
                parameter.reshape(-1)[index] = value - epsilon
            minus = residual_loss().detach().item()
        finally:
            with torch.no_grad():
                parameter.reshape(-1)[index] = value
        finite_difference = torch.tensor((plus-minus)/(2*epsilon), dtype=torch.float64)
        torch.testing.assert_close(analytic, finite_difference, atol=2e-8, rtol=5e-5)

        before = torch.cat([p.detach().reshape(-1).clone() for p in parameters])
        optimizer.step()
        after = torch.cat([p.detach().reshape(-1) for p in parameters])
        self.assertTrue(bool(torch.any(before != after)))
        improved = residual_loss().detach()
        self.assertTrue(bool(torch.isfinite(improved)))
        self.assertLess(improved.item(), loss.detach().item())

    def test_predicted_cell_averages_match_independent_32_point_quadrature(self):
        with torch.random.fork_rng(devices=[]):
            torch.manual_seed(789)
            model = make_model().eval()
        cells = 4
        times = coordinates([0, .31, 1])
        actual = predicted_cell_averages(model, cells, times)
        nodes, weights = np.polynomial.legendre.leggauss(32)
        expected = torch.empty((times.numel(), cells), dtype=torch.float64, device="cpu")
        with torch.no_grad():
            for t_index, t in enumerate(times.tolist()):
                for cell in range(cells):
                    center = (cell+.5)/cells
                    points = coordinates([[center+node/(2*cells), t] for node in nodes])
                    field = predict(model, points)[:, 0]
                    expected[t_index, cell] = sum(float(w)*float(c)/2 for w, c in zip(weights, field))
        torch.testing.assert_close(actual, expected, atol=2e-9, rtol=2e-9)


if __name__ == "__main__":
    unittest.main()
