"""Independent physical examples for F1 references; no native result fixtures."""
import math
from pathlib import Path
import sys
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from breakwater_science.front_reference import (continuum, displacement, outlet_flow,
    point_values, outlet_peak_reference, poisson_weights, poisson_decimal_disagreement, quadrature_averages,
    reference_check, semidiscrete, threshold_reference)


def pulse(regime="front_smooth", cells=64):
    return dict(regime=regime, pipe=dict(cells=cells, length_m=1., area_m2=.2,
        dispersion_m2_h=0., decay_per_h=0.), config=dict(horizon_h=4., output_step_h=.0125,
        cfl_safety=.02, boundary="dirichlet_inlet", max_substeps=1000000),
        source_mg_l_h=[0.]*cells, initial_mg_l=[999.]*cells,
        reference_parameters=dict(bump_start_m=.125, bump_width_m=.5),
        flow=[dict(time_h=0., flow_m3h=.05)], inlet=[dict(time_h=0., concentration_mg_l=0.)])


def scheduled(cells=64, horizon=2.2):
    case = pulse(cells=cells)
    case.update(regime="scheduled_inlet", reference_parameters=dict(initial_mg_l=0.),
        flow=[dict(time_h=t, flow_m3h=v*.2) for t, v in [(0.,.4),(.137,0.),(.293,.7)]],
        inlet=[dict(time_h=t, concentration_mg_l=v) for t, v in [(0.,0.),(.11,1.),(.29,.2),(.47,.8),(.63,0.)]])
    case["config"]["horizon_h"] = horizon
    return case


class PhysicalReferenceTests(unittest.TestCase):
    def test_smooth_initial_integral_and_litre_conversion(self):
        c = pulse()
        ref = continuum(c, 0.)
        self.assertAlmostEqual(ref["inventory_mg"], 50., places=12)
        self.assertAlmostEqual(sum(ref["concentration_mg_l"])/64, .25, places=13)

    def test_box_partial_cell_averages_are_geometric(self):
        c = pulse("front_box", cells=4)
        np.testing.assert_array_equal(continuum(c, 0.)["concentration_mg_l"], [.5,1.,.5,0.])
        np.testing.assert_array_equal(continuum(c, .5)["concentration_mg_l"], [0.,1.,1.,0.])
        self.assertEqual(continuum(c, 0.)["inventory_mg"], 100.)

    def test_translated_pulses_leave_pipe_without_mass_renormalization(self):
        for kind, mass in [("front_box",100.), ("front_smooth",50.)]:
            c = pulse(kind)
            ref = continuum(c, 4.)
            np.testing.assert_array_equal(ref["concentration_mg_l"], np.zeros(64))
            self.assertEqual(ref["inventory_mg"], 0.)
            self.assertEqual(ref["cumulative_outlet_mg"], mass)
            semi = semidiscrete(c, 4.)
            self.assertGreater(float(np.sum(semi)), 0.)
            self.assertLess(float(np.sum(semi)), float(np.sum(continuum(c, 0.)["concentration_mg_l"])))

    def test_native_buffers_do_not_construct_reference(self):
        c = pulse()
        a = continuum(c, 1.)
        c["initial_mg_l"] = [-123.]*64
        c["result"] = dict(concentration_mg_l=[[987.]*64])
        np.testing.assert_array_equal(a["concentration_mg_l"], continuum(c, 1.)["concentration_mg_l"])

    def test_point_outlet_is_distinct_from_last_cell_average(self):
        c = pulse("front_box", cells=4)
        self.assertEqual(float(point_values(c, [1.], 1.25)[0]), 0.)
        self.assertEqual(continuum(c, 1.25)["concentration_mg_l"][-1], .75)

    def test_box_point_arrivals_use_right_continuous_time(self):
        c = pulse("front_box")
        self.assertEqual(float(point_values(c, [1.], math.nextafter(1.5,0.))[0]), 0.)
        self.assertEqual(float(point_values(c, [1.], 1.5)[0]), 1.)
        self.assertEqual(float(point_values(c, [1.], 3.5)[0]), 0.)

    def test_smooth_point_peak_and_exact_first_crossing(self):
        c = pulse()
        self.assertEqual(float(point_values(c, [1.], 2.5)[0]), 1.)
        self.assertEqual(threshold_reference(c, .5)["first_upward_h"], 2.)
        for threshold in [.1,.5,.9]:
            t = 1.5+math.acos(1-2*threshold)/math.pi
            self.assertAlmostEqual(threshold_reference(c, threshold)["first_upward_h"], t, places=14)
            self.assertAlmostEqual(float(point_values(c, [1.], t)[0]), threshold, places=13)

    def test_all_box_thresholds_cross_at_front_arrival(self):
        for threshold in [.1,.5,.9]:
            self.assertEqual(threshold_reference(pulse("front_box"),threshold)["upward_crossings_h"], [1.5])

    def test_no_crossing_before_horizon(self):
        c = pulse(); c["config"]["horizon_h"] = 1.
        self.assertIsNone(threshold_reference(c,.5)["first_upward_h"])

    def test_scheduled_displacement_and_pause(self):
        c = scheduled()
        self.assertAlmostEqual(displacement(c, .137), .0548, places=15)
        self.assertEqual(displacement(c, .2), displacement(c, .293))
        np.testing.assert_array_equal(continuum(c, .15)["concentration_mg_l"], continuum(c, .25)["concentration_mg_l"])
        self.assertEqual(outlet_flow(c, .137), 0.)
        self.assertAlmostEqual(outlet_flow(c, .293), .14, places=15)

    def test_scheduled_first_and_repeated_crossings(self):
        c = scheduled()
        result = threshold_reference(c, .5)
        first = .293+(1.-.4*(.137-.055))/.7
        self.assertAlmostEqual(result["first_upward_h"], first, places=14)
        self.assertEqual(result["upward_crossing_count"], 2)
        self.assertAlmostEqual(result["upward_crossings_h"][1], .38+1/.7, places=14)
        self.assertIsNone(threshold_reference(scheduled(horizon=.6),.1)["first_upward_h"])

    def test_characteristic_peak_is_not_a_sampled_peak(self):
        for kind in ["front_smooth","front_box"]:
            self.assertEqual(outlet_peak_reference(pulse(kind)),1.)
        self.assertEqual(outlet_peak_reference(scheduled(horizon=.6)),0.)
        self.assertEqual(outlet_peak_reference(scheduled()),1.)
        c=pulse();c["config"]["horizon_h"]=2.
        self.assertAlmostEqual(outlet_peak_reference(c),.5,places=14)

    def test_no_discharge_has_no_effluent_peak(self):
        c=scheduled();c["flow"]=[dict(time_h=0.,flow_m3h=0.)]
        self.assertIsNone(outlet_peak_reference(c))

    def test_scheduled_mass_and_independent_spatial_quadrature(self):
        c = scheduled(cells=32)
        for t in [.0,.11,.137,.2,.293,.47,.6,1.65,1.8,2.2]:
            ref = continuum(c,t)
            np.testing.assert_allclose(ref["concentration_mg_l"], quadrature_averages(c,t,64),atol=2e-13,rtol=0)
            self.assertAlmostEqual(ref["inventory_mg"], ref["cumulative_inlet_mg"]-ref["cumulative_outlet_mg"],places=11)
        self.assertIsNone(semidiscrete(c,1.))

    def test_initial_references_agree_with_split_quadrature(self):
        for regime in ["front_smooth","front_box"]:
            c = pulse(regime, cells=17)
            for t in [0.,.1,1.5,2.137,3.5,4.]:
                np.testing.assert_allclose(continuum(c,t)["concentration_mg_l"],quadrature_averages(c,t,64),atol=2e-14,rtol=0)

    def test_reference_check_reports_uncertainty_not_formal_bound(self):
        result = reference_check(pulse(),np.array([0.,1.,4.]))
        self.assertLess(result["crosscheck_disagreement_mg_l"],1e-10)
        self.assertGreater(result["uncertainty_estimate_mg_l"],0.)
        self.assertIn("not a formal",result["scope"])

    def test_invalid_reference_domain_is_rejected(self):
        for key,value in [("cells",True),("cells",1),("area_m2",0.),("decay_per_h",.1),("length_m",math.inf)]:
            with self.subTest(key=key,value=value):
                c = pulse();c["pipe"][key]=value
                with self.assertRaises(ValueError):continuum(c,0.)
        for t in [-1.,math.nan,5.]:
            with self.assertRaises(ValueError):continuum(pulse(),t)
        for x in [-.1,math.inf,1.1]:
            with self.assertRaises(ValueError):point_values(pulse(),[x],0.)

    def test_invalid_source_schedule_and_threshold_are_rejected(self):
        c=pulse();c["source_mg_l_h"][0]=1.
        with self.assertRaises(ValueError):continuum(c,0.)
        c=pulse();c["flow"].append(c["flow"][0].copy())
        with self.assertRaises(ValueError):displacement(c,1.)
        for threshold in [0.,1.,math.nan,True]:
            with self.assertRaises(ValueError):threshold_reference(pulse(),threshold)


class PoissonSemigroupTests(unittest.TestCase):
    def test_zero_clock_is_identity(self):
        np.testing.assert_array_equal(poisson_weights(0.,4),[1.,0.,0.,0.])
        c=pulse();np.testing.assert_array_equal(semidiscrete(c,0.),continuum(c,0.)["concentration_mg_l"])

    def test_known_one_and_two_cell_convolution(self):
        weights=poisson_weights(2.,3)
        np.testing.assert_allclose(weights,np.exp(-2)*np.array([1.,2.,2.]),atol=1e-16,rtol=0)
        c=pulse("front_box",4);z=1.
        expected=np.exp(-z)*np.array([.5,1.5,1.75,13/12])
        np.testing.assert_allclose(semidiscrete(c,1.),expected,atol=2e-16,rtol=0)

    def test_underflowing_zero_order_weight_does_not_erase_mode(self):
        values=poisson_weights(800.,1024)
        self.assertEqual(values[0],0.)
        self.assertGreater(values[800],.01)
        self.assertLess(poisson_decimal_disagreement(800.,1024),1e-12)

    def test_kernel_is_never_renormalized(self):
        weights=poisson_weights(10.,3)
        self.assertLess(float(np.sum(weights)),.01)
        self.assertAlmostEqual(float(np.sum(weights)),61*math.exp(-10),places=16)

    def test_semigroup_composes_with_open_escape(self):
        c=pulse("front_box",32)
        initial=continuum(c,0.)["concentration_mg_l"]
        a=np.convolve(initial,poisson_weights(4.,32))[:32]
        b=np.convolve(a,poisson_weights(4.,32))[:32]
        np.testing.assert_allclose(b,semidiscrete(c,1.),atol=8e-16,rtol=0)

    def test_invalid_clock_and_count(self):
        for z,n in [(-1.,3),(math.inf,3),(True,3),(1.,0),(1.,True)]:
            with self.assertRaises(ValueError):poisson_weights(z,n)


if __name__=="__main__":
    unittest.main()
