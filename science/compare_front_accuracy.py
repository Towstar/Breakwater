"""Private F1b comparator: 0 targets met, 1 completed/unmet, 2 integrity/run failure.

Production F1/A9 code, configuration, reference definitions and evidence stay frozen.
No candidate Poisson oracle, clipping or product qualification is supplied.
"""
from __future__ import annotations
import argparse
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
import qualify_front_accuracy as f1
from breakwater_science.front_reference import (continuum, outlet_flow, outlet_peak_reference,
    point_values, quadrature_averages, require, number)

ROOT = Path(__file__).resolve().parents[1]
METHOD = "fvm-muscl-mc-ssprk2-experimental-0.1.0"
BASE_CONFIG_SHA = "6d8d1ffdc8b6574241b2d006aeb697457a6e9f56cd83610164f7b76c6cc69d8a"
TARGETS = dict(n512_worst_field_error_ratio_maximum=.5, paired_worst_field_regression_floor_multiplier=10,
    extended_sampled_outlet_peak_error_maximum_mg_l=.1, extended_upward_crossing_counts=[[.1,1],[.5,2],[.9,1]],
    n512_first_bracket_endpoint_error_maximum_h=.025)
PROBES = [dict(regime=r,cells=512,cfl=c,paired_cfl=p) for r,c,p in
    (("front_smooth",[.08,.04],.02),("front_box",[.08,.04],.02),("scheduled_inlet",[.4,.2],.1))]

def load_json(raw):
    def pairs(items):
        result = {}
        for key,value in items:
            require(key not in result,"Duplicate JSON key: "+key)
            result[key] = value
        return result
    def invalid(value):
        raise ValueError("Nonfinite JSON constant: "+value)
    return json.loads(raw,object_pairs_hook=pairs,parse_constant=invalid)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def reference_check(case,times,multiplier=128):
    """Continuum-only check. The upwind Poisson kernel belongs to the baseline."""
    maximum=0.
    for t in times:
        exact=continuum(case,float(t))["concentration_mg_l"]
        for order in (32,64):
            maximum=max(maximum,float(np.max(np.abs(exact-quadrature_averages(case,float(t),order)))))
    roundoff=multiplier*np.finfo(float).eps*(case["pipe"]["cells"]+len(case["flow"])+len(case["inlet"])+1)
    return dict(maximum_quadrature_disagreement_mg_l=maximum,crosscheck_disagreement_mg_l=maximum,
        uncertainty_estimate_mg_l=max(maximum,float(roundoff)),
        scope="Continuum quadrature/roundoff estimate; no candidate semidiscrete reference or formal enclosure")

def validate_config(config):
    expected = dict(study_version=1,qualification_variant="front_comparison",
        baseline_revision="487d80dd54df7d7b6567dfed1170dea9a5e59cd7",baseline_config_sha256=BASE_CONFIG_SHA,
        baseline_solver_version="fvm-upwind-euler-0.1.0",candidate_solver_version=METHOD,
        baseline_arguments=["--front-accuracy"],candidate_arguments=[],baseline_case_count=22,candidate_case_count=28,
        candidate_cfl_maximum=.5,stage_evaluations_per_step=2,time_probes=PROBES,targets=TARGETS)
    require(set(config)==set(expected)|{"notes"},"Wrong F1b configuration fields")
    # JSON bool/int equality must not admit forged versions or resource counts.
    for key,value in expected.items():
        require(json.dumps(config[key],sort_keys=True)==json.dumps(value,sort_keys=True),"Changed F1b declaration: "+key)
    require(isinstance(config["notes"],list) and all(isinstance(v,str) for v in config["notes"]),"Invalid study notes")

def candidate_specs(base):
    specs = f1.expected_cases(base)
    for probe in PROBES:
        for cfl in probe["cfl"]:
            regime = probe["regime"]
            specs[f"{regime}.time_probe.n512.cfl{cfl:g}"] = dict(regime=regime,study="time_probe",cells=512,cfl=cfl,
                horizon=2.2 if regime=="scheduled_inlet" else 4.,step=.0125)
    require(len(specs)==28,"Wrong private comparison matrix")
    return specs

def validate_matrix(document, base):
    require(set(document)=={"fixture_version","qualification_variant","solver_version","cases"} and
        type(document["fixture_version"]) is int and document["fixture_version"]==1 and
        document["qualification_variant"]=="front_comparison" and document["solver_version"]==METHOD,
        "Wrong candidate fixture/method identity")
    specs = candidate_specs(base)
    cases = document["cases"]
    require(isinstance(cases,list) and len(cases)==28 and len({c["id"] for c in cases})==28 and
        {c["id"] for c in cases}==set(specs),"Wrong, duplicate or missing candidate case identities")
    return specs

def integration_schedule(case):
    """Independent replay of the declared event clock, without evaluating any field."""
    c,p = case["config"],case["pipe"]
    times = f1.output_grid(c["horizon_h"],c["output_step_h"])
    t,steps,maximum,qi,ii = 0.,0,0.,0,0
    q,inlet = case["flow"],case["inlet"]
    for output in times[1:]:
        while t < output:
            while qi+1<len(q) and q[qi+1]["time_h"]<=t: qi+=1
            while ii+1<len(inlet) and inlet[ii+1]["time_h"]<=t: ii+=1
            rate = (q[qi]["flow_m3h"]/p["area_m2"])*(1/(p["length_m"]/p["cells"]))
            event = min(float(output),q[qi+1]["time_h"] if qi+1<len(q) else math.inf,
                        inlet[ii+1]["time_h"] if ii+1<len(inlet) else math.inf)
            dt = c["cfl_safety"]/rate if rate else math.inf
            end = event if event-t<=dt else min(t+dt,event)
            if (end-t)*rate>c["cfl_safety"]: end=math.nextafter(end,t)
            require(math.isfinite(end) and end>t,"Nonadvancing candidate event clock")
            maximum=max(maximum,(end-t)*rate);steps+=1;t=end
            require(steps<=min(c["max_substeps"],50000000//(2*p["cells"])),"Candidate stage work exceeds existing bounds")
    return steps,maximum

def validate_result(case, initial, base):
    c,p,r,g = case["config"],case["pipe"],case["result"],base["integrity"]
    require(r["solver_version"]==METHOD,"Wrong per-result candidate provenance")
    times=f1.output_grid(c["horizon_h"],c["output_step_h"])
    require(np.array_equal(f1.array(r["time_h"],times.shape,"candidate times"),times),"Missing, shifted or duplicate candidate timestamp")
    n,frames=p["cells"],len(times)
    require(frames*n<=4000000,"Candidate saved-value limit exceeded")
    field=f1.array(r["concentration_mg_l"],(frames,n),"candidate field")
    require(np.array_equal(field[0],initial),"Candidate initial output changed")
    require(np.array_equal(f1.array(r["x_m"],(n,),"candidate cell identities"),
                           (np.arange(n)+.5)*p["length_m"]/n),"Candidate cell ordering/locations changed")
    minimum,maximum=float(np.min(field)),float(np.max(field))
    require(minimum>=-g["minimum_concentration_tolerance_mg_l"] and maximum<=1+g["maximum_concentration_tolerance_mg_l"],
        "Candidate field violates physical range; values must remain unclipped")
    ledger={key:f1.array(r[key],times.shape,key) for key in (*f1.LEDGER,"mass_balance_residual_mg")}
    scale=max(1.,*(float(np.max(np.abs(a))) for a in ledger.values()))
    tolerance=g["ledger_absolute_tolerance_mg"]+g["ledger_relative_tolerance"]*scale
    liters=1000*p["area_m2"]*p["length_m"]/n
    inventory=np.array([math.fsum(float(v)*liters for v in row) for row in field])
    require(np.max(np.abs(inventory-ledger["inventory_mg"]))<=tolerance,"Candidate inventory/litre conversion inconsistent")
    require(all(np.all(ledger[key]==0) for key in ("cumulative_source_mg","cumulative_decay_mg")),"Unsupported candidate source/decay ledger")
    balance=inventory-inventory[0]-ledger["cumulative_inlet_mg"]+ledger["cumulative_outlet_mg"]
    require(np.max(np.abs(balance))<=tolerance and np.max(np.abs(balance-ledger["mass_balance_residual_mg"]))<=tolerance,
        "Inconsistent independent candidate signed mass ledger")
    for key in ("cumulative_inlet_mg","cumulative_outlet_mg"):
        require(ledger[key][0]==0 and np.all(np.diff(ledger[key])>=-tolerance),"Invalid candidate cumulative exchange")
    d,e=r["diagnostics"],case["execution"]
    require(set(d)=={"substeps","max_cfl_number","minimum_concentration_mg_l","max_mass_balance_residual_mg"} and
        set(e)=={"stage_evaluations","cell_stage_updates","maximum_concentration_mg_l"},"Wrong candidate diagnostic shape")
    for value in [*d.values(),*e.values()]: number(value)
    steps,cfl=integration_schedule(case)
    require(type(d["substeps"]) is int and d["substeps"]==steps and
        type(e["stage_evaluations"]) is int and e["stage_evaluations"]==2*steps and
        type(e["cell_stage_updates"]) is int and e["cell_stage_updates"]==2*steps*n,
        "Forged candidate substep/stage counts")
    require(d["max_cfl_number"]==cfl and cfl<=c["cfl_safety"],"Candidate CFL diagnostics disagree with event clock")
    require(-g["minimum_concentration_tolerance_mg_l"]<=d["minimum_concentration_mg_l"]<=minimum+1e-12 and
        maximum-1e-12<=e["maximum_concentration_mg_l"]<=1+g["maximum_concentration_tolerance_mg_l"] and
        0<=d["max_mass_balance_residual_mg"]<=tolerance,"Inconsistent candidate stage extrema/balance diagnostics")
    return times,field,ledger,float(np.max(np.abs(balance)))

def evaluate_case(case,spec,base):
    initial=f1.validate_inputs(case,spec,base)
    source=f1.array(case["source_mg_l_h"],(spec["cells"],),"candidate source")
    require(np.all(source==0),"Nonzero candidate source")
    times,field,ledger,residual=validate_result(case,initial,base)
    check=reference_check(case,times,base["integrity"]["reference_roundoff_multiplier"])
    require(check["crosscheck_disagreement_mg_l"]<=base["integrity"]["reference_crosscheck_maximum_mg_l"],"Reference cross-check failed")
    frames,points=[],[]
    ledger_errors={key:0. for key in f1.LEDGER}
    for i,t in enumerate(times):
        ref=continuum(case,float(t));exact=ref["concentration_mg_l"]
        point=float(point_values(case,[case["pipe"]["length_m"]],float(t))[0]) if outlet_flow(case,float(t))>0 else None
        points.append(point)
        frames.append(dict(time_h=float(t),continuum_error=f1.norm(field[i]-exact),
            signed_peak_error_mg_l=float(np.max(field[i])-np.max(exact)),
            signed_last_cell_error_mg_l=float(field[i,-1]-exact[-1]),
            signed_outlet_point_error_mg_l=None if point is None else float(field[i,-1]-point),
            continuum_last_cell_minus_point_mg_l=None if point is None else float(exact[-1]-point)))
        for key in f1.LEDGER: ledger_errors[key]=max(ledger_errors[key],abs(float(ledger[key][i])-ref[key]))
    g=base["integrity"]
    require(ledger_errors["cumulative_inlet_mg"]<=g["ledger_absolute_tolerance_mg"]+g["ledger_relative_tolerance"]*
        max(1.,float(np.max(np.abs(ledger["cumulative_inlet_mg"])))),"Incorrect independent candidate inlet injection")
    valid=[i for i,p in enumerate(points) if p is not None]
    native_peak=max(float(field[i,-1]) for i in valid) if valid else None
    sampled_peak=max(points[i] for i in valid) if valid else None
    continuous_peak=outlet_peak_reference(case)
    return dict(id=case["id"],regime=case["regime"],study=case["study"],cells=spec["cells"],cfl_safety=spec["cfl"],
        substeps=case["result"]["diagnostics"]["substeps"],execution=case["execution"],reference=check,
        source_frame_count=len(times),maximum_signed_mass_residual_mg=residual,analytic_ledger_errors_mg=ledger_errors,
        per_frame=frames,terminal_continuum_error=frames[-1]["continuum_error"],
        worst_continuum_error=f1.maximum_norm([f["continuum_error"] for f in frames]),error_scope="coupled_space_time",
        worst_field_peak_error_mg_l=max(abs(f["signed_peak_error_mg_l"]) for f in frames),
        outlet=dict(available_frames=len(valid),point_error=f1.norm([frames[i]["signed_outlet_point_error_mg_l"] for i in valid]) if valid else None,
            maximum_last_cell_error_mg_l=max(abs(f["signed_last_cell_error_mg_l"]) for f in frames),
            sampled_native_peak_mg_l=native_peak,sampled_continuum_point_peak_mg_l=sampled_peak,
            continuum_peak_mg_l=continuous_peak,sampled_peak_error_mg_l=native_peak-sampled_peak if valid else None,
            sampled_native_minus_continuous_peak_mg_l=native_peak-continuous_peak if valid else None,
            reference_peak_sampling_gap_mg_l=continuous_peak-sampled_peak if valid else None,
            scope="Last-cell average proxy; physical point and cell-average errors are separate; native peaks are sampled"),
        observations=f1.observation_studies(case,times,field,base))

def comparisons(baseline,candidate,base):
    old={r["id"]:r for r in baseline["cases"]};new={r["id"]:r for r in candidate}
    gates=[];paired=[]
    def gate(identity,passed,**values): gates.append(dict(id=identity,passed=bool(passed),**values))
    metrics=("mean_l1_mg_l","rms_mg_l")
    for identity,a in old.items():
        b=new[identity]
        floor=10*max(a["reference"]["uncertainty_estimate_mg_l"],b["reference"]["uncertainty_estimate_mg_l"])
        ratios={}
        for metric in metrics:
            before,after=a["worst_continuum_error"][metric],b["worst_continuum_error"][metric]
            ratios[metric]=after/before if min(before,after)>floor else None
            gate(identity+".regression."+metric,after<=before+floor,baseline=before,candidate=after,floor_mg_l=floor)
        paired.append(dict(id=identity,comparison_floor_mg_l=floor,worst_field_error_ratios=ratios,
            ratio_scope="Unresolved if either error is at/below the reference-resolution floor"))
    representatives=["front_smooth.space.n512.cfl0.02","front_box.space.n512.cfl0.02",
                     "scheduled_inlet.extended_space.n512.cfl0.1"]
    for identity in representatives:
        a,b=old[identity],new[identity]
        for metric in metrics:
            floor=10*max(a["reference"]["uncertainty_estimate_mg_l"],b["reference"]["uncertainty_estimate_mg_l"])
            before,after=a["worst_continuum_error"][metric],b["worst_continuum_error"][metric]
            resolved=min(before,after)>floor
            gate(identity+".half_error."+metric,resolved and after<=.5*before,baseline=before,candidate=after,
                 ratio=after/before if resolved else None,resolved=resolved,maximum_ratio=.5)
        for row in b["observations"]:
            if row["stride"]!=1: continue
            bracket,exact=row["native"]["first_upward_bracket_h"],row["continuum"]["first_upward_h"]
            error=max(abs(v-exact) for v in bracket) if bracket is not None and exact is not None else None
            gate(identity+f".first_bracket.{row['threshold_mg_l']:g}",error is not None and error<=.025,
                native_bracket_h=bracket,reference_first_h=exact,maximum_endpoint_error_h=error,limit_h=.025)
    extended=new[representatives[-1]]
    peak_error=abs(extended["outlet"]["sampled_peak_error_mg_l"])
    gate("extended.sampled_peak",peak_error<=.1,error_mg_l=peak_error,limit_mg_l=.1)
    for threshold,count in TARGETS["extended_upward_crossing_counts"]:
        row=next(r for r in extended["observations"] if r["stride"]==1 and r["threshold_mg_l"]==threshold)
        actual=row["native"]["upward_crossing_count"]
        gate(f"extended.crossing_count.{threshold:g}",actual==count,actual=actual,expected=count)
    require(len(gates)==63 and len({g["id"] for g in gates})==63,"Wrong predeclared gate count")
    return dict(gates=gates,gate_count=len(gates),passed_gate_count=sum(g["passed"] for g in gates),
                all_targets_met=all(g["passed"] for g in gates),paired_cases=paired)

def sensitivity(document,rows):
    cases={c["id"]:c for c in document["cases"]};measured={r["id"]:r for r in rows}
    studies=[]
    def differences(ids,scope):
        results=[]
        for left,right in zip(ids,ids[1:]):
            a,b=cases[left]["result"],cases[right]["result"]
            require(a["time_h"]==b["time_h"] and a["x_m"]==b["x_m"],"Time sensitivity changed observation/mesh clocks")
            delta=np.asarray(a["concentration_mg_l"])-np.asarray(b["concentration_mg_l"])
            per_frame=[dict(time_h=t,error=f1.norm(d)) for t,d in zip(a["time_h"],delta)]
            floor=10*max(measured[left]["reference"]["uncertainty_estimate_mg_l"],measured[right]["reference"]["uncertainty_estimate_mg_l"])
            results.append(dict(coarser_id=left,finer_id=right,worst_difference=f1.maximum_norm([r["error"] for r in per_frame]),
                                resolution_floor_mg_l=floor,per_frame=per_frame))
        orders=[]
        for a,b in zip(results,results[1:]):
            floor=max(a["resolution_floor_mg_l"],b["resolution_floor_mg_l"])
            orders.append({key:math.log2(a["worst_difference"][key]/b["worst_difference"][key])
                if min(a["worst_difference"][key],b["worst_difference"][key])>floor else None
                for key in ("mean_l1_mg_l","rms_mg_l","maximum_mg_l")})
        return dict(scope=scope,case_ids=ids,differences=results,observed_difference_orders=orders,
                    limitation="Empirical native step sensitivity; no candidate semidiscrete oracle or certified temporal error bound")
    for p in PROBES:
        regime=p["regime"];study="extended_space" if regime=="scheduled_inlet" else "space"
        ids=[f"{regime}.time_probe.n512.cfl{c:g}" for c in p["cfl"]]
        ids.append(f"{regime}.{study}.n512.cfl{p['paired_cfl']:g}")
        studies.append(differences(ids,"N512_three_level_time_sensitivity"))
    for regime in ("front_smooth","front_box"):
        studies.append(differences([f"{regime}.time.n128.cfl{c:g}" for c in (.4,.2,.1,.05)],"N128_F1_time_sensitivity"))
    coupled=[]
    for regime,study in (("front_smooth","space"),("front_box","space"),("scheduled_inlet","space"),("scheduled_inlet","extended_space")):
        group=sorted([r for r in rows if r["regime"]==regime and r["study"]==study],key=lambda r:r["cells"])
        orders=[]
        for a,b in zip(group,group[1:]):
            floor=10*max(a["reference"]["uncertainty_estimate_mg_l"],b["reference"]["uncertainty_estimate_mg_l"])
            orders.append({key:math.log2(a["worst_continuum_error"][key]/b["worst_continuum_error"][key])
                if min(a["worst_continuum_error"][key],b["worst_continuum_error"][key])>floor else None
                for key in ("mean_l1_mg_l","rms_mg_l","maximum_mg_l")})
        coupled.append(dict(regime=regime,study=study,case_ids=[r["id"] for r in group],scope="coupled_space_time",
            observed_orders=orders))
    return dict(time_sensitivity=studies,coupled_refinement=coupled)

def evaluate(baseline_document,candidate_document,base):
    specs=validate_matrix(candidate_document,base)
    baseline=f1.evaluate(baseline_document,base)
    old={c["id"]:c for c in baseline_document["cases"]}
    for c in candidate_document["cases"]:
        if c["id"] in old:
            before={k:v for k,v in old[c["id"]].items() if k!="result"}
            after={k:v for k,v in c.items() if k not in ("result","execution")}
            require(before==after,"Paired candidate/baseline physical inputs differ: "+c["id"])
    rows=[evaluate_case(c,specs[c["id"]],base) for c in candidate_document["cases"]]
    decision=comparisons(baseline,rows,base)
    return dict(status="completed",accuracy_status="synthetic_targets_met" if decision["all_targets_met"] else "synthetic_targets_unmet",
        production_qualification="not_established",baseline=baseline,candidate=dict(case_count=len(rows),cases=rows,
        solver_version=METHOD,**sensitivity(candidate_document,rows)),comparison=decision,
        limitations=["Candidate supports prescribed-inlet nonnegative-flow pure advection only; endpoint slopes remain zero.",
            "Candidate refinement is coupled space/time; empirical time differences are not exact temporal errors.",
            "Final-cell averages approximate physical outlet concentrations; sampled peaks and bracket times retain observation limitations.",
            "Existing Lean propositions apply to the original stencil, not this candidate or IEEE/scheduler/accuracy correctness.",
            "All study targets are synthetic; neither completion nor target success authorizes production adoption or simulated controls.",
            "Original F1 time-contamination failures and original coarse A9 failure retain their original scope."])

def write_summary(path,report):
    lines=["# F1b private advection comparison","",f"Study: **{report['status']}**; accuracy: **{report.get('accuracy_status','not_established')}**.",""]
    if report["status"]=="completed":
        d=report["comparison"];lines += [f"Synthetic targets: {d['passed_gate_count']}/{d['gate_count']} passed. Production qualification: not established.","",
            "| N512 case | Worst mean-L1 mg/L | Worst RMS mg/L | Worst maximum mg/L | Saved outlet peak mg/L |",
            "|---|---:|---:|---:|---:|"]
        for r in report["candidate"]["cases"]:
            if r["cells"]==512 and r["study"] in ("space","extended_space"):
                e=r["worst_continuum_error"]
                lines.append(f"| {r['id']} | {e['mean_l1_mg_l']:.9g} | {e['rms_mg_l']:.9g} | {e['maximum_mg_l']:.9g} | {r['outlet']['sampled_native_peak_mg_l']:.9g} |")
        lines += ["","Failed targets:",""]+["- "+g["id"] for g in d["gates"] if not g["passed"]]
    if "error" in report: lines += ["Failure: "+report["error"]]
    lines += ["",*["- "+v for v in report.get("limitations",[])],""]
    path.write_bytes("\n".join(lines).encode())

def execute(native,arguments,output,prefix):
    stdout,stderr=output/(prefix+"-reference.json"),output/(prefix+"-stderr.txt")
    started=time.perf_counter()
    with stdout.open("wb") as out,stderr.open("wb") as err:
        run=subprocess.run([str(native),*arguments],stdout=out,stderr=err,timeout=300,check=False)
    metadata=dict(command=[str(native),*arguments],exit=run.returncode,elapsed_s=time.perf_counter()-started,
        executable_sha256=sha(native),data_sha256=sha(stdout),data_bytes=stdout.stat().st_size,stderr_sha256=sha(stderr))
    f1.write_json(output/(prefix+"-execution.json"),metadata)
    require(run.returncode==0 and stderr.stat().st_size==0,f"{prefix} native execution failed; raw evidence retained")
    require(stdout.stat().st_size<=128*1024*1024,"Private native output exceeds 128 MiB")
    return load_json(stdout.read_bytes()),metadata

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native",required=True,type=Path)
    parser.add_argument("--baseline-native",required=True,type=Path)
    parser.add_argument("--config",type=Path,default=Path(__file__).with_name("f1b_front_comparison_config.json"))
    parser.add_argument("--output",required=True,type=Path)
    args=parser.parse_args(argv);output=args.output.resolve()
    try: output.mkdir(parents=True,exist_ok=False)
    except FileExistsError:
        print("Refusing to overwrite an existing F1b attempt directory",file=sys.stderr);return 2
    report=dict(status="failed",accuracy_status="not_established",production_qualification="not_established",
        started_utc=datetime.now(timezone.utc).isoformat(),python=platform.python_version(),numpy=np.__version__,
        interpreter=sys.executable,command=[str(v) for v in argv] if argv is not None else sys.argv)
    try:
        raw=args.config.read_bytes();(output/"config-input.json").write_bytes(raw)
        config=load_json(raw);validate_config(config)
        frozen=ROOT/"science/f1_front_accuracy_config.json"
        require(sha(frozen)==BASE_CONFIG_SHA,"Frozen F1 configuration hash changed")
        base=load_json(frozen.read_bytes());f1.validate_config(base)
        sources=[ROOT/p for p in ("science/compare_front_accuracy.py","science/FRONT_COMPARISON_DECLARATION.md",
            "science/f1_front_accuracy_config.json","science/qualify_front_accuracy.py","science/breakwater_science/front_reference.py",
            "science/breakwater_science/transport_reference.py","engine/experimental/front_advection.hpp","engine/experimental/front_advection.cpp",
            "engine/examples/front_comparison_reference.cpp","engine/examples/transport_qualification_reference.cpp",
            "engine/src/transport.cpp","engine/include/breakwater/transport.hpp","engine/CMakeLists.txt",
            "science/tests/test_front_comparison.py","engine/tests/front_experiment_tests.cpp")]
        native=args.native.resolve(strict=True);baseline=args.baseline_native.resolve(strict=True)
        report.update(config_sha256=hashlib.sha256(raw).hexdigest(),config=config,
            source_hashes={p.relative_to(ROOT).as_posix():sha(p) for p in sources},
            candidate_executable_sha256=sha(native),baseline_executable_sha256=sha(baseline))
        f1.write_json(output/"preflight.json",report)
        old,old_execution=execute(baseline,config["baseline_arguments"],output,"baseline")
        new,new_execution=execute(native,config["candidate_arguments"],output,"candidate")
        report.update(baseline_execution=old_execution,candidate_execution=new_execution)
        started=time.perf_counter();report.update(evaluate(old,new,base));report["analysis_s"]=time.perf_counter()-started
    except (ValueError,KeyError,TypeError,IndexError,RuntimeError,FloatingPointError,OverflowError,OSError,subprocess.SubprocessError) as error:
        report.update(status="failed",error=str(error))
    report["finished_utc"]=datetime.now(timezone.utc).isoformat()
    f1.write_json(output/"report.json",report);write_summary(output/"report.md",report)
    print(f"F1b {report['status']}: {output/'report.json'}")
    if "error" in report: print(report["error"])
    if report["status"]!="completed": return 2
    print(f"Synthetic targets: {report['comparison']['passed_gate_count']}/{report['comparison']['gate_count']}")
    return 0 if report["comparison"]["all_targets_met"] else 1

if __name__=="__main__": raise SystemExit(main())
