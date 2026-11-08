"""Offline fixed-input EOG detector run, independent of the pending worker binding.

Reads only network.json, ops.json and telemetry.csv. This is not an answer-key
evaluator and never estimates accuracy from matching its own generated fixtures.
"""
from __future__ import annotations

import argparse
from dataclasses import asdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import time

from eog_detection import METHOD, detect, ongoing_alerts
from eog_inputs import InputError, load_inputs
from eog_model import ModelError
from eog_observations import ObservationError


def execute(folder, cutoff=None):
    start = time.perf_counter()
    loaded = load_inputs(folder)
    load_s = time.perf_counter() - start
    if cutoff is None:
        cutoff = loaded.observations.last_unix
    result = detect(loaded.model, loaded.observations.at(cutoff))
    report = {"report_version": 1, "status": "completed", "mode": "offline_fixed_input_detection",
              "generated_utc": datetime.now(timezone.utc).isoformat(),
              "source_first_unix": loaded.observations.first_unix,
              "source_last_unix": loaded.observations.last_unix,
              "actual_cutoff_unix": cutoff,
              "raw_input_set_identity": loaded.raw_input_set_identity,
              "file_hashes": dict(loaded.file_hashes), "input_rows": loaded.observations.sample_count,
              "system_count": len(loaded.model.systems), "facility_count": len(loaded.model.facilities),
              "catalog_channels": len(loaded.model.channels()),
              "load_seconds": load_s, "total_seconds": time.perf_counter() - start,
              "detection": asdict(result), "ongoing_alert_ids": [e.id for e in ongoing_alerts(result)],
              "source_hashes": {name: hashlib.sha256((Path(__file__).parent / name).read_bytes()).hexdigest()
                                for name in ("eog_model.py", "eog_inputs.py", "eog_observations.py", "eog_detection.py", "inspect_eog.py")},
              "limitations": ["Declared causal heuristics; no calibrated confidence or learned fault model.",
                              "No production capture verifier/process binding or application integration is executed.",
                              "No answer key is loaded, and this report makes no accuracy/optimality claim.",
                              "Recorded observations and simulated assumptions remain distinct."]}
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-folder", type=Path, required=True)
    parser.add_argument("--cutoff-unix", type=int)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = execute(args.dataset_folder, args.cutoff_unix)
        encoded = json.dumps(report, ensure_ascii=False, allow_nan=False, indent=2).encode("utf-8") + b"\n"
        if len(encoded) > 4 * 1024 * 1024:
            raise InputError("limit_exceeded", "Complete offline report exceeds 4 MiB; no evidence is silently truncated.")
        status = 0
    except (InputError, ModelError, ObservationError) as error:
        report = {"report_version": 1, "status": "failed", "method": METHOD,
                  "diagnostic": {"code": error.code, "message": str(error)}}
        encoded = json.dumps(report, ensure_ascii=False, allow_nan=False, indent=2).encode("utf-8") + b"\n"
        status = 1
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=args.report.parent, prefix="eog-", suffix=".tmp", delete=False) as handle:
        temporary = Path(handle.name)
        handle.write(encoded)
    try:
        temporary.replace(args.report)
    finally:
        temporary.unlink(missing_ok=True)
    print(json.dumps({"status": report["status"], "report": str(args.report),
                      "events": len(report.get("detection", {}).get("events", [])),
                      "diagnostic": report.get("diagnostic")}, ensure_ascii=False))
    return status


if __name__ == "__main__":
    raise SystemExit(main())
