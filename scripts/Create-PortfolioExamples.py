"""Create small self-authored recorded datasets; never reads official recordings.

The schema/units follow the supported EOG adapter. These are local illustrative
traces, not calibrated field observations or detection qualification evidence.
"""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path


START = int(datetime(2026, 11, 16, 9, tzinfo=timezone.utc).timestamp())


def create_dataset(folder, renamed=False):
    folder.mkdir(parents=True, exist_ok=False)
    systems, operations, all_facilities = [], {}, []
    groups = [("DELTA/α", 2)] if renamed else [("NORTH", 2), ("SOUTH", 1)]
    for system_index, (sid, count) in enumerate(groups):
        lat, lon = 31.9 + system_index * 0.045, -102.3
        facilities, travel = [], {}
        for index in range(count):
            fid = sid + (f"/STATION-{index + 1}" if renamed else f"_PAD_{chr(65 + index)}")
            wells = [{"id": fid + f"_WELL_{j + 1}", "baseline_water_bpd": water,
                      "oil_bpd": oil, "gas_mcfd": gas}
                     for j, (water, oil, gas) in enumerate(((450, 560, 2000), (700, 170, 600), (650, 390, 1400)))]
            facility = {"id": fid, "lat": lat + index * 0.005, "lon": lon + index * 0.004,
                        "wells": wells, "tank": {"id": fid + "_TANK", "capacity_bbl": 800},
                        "pump": {"id": fid + "_PUMP", "max_flow_bpd": 2700, "start_level_pct": 70, "stop_level_pct": 30},
                        "pipeline": {"id": fid + "_PIPE", "max_bpd": 3000, "max_pressure_psi": 1700}}
            facilities.append(facility); all_facilities.append(facility)
            travel[fid] = {"response_min": 75, "cycle_min": 230}
        systems.append({"id": sid, "center": [lat, lon], "truck_yard": [lat - 0.003, lon - 0.002],
                        "disposal_site": {"id": sid + "_DISPOSAL", "lat": lat + 0.009,
                                          "lon": lon + 0.008, "pressure_psi": 1050}, "facilities": facilities})
        operations[sid] = {"emergency_fleet_size": 2, "facilities": travel}
    network = {"meta": {"units": {"volume": "bbl", "rate": "bpd", "gas_rate": "mcfd", "pressure": "psi"}, "timezone": "UTC"},
               "defaults": {"tank_limits_pct": {"warning": 85, "shutdown": 92}}, "systems": systems}
    ops = {"dataset_id": "breakwater-portfolio-renamed-v1" if renamed else "breakwater-portfolio-local-v1",
           "prices": {"oil_per_bbl": 60, "gas_per_mcf": 3}, "expected_repair_hours": {"pump_failure": 6},
           "emergency_trucking": {"truck_capacity_bbl": 150, "cost_per_trip": 905}, "systems": operations}
    for name, value in (("network.json", network), ("ops.json", ops)):
        (folder / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    states = {f["id"]: {"volume": 440.0 + i * 24.0, "running": False} for i, f in enumerate(all_facilities)}
    with (folder / "telemetry.csv").open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(("timestamp", "ts_unix", "asset_id", "metric", "value"))
        for step in range(37):
            stamp = START + step * 300
            iso = datetime.fromtimestamp(stamp, timezone.utc).isoformat().replace("+00:00", "Z")
            for index, facility in enumerate(all_facilities):
                state = states[facility["id"]]
                failed = index == 0 and step >= 24
                if not failed:
                    if state["volume"] >= 560: state["running"] = True
                    elif state["volume"] <= 240: state["running"] = False
                status = 2 if failed else int(state["running"])
                flow = 2700.0 if status == 1 else 0.0
                rows = [(facility["id"], "inflow_bpd", 1800.0),
                        (facility["tank"]["id"], "level_pct", state["volume"] / 8),
                        (facility["pump"]["id"], "status", status),
                        (facility["pump"]["id"], "flow_bpd", flow),
                        (facility["pump"]["id"], "discharge_psi", 1200 if status == 1 else 1050),
                        (facility["pipeline"]["id"], "receive_flow_bpd", flow),
                        (facility["pipeline"]["id"], "pressure_psi", 1100 if status == 1 else 1050)]
                for well in facility["wells"]:
                    rows.extend(((well["id"], "status", 1), (well["id"], "water_bpd", well["baseline_water_bpd"])))
                for asset, metric, value in rows: writer.writerow((iso, stamp, asset, metric, format(value, ".17g")))
                state["volume"] += (1800.0 - flow) / 24 * (300 / 3600)
    return {name: hashlib.sha256((folder / name).read_bytes()).hexdigest() for name in ("network.json", "ops.json", "telemetry.csv")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="New directory; existing datasets are never overwritten")
    args = parser.parse_args()
    if args.output.exists(): parser.error("Choose a new output directory.")
    args.output.mkdir(parents=True)
    manifest = {"format": 1, "source": "Self-authored local illustrative data; no official recordings", "datasets": {}}
    for name, renamed in (("operations", False), ("renamed-topology", True)):
        manifest["datasets"][name] = create_dataset(args.output / name, renamed)
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
