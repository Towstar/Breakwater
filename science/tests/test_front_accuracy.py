"""F1 checker/measurement regressions; actual native execution is a separate gate.

Synthetic parser buffers below are deliberately not claimed as native simulations.
"""
import copy
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from qualify_front_accuracy import (INTEGRITY, array, evaluate_case, expected_cases, main, norm,
    observation_studies, output_grid, refinement, sampled_crossings, validate_config,
    validate_inputs, validate_matrix, validate_result)
from breakwater_science.front_reference import continuum
from test_front_reference import pulse, scheduled

SCIENCE=Path(__file__).resolve().parents[1]


def config():
    return json.loads((SCIENCE/"f1_front_accuracy_config.json").read_text())


def parser_case():
    c=pulse();c.update(id="front_smooth.space.n64.cfl0.02",study="space")
    times=output_grid(4.,.0125).tolist()
    initial=continuum(c,0.)["concentration_mg_l"].tolist()
    c["initial_mg_l"]=initial
    # A stationary but conserving buffer exercises parser constraints only. It is
    # not a numerical solution and cannot establish accuracy qualification.
    c["result"]=dict(solver_version="fvm-upwind-euler-0.1.0",x_m=[(i+.5)/64 for i in range(64)],
        time_h=times,concentration_mg_l=[initial.copy() for _ in times],
        inventory_mg=[50.]*len(times),cumulative_inlet_mg=[0.]*len(times),
        cumulative_outlet_mg=[0.]*len(times),cumulative_source_mg=[0.]*len(times),
        cumulative_decay_mg=[0.]*len(times),mass_balance_residual_mg=[0.]*len(times),
        diagnostics=dict(substeps=3200,max_cfl_number=.02,minimum_concentration_mg_l=0.,
            max_mass_balance_residual_mg=0.))
    return c


class MatrixTests(unittest.TestCase):
    def metadata(self):
        c=config();specs=expected_cases(c)
        return dict(fixture_version=1,qualification_variant="front_accuracy",solver_version="fvm-upwind-euler-0.1.0",
            cases=[dict(id=i) for i in specs],rejections=[dict(id=k,exception_type=v,message="explicit rejection")
                for k,v in c["expected_rejections"].items()])

    def test_declared_case_families_and_sizes(self):
        specs=expected_cases(config())
        self.assertEqual(len(specs),22)
        self.assertEqual(sum(s["regime"]=="front_box" for s in specs.values()),8)
        self.assertEqual(sum(s["regime"]=="front_smooth" for s in specs.values()),8)
        self.assertEqual(sum(s["study"]=="extended_space" for s in specs.values()),3)
        self.assertEqual(specs["scheduled_inlet.space.n512.cfl0.1"]["step"],.05)

    def test_metadata_matrix_validates_without_inventing_fields(self):
        self.assertEqual(len(validate_matrix(self.metadata(),config())),22)

    def test_missing_duplicate_foreign_case_ids_rejected(self):
        for mode in ["missing","duplicate","foreign"]:
            d=self.metadata()
            if mode=="missing":d["cases"].pop()
            elif mode=="duplicate":d["cases"][-1]=d["cases"][0].copy()
            else:d["cases"][-1]["id"]="invented"
            with self.subTest(mode=mode),self.assertRaises(ValueError):validate_matrix(d,config())

    def test_wrong_method_variant_and_boolean_version_rejected(self):
        for key,value in [("fixture_version",True),("qualification_variant","baseline"),("solver_version","foreign")]:
            d=self.metadata();d[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):validate_matrix(d,config())

    def test_missing_or_changed_budget_rejection_rejected(self):
        for mode in ["missing","type","message"]:
            d=self.metadata()
            if mode=="missing":d["rejections"].pop()
            elif mode=="type":d["rejections"][0]["exception_type"]="ignored"
            else:d["rejections"][0]["message"]=""
            with self.subTest(mode=mode),self.assertRaises(ValueError):validate_matrix(d,config())

    def test_predeclared_gates_are_not_relaxable(self):
        for key in INTEGRITY:
            c=config();c["integrity"][key]*=2
            with self.subTest(key=key),self.assertRaises(ValueError):validate_config(c)

    def test_changed_matrix_and_thresholds_rejected(self):
        for field in ["thresholds_mg_l","observation_strides"]:
            c=config();c[field].pop()
            with self.assertRaises(ValueError):validate_config(c)
        c=config();c["initial_pulses"]["space_cells"][-1]=1024
        with self.assertRaises(ValueError):validate_config(c)
        c=config();c["study_version"]=True
        with self.assertRaises(ValueError):validate_config(c)

    def test_legacy_a9_configs_unchanged(self):
        # Fixed historical forcing and gates; only A9's existing refined mesh
        # declaration differs. F1 is an independent config, not a replacement.
        a=json.loads((SCIENCE/"a9_transport_config.json").read_text())
        b=json.loads((SCIENCE/"a9_transport_refined_config.json").read_text())
        self.assertEqual(a["expected_case_count"],45)
        self.assertEqual(a["common_gates"],b["common_gates"])
        self.assertEqual(a["spatial_gates"],b["spatial_gates"])
        self.assertEqual(a["temporal_gates"],b["temporal_gates"])


class NativeEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.c=config();self.case=parser_case();self.spec=expected_cases(self.c)[self.case["id"]]
        self.initial=validate_inputs(self.case,self.spec,self.c)

    def test_well_shaped_parser_buffer_is_not_an_accuracy_verdict(self):
        t,f,ledger,residual=validate_result(self.case,self.initial,self.c)
        self.assertEqual(f.shape,(321,64));self.assertLess(residual,1e-12)

    def test_wrong_forcing_geometry_and_selection_identity_rejected(self):
        for mode in ["flow","inlet","geometry","family","step","source","support"]:
            c=copy.deepcopy(self.case)
            if mode=="flow":c["flow"][0]["flow_m3h"]+=.001
            elif mode=="inlet":c["inlet"][0]["concentration_mg_l"]=.01
            elif mode=="geometry":c["pipe"]["area_m2"]*=1000
            elif mode=="family":c["regime"]="front_box"
            elif mode=="step":c["config"]["output_step_h"]*=2
            elif mode=="source":c["source_mg_l_h"][0]=.1
            else:c["reference_parameters"]["bump_start_m"]+=.001
            with self.subTest(mode=mode),self.assertRaises(ValueError):validate_inputs(c,self.spec,self.c)

    def test_forged_initial_averages_rejected(self):
        self.case["initial_mg_l"][0]=.2
        with self.assertRaises(ValueError):validate_inputs(self.case,self.spec,self.c)

    def test_missing_shifted_duplicate_output_times_rejected_exactly(self):
        for mode in ["missing","shifted","duplicate"]:
            c=copy.deepcopy(self.case)
            if mode=="missing":c["result"]["time_h"].pop()
            elif mode=="shifted":c["result"]["time_h"][1]=math.nextafter(.0125,math.inf)
            else:c["result"]["time_h"][1]=0.
            with self.subTest(mode=mode),self.assertRaises(ValueError):validate_result(c,self.initial,self.c)

    def test_shifted_or_reversed_cell_identity_rejected(self):
        for mode in ["shifted","reversed"]:
            c=copy.deepcopy(self.case)
            if mode=="reversed":c["result"]["x_m"].reverse()
            else:c["result"]["x_m"][0]=math.nextafter(c["result"]["x_m"][0],math.inf)
            with self.assertRaises(ValueError):validate_result(c,self.initial,self.c)

    def test_malformed_nonfinite_boolean_and_clipped_field_rejected(self):
        for bad in [math.nan,math.inf,True,"0",None,-.01,1.01]:
            c=copy.deepcopy(self.case);c["result"]["concentration_mg_l"][1][0]=bad
            with self.subTest(value=bad),self.assertRaises(ValueError):validate_result(c,self.initial,self.c)
        c=copy.deepcopy(self.case);c["result"]["concentration_mg_l"][1].pop()
        with self.assertRaises(ValueError):validate_result(c,self.initial,self.c)

    def test_corrupted_concentration_inventory_and_signed_ledger_rejected(self):
        for name in ["inventory_mg","cumulative_outlet_mg","cumulative_inlet_mg","mass_balance_residual_mg",
                     "cumulative_source_mg","cumulative_decay_mg"]:
            c=copy.deepcopy(self.case);c["result"][name][-1]+=1.
            with self.subTest(name=name),self.assertRaises(ValueError):validate_result(c,self.initial,self.c)
        c=copy.deepcopy(self.case);c["result"]["concentration_mg_l"][-1][0]+=.1
        with self.assertRaises(ValueError):validate_result(c,self.initial,self.c)

    def test_wrong_litre_conversion_rejected(self):
        self.case["result"]["inventory_mg"]=[.05]*321
        with self.assertRaises(ValueError):validate_result(self.case,self.initial,self.c)

    def test_invalid_diagnostics_work_cfl_and_provenance_rejected(self):
        for key,value in [("substeps",True),("substeps",1000001),("substeps",0),
                          ("max_cfl_number",.021),("minimum_concentration_mg_l",.1),
                          ("max_mass_balance_residual_mg",math.inf)]:
            c=copy.deepcopy(self.case);c["result"]["diagnostics"][key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):validate_result(c,self.initial,self.c)
        c=copy.deepcopy(self.case);c["result"]["solver_version"]="foreign"
        with self.assertRaises(ValueError):validate_result(c,self.initial,self.c)

    def test_bad_reference_check_cannot_fall_back(self):
        with patch("qualify_front_accuracy.reference_check",return_value={"crosscheck_disagreement_mg_l":1e-5}):
            with self.assertRaisesRegex(ValueError,"Reference cross-check failed"):
                evaluate_case(self.case,self.spec,self.c)


class ObservationTests(unittest.TestCase):
    def test_crossing_returns_bracket_not_invented_exact_time(self):
        r=sampled_crossings([0.,1.,2.],[0.,.4,.8],.5)
        self.assertEqual(r["first_upward_bracket_h"],[1.,2.]);self.assertNotIn("interpolated_h",r)

    def test_initial_equality_and_repeated_crossings(self):
        r=sampled_crossings([0.,1.,2.,3.,4.],[.5,.4,.6,.3,.7],.5)
        self.assertEqual(r["status"],"initially_exceeded")
        self.assertEqual(r["first_upward_bracket_h"],[0.,0.])
        self.assertEqual(r["upward_crossing_count"],3);self.assertTrue(r["multiple_upward_crossings"])

    def test_no_crossing_and_unavailable_discharge_are_distinct(self):
        self.assertEqual(sampled_crossings([0.,1.],[.1,.2],.5)["status"],"not_reached")
        self.assertEqual(sampled_crossings([0.,1.],[None,None],.5)["status"],"unavailable")

    def test_missing_discharge_is_not_bridged(self):
        r=sampled_crossings([0.,1.,2.],[0.,None,.8],.5)
        self.assertEqual(r["status"],"unresolved_gap");self.assertIsNone(r["first_upward_bracket_h"])

    def test_paused_outlet_field_is_not_a_discharge_observation(self):
        c=scheduled();times=np.array([0.,.137,.2,.293,.4])
        field=np.zeros((5,64));field[1:3,-1]=1.
        rows=observation_studies(c,times,field,config())
        self.assertTrue(all(r["native"]["first_upward_bracket_h"] is None for r in rows))

    def test_boolean_sample_cannot_impersonate_exact_numeric_one(self):
        with self.assertRaisesRegex(ValueError,"Boolean"):
            array([0.,True],(2,),"forged numeric one")

    def test_invalid_trace_times_values_and_threshold_rejected(self):
        for times,values,threshold in [([0.,0.],[0.,1.],.5),([0.,math.inf],[0.,1.],.5),
                ([0.,1.],[0.,True],.5),([0.,1.],[0.,math.nan],.5),([0.,1.],[0.,1.],0.)]:
            with self.assertRaises(ValueError):sampled_crossings(times,values,threshold)

    def test_decimation_uses_original_indices_and_retains_endpoint(self):
        c=pulse();times=output_grid(4.,.0125)
        field=np.zeros((len(times),64));field[200:240,-1]=1.
        original=field.copy();result=observation_studies(c,times,field,config())
        np.testing.assert_array_equal(field,original)
        self.assertEqual(len(result),9)
        for r in result:
            self.assertEqual(r["original_indices"][-1],320)
            self.assertEqual(r["original_indices"],list(range(0,321,r["stride"])))

    def test_endpoint_retained_when_stride_does_not_divide_grid(self):
        c=pulse();times=np.array([0.,.1,.2,.3,.4,.5])
        result=observation_studies(c,times,np.zeros((6,64)),config())
        self.assertEqual(result[-1]["original_indices"],[0,4,5])

    def test_sparse_observations_can_miss_a_pulse_honestly(self):
        c=pulse();times=output_grid(4.,.0125);field=np.zeros((321,64));field[161,-1]=1.
        result=observation_studies(c,times,field,config())
        self.assertFalse(result[1]["crossing_missed"])
        self.assertTrue(result[4]["crossing_missed"])

    def test_exact_grid_retains_one_ulp_endpoint(self):
        horizon=math.nextafter(.9,math.inf);times=output_grid(horizon,.1)
        self.assertEqual(len(times),11);self.assertEqual(times[-2],.9);self.assertEqual(times[-1],horizon)

    def test_norms_have_concentration_units_and_independent_values(self):
        r=norm([1.,-2.,2.]);self.assertEqual(r["maximum_mg_l"],2.)
        self.assertAlmostEqual(r["mean_l1_mg_l"],5/3);self.assertAlmostEqual(r["rms_mg_l"],math.sqrt(3))

    def test_norms_preserve_tiny_values_without_auxiliary_underflow(self):
        result=norm([1e-200,0.])
        self.assertGreater(result["rms_mg_l"],0.)
        self.assertAlmostEqual(result["rms_mg_l"]/(1e-200/math.sqrt(2)),1.,places=14)

    def test_reference_floor_marks_orders_unresolved(self):
        rows=[dict(regime="front_box",study="space",cells=n,cfl_safety=.02,id=str(n),
            worst_spatial_error=dict(mean_l1_mg_l=e),reference=dict(uncertainty_estimate_mg_l=1e-12))
            for n,e in [(64,1e-13),(128,5e-14)]]
        self.assertEqual(refinement(rows,config())[0]["observed_orders"],[None])


class CommandTests(unittest.TestCase):
    def test_existing_attempt_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory);sentinel=path/"keep.txt";sentinel.write_text("original")
            self.assertEqual(main(["--native","absent.exe","--output",directory]),2)
            self.assertEqual(sentinel.read_text(),"original");self.assertEqual(len(list(path.iterdir())),1)

    def test_invalid_config_is_retained_as_failed_attempt(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/"attempt";bad=Path(directory)/"config.json";bad.write_text("{}")
            self.assertEqual(main(["--native","absent.exe","--config",str(bad),"--output",str(output)]),1)
            report=json.loads((output/"report.json").read_text())
            self.assertEqual(report["status"],"failed");self.assertEqual(report["accuracy_status"],"not_established")
            self.assertEqual((output/"config-input.json").read_text(),"{}")
            self.assertTrue((output/"preflight.json").is_file())

    def test_nonzero_native_exit_retains_raw_diagnostics(self):
        with tempfile.TemporaryDirectory() as directory:
            native=Path(directory)/"native.exe";native.write_bytes(b"test-only executable identity")
            output=Path(directory)/"attempt"
            fake=subprocess.CompletedProcess([],7,stdout=b"partial",stderr=b"failure")
            with patch("qualify_front_accuracy.subprocess.run",return_value=fake):
                self.assertEqual(main(["--native",str(native),"--output",str(output)]),1)
            self.assertEqual((output/"native-reference.json").read_bytes(),b"partial")
            self.assertEqual((output/"native-stderr.txt").read_bytes(),b"failure")
            self.assertTrue((output/"preflight.json").is_file())


if __name__=="__main__":
    unittest.main()
