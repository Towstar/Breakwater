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


def write(path, value):
    path.write_bytes(json.dumps(value, ensure_ascii=False, allow_nan=False, indent=2).encode() + b"\n")


def qualify(evaluator_records, output, contracts, cutoffs):
    contract = contract_module(contracts)
    output.mkdir(parents=True, exist_ok=False)
    summary = {"qualification": "independent_real_worker_recomputation", "adopted_contract": ADOPTED_CONTRACT,
               "source_evaluator_method": "eog-native-seconds-0.1.0", "cuts": [],
               "limitations": ["Correlated official practice prefixes, not held-out qualification or calibrated onset uncertainty.",
                               "Shared capture/model parsing; independent causal reconstruction, arithmetic, calendars and economics.",
                               "No native DLL, detector state, evaluator trajectory or evaluator cost is passed to the validation worker."]}
    for cutoff in cutoffs:
        source = evaluator_records / str(cutoff)
        request_raw = (source / "evaluate_plan-request.json").read_bytes()
        response_raw = (source / "evaluate_plan-response.json").read_bytes()
        request, response = contract.decode_document(request_raw), contract.decode_document(response_raw)
        contract.validate_document(request)
        contract.validate_document(response, context={"request": request})
        if response["status"] != "completed" or not response["result"]["evaluation"]["feasible"]:
            raise ValueError("Native source record is not a completed admissible evaluation.")
        request["operation"] = "validate_plan"
        request["request_id"] = "independent-" + str(cutoff)
        request["arguments"]["evaluator_method"] = summary["source_evaluator_method"]
        root = output / str(cutoff); root.mkdir()
        def execute(candidate, name):
            begin = time.perf_counter()
            run = subprocess.run([sys.executable, str(Path(__file__).with_name("eog_worker.py")),
                                  "--contracts-root", str(contracts)], input=encode_response(candidate),
                                 capture_output=True, timeout=120)
            elapsed = time.perf_counter() - begin
            if run.returncode or run.stderr or run.stdout.count(b"\n") != 1:
                raise RuntimeError("Independent worker failed process/framing requirements.")
            result = contract.decode_document(run.stdout)
            contract.validate_document(result, context={"request": candidate})
            write(root / (name + "-request.json"), candidate)
            write(root / (name + "-response.json"), result)
            return result, elapsed
        checked, elapsed = execute(request, "validate")
        validation = checked["result"]["validation"] if checked["status"] == "completed" else None
        native = response["result"]["evaluation"]
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
            "matched_native_cost_outcome_shutdowns_exactly": bool(matched), "forged_one_ulp_horizon_rejected": bool(invalid),
            "validated_facilities": len(validation["validated_horizons"]) if validation else 0,
            "independent_cost": validation["cost"] if validation else None, "wall_s": elapsed, "forged_wall_s": reject_elapsed})
    summary["passed"] = bool(summary["cuts"]) and all(c["matched_native_cost_outcome_shutdowns_exactly"] and
        c["forged_one_ulp_horizon_rejected"] for c in summary["cuts"])
    write(output / "summary.json", summary)
    print(json.dumps(summary, ensure_ascii=True, indent=2))
    return summary["passed"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evaluator-records", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--contracts-root", required=True, type=Path)
    parser.add_argument("--cutoff-unix", required=True, action="append", type=int)
    args = parser.parse_args()
    return 0 if qualify(args.evaluator_records.resolve(), args.output_dir.resolve(),
                        args.contracts_root.resolve(), args.cutoff_unix) else 1


if __name__ == "__main__":
    raise SystemExit(main())
