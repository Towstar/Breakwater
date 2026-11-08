"""Revalidate retained native plans through actual independent worker processes.

Reads explicit E3 evidence files/captures, never answer keys. The independent
worker receives the exact plan/capture and evaluator method, not native outcomes,
trajectories, claimed costs or a DLL path. Native records are compared afterward.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import time

from eog_worker import ADOPTED_CONTRACT, contract_module, encode_response
from eog_inputs import load_inputs
from eog_plan_validator import validate
from native_eog import NativeEvaluator


def write(path, value):
    path.write_bytes(json.dumps(value, ensure_ascii=False, allow_nan=False, indent=2).encode() + b"\n")


def qualify(evaluator_records, output, contracts, cutoffs, native_library=None):
    contract = contract_module(contracts)
    evaluator_method = NativeEvaluator(native_library).method if native_library is not None else "eog-native-seconds-0.1.0"
    output.mkdir(parents=True, exist_ok=False)
    summary = {"qualification": "independent_real_worker_recomputation", "adopted_contract": ADOPTED_CONTRACT,
               "source_evaluator_method": evaluator_method, "cuts": [],
               "limitations": ["Correlated official practice prefixes, not held-out qualification or calibrated onset uncertainty.",
                               "Shared capture/model parsing; independent causal reconstruction, arithmetic, calendars and economics.",
                               "No native DLL, detector state, evaluator trajectory or evaluator cost is passed to the validation worker."]}
    if native_library is not None:
        summary["fresh_native_library_sha256"] = hashlib.sha256(native_library.read_bytes()).hexdigest()
    for cutoff in cutoffs:
        source = evaluator_records / str(cutoff)
        request_raw = (source / "evaluate_plan-request.json").read_bytes()
        response_raw = (source / "evaluate_plan-response.json").read_bytes()
        request, response = contract.decode_document(request_raw), contract.decode_document(response_raw)
        contract.validate_document(request)
        contract.validate_document(response, context={"request": request})
        if response["status"] != "completed" or not response["result"]["evaluation"]["feasible"]:
            raise ValueError("Native source record is not a completed admissible evaluation.")
        native_request = copy.deepcopy(request)
        request["operation"] = "validate_plan"
        request["request_id"] = "independent-" + str(cutoff)
        request["arguments"]["evaluator_method"] = summary["source_evaluator_method"]
        root = output / str(cutoff); root.mkdir()
        def execute(candidate, name, native=False):
            begin = time.perf_counter()
            command = [sys.executable, str(Path(__file__).with_name("eog_worker.py")), "--contracts-root", str(contracts)]
            if native:
                command += ["--native-library", str(native_library)]
            run = subprocess.run(command, input=encode_response(candidate),
                                 capture_output=True, timeout=120)
            elapsed = time.perf_counter() - begin
            if run.returncode or run.stderr or run.stdout.count(b"\n") != 1:
                raise RuntimeError("Independent worker failed process/framing requirements.")
            result = contract.decode_document(run.stdout)
            contract.validate_document(result, context={"request": candidate})
            write(root / (name + "-request.json"), candidate)
            write(root / (name + "-response.json"), result)
            return result, elapsed
        frozen = response["result"]["evaluation"]
        fresh_matches, native_elapsed = None, None
        if native_library is not None:
            native_request["request_id"] += "-rechecked"
            fresh, native_elapsed = execute(native_request, "native-evaluation", native=True)
            if fresh["status"] != "completed" or not fresh["result"]["evaluation"]["feasible"]:
                raise RuntimeError("Fresh native worker did not complete the retained admissible plan.")
            native = fresh["result"]["evaluation"]
            fresh_matches = all(native[key] == frozen[key] for key in ("cost", "outcome", "shutdowns", "trajectories"))
        else:
            native = frozen
        checked, elapsed = execute(request, "validate")
        validation = checked["result"]["validation"] if checked["status"] == "completed" else None
        matched = validation is not None and validation["feasible"] and all(validation[key] == native[key] for key in ("cost", "outcome", "shutdowns"))
        if validation is not None:
            # Retain itemized per-well loss and volume/resource ledgers outside
            # the public schema. The audit must exactly match the real worker.
            loaded = load_inputs(request["capture_directory"], capture_cutoff_unix=cutoff)
            expected = {name: item["sha256"] for name, item in request["capture_manifest"]["captured_files"].items()}
            if dict(loaded.file_hashes) != expected:
                raise ValueError("Audit loaded bytes differ from the verified capture.")
            audited = validate(loaded.model, loaded.observations.at(cutoff), request["arguments"]["plan"], summary["source_evaluator_method"])
            if audited.record != validation:
                raise RuntimeError("Retained itemized audit differs from the worker validation.")
            write(root / "itemized-audit.json", audited.audit)
        bad = copy.deepcopy(request)
        bad["request_id"] += "-forged"
        bad_plan = bad["arguments"]["plan"]
        deadline = bad_plan["repair_deadlines"][0]
        deadline["repair_elapsed_s"] = math.nextafter(deadline["repair_elapsed_s"], math.inf)
        bad_plan["plan_hash"] = contract.plan_identity(bad_plan)
        rejected, reject_elapsed = execute(bad, "forged-horizon")
        assessed = rejected["result"]["validation"] if rejected["status"] == "completed" else None
        invalid = assessed is not None and not assessed["feasible"] and assessed["cost"] is None and bool(assessed["violations"])
        summary["cuts"].append({"cutoff_unix": cutoff, "native_request_sha256": hashlib.sha256(request_raw).hexdigest(),
            "native_response_sha256": hashlib.sha256(response_raw).hexdigest(), "validation_status": checked["status"],
            "fresh_native_matches_frozen_cost_outcome_crossings_trajectories": fresh_matches,
            "fresh_native_wall_s": native_elapsed,
            "matched_native_cost_outcome_shutdowns_exactly": bool(matched), "forged_one_ulp_horizon_rejected": bool(invalid),
            "validated_facilities": len(validation["validated_horizons"]) if validation else 0,
            "independent_cost": validation["cost"] if validation else None, "wall_s": elapsed, "forged_wall_s": reject_elapsed})
    summary["passed"] = bool(summary["cuts"]) and all(c["matched_native_cost_outcome_shutdowns_exactly"] and
        c["forged_one_ulp_horizon_rejected"] and c["fresh_native_matches_frozen_cost_outcome_crossings_trajectories"] is not False
        for c in summary["cuts"])
    write(output / "summary.json", summary)
    print(json.dumps(summary, ensure_ascii=True, indent=2))
    return summary["passed"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evaluator-records", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--contracts-root", required=True, type=Path)
    parser.add_argument("--cutoff-unix", required=True, action="append", type=int)
    parser.add_argument("--native-library", type=Path, help="Optionally re-execute native evaluation separately before independent validation.")
    args = parser.parse_args()
    return 0 if qualify(args.evaluator_records.resolve(), args.output_dir.resolve(),
                        args.contracts_root.resolve(), args.cutoff_unix,
                        args.native_library.resolve() if args.native_library else None) else 1


if __name__ == "__main__":
    raise SystemExit(main())
