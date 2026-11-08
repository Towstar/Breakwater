"""Independent fixed-input countdown/exposure oracle, with no detector imports.

Offline verification only. Reconstructs complete CSV frames and categorical fault
onsets directly, using network/ops dictionaries rather than the compiled model.
It does not validate intervention plans or read answer keys.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


def verify(folder, report_paths):
    network = json.loads((folder / "network.json").read_text(encoding="utf-8-sig"))
    ops = json.loads((folder / "ops.json").read_text(encoding="utf-8-sig"))
    hashes = {name: hashlib.sha256((folder / name).read_bytes()).hexdigest()
              for name in ("network.json", "ops.json", "telemetry.csv")}
    facilities = [f for system in network["systems"] for f in system["facilities"]]
    checks = []
    for path in report_paths:
        report = json.loads(path.read_text(encoding="utf-8"))
        if report["status"] != "completed" or report["file_hashes"] != hashes:
            raise ValueError("Oracle/report source bytes do not match.")
        cutoff = report["actual_cutoff_unix"]
        previous, current, current_time, faults, productive = {}, {}, None, {}, {}
        def accept(frame, timestamp):
            for facility in facilities:
                fid, pid = facility["id"], facility["pump"]["id"]
                if frame[(pid, "status")] == 2:
                    if fid not in faults:
                        faults[fid] = timestamp
                        productive[fid] = tuple(w["id"] for w in facility["wells"]
                                                if previous.get((w["id"], "status")) == 1)
                else:
                    faults.pop(fid, None)
                    productive.pop(fid, None)
        with (folder / "telemetry.csv").open(encoding="utf-8-sig", newline="") as handle:
            for row in csv.DictReader(handle):
                timestamp = int(row["ts_unix"])
                if timestamp > cutoff:
                    break
                if current_time is not None and timestamp != current_time:
                    accept(current, current_time)
                    previous = current
                    current = {}
                current_time = timestamp
                current[(row["asset_id"], row["metric"])] = float(row["value"])
        if current_time != cutoff:
            raise ValueError("This independent oracle requires an exact complete-frame cutoff.")
        accept(current, current_time)
        actual = {event["facility_id"]: event for event in report["detection"]["events"]
                  if event["category"] == "pump_failure" and event["state"] == "ongoing"}
        if set(actual) != set(faults):
            raise ValueError("Current fault facility coverage differs from the raw categorical oracle.")
        for facility in facilities:
            fid = facility["id"]
            if fid not in faults:
                continue
            level = current[(facility["tank"]["id"], "level_pct")]
            inflow = current[(fid, "inflow_bpd")]
            shutdown = network["defaults"]["tank_limits_pct"]["shutdown"]
            all_shut = all(current[(w["id"], "status")] == 0 for w in facility["wells"])
            countdown = (0 if all_shut and level >= shutdown - 0.5 else
                         ((shutdown - level) / 100 * facility["tank"]["capacity_bbl"]) * 24 / inflow)
            remaining = ops["expected_repair_hours"]["pump_failure"] - (cutoff - faults[fid]) / 3600
            revenue = sum(w["oil_bpd"] * ops["prices"]["oil_per_bbl"] + w["gas_mcfd"] * ops["prices"]["gas_per_mcf"]
                          for w in facility["wells"] if w["id"] in productive[fid]) / 24
            event = actual[fid]
            passed = all(event[name] is not None and math.isclose(event[name], value, rel_tol=1e-12, abs_tol=1e-9)
                         for name, value in (("countdown_hours", countdown), ("repair_remaining_hours", remaining),
                                             ("hourly_revenue_exposure", revenue)))
            checks.append({"cutoff_unix": cutoff, "facility_id": fid, "passed": passed,
                           "expected_countdown_h": countdown, "actual_countdown_h": event["countdown_hours"],
                           "expected_remaining_h": remaining, "actual_remaining_h": event["repair_remaining_hours"],
                           "expected_exposure_per_h": revenue, "actual_exposure_per_h": event["hourly_revenue_exposure"]})
    return {"verification_version": 1, "scope": "raw-input current fault/countdown/repair/exposure only",
            "file_hashes": hashes, "checks": checks, "passed": bool(checks) and all(c["passed"] for c in checks),
            "limitations": ["Exact complete-frame cutoffs and known pre-fault productive states only.",
                            "Not an intervention-plan validator or a model of recorded future noise.",
                            "No answer-key access; no code imported from detector/model/input modules."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-folder", required=True, type=Path)
    parser.add_argument("--detection-reports", required=True, nargs="+", type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    report = verify(args.dataset_folder, args.detection_reports)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "cases": len(report["checks"])}))
    return 0 if report["passed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
