"""Offline bounded-search qualification using explicit captures and known fixtures.

Never loads labels. Real process search is followed by fresh evaluation and a
separate no-DLL validation of the exact selected plan. Generated known-value
fixtures are explicitly test infrastructure, never production worker imports.
"""
from __future__ import annotations

import argparse
import copy
from dataclasses import asdict, replace
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import time

from eog_search import METHOD, search
from eog_worker import ADOPTED_CONTRACT, contract_module, encode_response
from native_eog import NativeEvaluator


def write(path, value):
    path.write_bytes(json.dumps(value, ensure_ascii=False, allow_nan=False, indent=2).encode() + b"\n")


def known_cases(output, native, contract):
    sys.path.insert(0, str(Path(__file__).with_name("tests")))
    from test_eog_search import fixture
    cases = [
        ("one-truck", {}, 3, "No-truck loss >=10; any truck costs >=3; an early single full load attains 3."),
        ("shared-fleet", {"levels": (80, 80)}, 13,
         "One truck cannot return during either repair; unserved low-value field costs >=10 and serving the other costs >=3."),
        ("sequential-yard", {"levels": (80, 75), "response": 2, "cycle": 5, "capacity": 20, "trip_cost": 1}, 2,
         "A zero/one-load plan costs >=6; two sequential yard jobs can serve both fields for 2."),
        ("mixed", {"water": (1, 1), "revenue": (0.1, 10), "response": 2, "capacity": 20, "trip_cost": 1}, 2,
         "One load costs 1 and leaves >=10 bbl to curtail at >=0.1/bbl; no load costs >=11 and >=2 loads cost >=2."),
        ("nine-economic-loads", {"water": (1, 1), "revenue": (0.1, 10), "horizon": 100,
             "response": 2, "cycle": 5, "capacity": 10, "trip_cost": 5}, 55,
         "190 bbl need removing/avoiding; cheap water can avoid 100 for 10, leaving nine 5-cost loads. Fewer need expensive curtailment; more cost more than cheap water saved."),
        ("well-only", {"water": (1, 2, 3), "revenue": (1, 3, 12), "levels": (0,), "horizon": 4, "fleet": 0, "shutdown": 10}, 24,
         "Independent terminal fractional-knapsack lower bound: 4*1 + 4*3 + (2/3)*12 =24."),
        ("late-pickup", {"water": (1, 1), "revenue": (0.1, 10), "response": 8, "capacity": 20, "trip_cost": 1}, 2.8,
         "First pickup at 8 requires cheap well stop strictly before 2 to avoid the 90-bbl threshold; loss infimum 1.8 plus trip 1. Strict avoidance gives an infimum, not an attained real-number minimum.")]
    records = []
    for name, parameters, expected, derivation in cases:
        model, view = fixture(**parameters)
        if name == "shared-fleet":
            field = model.facilities[1]
            well = replace(field.wells[0], oil_bpd=172800, revenue_per_hour=7200)
            model = replace(model, facilities=(model.facilities[0], replace(field, wells=(well,))))
        found = search(model, view, native, identity=contract.plan_identity,
                       clock_context="snapshot_now", candidate_limit=3000, time_limit_s=10)
        chosen = next(e for e in found.result["evaluations"] if e["plan"]["plan_id"] == found.result["selected_plan_id"])
        checked = found.validation.record
        passed = math.isclose(chosen["cost"]["total"], expected, rel_tol=0, abs_tol=1e-9)
        passed = passed and checked["feasible"] and checked["independent"] and checked["cost"] == chosen["cost"]
        samples = [asdict(s) for channel in model.channels() for s in view.series(channel.asset_id, channel.metric)]
        write(output / (name + ".json"), {"parameters": parameters, "model": asdict(model), "samples": samples,
              "causal_cutoff_unix": view.cutoff_unix, "expected_cost_or_infimum": expected,
              "derivation": derivation, "passed": passed, "search_result": found.result,
              "search_audit": found.audit, "itemized_validation_audit": found.validation.audit})
        records.append({"case": name, "expected_cost_or_infimum": expected, "selected_total": chosen["cost"]["total"],
                        "evaluated_candidates": found.result["search"]["evaluated_candidates"], "passed": passed})
    return records


def qualify(source, output, contracts, native_library, cutoffs, candidate_limit, time_limit_s):
    contract = contract_module(contracts)
    native = NativeEvaluator(native_library)
    output.mkdir(parents=True, exist_ok=False)
    fixtures = output / "known"; fixtures.mkdir()
    summary = {"qualification": "bounded_joint_search_real_process_and_known_cases", "search_method": METHOD,
        "evaluator_method": native.method, "adopted_contract": ADOPTED_CONTRACT,
        "native_library_sha256": hashlib.sha256(native_library.read_bytes()).hexdigest(), "cuts": [],
        "limitations": ["Correlated official practice prefixes, not held-out data or calibrated onset/inspection qualification.",
            "Declared finite candidate family; no continuous/global optimality inferred from native feasibility or independent validation.",
            "Worker wall times include startup, capture verification, loading and snapshot preparation; reported planning time excludes those setup steps.",
            "Only selected plans are independently recomputed; other retained native alternatives are not independently certified.",
            "Known test bounds are analytical arguments for the stated synthetic fixtures, not physical validation or universal proofs."]}
    summary["known_cases"] = known_cases(fixtures, native, contract)
    for cutoff in cutoffs:
        root = output / str(cutoff); root.mkdir()
        raw_request = (source / str(cutoff) / "evaluate_plan-request.json").read_bytes()
        frozen_response = contract.decode_document((source / str(cutoff) / "evaluate_plan-response.json").read_bytes())
        request = contract.decode_document(raw_request)
        contract.validate_document(request)
        contract.validate_document(frozen_response, context={"request": request})
        baseline = frozen_response["result"]["evaluation"]
        if baseline["plan"]["truck_jobs"] or baseline["plan"]["well_shut_ins"] or not baseline["feasible"]:
            raise ValueError("Source is not an admissible no-action baseline.")
        request["operation"] = "search_plans"
        request["request_id"] = "joint-search-" + str(cutoff)
        request["arguments"] = {"system_ids": [], "candidate_limit": candidate_limit, "time_limit_s": time_limit_s}
        def execute(candidate, name, *, use_native):
            command = [sys.executable, str(Path(__file__).with_name("eog_worker.py")), "--contracts-root", str(contracts)]
            if use_native: command += ["--native-library", str(native_library)]
            begin = time.perf_counter()
            completed = subprocess.run(command, input=encode_response(candidate), capture_output=True, timeout=180)
            elapsed = time.perf_counter() - begin
            write(root / (name + "-request.json"), candidate)
            if completed.returncode or completed.stderr or completed.stdout.count(b"\n") != 1:
                raise RuntimeError("Worker process/framing failed in " + name)
            response = contract.decode_document(completed.stdout)
            write(root / (name + "-response.json"), response)
            contract.validate_document(response, context={"request": candidate})
            return response, elapsed
        found, search_wall = execute(request, "search", use_native=True)
        if found["status"] != "completed" or found["result"]["selected_plan_id"] is None:
            raise RuntimeError("Search did not return an independently checked selection.")
        result = found["result"]
        selected = next(e for e in result["evaluations"] if e["plan"]["plan_id"] == result["selected_plan_id"])
        retained_baseline = next(e for e in result["evaluations"] if not e["plan"]["truck_jobs"] and not e["plan"]["well_shut_ins"])
        same_baseline = all(retained_baseline[key] == baseline[key] for key in ("cost", "outcome", "shutdowns", "trajectories"))
        reevaluate = copy.deepcopy(request)
        reevaluate.update(operation="evaluate_plan", request_id="selected-native-" + str(cutoff))
        reevaluate["arguments"] = {"plan": selected["plan"]}
        fresh, fresh_wall = execute(reevaluate, "selected-native", use_native=True)
        fresh_eval = fresh["result"]["evaluation"] if fresh["status"] == "completed" else None
        native_equal = fresh_eval is not None and all(fresh_eval[key] == selected[key] for key in (
            "plan", "feasible", "cost", "outcome", "shutdowns", "trajectories", "fleet_occupancy"))
        independently = copy.deepcopy(reevaluate)
        independently.update(operation="validate_plan", request_id="selected-independent-" + str(cutoff))
        independently["arguments"]["evaluator_method"] = native.method
        checked, validation_wall = execute(independently, "selected-independent", use_native=False)
        validation = checked["result"]["validation"] if checked["status"] == "completed" else None
        independent_equal = validation is not None and validation["independent"] and validation["feasible"] and all(
            validation[key] == selected[key] for key in ("cost", "outcome", "shutdowns"))
        independent_equal = independent_equal and validation["plan_hash"] == selected["plan"]["plan_hash"]
        independent_equal = independent_equal and validation["validated_horizons"] == selected["plan"]["repair_deadlines"]
        record = {"cutoff_unix": cutoff, "search_status": found["status"], "baseline_matches_frozen": same_baseline,
            "selected_matches_fresh_native_exactly": bool(native_equal), "selected_matches_no_dll_validation_exactly": bool(independent_equal),
            "baseline_total": baseline["cost"]["total"], "selected_cost": selected["cost"],
            "selected_outcome": selected["outcome"], "selected_plan_hash": selected["plan"]["plan_hash"],
            "validated_facilities": len(validation["validated_horizons"]) if validation else 0,
            "search": result["search"], "search_wall_s": search_wall, "fresh_native_wall_s": fresh_wall,
            "independent_wall_s": validation_wall}
        record["passed"] = bool(same_baseline and native_equal and independent_equal and selected["cost"]["total"] <= baseline["cost"]["total"])
        summary["cuts"].append(record)
        write(output / "summary.json", summary)
        print(json.dumps(record), flush=True)
    summary["passed"] = all(c["passed"] for c in summary["known_cases"]) and bool(summary["cuts"]) and all(c["passed"] for c in summary["cuts"])
    write(output / "summary.json", summary)
    return summary["passed"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evaluator-records", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--contracts-root", required=True, type=Path)
    parser.add_argument("--native-library", required=True, type=Path)
    parser.add_argument("--cutoff-unix", required=True, action="append", type=int)
    parser.add_argument("--candidate-limit", type=int, default=3000)
    parser.add_argument("--time-limit-s", type=float, default=15)
    args = parser.parse_args()
    return 0 if qualify(args.evaluator_records.resolve(), args.output_dir.resolve(), args.contracts_root.resolve(),
                        args.native_library.resolve(), args.cutoff_unix, args.candidate_limit, args.time_limit_s) else 1


if __name__ == "__main__":
    raise SystemExit(main())
