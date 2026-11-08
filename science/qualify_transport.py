"""A9 bounded native transport qualification, with independent physical references.

All acceptance gates are loaded before native execution. Failed measurements and
native output are retained; no clipping, CFL changes or hidden reference fallback.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import platform
import subprocess
import sys
import time

import numpy as np

from breakwater_science.transport_reference import continuum, semidiscrete, reference_diagnostics

ROOT = Path(__file__).resolve().parents[1]
LEDGER = ("inventory_mg", "cumulative_inlet_mg", "cumulative_outlet_mg",
          "cumulative_source_mg", "cumulative_decay_mg")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def norm(error):
    values = np.asarray(error, dtype=np.float64)
    return {"mean_l1_mg_l": float(np.mean(np.abs(values))),
            "rms_mg_l": float(np.sqrt(np.mean(values*values))),
            "maximum_mg_l": float(np.max(np.abs(values)))}


def gate(gates, name, passed, **evidence):
    gates.append({"name": name, "passed": bool(passed), **evidence})


def expected_matrix(config):
    matrix = config["matrix"]
    expected = set()
    for regime, spec in matrix["regimes"].items():
        if regime in config["spatial_gates"]:
            for mode in spec.get("mode_fractions", [None]):
                for cells in spec.get("space_cells",matrix["space_cells"]):
                    expected.add((regime, "space", mode, cells, spec["space_cfl"]))
        if regime in config["temporal_gates"]:
            for cfl in spec.get("time_cfl", matrix["time_cfl"]):
                expected.add((regime, "time", spec.get("time_mode_fraction"),
                              spec.get("cells", matrix["time_cells"]), cfl))
        if regime == "source_only":
            expected.add((regime, "exact", None, spec["cells"], spec["cfl"]))
    return expected


def case_key(case):
    return (case["regime"], case["study"],
            case["reference_parameters"].get("mode_fraction"),
            case["pipe"]["cells"], case["config"]["cfl_safety"])


def validate_matrix(document, config):
    require(document["fixture_version"] == 1, "Unsupported native fixture version")
    require(document.get("qualification_variant","baseline") == config.get("qualification_variant","baseline"),
            "Native matrix variant does not match the declared follow-up")
    cases = document["cases"]
    require(len(cases) == config["expected_case_count"], "Native fixture case count changed")
    require(len({case["id"] for case in cases}) == len(cases), "Duplicate native fixture ID")
    require(len({case_key(case) for case in cases}) == len(cases), "Duplicate regime/refinement configuration")
    require({case_key(case) for case in cases} == expected_matrix(config),
            "Native fixture does not match the declared qualification matrix")
    require(Counter(case["regime"] for case in cases) ==
            Counter({name: item["count"] for name, item in config["matrix"]["regimes"].items()}),
            "Regime counts do not match the declared matrix")
    actual = document["rejections"]
    require(len({row["id"] for row in actual}) == len(actual), "Duplicate rejection ID")
    require({row["id"]: row["exception_type"] for row in actual} == config["expected_rejections"],
            "Expected work/output-limit rejection changed or was omitted")
    require(all(isinstance(row["message"], str) and row["message"] for row in actual),
            "Rejected requests must retain their explicit error")


def validate_inputs(case, config):
    regime, p, c, ref = case["regime"], case["pipe"], case["config"], case["reference_parameters"]
    spec = config["matrix"]["regimes"][regime]
    tolerance = config["common_gates"]["input_absolute_tolerance"]
    diffusion = .05 if regime in ("closed_diffusion", "open_diffusion") else (.1 if regime == "mixed_adr" else 0.)
    decay = 1. if regime == "reaction_source" else (.1 if regime == "mixed_adr" else 0.)
    for name, expected in (("length_m", 1.), ("area_m2", .2),
                           ("dispersion_m2_h", diffusion), ("decay_per_h", decay)):
        require(abs(p[name]-expected) <= tolerance, case["id"]+": physical input changed: "+name)
    require(c["horizon_h"] == spec["horizon_h"], "Requested qualification horizon changed")
    require(c["output_step_h"] == spec.get("output_step_h", c["horizon_h"]), "Output grid changed")
    closed = regime in ("closed_diffusion", "reaction_source", "source_only")
    require(c["boundary"] == ("closed_no_flux" if closed else "dirichlet_inlet"), "Boundary changed")
    require(c["max_substeps"] == 1000000, "Qualification must not silently alter native step budget")
    flow = [(0., .05 if regime == "advection" else 0.)]
    inlet = [] if closed else [(0., 0.)]
    params = {}
    if regime in ("advection", "scheduled_advection"):
        params = {"bump_start_m": .125, "bump_width_m": .5}
    if regime.startswith("scheduled_"):
        flow = [(0., .4*.2), (.137, 0.), (.293, .7*.2)]
    if regime == "scheduled_inlet":
        params = {"initial_mg_l": 0.}
        inlet = [(0., 0.), (.11, 1.), (.29, .2), (.47, .8), (.63, 0.)]
    if regime == "closed_diffusion":
        params = {"mean_mg_l": .6, "amplitude_mg_l": .3}
    if regime == "open_diffusion":
        params = {"amplitude_mg_l": .7}
    if regime == "mixed_adr":
        fraction = ref["mode_fraction"]
        params = {"mode_fraction": fraction}
        beta = math.pi*fraction
        a = -beta/math.tan(beta)
        flow = [(0., .2*2*.1*a)]
    if regime in ("reaction_source", "source_only"):
        params = {"initial_mg_l": 1. if regime == "reaction_source" else .1,
                  "source_mg_l_h": .25}
    require(set(ref) == set(params) and all(abs(ref[k]-v) <= tolerance for k, v in params.items()),
            "Analytic-reference parameters changed")
    for key, expected, value_name in (("flow", flow, "flow_m3h"), ("inlet", inlet, "concentration_mg_l")):
        observed = [(row["time_h"], row[value_name]) for row in case[key]]
        require(len(observed) == len(expected) and
                all(abs(a-x) <= tolerance and abs(b-y) <= tolerance
                    for (a,b),(x,y) in zip(observed,expected)), "Forcing schedule changed: "+key)
    source = np.asarray(case["source_mg_l_h"])
    require(source.shape == (p["cells"],) and
            np.all(np.abs(source-(.25 if regime in ("reaction_source", "source_only") else 0.)) <= tolerance),
            "Source profile changed")


def evaluate_case(case, config):
    validate_inputs(case, config)
    common = config["common_gates"]
    result, p, c = case["result"], case["pipe"], case["config"]
    cells, horizon = p["cells"], c["horizon_h"]
    times = np.asarray(result["time_h"], dtype=np.float64)
    field = np.asarray(result["concentration_mg_l"], dtype=np.float64)
    require(2 <= len(times) <= 10001 and times[0] == 0. and times[-1] == horizon and
            np.all(np.diff(times) > 0), "Native result lost the exact requested interval")
    requested_times = [0.]
    for ordinal in range(1,10001):
        sample = ordinal*c["output_step_h"]
        if sample >= horizon:
            requested_times.append(horizon)
            break
        requested_times.append(sample)
    require(np.array_equal(times,np.asarray(requested_times)), "Native result omitted or shifted a requested output")
    require(field.shape == (len(times), cells) and np.isfinite(field).all(), "Malformed native field")
    require(np.allclose(result["x_m"], (np.arange(cells)+.5)/cells, atol=1e-15, rtol=0),
            "Native cell-center identities changed")
    initial = np.asarray(case["initial_mg_l"], dtype=np.float64)
    oracle0 = continuum(case, 0.)
    require(initial.shape == (cells,) and np.max(np.abs(initial-oracle0["concentration_mg_l"])) <=
            common["input_absolute_tolerance"], "Initial values are not the declared analytic cell averages")
    require(np.array_equal(initial, field[0]), "Native output does not preserve captured initial data")
    arrays = {name: np.asarray(result[name], dtype=np.float64) for name in LEDGER}
    arrays["mass_balance_residual_mg"] = np.asarray(result["mass_balance_residual_mg"], dtype=np.float64)
    require(all(v.shape == times.shape and np.isfinite(v).all() for v in arrays.values()), "Malformed native ledger")
    diagnostics = result["diagnostics"]
    require(all(isinstance(value, (int,float)) and not isinstance(value,bool) and
                math.isfinite(float(value)) for value in diagnostics.values()), "Nonfinite native diagnostics")
    require(isinstance(diagnostics["substeps"], int) and diagnostics["substeps"] > 0,
            "Native substep count must be a positive integer")
    require(diagnostics["max_cfl_number"] >= 0 and diagnostics["max_mass_balance_residual_mg"] >= 0,
            "Native maximum CFL and mass residual must be nonnegative")
    gates = []
    scale = max(1., *(float(np.max(np.abs(values))) for values in arrays.values()))
    tolerance = common["ledger_absolute_tolerance_mg"] + common["ledger_relative_tolerance"]*scale
    inventories = np.array([math.fsum(row)*1000*p["area_m2"]*p["length_m"]/cells for row in field])
    balance = (inventories-inventories[0]-arrays["cumulative_inlet_mg"]+
               arrays["cumulative_outlet_mg"]-arrays["cumulative_source_mg"]+arrays["cumulative_decay_mg"])
    gate(gates, "independent_inventory", np.max(np.abs(inventories-arrays["inventory_mg"])) <= tolerance)
    gate(gates, "independent_signed_balance", np.max(np.abs(balance)) <= tolerance,
         maximum_residual_mg=float(np.max(np.abs(balance))), tolerance_mg=tolerance)
    gate(gates, "recorded_balance", np.max(np.abs(balance-arrays["mass_balance_residual_mg"])) <= tolerance)
    gate(gates, "substep_balance", diagnostics["max_mass_balance_residual_mg"] <= tolerance)
    gate(gates, "nonnegative_field", np.min(field) >= -common["minimum_concentration_tolerance_mg_l"],
         minimum_mg_l=float(np.min(field)))
    gate(gates, "maximum_principle", np.max(field) <= common["maximum_concentration_mg_l"]+
         common["maximum_concentration_tolerance_mg_l"], maximum_mg_l=float(np.max(field)))
    gate(gates, "declared_cfl", diagnostics["max_cfl_number"] <= c["cfl_safety"]+common["cfl_tolerance"])
    gate(gates, "bounded_work", 0 < diagnostics["substeps"] <= min(c["max_substeps"], 50000000//cells))
    gate(gates, "diagnostic_minimum", diagnostics["minimum_concentration_mg_l"] >=
         -common["minimum_concentration_tolerance_mg_l"] and
         diagnostics["minimum_concentration_mg_l"] <= float(np.min(field))+1e-12)
    refs = [continuum(case, float(t)) for t in times]
    errors = [norm(row-ref["concentration_mg_l"]) for row, ref in zip(field,refs)]
    ledger_errors = {name: max(abs(float(arrays[name][i])-float(ref[name]))
                              for i,ref in enumerate(refs) if ref[name] is not None) for name in LEDGER}
    terminal = refs[-1]
    mass_scale = max(1., abs(oracle0["inventory_mg"]),
                     *(abs(terminal[name]) for name in LEDGER if terminal[name] is not None))
    sem = semidiscrete(case, horizon)
    temporal = None if sem is None else norm(field[-1]-sem)
    spatial = None if sem is None else norm(sem-terminal["concentration_mg_l"])
    ref_diagnostics = reference_diagnostics(case)
    reference_roundoff = 0.
    if case["regime"] == "mixed_adr":
        gate(gates, "mixed_eigen_residual", ref_diagnostics["normalized_eigen_residual"] <=
             common["mixed_eigen_relative_residual_maximum"])
        gate(gates, "mixed_eigen_orthogonality", ref_diagnostics["orthogonality_error"] <=
             common["mixed_eigen_orthogonality_maximum"])
        gate(gates, "mixed_matrix_reconstruction", ref_diagnostics["normalized_reconstruction_error"] <=
             common["mixed_matrix_reconstruction_relative_maximum"])
        gate(gates, "mixed_initial_reconstruction", ref_diagnostics["initial_reconstruction_maximum_mg_l"] <=
             common["mixed_initial_reconstruction_maximum_mg_l"])
        reference_roundoff = (common["reference_roundoff_multiplier"]*np.finfo(float).eps*cells*
            ref_diagnostics["similarity_condition"]*max(1.,abs(ref_diagnostics["min_eigenvalue_per_h"]))*
            horizon*max(1.,float(np.max(np.abs(initial)))))
        reference_error = spatial if case["study"] == "space" else temporal
        gate(gates, "mixed_reference_resolution", reference_error["mean_l1_mg_l"] >
             common["reference_error_resolution_factor"]*reference_roundoff,
             roundoff_estimate_mg_l=reference_roundoff)
    if case["study"] == "space" and sem is not None:
        gate(gates, "separated_spatial_error", temporal["mean_l1_mg_l"]+reference_roundoff <=
             common["low_cfl_time_to_spatial_error_maximum"]*max(0.,spatial["mean_l1_mg_l"]-reference_roundoff),
             time_error_mg_l=temporal["mean_l1_mg_l"], spatial_error_mg_l=spatial["mean_l1_mg_l"])
    if case["regime"] in ("open_diffusion", "mixed_adr"):
        gate(gates, "negative_signed_inlet", arrays["cumulative_inlet_mg"][-1] < 0 and terminal["cumulative_inlet_mg"] < 0)
    if all(knot["flow_m3h"] == 0 for knot in case["flow"]):
        gate(gates, "zero_flow_outlet", np.array_equal(arrays["cumulative_outlet_mg"], np.zeros(len(times))))
    if case["regime"] == "scheduled_inlet":
        gate(gates, "exact_scheduled_injection", ledger_errors["cumulative_inlet_mg"] <= tolerance,
             error_mg=ledger_errors["cumulative_inlet_mg"])
    if case["regime"].startswith("scheduled_"):
        paused = [i for i,t in enumerate(times) if .137 < t < .293]
        gate(gates, "advection_pause_freezes_field", len(paused) >= 2 and
             all(np.array_equal(field[i], field[paused[0]]) for i in paused))
    if case["regime"] == "source_only":
        gate(gates, "source_only_exact", max(e["maximum_mg_l"] for e in errors) <= common["zero_error_floor_mg_l"])
    for item in gates:
        item["case_id"] = case["id"]
    return {"id": case["id"], "regime": case["regime"], "study": case["study"], "cells": cells,
            "mode_fraction": case["reference_parameters"].get("mode_fraction"),
            "cfl_safety": c["cfl_safety"], "substeps": diagnostics["substeps"],
            "mean_dt_h": horizon/diagnostics["substeps"], "max_cfl_number": diagnostics["max_cfl_number"],
            "terminal_continuum_error": errors[-1], "maximum_frame_continuum_error": {
                key:max(e[key] for e in errors) for key in errors[-1]},
            "temporal_error": temporal, "spatial_error": spatial, "reference_diagnostics":ref_diagnostics,
            "reference_roundoff_estimate_mg_l":float(reference_roundoff),
            "analytic_ledger_errors_mg":ledger_errors, "analytic_mass_scale_mg":mass_scale,
            "maximum_signed_mass_residual_mg":float(np.max(np.abs(balance))), "gates":gates}


def evaluate(document, config):
    validate_matrix(document, config)
    rows = []
    for case in document["cases"]:
        require(case["result"]["solver_version"] == document["solver_version"], "Native method identities differ")
        rows.append(evaluate_case(case,config))
    groups = {}
    for row in rows:
        groups.setdefault((row["regime"],row["study"],row["mode_fraction"]),[]).append(row)
    studies, gates = [], []
    for (regime,study,mode), group in sorted(groups.items(),key=lambda item:str(item[0])):
        if study == "exact":
            fine = group[0]
        else:
            spatial = study == "space"
            group.sort(key=lambda row:row["cells"] if spatial else -row["cfl_safety"])
            limits = config["spatial_gates" if spatial else "temporal_gates"][regime]
            metric = "terminal_continuum_error" if spatial else "temporal_error"
            values = [row[metric]["mean_l1_mg_l"] for row in group]
            require(all(value > config["common_gates"]["zero_error_floor_mg_l"] for value in values),
                    "Refinement error reached the roundoff floor; order is unresolved")
            orders = [math.log(a/b)/math.log((right["cells"]/left["cells"]) if spatial else
                      (left["cfl_safety"]/right["cfl_safety"]))
                      for a,b,left,right in zip(values,values[1:],group,group[1:])]
            name = regime+"."+study+(f".mode{mode}" if mode is not None else "")
            gate(gates,name+".refinement_order", all(limits["order_min"] <= p <= limits["order_max"] for p in orders),
                 measured_orders=orders, minimum=limits["order_min"],maximum=limits["order_max"])
            fine = group[-1]
            gate(gates,name+".finest_field_error", values[-1] <= limits["finest_mean_l1_max_mg_l"],
                 mean_l1_mg_l=values[-1],maximum_mg_l=limits["finest_mean_l1_max_mg_l"])
            studies.append({"regime":regime,"study":study,"mode_fraction":mode,
                            "refinement_scope":("coupled_mesh_and_time" if spatial and fine["temporal_error"] is None
                                else "spatial_with_measured_time_contamination" if spatial
                                else "time_at_fixed_spatial_mesh"),
                            "case_ids":[r["id"] for r in group],"mean_l1_errors_mg_l":values,"observed_orders":orders})
        relative = max(fine["analytic_ledger_errors_mg"].values())/fine["analytic_mass_scale_mg"]
        gate(gates,regime+"."+study+f".mode{mode}.finest_analytic_ledger",
             relative <= config["common_gates"]["finest_analytic_ledger_relative_maximum"],
             normalized_maximum_error=relative)
    all_gates = gates+[g for row in rows for g in row["gates"]]
    return {"status":"passed" if all(g["passed"] for g in all_gates) else "failed",
            "native_solver_version":document["solver_version"],"case_count":len(rows),
            "gate_count":len(all_gates),"failed_gates":[g for g in all_gates if not g["passed"]],
            "limitations":["The scheduled-inlet front study refines mesh and time together at fixed CFL; it has no semidiscrete time-error separation.",
                "Passing mean-L1 gates does not qualify local peak or maximum-norm accuracy for a nonsmooth inlet front.",
                "Mixed-ADR eigendecomposition uncertainty is a conditioning-based floating-point estimate, not a formal enclosure.",
                "Only the declared synthetic cases and mesh ranges are qualified; coarse scheduled-inlet baseline convergence remains failed."],
            "studies":studies,"cases":rows,"study_gates":gates,"rejections":document["rejections"]}


def write_json(path,value):
    path.write_text(json.dumps(value,indent=2,allow_nan=False)+"\n",encoding="utf-8")


def write_summary(path, report):
    lines=["# A9 transport qualification", "", "Status: **"+report["status"].upper()+"**", "",
           f"Cases: {report.get('case_count',0)}; gates: {report.get('gate_count',0)}.", "",
           "| Regime / mode | Study | Error scope | Observed orders | Finest mean-L1 (mg/L) |",
           "|---|---|---|---|---:|"]
    for row in report.get("studies",[]):
        mode="" if row["mode_fraction"] is None else "/"+str(row["mode_fraction"])
        orders=", ".join(f"{v:.4f}" for v in row["observed_orders"])
        lines.append(f"| {row['regime']}{mode} | {row['study']} | {row['refinement_scope']} | {orders} | {row['mean_l1_errors_mg_l'][-1]:.9g} |")
    lines += ["", "Limits:", ""]+["- "+item for item in report.get("limitations",[])]
    if report.get("failed_gates"):
        lines += ["", "Failed gates:", ""]+["- "+g.get("case_id","")+" "+g["name"] for g in report["failed_gates"]]
    if "error" in report: lines += ["", "Failure: "+report["error"]]
    lines += ["", "Full inputs, fields, signed ledgers and individual gate evidence are retained in the adjacent JSON files.", ""]
    path.write_text("\n".join(lines),encoding="utf-8")


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native",type=Path,required=True)
    parser.add_argument("--config",type=Path,default=Path(__file__).with_name("a9_transport_config.json"))
    parser.add_argument("--output",type=Path,default=Path("out/a9/qualification"))
    args=parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    report={"status":"failed","started_utc":datetime.now(timezone.utc).isoformat(),
            "python":platform.python_version(),"numpy":np.__version__}
    try:
        config=json.loads(args.config.read_text(encoding="utf-8"))
        require(config["qualification_version"]==1,"Unsupported qualification config")
        native=args.native.resolve(strict=True)
        report.update(config_sha256=hashlib.sha256(args.config.read_bytes()).hexdigest(),
                      native_executable_sha256=hashlib.sha256(native.read_bytes()).hexdigest(),
                      config=config,native=str(native))
        sources=[Path(__file__),Path(__file__).parent/"breakwater_science/transport_reference.py",
                 ROOT/"engine/examples/transport_qualification_reference.cpp",ROOT/"engine/src/transport.cpp"]
        report["source_hashes"]=[{"path":p.relative_to(ROOT).as_posix(),"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for p in sources]
        write_json(output/"preflight.json",report)
        started=time.perf_counter()
        native_arguments=config.get("native_arguments",[])
        require(native_arguments in ([],["--refined-inlet"]),"Unsupported native qualification arguments")
        report["native_arguments"]=native_arguments
        run=subprocess.run([str(native),*native_arguments],capture_output=True,timeout=300,check=False)
        report["native_execution_s"]=time.perf_counter()-started
        (output/"native-reference.json").write_bytes(run.stdout)
        (output/"native-stderr.txt").write_bytes(run.stderr)
        require(run.returncode==0 and not run.stderr,
                f"Native fixture execution failed (exit {run.returncode}); raw evidence retained")
        report.update(evaluate(json.loads(run.stdout),config))
    except (ValueError,KeyError,TypeError,IndexError,RuntimeError,FloatingPointError,OverflowError,OSError,subprocess.SubprocessError) as error:
        report.update(status="failed",error=str(error))
    report["finished_utc"]=datetime.now(timezone.utc).isoformat()
    write_json(output/"report.json",report)
    write_summary(output/"report.md",report)
    print(f"A9 {report['status'].upper()}: {output/'report.json'}")
    if "case_count" in report: print(f"cases={report['case_count']}; gates={report['gate_count']}; failures={len(report['failed_gates'])}")
    for failed in report.get("failed_gates",[]):print(json.dumps(failed,sort_keys=True))
    if "error" in report:print(report["error"])
    return 0 if report["status"]=="passed" else 1


if __name__=="__main__":
    raise SystemExit(main())
