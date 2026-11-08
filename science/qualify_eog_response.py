"""Offline fixed-input provider qualification, never an answer-key evaluator.

Creates causal capture fixtures from the three explicitly named source files and
executes the real one-shot worker. This test capture generator is not the desktop
atomic capture producer. It supports single-line CSV records and preserves their
original bytes; unsupported multiline records fail explicitly.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from eog_inputs import load_inputs
from eog_response import compile_context
from eog_worker import ADOPTED_CONTRACT, BINDINGS, contract_module, encode_response


def write_json(path, value):
    path.write_bytes(json.dumps(value, ensure_ascii=False, allow_nan=False, indent=2).encode("utf-8") + b"\n")


def causal_csv(raw, cutoff):
    lines = raw.splitlines(keepends=True)
    result = [lines[0]]
    for line in lines[1:]:
        row = next(csv.reader([line.decode("utf-8", "strict")], strict=True))
        if len(row) != 5:
            raise ValueError("Qualification fixtures require single-line five-column CSV records.")
        if int(row[1]) > cutoff:
            break
        result.append(line)
    return b"".join(result)


def qualify(folder, output, contracts_root, native, cutoffs):
    contract = contract_module(contracts_root)
    loaded = load_inputs(folder)
    # Byte re-read must agree with the source snapshot that supplied its clock.
    contents = {name: (folder / name).read_bytes() for name in contract.FILENAMES}
    hashes = {name: hashlib.sha256(raw).hexdigest() for name, raw in contents.items()}
    if hashes != dict(loaded.file_hashes):
        raise ValueError("Source changed before fixture capture.")
    output.mkdir(parents=True, exist_ok=False)
    summary = {"qualification": "offline_real_worker_fixed_input", "adopted_contract": ADOPTED_CONTRACT,
               "source_hashes": hashes, "dataset_identity": contract.dataset_identity(hashes),
               "source_first_unix": loaded.observations.first_unix, "source_now_unix": loaded.observations.last_unix,
               "native_library_sha256": hashlib.sha256(native.read_bytes()).hexdigest(), "cuts": [],
               "limitations": ["No answer-key access, independent plan validation, held-out accuracy or installed application acceptance.",
                               "Fixture generator is not the B-owned atomic desktop capture producer.",
                               "Wall times include protected capture verification/loading/process startup; they are not optimized benchmarks."]}
    for cutoff in cutoffs:
        root = output / str(cutoff); root.mkdir()
        captured = root / "capture"; captured.mkdir()
        captured_contents = {**contents, "telemetry.csv": causal_csv(contents["telemetry.csv"], cutoff)}
        for name, raw in captured_contents.items():
            (captured / name).write_bytes(raw)
        files = {name: {"sha256": hashlib.sha256(raw).hexdigest(), "size_bytes": len(raw)} for name, raw in captured_contents.items()}
        manifest = {"eog_version": 1, "kind": "capture_manifest", "dataset_identity": summary["dataset_identity"],
                    "source_hashes": hashes, "captured_files": files, "source_first_unix": summary["source_first_unix"],
                    "source_now_unix": summary["source_now_unix"], "cutoff_unix": cutoff,
                    "context": "snapshot_now" if cutoff == summary["source_now_unix"] else "historical_cursor",
                    "sample_period_s": 300, "max_age_s": 300}
        manifest["capture_id"] = contract.capture_identity(manifest)
        contract.verify_capture(captured, manifest)
        request = {"eog_version": 1, "kind": "analysis_request", "request_id": "qualification-detect-" + str(cutoff),
                   "operation": "detect", "method_version": 1, **{key: manifest[key] for key in ("dataset_identity", "capture_id", "cutoff_unix")},
                   "capture_directory": str(captured), "capture_manifest": manifest, "arguments": {"system_ids": []}}
        def execute(request):
            start = time.perf_counter()
            run = subprocess.run([sys.executable, str(Path(__file__).with_name("eog_worker.py")),
                                  "--contracts-root", str(contracts_root), "--native-library", str(native)],
                                 input=encode_response(request), capture_output=True, timeout=120)
            elapsed = time.perf_counter() - start
            if run.returncode or run.stderr or run.stdout.count(b"\n") != 1:
                raise RuntimeError("Provider process failed its framing/exit/log boundary.")
            response = contract.decode_document(run.stdout)
            contract.validate_document(response, context={"request": request})
            write_json(root / (request["operation"] + "-request.json"), request)
            write_json(root / (request["operation"] + "-response.json"), response)
            return response, elapsed
        detection, detect_elapsed = execute(request)
        local = load_inputs(captured, capture_cutoff_unix=cutoff)
        try:
            context = compile_context(local.model, local.observations.at(cutoff))
        except ValueError as error:
            summary["cuts"].append({"cutoff_unix": cutoff, "detect_status": detection["status"],
                                    "evaluation_fixture_unavailable": getattr(error, "code", "context_unavailable")})
            continue
        plan = {"plan_id": "no-action-" + str(cutoff), "context": manifest["context"], "start_unix": cutoff,
                "repair_deadlines": [{"facility_id": f.id, "repair_elapsed_s": n["repair_s"]}
                                     for f, n in zip(context["facilities"], context["snapshot"]["facilities"])],
                "truck_jobs": [], "well_shut_ins": [], "inspection_assets": []}
        plan["plan_hash"] = contract.plan_identity(plan)
        request = {**request, "request_id": "qualification-evaluate-" + str(cutoff), "operation": "evaluate_plan", "arguments": {"plan": plan}}
        evaluation, eval_elapsed = execute(request)
        result = evaluation["result"]["evaluation"] if evaluation["status"] == "completed" else None
        summary["cuts"].append({"cutoff_unix": cutoff, "capture_id": manifest["capture_id"], "detect_status": detection["status"],
            "event_count": len(detection["result"]["events"]) if detection["status"] == "completed" else None,
            "evaluate_status": evaluation["status"], "feasible": result["feasible"] if result else None,
            "outcome": result["outcome"] if result else None, "cost": result["cost"] if result else None,
            "facility_count": len(plan["repair_deadlines"]), "detect_wall_s": detect_elapsed, "evaluate_wall_s": eval_elapsed})
    summary["passed"] = bool(summary["cuts"]) and all(c["detect_status"] == "completed" and
        c.get("evaluate_status") == "completed" and c.get("feasible") for c in summary["cuts"])
    write_json(output / "summary.json", summary)
    print(json.dumps(summary, ensure_ascii=True, allow_nan=False, indent=2))
    return summary["passed"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-folder", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--contracts-root", required=True, type=Path)
    parser.add_argument("--native-library", required=True, type=Path)
    parser.add_argument("--cutoff-unix", required=True, action="append", type=int)
    args = parser.parse_args()
    return 0 if qualify(args.dataset_folder.resolve(), args.output_dir.resolve(), args.contracts_root.resolve(),
                        args.native_library.resolve(), args.cutoff_unix) else 1


if __name__ == "__main__":
    raise SystemExit(main())
