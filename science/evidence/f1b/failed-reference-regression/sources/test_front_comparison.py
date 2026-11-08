"""F1b analysis/protocol tests. Synthetic buffers below are parser evidence only.

Actual 22/28-case native executions are separate evidence gates.
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
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import compare_front_accuracy as study
import qualify_front_accuracy as f1
from breakwater_science import front_reference
from test_front_accuracy import parser_case

SCIENCE=Path(__file__).resolve().parents[1]
def base(): return json.loads((SCIENCE/"f1_front_accuracy_config.json").read_bytes())
def config(): return json.loads((SCIENCE/"f1b_front_comparison_config.json").read_bytes())
def candidate():
    c=parser_case();r=c["result"];r["solver_version"]=study.METHOD
    steps,cfl=study.integration_schedule(c)
    r["diagnostics"]["substeps"]=steps;r["diagnostics"]["max_cfl_number"]=cfl
    c["execution"]=dict(stage_evaluations=2*steps,cell_stage_updates=2*steps*c["pipe"]["cells"],maximum_concentration_mg_l=1.)
    return c
def gate_rows():
    old=[];new=[]
    for identity in f1.expected_cases(base()):
        row=dict(id=identity,reference=dict(uncertainty_estimate_mg_l=1e-12),
            worst_continuum_error=dict(mean_l1_mg_l=.1,rms_mg_l=.2,maximum_mg_l=.4))
        old.append(row);row=copy.deepcopy(row);row["worst_continuum_error"].update(mean_l1_mg_l=.04,rms_mg_l=.08)
        row["outlet"]=dict(sampled_peak_error_mg_l=-.05)
        row["observations"]=[dict(stride=1,threshold_mg_l=t,continuum=dict(first_upward_h=1.),
            native=dict(first_upward_bracket_h=[1.,1.0125],upward_crossing_count=2 if t==.5 and identity.startswith("scheduled_inlet.extended") else 1)) for t in (.1,.5,.9)]
        new.append(row)
    return dict(cases=old),new

class DeclarationTests(unittest.TestCase):
    def test_fixed_configuration(self): study.validate_config(config())
    def test_boolean_versions_and_counts_rejected(self):
        for key in ("study_version","stage_evaluations_per_step","candidate_case_count"):
            c=config();c[key]=True
            with self.subTest(key=key),self.assertRaises(ValueError): study.validate_config(c)
    def test_nested_boolean_and_changed_targets_rejected(self):
        for value in (True,2):
            c=config();c["targets"]["extended_upward_crossing_counts"][0][1]=value
            with self.subTest(value=value),self.assertRaises(ValueError): study.validate_config(c)
    def test_changed_method_forcing_matrix_and_budget_rejected(self):
        for key,value in (("candidate_solver_version","legacy"),("candidate_cfl_maximum",.9),("candidate_case_count",22),
                          ("baseline_config_sha256","forged"),("candidate_arguments",["--different"])):
            c=config();c[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError): study.validate_config(c)
    def test_exact_22_plus_six_case_identities(self):
        expected=study.candidate_specs(base());self.assertEqual(len(expected),28)
        self.assertEqual({k for k in expected if expected[k]["study"]!="time_probe"},set(f1.expected_cases(base())))
        self.assertEqual(sum(v["study"]=="time_probe" for v in expected.values()),6)
    def metadata(self):
        return dict(fixture_version=1,qualification_variant="front_comparison",solver_version=study.METHOD,
                    cases=[dict(id=k) for k in study.candidate_specs(base())])
    def test_matrix_validates(self): self.assertEqual(len(study.validate_matrix(self.metadata(),base())),28)
    def test_missing_duplicate_foreign_and_extra_cases_rejected(self):
        for mode in ("missing","duplicate","foreign","extra"):
            d=self.metadata()
            if mode=="missing": d["cases"].pop()
            elif mode=="duplicate": d["cases"][-1]=d["cases"][0]
            elif mode=="foreign": d["cases"][-1]["id"]="forged"
            else: d["cases"].append(dict(id="extra"))
            with self.subTest(mode=mode),self.assertRaises(ValueError): study.validate_matrix(d,base())
    def test_root_provenance_bool_version_and_extra_fields_rejected(self):
        for key,value in (("solver_version","fvm-upwind-euler-0.1.0"),("fixture_version",True),
                          ("qualification_variant","front_accuracy"),("extra",0)):
            d=self.metadata();d[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError): study.validate_matrix(d,base())
    def test_nonfinite_and_duplicate_json_rejected(self):
        for raw in ('{"a":NaN}','{"a":Infinity}','{"a":1,"a":2}'):
            with self.subTest(raw=raw),self.assertRaises(ValueError): study.load_json(raw)

class CandidateIntegrityTests(unittest.TestCase):
    def validate(self,c): return study.validate_result(c,np.asarray(c["initial_mg_l"]),base())
    def test_conserving_synthetic_parser_buffer(self): self.validate(candidate())
    def test_shifted_missing_duplicate_timestamps(self):
        for mode in ("shift","missing","duplicate"):
            c=candidate();t=c["result"]["time_h"]
            if mode=="shift":t[3]=math.nextafter(t[3],1.)
            elif mode=="missing":t.pop()
            else:t[3]=t[2]
            with self.subTest(mode=mode),self.assertRaises(ValueError):self.validate(c)
    def test_wrong_cell_order_rejected(self):
        c=candidate();c["result"]["x_m"].reverse()
        with self.assertRaises(ValueError):self.validate(c)
    def test_corrupted_and_boolean_fields_rejected(self):
        for value in (float("nan"),float("inf"),True,-.01,1.01,.75):
            c=candidate();c["result"]["concentration_mg_l"][3][0]=value
            with self.subTest(value=value),self.assertRaises(ValueError):self.validate(c)
    def test_field_shapes_and_initial_identity_rejected(self):
        for mode in ("row","tail","initial"):
            c=candidate();f=c["result"]["concentration_mg_l"]
            if mode=="row": f[3].pop()
            elif mode=="tail": f.pop()
            else:f[0][0]=.1
            with self.subTest(mode=mode),self.assertRaises(ValueError):self.validate(c)
    def test_wrong_per_result_provenance_rejected(self):
        c=candidate();c["result"]["solver_version"]="fvm-upwind-euler-0.1.0"
        with self.assertRaises(ValueError):self.validate(c)
    def test_forged_substeps_even_with_consistent_stage_counts_rejected(self):
        c=candidate();c["result"]["diagnostics"]["substeps"]+=1
        c["execution"]["stage_evaluations"]+=2;c["execution"]["cell_stage_updates"]+=128
        with self.assertRaises(ValueError):self.validate(c)
    def test_stage_counts_boolean_and_work_forgery_rejected(self):
        for key,value in (("stage_evaluations",True),("stage_evaluations",2),("cell_stage_updates",50000001)):
            c=candidate();c["execution"][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.validate(c)
    def test_forged_cfl_and_stage_extrema_rejected(self):
        for target,key,value in (("diagnostics","max_cfl_number",.5),("diagnostics","minimum_concentration_mg_l",.1),
                                  ("execution","maximum_concentration_mg_l",1.01),("execution","maximum_concentration_mg_l",.1)):
            c=candidate();container=c["execution"] if target=="execution" else c["result"][target];container[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.validate(c)
    def test_inconsistent_signed_ledgers_and_litre_units_rejected(self):
        for key in (*f1.LEDGER,"mass_balance_residual_mg"):
            c=candidate();c["result"][key][3]+=1
            with self.subTest(key=key),self.assertRaises(ValueError):self.validate(c)
    def test_nonzero_source_mapping_rejected(self):
        c=candidate();c["source_mg_l_h"][0]=.1
        spec=f1.expected_cases(base())[c["id"]]
        with self.assertRaises(ValueError):study.evaluate_case(c,spec,base())
    def test_candidate_analysis_never_uses_upwind_semidiscrete_solution(self):
        c=candidate();spec=f1.expected_cases(base())[c["id"]]
        with patch.object(f1,"semidiscrete",side_effect=AssertionError("Invalid candidate oracle")):
            result=study.evaluate_case(c,spec,base())
        self.assertEqual(result["error_scope"],"coupled_space_time")
        self.assertNotIn("worst_spatial_error",result)
        self.assertEqual(len(result["per_frame"]),321)
    def test_reference_disagreement_above_frozen_limit_rejected(self):
        c=candidate();spec=f1.expected_cases(base())[c["id"]]
        with patch.object(study,"reference_check",return_value=dict(crosscheck_disagreement_mg_l=1.0001e-10)):
            with self.assertRaisesRegex(ValueError,"Reference cross-check"):study.evaluate_case(c,spec,base())
    def test_candidate_crosscheck_excludes_legacy_poisson_kernel(self):
        c=candidate();spec=f1.expected_cases(base())[c["id"]]
        with patch.object(front_reference,"poisson_decimal_disagreement",side_effect=AssertionError("Legacy-only kernel")):
            result=study.evaluate_case(c,spec,base())
        self.assertEqual(result["reference"]["scope"],"Continuum quadrature/roundoff estimate; no candidate semidiscrete reference or formal enclosure")

class DecisionTests(unittest.TestCase):
    def test_exact_63_gates_with_independent_fixed_expectations(self):
        old,new=gate_rows();r=study.comparisons(old,new,base())
        self.assertEqual((r["gate_count"],r["passed_gate_count"],r["all_targets_met"]),(63,63,True))
    def test_half_error_target_and_regression_are_distinct(self):
        old,new=gate_rows();new[3]["worst_continuum_error"]["mean_l1_mg_l"]=.06
        r=study.comparisons(old,new,base());failed=[g["id"] for g in r["gates"] if not g["passed"]]
        self.assertEqual(failed,[new[3]["id"]+".half_error.mean_l1_mg_l"])
    def test_reference_floor_regression_guard(self):
        old,new=gate_rows();new[0]["worst_continuum_error"]["mean_l1_mg_l"] = .1+5e-12
        r=study.comparisons(old,new,base());self.assertTrue(r["all_targets_met"])
        new[0]["worst_continuum_error"]["mean_l1_mg_l"] = .1+2e-11
        self.assertFalse(study.comparisons(old,new,base())["all_targets_met"])
    def test_unresolved_ratio_cannot_claim_half_error(self):
        old,new=gate_rows();old["cases"][3]["worst_continuum_error"]["mean_l1_mg_l"]=1e-12
        new[3]["worst_continuum_error"]["mean_l1_mg_l"]=1e-13
        r=study.comparisons(old,new,base());self.assertFalse(r["all_targets_met"])
        self.assertIsNone(r["paired_cases"][3]["worst_field_error_ratios"]["mean_l1_mg_l"])
    def test_missing_crossing_and_merged_repeated_events_fail(self):
        old,new=gate_rows();row=new[-1]
        row["observations"][2]["native"].update(first_upward_bracket_h=None,upward_crossing_count=0)
        row["observations"][1]["native"]["upward_crossing_count"]=1
        r=study.comparisons(old,new,base());self.assertEqual(r["passed_gate_count"],60)
    def test_peak_compares_same_observation_grid(self):
        old,new=gate_rows();new[-1]["outlet"]["sampled_peak_error_mg_l"]=-.10001
        self.assertEqual(study.comparisons(old,new,base())["passed_gate_count"],62)
    def test_both_bracket_endpoints_checked_without_midpoint_substitution(self):
        old,new=gate_rows();new[3]["observations"][0]["native"]["first_upward_bracket_h"]=[.97,1.01]
        self.assertEqual(study.comparisons(old,new,base())["passed_gate_count"],62)
    def test_threshold_timing_equality_and_nextafter_rejection(self):
        old,new=gate_rows();row=new[3]["observations"][0];row["continuum"]["first_upward_h"]=0
        row["native"]["first_upward_bracket_h"]=[0,.025]
        self.assertTrue(study.comparisons(old,new,base())["all_targets_met"])
        row["native"]["first_upward_bracket_h"][1]=math.nextafter(.025,1.)
        self.assertFalse(study.comparisons(old,new,base())["all_targets_met"])
    def test_time_sensitivity_known_quadratic_order_and_clock_identity(self):
        cases=[];rows=[]
        for identity,spec in study.candidate_specs(base()).items():
            cases.append(dict(id=identity,result=dict(time_h=[0.,1.],x_m=[.25,.75],
                concentration_mg_l=[[0.,0.],[spec["cfl"]**2,spec["cfl"]**2]])))
            rows.append(dict(id=identity,reference=dict(uncertainty_estimate_mg_l=1e-12),
                regime=spec["regime"],study=spec["study"],cells=spec["cells"],worst_continuum_error=dict(mean_l1_mg_l=1.,rms_mg_l=1.,maximum_mg_l=1.)))
        result=study.sensitivity(dict(cases=cases),rows)
        self.assertEqual(len(result["time_sensitivity"]),5)
        for group in result["time_sensitivity"]:
            for order in group["observed_difference_orders"]:self.assertAlmostEqual(order["mean_l1_mg_l"],2.)
        cases[-1]["result"]["time_h"][-1]=math.nextafter(1.,2.)
        with self.assertRaises(ValueError):study.sensitivity(dict(cases=cases),rows)

class CommandTests(unittest.TestCase):
    def args(self,out):return ["--native",sys.executable,"--baseline-native",sys.executable,"--config",str(SCIENCE/"f1b_front_comparison_config.json"),"--output",str(out)]
    def test_existing_directory_refused_without_changes(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/"sentinel";p.write_text("keep")
            self.assertEqual(study.main(self.args(folder)),2);self.assertEqual(p.read_text(),"keep")
    def test_invalid_configuration_retains_failed_attempt(self):
        with tempfile.TemporaryDirectory() as folder:
            cfg=Path(folder)/"bad.json";cfg.write_text('{"study_version":2}')
            out=Path(folder)/"attempt";args=self.args(out);args[args.index("--config")+1]=str(cfg)
            self.assertEqual(study.main(args),2);self.assertEqual((out/"config-input.json").read_bytes(),cfg.read_bytes())
            self.assertEqual(json.loads((out/"report.json").read_bytes())["status"],"failed")
    def test_missing_executable_explicit_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/"attempt";args=self.args(out);args[1]=str(Path(folder)/"absent.exe")
            self.assertEqual(study.main(args),2);self.assertTrue((out/"report.json").exists())
    def test_completed_unmet_and_met_have_different_exits(self):
        for met,expected in ((False,1),(True,0)):
            with self.subTest(met=met),tempfile.TemporaryDirectory() as folder:
                report=dict(status="completed",accuracy_status="synthetic_targets_met" if met else "synthetic_targets_unmet",
                    candidate=dict(cases=[]),comparison=dict(passed_gate_count=63 if met else 62,gate_count=63,all_targets_met=met,gates=[]))
                with patch.object(study,"execute",return_value=({},{})),patch.object(study,"evaluate",return_value=report):
                    self.assertEqual(study.main(self.args(Path(folder)/"attempt")),expected)
    def test_real_process_stdout_retained_and_hashed(self):
        with tempfile.TemporaryDirectory() as folder:
            document,metadata=study.execute(Path(sys.executable),["-c","print('{}')"],Path(folder),"probe")
            self.assertEqual(document,{});self.assertEqual(metadata["exit"],0)
            self.assertEqual(metadata["data_sha256"],study.sha(Path(folder)/"probe-reference.json"))
    def test_nonzero_process_and_extra_stderr_retained(self):
        for code in ("raise SystemExit(3)","import sys; print('{}'); print('extra',file=sys.stderr)"):
            with self.subTest(code=code),tempfile.TemporaryDirectory() as folder:
                with self.assertRaises(ValueError):study.execute(Path(sys.executable),["-c",code],Path(folder),"probe")
                self.assertTrue((Path(folder)/"probe-execution.json").exists())
    def test_timeout_retains_partial_streams(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(study.subprocess,"run",side_effect=subprocess.TimeoutExpired("probe",300)):
            with self.assertRaises(subprocess.TimeoutExpired):study.execute(Path(sys.executable),[],Path(folder),"probe")
            self.assertTrue((Path(folder)/"probe-reference.json").exists())

if __name__=="__main__":unittest.main()
