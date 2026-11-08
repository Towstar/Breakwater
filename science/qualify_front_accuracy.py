"""F1 characterization of the unchanged native transport solver.

Completion means checked execution/analysis, not product accuracy qualification.
The private native fixture, original indices, raw fields and failed attempts are
retained. No clipping, acceptance-gate adjustment or reference fallback is used.
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

from breakwater_science.front_reference import (continuum, displacement, geometry, number,
    outlet_flow, outlet_peak_reference, point_values, reference_check, require, semidiscrete, threshold_reference)

ROOT = Path(__file__).resolve().parents[1]
LEDGER = ("inventory_mg", "cumulative_inlet_mg", "cumulative_outlet_mg",
          "cumulative_source_mg", "cumulative_decay_mg")
INTEGRITY = dict(reference_crosscheck_maximum_mg_l=1e-10, reference_roundoff_multiplier=128,
    reference_resolution_factor=10, input_absolute_tolerance=2e-13,
    ledger_absolute_tolerance_mg=1e-9, ledger_relative_tolerance=1e-10,
    minimum_concentration_tolerance_mg_l=5e-12, maximum_concentration_tolerance_mg_l=1e-10,
    cfl_tolerance=1e-12, spatial_time_contamination_maximum=.05)


def norm(values):
    values = np.asarray(values, dtype=np.float64)
    require(values.size > 0 and np.isfinite(values).all(), "Norm requires finite nonempty values")
    scale = float(np.max(np.abs(values)))
    relative = np.abs(values)/scale if scale else np.zeros_like(values)
    return dict(mean_l1_mg_l=scale*float(np.mean(relative)),
                rms_mg_l=scale*float(np.sqrt(np.mean(relative*relative))), maximum_mg_l=scale)


def maximum_norm(rows):
    return {key: max(row[key] for row in rows) for key in rows[0]}


def array(value, shape, label):
    result = np.asarray(value)
    require(result.dtype.kind in "iuf" and result.shape == shape, "Wrong numeric shape/type: "+label)
    require(all(isinstance(v, (int, float, np.integer, np.floating)) and not isinstance(v, (bool, np.bool_))
                for v in np.asarray(value, dtype=object).flat), "Non-numeric or Boolean sample: "+label)
    result = result.astype(np.float64)
    require(np.isfinite(result).all(), "Nonfinite "+label)
    return result


def output_grid(horizon, step):
    require(number(horizon) > 0 and horizon <= 24 and number(step) > 0, "Invalid output interval")
    values = [0.]
    for k in range(1, 10001):
        value = k*step
        if value >= horizon:
            return np.array([*values, horizon])
        require(value > values[-1], "Non-advancing output grid")
        values.append(value)
    raise ValueError("Output work limit exceeded")


def validate_config(c):
    require(type(c["study_version"]) is int and c["study_version"] == 1 and c["qualification_variant"] == "front_accuracy" and
            c["native_arguments"] == ["--front-accuracy"] and c["expected_case_count"] == 22,
            "Wrong F1 study identity")
    require(c["geometry"] == dict(length_m=1., area_m2=.2, dispersion_m2_h=0., decay_per_h=0.) and
            c["integrity"] == INTEGRITY, "Predeclared geometry/integrity gates changed")
    require(c["initial_pulses"] == dict(regimes=["front_smooth", "front_box"], space_cells=[64,128,256,512],
            space_cfl=.02, time_cells=128, time_cfl=[.4,.2,.1,.05], horizon_h=4., output_step_h=.0125,
            velocity_m_h=.25, start_m=.125, width_m=.5), "Predeclared pulse matrix changed")
    require(c["scheduled_inlet"] == dict(cells=[128,256,512], cfl=.1, legacy_horizon_h=.6,
            legacy_output_step_h=.05, extended_horizon_h=2.2, extended_output_step_h=.0125,
            velocity_knots=[[0.,.4],[.137,0.],[.293,.7]],
            inlet_knots=[[0.,0.],[.11,1.],[.29,.2],[.47,.8],[.63,0.]]), "Predeclared inlet matrix changed")
    require(c["thresholds_mg_l"] == [.1,.5,.9] and c["observation_strides"] == [1,2,4],
            "Predeclared observations changed")
    require(c["expected_rejections"] == dict(substep_budget="runtime_error",
            output_frame_budget="invalid_argument", output_value_budget="invalid_argument"), "Rejection matrix changed")


def expected_cases(c):
    """Independent declared metadata, never expanded from native result contents."""
    validate_config(c)
    specs = []
    p, inlet = c["initial_pulses"], c["scheduled_inlet"]
    for regime in p["regimes"]:
        for study, meshes, cfls in (("space", p["space_cells"], [p["space_cfl"]]),
                                   ("time", [p["time_cells"]], p["time_cfl"])):
            for n in meshes:
                for cfl in cfls:
                    specs.append((regime, study, n, cfl, p["horizon_h"], p["output_step_h"]))
    for study, prefix in (("space", "legacy"), ("extended_space", "extended")):
        for n in inlet["cells"]:
            specs.append(("scheduled_inlet", study, n, inlet["cfl"], inlet[prefix+"_horizon_h"],
                          inlet[prefix+"_output_step_h"]))
    result = {}
    for regime, study, n, cfl, horizon, step in specs:
        identity = f"{regime}.{study}.n{n}.cfl{cfl:g}"
        result[identity] = dict(regime=regime, study=study, cells=n, cfl=cfl, horizon=horizon, step=step)
    require(len(result) == 22, "Duplicate or missing planned case")
    return result


def validate_matrix(document, config):
    expected = expected_cases(config)
    require(type(document["fixture_version"]) is int and document["fixture_version"] == 1 and document["qualification_variant"] == "front_accuracy" and
            document["solver_version"] == "fvm-upwind-euler-0.1.0", "Wrong native method/variant")
    cases = document["cases"]
    require(len(cases) == len(expected) and len({c["id"] for c in cases}) == len(cases) and
            {c["id"] for c in cases} == set(expected), "Wrong F1 case identities/count")
    rejections = document["rejections"]
    require(len(rejections) == 3 and {r["id"]: r["exception_type"] for r in rejections} ==
            config["expected_rejections"] and all(isinstance(r["message"], str) and r["message"] for r in rejections),
            "Missing or forged work-limit rejection")
    return expected


def validate_inputs(case, spec, config):
    require(case["regime"] == spec["regime"] and case["study"] == spec["study"], "Wrong case family/study")
    require(case["pipe"] == dict(config["geometry"], cells=spec["cells"]), "Changed native geometry")
    require(case["config"] == dict(horizon_h=spec["horizon"], output_step_h=spec["step"],
            cfl_safety=spec["cfl"], boundary="dirichlet_inlet", max_substeps=1000000), "Changed native configuration")
    n = spec["cells"]
    source = array(case["source_mg_l_h"], (n,), "source")
    require(np.all(source == 0.), "Unexpected source")
    if spec["regime"] == "scheduled_inlet":
        p = config["scheduled_inlet"]
        flow = [dict(time_h=t, flow_m3h=v*.2) for t, v in p["velocity_knots"]]
        inlet = [dict(time_h=t, concentration_mg_l=v) for t, v in p["inlet_knots"]]
        parameters = dict(initial_mg_l=0.)
    else:
        flow = [dict(time_h=0., flow_m3h=.25*.2)]
        inlet = [dict(time_h=0., concentration_mg_l=0.)]
        parameters = dict(bump_start_m=.125, bump_width_m=.5)
    require(case["flow"] == flow and case["inlet"] == inlet and case["reference_parameters"] == parameters,
            "Changed forcing or reference identity")
    initial = array(case["initial_mg_l"], (n,), "initial field")
    require(np.max(np.abs(initial-continuum(case, 0.)["concentration_mg_l"])) <=
            config["integrity"]["input_absolute_tolerance"], "Initial field differs from physical cell averages")
    return initial


def sampled_crossings(times, values, threshold):
    """First upward bracket; missing discharge is unknown, never interpolated away."""
    times = array(times, (len(times),), "crossing times")
    require(times.ndim == 1 and len(times) == len(values) and len(times) >= 1 and
            np.isfinite(times).all() and np.all(np.diff(times) > 0), "Invalid crossing times")
    threshold = number(threshold)
    require(0 < threshold < 1, "F1 thresholds must lie within the unit pulse range")
    brackets, unresolved, previous = [], 0, None
    initial = values[0] is not None and number(values[0]) >= threshold
    if initial:
        brackets.append([float(times[0]), float(times[0])])
    for i, value in enumerate(values):
        if value is None:
            previous = None
            continue
        value = number(value)
        if i > 0 and previous is not None and previous < threshold <= value:
            brackets.append([float(times[i-1]), float(times[i])])
        elif i > 0 and previous is None and value >= threshold:
            unresolved += 1
        previous = value
    status = "unavailable" if all(v is None for v in values) else "initially_exceeded" if initial else "reached" if brackets else "unresolved_gap" if unresolved else "not_reached"
    return dict(status=status, first_upward_bracket_h=brackets[0] if brackets else None,
                upward_brackets_h=brackets, upward_crossing_count=len(brackets),
                multiple_upward_crossings=len(brackets) > 1, unresolved_gap_count=unresolved)


def observation_studies(case, times, field, config):
    available = [outlet_flow(case, float(t)) > 0 for t in times]
    native = [float(row[-1]) if ok else None for row, ok in zip(field, available)]
    rows = []
    for stride in config["observation_strides"]:
        indices = list(range(0, len(times), stride))
        if indices[-1] != len(times)-1:
            indices.append(len(times)-1)
        for threshold in config["thresholds_mg_l"]:
            reference = threshold_reference(case, threshold)
            sampled = sampled_crossings(times[indices], [native[i] for i in indices], threshold)
            bracket, exact = sampled["first_upward_bracket_h"], reference["first_upward_h"]
            error = [v-exact for v in bracket] if bracket is not None and exact is not None else None
            rows.append(dict(stride=stride, original_indices=indices, threshold_mg_l=threshold,
                continuum=reference, native=sampled, signed_timing_error_bracket_h=error,
                crossing_missed=exact is not None and bracket is None,
                sampled_crossing_without_continuum_crossing=exact is None and bracket is not None))
    return rows


def validate_result(case, initial, config):
    spec, r, p, gates = case["config"], case["result"], case["pipe"], config["integrity"]
    require(r["solver_version"] == "fvm-upwind-euler-0.1.0", "Wrong per-result method identity")
    times = output_grid(spec["horizon_h"], spec["output_step_h"])
    recorded = array(r["time_h"], times.shape, "output times")
    require(np.array_equal(recorded, times), "Missing, shifted or duplicate output time")
    n, frames = p["cells"], len(times)
    require(frames*n <= 4000000, "Saved field exceeds native resource bound")
    field = array(r["concentration_mg_l"], (frames, n), "field")
    require(np.array_equal(field[0], initial), "Native initial output changed")
    x = array(r["x_m"], (n,), "cell identities")
    require(np.array_equal(x, (np.arange(n)+.5)*p["length_m"]/n), "Cell identity/ordering changed")
    require(np.min(field) >= -gates["minimum_concentration_tolerance_mg_l"] and
            np.max(field) <= 1+gates["maximum_concentration_tolerance_mg_l"], "Field violates declared physical range")
    ledger = {name: array(r[name], times.shape, name) for name in (*LEDGER, "mass_balance_residual_mg")}
    scale = max(1., *(float(np.max(np.abs(v))) for v in ledger.values()))
    tolerance = gates["ledger_absolute_tolerance_mg"]+gates["ledger_relative_tolerance"]*scale
    volume_l = 1000*p["area_m2"]*p["length_m"]/n
    inventory = np.array([math.fsum(float(v)*volume_l for v in row) for row in field])
    balance = inventory-inventory[0]-ledger["cumulative_inlet_mg"]+ledger["cumulative_outlet_mg"]
    require(np.max(np.abs(inventory-ledger["inventory_mg"])) <= tolerance, "Wrong litre conversion/inventory")
    require(np.max(np.abs(balance)) <= tolerance and
            np.max(np.abs(balance-ledger["mass_balance_residual_mg"])) <= tolerance,
            "Inconsistent independently reconstructed signed ledger")
    require(np.all(ledger["cumulative_source_mg"] == 0) and np.all(ledger["cumulative_decay_mg"] == 0),
            "Unexpected source/decay ledger")
    for name in ("cumulative_inlet_mg", "cumulative_outlet_mg"):
        require(ledger[name][0] == 0 and np.all(np.diff(ledger[name]) >= -tolerance), "Invalid positive-flow cumulative ledger")
    d = r["diagnostics"]
    require(set(d) == {"substeps", "max_cfl_number", "minimum_concentration_mg_l", "max_mass_balance_residual_mg"},
            "Wrong diagnostic shape")
    require(all(math.isfinite(number(v)) for v in d.values()), "Nonfinite diagnostics")
    require(isinstance(d["substeps"], int) and not isinstance(d["substeps"], bool) and
            0 < d["substeps"] <= min(spec["max_substeps"], 50000000//n), "Exceeded native integration budget")
    require(0 <= d["max_cfl_number"] <= spec["cfl_safety"]+gates["cfl_tolerance"] and
            -gates["minimum_concentration_tolerance_mg_l"] <= d["minimum_concentration_mg_l"] <= float(np.min(field))+1e-12 and
            0 <= d["max_mass_balance_residual_mg"] <= tolerance, "Inconsistent CFL/minimum/balance diagnostics")
    return times, field, ledger, float(np.max(np.abs(balance)))


def evaluate_case(case, spec, config):
    initial = validate_inputs(case, spec, config)
    times, field, ledger, residual = validate_result(case, initial, config)
    check = reference_check(case, times, config["integrity"]["reference_roundoff_multiplier"])
    require(check["crosscheck_disagreement_mg_l"] <= config["integrity"]["reference_crosscheck_maximum_mg_l"],
            "Reference cross-check failed; no fallback or reference substitution")
    uncertainty = check["uncertainty_estimate_mg_l"]
    per_frame, spatial, temporal, ratios, point_trace = [], [], [], [], []
    ledger_errors = {name: 0. for name in LEDGER}
    for i, t in enumerate(times):
        ref = continuum(case, float(t))
        exact = ref["concentration_mg_l"]
        sem = semidiscrete(case, float(t))
        continuous = norm(field[i]-exact)
        point = float(point_values(case, [case["pipe"]["length_m"]], float(t))[0]) if outlet_flow(case, float(t)) > 0 else None
        point_trace.append(point)
        row = dict(time_h=float(t), continuum_error=continuous,
            signed_peak_error_mg_l=float(np.max(field[i])-np.max(exact)),
            signed_last_cell_error_mg_l=float(field[i,-1]-exact[-1]),
            signed_outlet_point_error_mg_l=None if point is None else float(field[i,-1]-point),
            continuum_last_cell_minus_point_mg_l=None if point is None else float(exact[-1]-point))
        if sem is not None:
            time_error, space_error = norm(field[i]-sem), norm(sem-exact)
            temporal.append(time_error); spatial.append(space_error)
            row.update(temporal_error=time_error, spatial_error=space_error,
                signed_temporal_last_cell_error_mg_l=float(field[i,-1]-sem[-1]),
                signed_spatial_last_cell_error_mg_l=float(sem[-1]-exact[-1]))
            if space_error["mean_l1_mg_l"] > config["integrity"]["reference_resolution_factor"]*uncertainty:
                ratios.append((time_error["mean_l1_mg_l"]+uncertainty)/(space_error["mean_l1_mg_l"]-uncertainty))
        per_frame.append(row)
        for name in LEDGER:
            ledger_errors[name] = max(ledger_errors[name], abs(float(ledger[name][i])-ref[name]))
    scale = max(1., float(np.max(np.abs(ledger["cumulative_inlet_mg"]))))
    require(ledger_errors["cumulative_inlet_mg"] <= INTEGRITY["ledger_absolute_tolerance_mg"]+
            INTEGRITY["ledger_relative_tolerance"]*scale, "Incorrect independent inlet injection")
    valid = [i for i, value in enumerate(point_trace) if value is not None]
    point_error = [per_frame[i]["signed_outlet_point_error_mg_l"] for i in valid]
    native_peak = max(float(field[i,-1]) for i in valid) if valid else None
    sampled_peak = max(point_trace[i] for i in valid) if valid else None
    true_peak = outlet_peak_reference(case)
    return dict(id=case["id"], regime=case["regime"], study=case["study"], cells=case["pipe"]["cells"],
        cfl_safety=case["config"]["cfl_safety"], substeps=case["result"]["diagnostics"]["substeps"],
        source_frame_count=len(times), reference=check, maximum_signed_mass_residual_mg=residual,
        analytic_ledger_errors_mg=ledger_errors, per_frame=per_frame,
        terminal_continuum_error=per_frame[-1]["continuum_error"],
        worst_continuum_error=maximum_norm([r["continuum_error"] for r in per_frame]),
        worst_spatial_error=maximum_norm(spatial) if spatial else None,
        worst_temporal_error=maximum_norm(temporal) if temporal else None,
        maximum_resolved_time_to_space_ratio=max(ratios) if ratios else None,
        native_spatial_interpretation=("coupled_space_time" if not spatial else
            "time_contamination_below_5_percent" if ratios and max(ratios) <= INTEGRITY["spatial_time_contamination_maximum"] else
            "measured_separate_errors_native_contamination_exceeds_target"),
        worst_field_peak_error_mg_l=max(abs(r["signed_peak_error_mg_l"]) for r in per_frame),
        outlet=dict(available_frames=len(valid), point_error=norm(point_error) if valid else None,
            maximum_last_cell_error_mg_l=max(abs(r["signed_last_cell_error_mg_l"]) for r in per_frame),
            sampled_native_peak_mg_l=native_peak, sampled_continuum_point_peak_mg_l=sampled_peak,
            continuum_peak_mg_l=true_peak,
            sampled_peak_error_mg_l=native_peak-sampled_peak if valid else None,
            sampled_native_minus_continuous_peak_mg_l=native_peak-true_peak if valid else None,
            reference_peak_sampling_gap_mg_l=true_peak-sampled_peak if valid else None,
            scope="Native peak is sampled; reference peak is characteristic supremum; point/last-cell errors are separate"),
        observations=observation_studies(case, times, field, config))


def refinement(rows, config):
    groups = {}
    for row in rows:
        groups.setdefault((row["regime"], row["study"]), []).append(row)
    studies = []
    for (regime, study), group in sorted(groups.items()):
        group.sort(key=lambda r: -r["cfl_safety"] if study == "time" else r["cells"])
        metric = "worst_temporal_error" if study == "time" else "worst_spatial_error" if regime != "scheduled_inlet" else "worst_continuum_error"
        values = [r[metric]["mean_l1_mg_l"] for r in group]
        orders = []
        for left, right, a, b in zip(group, group[1:], values, values[1:]):
            floor = config["integrity"]["reference_resolution_factor"]*max(left["reference"]["uncertainty_estimate_mg_l"],
                right["reference"]["uncertainty_estimate_mg_l"])
            ratio = left["cfl_safety"]/right["cfl_safety"] if study == "time" else right["cells"]/left["cells"]
            orders.append(math.log(a/b)/math.log(ratio) if a > floor and b > floor else None)
        studies.append(dict(regime=regime, study=study, case_ids=[r["id"] for r in group],
            error_metric=metric, mean_l1_errors_mg_l=values, observed_orders=orders,
            scope="time_at_fixed_mesh" if study == "time" else "semidiscrete_spatial" if regime != "scheduled_inlet" else "coupled_space_time"))
    return studies


def evaluate(document, config):
    expected = validate_matrix(document, config)
    rows = [evaluate_case(case, expected[case["id"]], config) for case in document["cases"]]
    return dict(status="completed", accuracy_status="characterized_not_qualified", case_count=len(rows),
        native_solver_version=document["solver_version"], studies=refinement(rows, config), cases=rows,
        limitations=["No application-wide field, peak, outlet or threshold accuracy qualification is claimed.",
            "Scheduled-inlet refinement remains coupled space/time; no semidiscrete inlet oracle is supplied.",
            "Reference uncertainty is an observed disagreement/roundoff estimate, not a formal enclosure.",
            "Saved outlet peaks and crossing brackets do not resolve unsaved native states.",
            "The legacy coarse failure and refined 0.40191 mg/L front limitation retain their original scope."])


def write_json(path, value):
    path.write_bytes((json.dumps(value, indent=2, allow_nan=False)+"\n").encode("utf-8"))


def write_summary(path, report):
    lines = ["# F1 front/outlet/threshold study", "", "Study: **"+report["status"]+"**; accuracy: **"+
        report.get("accuracy_status", "not_established")+"**.", "",
        "| Family | Study | Error scope | Observed orders | Finest mean-L1 mg/L |",
        "|---|---|---|---|---:|"]
    for study in report.get("studies", []):
        orders = ", ".join("unresolved" if v is None else f"{v:.5f}" for v in study["observed_orders"])
        lines.append(f"| {study['regime']} | {study['study']} | {study['scope']} | {orders} | {study['mean_l1_errors_mg_l'][-1]:.9g} |")
    if "error" in report:
        lines += ["", "Failure: "+report["error"]]
    lines += ["", *["- "+s for s in report.get("limitations", [])], ""]
    path.write_bytes("\n".join(lines).encode("utf-8"))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--config", type=Path, default=Path(__file__).with_name("f1_front_accuracy_config.json"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.output.resolve()
    try:
        output.mkdir(parents=True, exist_ok=False)
    except FileExistsError:
        print("Refusing to overwrite an existing F1 attempt directory", file=sys.stderr)
        return 2
    report = dict(status="failed", accuracy_status="not_established", started_utc=datetime.now(timezone.utc).isoformat(),
                  python=platform.python_version(), numpy=np.__version__, command=sys.argv)
    try:
        raw_config = args.config.read_bytes()
        (output/"config-input.json").write_bytes(raw_config)
        sources = [Path(__file__), ROOT/"science/breakwater_science/front_reference.py",
            ROOT/"science/breakwater_science/transport_reference.py", ROOT/"engine/examples/transport_qualification_reference.cpp",
            ROOT/"engine/src/transport.cpp", ROOT/"engine/include/breakwater/transport.hpp"]
        report.update(config_sha256=hashlib.sha256(raw_config).hexdigest(), native=str(args.native.resolve()),
            native_executable_sha256=None,
            source_hashes={p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources})
        write_json(output/"preflight.json", report)
        config = json.loads(raw_config)
        validate_config(config)
        native = args.native.resolve(strict=True)
        report.update(config=config, native=str(native), native_executable_sha256=hashlib.sha256(native.read_bytes()).hexdigest())
        write_json(output/"preflight.json", report)
        started = time.perf_counter()
        run = subprocess.run([str(native), *config["native_arguments"]], capture_output=True, timeout=300, check=False)
        report["native_execution_s"] = time.perf_counter()-started
        (output/"native-reference.json").write_bytes(run.stdout)
        (output/"native-stderr.txt").write_bytes(run.stderr)
        report["native_data_sha256"] = hashlib.sha256(run.stdout).hexdigest()
        require(run.returncode == 0 and not run.stderr, f"Native execution failed (exit {run.returncode}); raw evidence retained")
        started = time.perf_counter()
        report.update(evaluate(json.loads(run.stdout), config))
        report["analysis_s"] = time.perf_counter()-started
    except (ValueError, KeyError, TypeError, IndexError, RuntimeError, FloatingPointError, OverflowError,
            OSError, subprocess.SubprocessError) as error:
        report.update(status="failed", error=str(error))
    report["finished_utc"] = datetime.now(timezone.utc).isoformat()
    write_json(output/"report.json", report)
    write_summary(output/"report.md", report)
    print(f"F1 {report['status']}: {output/'report.json'}")
    if "error" in report:
        print(report["error"])
    return 0 if report["status"] == "completed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
