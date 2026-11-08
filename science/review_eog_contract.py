"""Read-only, offline Person A probes for the exact proposed EOG contract.

This is review evidence, not the production detector/provider or an answer-key
evaluator. It reads only contract examples/schemas and creates its own tiny CSV.
"""
from __future__ import annotations

import argparse
import copy
from decimal import Decimal
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import tempfile


def review(root: Path, revision: str) -> dict:
    observed = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()
    if observed != revision:
        raise ValueError("Review checkout differs from the requested exact revision")
    subprocess.run(["git", "-C", str(root), "diff", "--exit-code", revision, "--",
                    "contracts/eog", "contracts/include/breakwater/eog.hpp"],
                   check=True, capture_output=True)
    contract = root / "contracts" / "eog"
    spec = importlib.util.spec_from_file_location("reviewed_eog_contract", contract / "validate.py")
    validator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(validator)

    def read(path):
        return json.loads((contract / path).read_text(encoding="utf-8"))

    def check(document, context=None):
        try:
            validator.validate_document(document, context=context)
            return {"accepted": True}
        except validator.ContractError as error:
            return {"accepted": False, "code": error.code}

    probes = []
    request = read("examples/evaluate-plan-request.json")
    result = read("examples/evaluate-plan-response.json")
    missing = copy.deepcopy(result)
    missing["result"]["evaluation"]["trajectories"] = []
    probes.append({"name": "completed-feasible-result-without-required-tank-trajectory",
                   "expected": "reject incomplete result, or explicitly mark coverage unavailable",
                   "observed": check(missing, {"request": request})})
    shortened = copy.deepcopy(result)
    trajectory = shortened["result"]["evaluation"]["trajectories"][0]
    time_key = "elapsed_s" if "elapsed_s" in trajectory else "times_unix"
    trajectory[time_key] = trajectory[time_key][:1]
    trajectory["volumes_bbl"] = trajectory["volumes_bbl"][:1]
    probes.append({"name": "completed-feasible-result-with-zero-forecast-horizon",
                   "expected": "reject incomplete result, or explicitly mark coverage unavailable",
                   "observed": check(shortened, {"request": request})})
    if time_key == "elapsed_s":
        fractional = read("examples/fractional-crossing-response.json")
    else:
        fractional = copy.deepcopy(result)
        fractional["result"]["evaluation"]["trajectories"][0][time_key][1] += 0.25
        fractional["result"]["evaluation"]["trajectories"][0][time_key][2] += 0.25
    probes.append({"name": "fractional-counterfactual-event-time",
                   "expected": "some exact relative/fractional event-time representation before numerical adoption",
                   "observed": check(fractional)})

    example_checks = {}
    if time_key == "elapsed_s":
        short_ulp = copy.deepcopy(result)
        points = short_ulp["result"]["evaluation"]["trajectories"][0][time_key]
        points[-1] = math.nextafter(points[-1], -math.inf)
        probes.append({"name": "forecast-one-ulp-before-required-endpoint",
                       "expected": "reject without feasibility epsilon",
                       "observed": check(short_ulp, {"request": request})})
        missing_facility = read("examples/two-facility-evaluation-response.json")
        missing_facility["result"]["evaluation"]["trajectories"].pop()
        probes.append({"name": "missing-one-of-two-required-facility-forecasts",
                       "expected": "reject missing facility", "observed": check(missing_facility)})
        empty_validation = read("examples/validate-plan-response.json")
        empty_validation["result"]["validation"]["validated_horizons"] = []
        probes.append({"name": "independent-validation-missing-required-horizons",
                       "expected": "reject incomplete validation",
                       "observed": check(empty_validation, {"request": read("examples/validate-plan-request.json")})})
        for filename in ("do-nothing-selected-search-response.json", "arrival-at-repair-infeasible-response.json",
                         "threshold-at-repair-response.json", "unknown-horizon-response.json"):
            observed_example = check(read("examples/" + filename))
            probes.append({"name": filename, "expected": "accept explicit reviewed outcome/unavailable semantics",
                           "observed": observed_example})
        no_action = read("examples/do-nothing-evaluation-response.json")["result"]["evaluation"]
        threshold_tie = read("examples/threshold-at-repair-response.json")["result"]["evaluation"]
        invalid_arrival = read("examples/arrival-at-repair-infeasible-response.json")["result"]["evaluation"]
        fractional_value = fractional["result"]["evaluation"]["shutdowns"][0]["elapsed_s"]
        example_checks = {
            "no_action_crossing_s": no_action["shutdowns"][0]["elapsed_s"] == 4320,
            "no_action_cost": Decimal(str(no_action["cost"]["total"])) == Decimal(100) * (Decimal(2) - Decimal("1.2")),
            "threshold_repair_equality": threshold_tie["shutdowns"][0]["elapsed_s"] == threshold_tie["plan"]["repair_deadlines"][0]["repair_elapsed_s"],
            "threshold_repair_loss_zero": threshold_tie["cost"]["lost_revenue"] == 0,
            "invalid_arrival_has_null_cost": not invalid_arrival["feasible"] and invalid_arrival["cost"] is None,
            "fractional_event_retained": fractional_value == (1 / 7) * 3600 and not fractional_value.is_integer(),
        }

    # Every source byte and source hash is correct; only the declared original
    # last timestamp is wrong. Snapshot-now should derive its endpoint from CSV.
    with tempfile.TemporaryDirectory(prefix="eog-contract-review-") as temp:
        folder = Path(temp)
        files = {"network.json": b"{}", "ops.json": b"{}",
                 "telemetry.csv": (b"timestamp,ts_unix,asset_id,metric,value\n"
                                   b"1970-01-01T00:00:00Z,0,pump,flow_bpd,0\n"
                                   b"1970-01-01T00:05:00Z,300,pump,flow_bpd,0\n")}
        for name, raw in files.items():
            (folder / name).write_bytes(raw)
        manifest = read("examples/capture-manifest.json")
        manifest.update(source_first_unix=0, source_now_unix=600, cutoff_unix=600,
                        context="snapshot_now")
        manifest["source_hashes"] = {name: hashlib.sha256(raw).hexdigest()
                                     for name, raw in files.items()}
        manifest["captured_files"] = {name: {"sha256": manifest["source_hashes"][name],
                                             "size_bytes": len(raw)}
                                      for name, raw in files.items()}
        manifest["dataset_identity"] = validator.dataset_identity(manifest["source_hashes"])
        manifest["capture_id"] = validator.capture_identity(manifest)
        try:
            verified = validator.verify_capture(folder, manifest)
            endpoint = {"accepted": True, "declared_now_unix": manifest["source_now_unix"],
                        "verified_csv_last_unix": verified["last_unix"]}
        except validator.ContractError as error:
            endpoint = {"accepted": False, "code": error.code}
        probes.append({"name": "snapshot-now-clock-differs-from-last-source-row",
                       "expected": "reject source-window mismatch", "observed": endpoint})

    d = Decimal
    cases = read("fixtures/analytical-cases.json")["cases"]
    truck_case, well_case, fleet_case = cases
    before = d(str(truck_case["initial_volume_bbl"])) + d(str(truck_case["inflow_bpd"])) / 24 * d(str(truck_case["trip_after_hours"]))
    after = before - d(str(truck_case["trip_volume_bbl"]))
    final = after + d(str(truck_case["inflow_bpd"])) / 24 * (
        d(str(truck_case["remaining_repair_hours"])) - d(str(truck_case["trip_after_hours"])))
    stop = d(str(well_case["shut_well_after_minutes"])) / 60
    horizon = d(str(well_case["remaining_repair_hours"]))
    water_rate = d(str(well_case["inflow_bpd"])) / 24
    avoided_rate = d(str(well_case["shut_well_water_bpd"])) / 24
    well_final = d(str(well_case["initial_volume_bbl"])) + water_rate * horizon - avoided_rate * (horizon - stop)
    well_cost = d(str(well_case["shut_well_revenue_per_hour"])) * (horizon - stop)
    arithmetic = {
        "truck_before_bbl": str(before), "truck_after_bbl": str(after),
        "truck_final_bbl": str(final), "well_final_bbl": str(well_final),
        "well_cost": str(well_cost),
        "expected_values_match": (
            before == d(str(truck_case["expected_volume_before_trip_bbl"])) and
            after == d(str(truck_case["expected_volume_after_trip_bbl"])) and
            final == d(str(truck_case["expected_final_volume_bbl"])) and
            d(str(truck_case["trip_cost"])) == d(str(truck_case["expected_total_cost"])) and
            (d(str(truck_case["shutdown_volume_bbl"])) - d(str(truck_case["initial_volume_bbl"]))) /
                (d(str(truck_case["inflow_bpd"])) / 24) == d(str(truck_case["expected_no_action_countdown_hours"])) and
            well_final == d(str(well_case["expected_final_volume_bbl"])) and
            well_cost == d(str(well_case["expected_total_cost"])) and
            fleet_case["overlapping_jobs_same_truck"] > fleet_case["fleet_size"] and
            fleet_case["expected_feasible"] is False
        ),
    }
    tracked = subprocess.check_output(
        ["git", "-C", str(root), "ls-tree", "-r", revision, "--",
         "contracts/eog", "contracts/include/breakwater/eog.hpp"], text=True)
    identities = []
    for line in tracked.splitlines():
        metadata, path = line.split("\t", 1)
        identities.append({"path": path, "git_blob_sha1": metadata.split()[2],
                           "sha256": hashlib.sha256((root / path).read_bytes()).hexdigest()})
    return {"reviewed_revision": revision, "source_files": identities,
            "production_input_access": False,
            "answer_key_access": False, "probes": probes,
            "new_example_arithmetic": example_checks, "independent_decimal_arithmetic": arithmetic}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", required=True, type=Path)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    report = review(args.checkout.resolve(), args.revision)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if (report["independent_decimal_arithmetic"]["expected_values_match"] and
                 all(report["new_example_arithmetic"].values())) else 1


if __name__ == "__main__":
    raise SystemExit(main())
