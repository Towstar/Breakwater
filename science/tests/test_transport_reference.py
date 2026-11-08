"""Independent A9 analytic averages, characteristic ledgers and spectral checks."""
import copy
import math
from pathlib import Path
import sys
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from breakwater_science.transport_reference import continuum, reference_diagnostics, semidiscrete


def case(regime, cells=16):
    diffusion, decay, horizon, boundary = 0.0, 0.0, .5, "dirichlet_inlet"
    parameters = {"bump_start_m": .125, "bump_width_m": .5}
    velocity = .25
    source = 0.0
    inlet = [{"time_h": 0.0, "concentration_mg_l": 0.0}]
    flow = None
    if regime == "closed_diffusion":
        diffusion, horizon, boundary = .05, .2, "closed_no_flux"
        parameters, velocity, inlet = {"mean_mg_l": .6, "amplitude_mg_l": .3}, 0.0, []
    elif regime == "open_diffusion":
        diffusion, horizon, velocity = .05, .2, 0.0
        parameters = {"amplitude_mg_l": .7}
    elif regime == "mixed_adr":
        diffusion, decay = .1, .1
        parameters = {"mode_fraction": .75}
        b = .75*math.pi
        velocity = 2*diffusion*(-b/math.tan(b))
    elif regime in ("reaction_source", "source_only"):
        decay = 1.0 if regime == "reaction_source" else 0.0
        horizon, boundary, velocity, source, inlet = 1.0, "closed_no_flux", 0.0, .25, []
        parameters = {"initial_mg_l": 1.0 if decay else .1, "source_mg_l_h": source}
    if regime in ("scheduled_advection", "scheduled_inlet"):
        horizon = .6
        flow = [{"time_h": t, "flow_m3h": v*.2} for t, v in ((0, .4), (.137, 0), (.293, .7))]
        if regime == "scheduled_inlet":
            parameters = {"initial_mg_l": 0.0}
            inlet = [{"time_h": t, "concentration_mg_l": value}
                     for t, value in ((0, 0), (.11, 1), (.29, .2), (.47, .8), (.63, 0))]
    return {"regime": regime,
            "pipe": {"cells": cells, "length_m": 1.0, "area_m2": .2,
                     "dispersion_m2_h": diffusion, "decay_per_h": decay},
            "config": {"horizon_h": horizon, "output_step_h": .1,
                       "cfl_safety": .5, "boundary": boundary},
            "flow": flow or [{"time_h": 0.0, "flow_m3h": velocity*.2}],
            "inlet": inlet, "source_mg_l_h": [source]*cells,
            "reference_parameters": parameters}


def quadrature(field, cells, *, breaks=()):
    nodes, weights = np.polynomial.legendre.leggauss(48)
    result = []
    for cell in range(cells):
        left, right = cell/cells, (cell+1)/cells
        points = [left, *sorted(x for x in breaks if left < x < right), right]
        integral = 0.0
        for a, b in zip(points, points[1:]):
            samples = [field((a+b)/2+(b-a)*node/2) for node in nodes]
            integral += (b-a)*float(np.dot(weights, samples))/2
        result.append(integral*cells)
    return np.array(result)


def displacement(time):
    return .4*min(time, .137) + .7*max(0.0, time-.293)


def inlet_value(time):
    if time <= .11:
        return time/.11
    if time <= .29:
        return 1-.8*(time-.11)/.18
    if time <= .47:
        return .2+.6*(time-.29)/.18
    if time <= .63:
        return .8-.8*(time-.47)/.16
    return 0.0


def inlet_characteristic(x, time):
    total = displacement(time)
    if x >= total:
        return 0.0
    if time <= .137:
        entered = time-x/.4
    elif time <= .293:
        entered = .137-x/.4
    elif x < .7*(time-.293):
        entered = time-x/.7
    else:
        entered = (total-x)/.4
    return inlet_value(entered)


class TransportReferenceTests(unittest.TestCase):
    def assertClose(self, actual, expected, atol=2e-12, rtol=2e-12):
        np.testing.assert_allclose(actual, expected, atol=atol, rtol=rtol)

    def test_smooth_analytic_cell_averages_match_independent_quadrature(self):
        for regime in ("closed_diffusion", "open_diffusion", "mixed_adr", "reaction_source", "source_only"):
            c = case(regime, 11)
            for time in (0.0, c["config"]["horizon_h"]):
                with self.subTest(regime=regime, time=time):
                    if regime == "closed_diffusion":
                        field = lambda x: .6+.3*math.cos(math.pi*x)*math.exp(-.05*math.pi**2*time)
                    elif regime == "open_diffusion":
                        field = lambda x: .7*math.sin(math.pi*x/2)*math.exp(-.05*(math.pi/2)**2*time)
                    elif regime == "mixed_adr":
                        b = .75*math.pi
                        a = -b/math.tan(b)
                        rate = .1+.1*(a*a+b*b)
                        field = lambda x: math.exp(a*(x-1)-rate*time)*math.sin(b*x)/math.sin(b)
                    elif regime == "reaction_source":
                        field = lambda x: .25+.75*math.exp(-time)
                    else:
                        field = lambda x: .1+.25*time
                    self.assertClose(continuum(c, time)["concentration_mg_l"], quadrature(field, 11))

    def test_translated_pulse_cell_averages_match_piecewise_quadrature(self):
        for regime in ("advection", "scheduled_advection"):
            c = case(regime, 13)
            for time in (0.0, .15, .4):
                with self.subTest(regime=regime, time=time):
                    shift = .25*time if regime == "advection" else displacement(time)
                    def field(x):
                        z = x-shift-.125
                        return math.sin(math.pi*z/.5)**2 if 0 < z < .5 else 0.0
                    expected = quadrature(field, 13, breaks=(.125+shift, .625+shift))
                    self.assertClose(continuum(c, time)["concentration_mg_l"], expected)

    def test_native_initial_and_result_values_are_not_used_by_oracles(self):
        for regime in ("advection", "closed_diffusion", "mixed_adr", "scheduled_inlet"):
            c = case(regime)
            expected = continuum(c, .1)
            sd = semidiscrete(c, .1)
            altered = copy.deepcopy(c)
            altered["initial_mg_l"] = [float("nan")]*16
            altered["result"] = {"concentration_mg_l": [[float("inf")]*16]}
            self.assertClose(continuum(altered, .1)["concentration_mg_l"], expected["concentration_mg_l"])
            if sd is not None:
                self.assertClose(semidiscrete(altered, .1), sd)

    def test_analytic_inventory_and_signed_ledgers_agree(self):
        for regime in ("advection", "scheduled_advection", "closed_diffusion", "open_diffusion",
                       "mixed_adr", "reaction_source", "source_only", "scheduled_inlet"):
            c = case(regime, 32)
            initial = continuum(c, 0.0)["inventory_mg"]
            for time in (0.0, c["config"]["horizon_h"]):
                with self.subTest(regime=regime, time=time):
                    r = continuum(c, time)
                    self.assertClose(r["inventory_mg"], 200*np.mean(r["concentration_mg_l"]), atol=2e-11)
                    residual = (r["inventory_mg"]-initial-r["cumulative_inlet_mg"]+
                                r["cumulative_outlet_mg"]-r["cumulative_source_mg"]+r["cumulative_decay_mg"])
                    self.assertClose(residual, 0.0, atol=5e-11)

    def test_open_diffusion_exchanges_negative_mass_without_water_flow(self):
        r = continuum(case("open_diffusion"), .2)
        initial = .7*200/(math.pi/2)
        self.assertLess(r["cumulative_inlet_mg"], 0.0)
        self.assertEqual(r["cumulative_outlet_mg"], 0.0)
        self.assertClose(r["inventory_mg"], initial+r["cumulative_inlet_mg"])

    def test_source_and_decay_ledgers_have_independent_ode_values(self):
        r = continuum(case("reaction_source"), 1.0)
        expected_final = .25+.75/math.e
        self.assertClose(r["inventory_mg"], 200*expected_final)
        self.assertClose(r["cumulative_source_mg"], 50.0)
        self.assertClose(r["cumulative_decay_mg"], 200*(.25+.75*(1-1/math.e)))
        s = continuum(case("source_only"), 1.0)
        self.assertClose(s["inventory_mg"], 70.0)
        self.assertEqual(s["cumulative_decay_mg"], 0.0)

    def test_scheduled_inlet_characteristics_match_piecewise_quadrature(self):
        c = case("scheduled_inlet", 11)
        for time in (.1, .2, .4, .6):
            with self.subTest(time=time):
                boundaries = [displacement(time)-displacement(knot)
                              for knot in (0, .11, .137, .29, .293, .47) if knot < time]
                expected = quadrature(lambda x: inlet_characteristic(x, time), 11,
                                      breaks=sorted(set(boundaries)))
                self.assertClose(continuum(c, time)["concentration_mg_l"], expected)

    def test_scheduled_inlet_mass_uses_velocity_weighted_time_integrals(self):
        c = case("scheduled_inlet")
        # Direct independent interval arithmetic: .4*(.055+.027*.94)
        # + .7*(.177*(.21+.8)/2 + .13*(.8+.15)/2), then 200 L/m.
        r = continuum(c, .6)
        self.assertClose(r["cumulative_inlet_mg"], 27.5893)
        self.assertEqual(r["cumulative_outlet_mg"], 0.0)
        self.assertClose(continuum(c, .2)["inventory_mg"], 6.4304)
        np.testing.assert_array_equal(continuum(c, .15)["concentration_mg_l"],
                                      continuum(c, .29)["concentration_mg_l"])

    def test_flow_pause_freezes_pulse_and_semidiscrete_reference(self):
        c = case("scheduled_advection")
        for function in (lambda t: continuum(c, t)["concentration_mg_l"], lambda t: semidiscrete(c, t)):
            np.testing.assert_array_equal(function(.15), function(.29))
        self.assertEqual(continuum(c, .6)["cumulative_inlet_mg"], 0.0)

    def test_initial_time_has_exact_semidiscrete_and_zero_ledger_limits(self):
        for regime in ("advection", "scheduled_advection", "closed_diffusion", "open_diffusion",
                       "mixed_adr", "reaction_source", "source_only"):
            c = case(regime)
            r = continuum(c, 0.0)
            np.testing.assert_array_equal(semidiscrete(c, 0.0), r["concentration_mg_l"])
            for key, value in r.items():
                if key.startswith("cumulative_"):
                    self.assertEqual(value, 0.0)
        self.assertIsNone(semidiscrete(case("scheduled_inlet"), .2))

    def test_upwind_poisson_solution_matches_independent_two_cell_odes(self):
        c = case("advection", 2)
        initial = continuum(c, 0.0)["concentration_mg_l"]
        time, rate = .4, .5  # v/dx=.25/.5
        expected = math.exp(-rate*time)*np.array([initial[0], initial[1]+rate*time*initial[0]])
        self.assertClose(semidiscrete(c, time), expected)

    def test_discrete_diffusion_mode_rates_match_face_flux_generator(self):
        for regime in ("closed_diffusion", "open_diffusion"):
            c = case(regime, 9)
            initial = continuum(c, 0.0)["concentration_mg_l"]
            mean = .6 if regime == "closed_diffusion" else 0.0
            mode = initial-mean
            flux = np.zeros(10)
            flux[1:-1] = -.05*np.diff(mode)*9
            if regime == "open_diffusion":
                flux[0] = -2*.05*mode[0]*9
            derivative = (flux[:-1]-flux[1:])*9
            beta = math.pi if regime == "closed_diffusion" else math.pi/2
            rate = 4*.05*math.sin(beta/18)**2*81
            self.assertClose(derivative, -rate*mode)

    def test_mixed_eigensolution_matches_independent_face_flux_derivative(self):
        c = case("mixed_adr", 7)
        initial = continuum(c, 0.0)["concentration_mg_l"]
        velocity = c["flow"][0]["flow_m3h"]/.2
        flux = np.empty(8)
        flux[0] = -2*.1*initial[0]*7
        flux[1:-1] = velocity*initial[:-1]-.1*np.diff(initial)*7
        flux[-1] = velocity*initial[-1]
        expected = (flux[:-1]-flux[1:])*7-.1*initial
        dt = 1e-5
        first = (semidiscrete(c, dt)-initial)/dt
        half = (semidiscrete(c, dt/2)-initial)/(dt/2)
        self.assertClose(2*half-first, expected, atol=3e-8, rtol=3e-8)

    def test_mixed_reference_eigen_checks_positivity_and_mesh_refinement(self):
        errors = []
        for cells in (16, 32, 64):
            c = case("mixed_adr", cells)
            diagnostics = reference_diagnostics(c)
            self.assertLess(diagnostics["normalized_eigen_residual"], 1e-12)
            self.assertLess(diagnostics["orthogonality_error"], 1e-12)
            self.assertLess(diagnostics["normalized_reconstruction_error"], 1e-11)
            self.assertLess(diagnostics["initial_reconstruction_maximum_mg_l"], 1e-11)
            self.assertLess(diagnostics["max_eigenvalue_per_h"], 0.0)
            self.assertGreater(diagnostics["similarity_condition"], 1.0)
            self.assertGreater(diagnostics["floating_reference_uncertainty_mg_l"], 0.0)
            solution = semidiscrete(c, .25)
            self.assertTrue(np.isfinite(solution).all())
            self.assertGreaterEqual(float(solution.min()), -1e-12)
            errors.append(float(np.mean(np.abs(solution-continuum(c, .25)["concentration_mg_l"]))))
        self.assertLess(errors[1], errors[0])
        self.assertLess(errors[2], errors[1])

    def test_mixed_modes_have_independent_continuum_flux_balance(self):
        for fraction in (.6, .75, .85):
            c = case("mixed_adr")
            c["reference_parameters"]["mode_fraction"] = fraction
            b = fraction*math.pi
            a = -b/math.tan(b)
            c["flow"][0]["flow_m3h"] = 2*.1*a*.2
            r = continuum(c, .5)
            self.assertLess(r["cumulative_inlet_mg"], 0.0)
            self.assertGreater(r["cumulative_outlet_mg"], 0.0)
            self.assertGreater(r["cumulative_decay_mg"], 0.0)
            self.assertClose(r["inventory_mg"]-continuum(c, 0)["inventory_mg"],
                             r["cumulative_inlet_mg"]-r["cumulative_outlet_mg"]-r["cumulative_decay_mg"])

    def test_missing_or_inconsistent_reference_parameters_fail_explicitly(self):
        c = case("advection")
        c["reference_parameters"].pop("bump_start_m")
        with self.assertRaises(KeyError):
            continuum(c, .1)
        c = case("mixed_adr")
        c["flow"][0]["flow_m3h"] *= 2
        with self.assertRaisesRegex(ValueError, "Flow"):
            continuum(c, .1)
        c = case("advection")
        c["source_mg_l_h"][0] = 1.0
        with self.assertRaisesRegex(ValueError, "Source"):
            continuum(c, .1)
        with self.assertRaisesRegex(ValueError, "outside"):
            continuum(case("advection"), -.1)


if __name__ == "__main__":
    unittest.main()
