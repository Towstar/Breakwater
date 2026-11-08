"""Create renamed, smaller EOG practice data using only the three fixed inputs.

This deterministic variation is derived practice data, not an independent held-out
accuracy set. No fault labels, detector, forecast, model or sensor physics is used.
Retained CSV fields other than asset_id are preserved byte for byte. On the
project's Windows platform, rename publishes the complete new directory without
replacing an existing destination. The output parent must already exist.
"""
from __future__ import annotations

import argparse
import copy
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import tempfile
import unicodedata

FILES = ("network.json", "ops.json", "telemetry.csv")
JSON_LIMIT = 4 * 1024 * 1024
CSV_LIMIT = 128 * 1024 * 1024
PROVENANCE_LIMIT = 16 * 1024 * 1024
MAX_ROWS = 2_000_000
MAX_LINE = 16 * 1024
METHOD = "eog-derived-demo-v1"
METRICS = {"facility": {"inflow_bpd"}, "well": {"water_bpd", "status"}, "tank": {"level_pct"},
           "pump": {"status", "flow_bpd", "discharge_psi"}, "pipeline": {"receive_flow_bpd", "pressure_psi"}, "disposal": set()}


class DemoError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise DemoError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def read_input(folder, name):
    path = folder / name
    require(not path.is_symlink() and path.is_file(), "Each fixed-name input must be a plain regular file.")
    before = path.stat()
    limit = CSV_LIMIT if name == "telemetry.csv" else JSON_LIMIT
    require(0 < before.st_size <= limit, "Input exceeds its fixed byte limit or is empty.")
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    after = path.stat()
    require(len(raw) == before.st_size == after.st_size and before.st_mtime_ns == after.st_mtime_ns,
            "Input changed during the bounded read.")
    return raw


def parse_json(raw):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "Duplicate JSON key.")
            result[key] = value
        return result

    def finite(text):
        value = float(text)
        require(math.isfinite(value), "Nonfinite JSON number.")
        return value

    def constant(_):
        raise DemoError("Nonfinite JSON constant.")

    try:
        text = raw.decode("utf-8-sig")
        depth, quoted, escaped = 0, False, False
        for char in text:
            if quoted:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    quoted = False
            elif char == '"':
                quoted = True
            elif char in "[{":
                depth += 1
                require(depth <= 64, "JSON nesting exceeds 64.")
            elif char in "]}":
                depth -= 1
        return json.loads(text, object_pairs_hook=unique, parse_float=finite, parse_constant=constant)
    except (UnicodeError, json.JSONDecodeError, RecursionError, OverflowError) as error:
        raise DemoError("Invalid bounded UTF-8 JSON.") from error


def shape(value, required, optional=()):
    require(type(value) is dict and set(required) <= set(value) and not set(value) - set(required) - set(optional),
            "Missing or unsupported JSON structure.")


def number(value, minimum=0, *, positive=False, maximum=None):
    require(type(value) in (int, float), "Expected a numeric field.")
    try:
        finite = math.isfinite(value)
    except OverflowError:
        finite = False
    require(finite and value >= minimum and (not positive or value > 0) and (maximum is None or value <= maximum),
            "Unsupported numeric configuration.")


def identity(value):
    require(type(value) is str and value and not any(unicodedata.category(c) == "Cc" for c in value), "Invalid ID.")
    try:
        require(len(value.encode("utf-16-le")) // 2 <= 256, "ID exceeds 256 UTF-16 units.")
        value.encode("utf-8")
    except UnicodeError as error:
        raise DemoError("Invalid Unicode ID.") from error


def location(value):
    require(type(value) is list and len(value) == 2, "Expected a two-coordinate location.")
    number(value[0], -90, maximum=90)
    number(value[1], -180, maximum=180)


def structure(network, ops):
    shape(network, ("meta", "defaults", "systems"))
    shape(network["meta"], ("timezone", "units"))
    require(network["meta"] == {"timezone": "UTC", "units": {"volume": "bbl", "rate": "bpd", "gas_rate": "mcfd", "pressure": "psi"}}, "Unsupported units/timezone.")
    shape(network["defaults"], ("tank_limits_pct",))
    limits = network["defaults"]["tank_limits_pct"]
    shape(limits, ("warning", "shutdown"))
    number(limits["warning"], maximum=100)
    number(limits["shutdown"], positive=True, maximum=100)
    require(limits["warning"] < limits["shutdown"], "Invalid threshold ordering.")
    shape(ops, ("prices", "expected_repair_hours", "emergency_trucking", "systems"), ("dataset_id",))
    if "dataset_id" in ops:
        identity(ops["dataset_id"])
    shape(ops["prices"], ("oil_per_bbl", "gas_per_mcf"))
    for value in ops["prices"].values(): number(value)
    shape(ops["expected_repair_hours"], ("pump_failure",))
    number(ops["expected_repair_hours"]["pump_failure"], positive=True, maximum=168)
    shape(ops["emergency_trucking"], ("truck_capacity_bbl", "cost_per_trip"))
    number(ops["emergency_trucking"]["truck_capacity_bbl"], positive=True)
    number(ops["emergency_trucking"]["cost_per_trip"])
    require(type(network["systems"]) is list and 1 <= len(network["systems"]) <= 64 and type(ops["systems"]) is dict, "Unsupported systems.")
    ids, assets, system_ids, facility_count, well_count = {}, {}, set(), 0, 0

    def add(value, kind):
        identity(value)
        require(value not in ids and len(ids) < 32768, "Duplicate ID or asset count exceeds its bound.")
        ids[value] = kind
        if kind != "system": assets[value] = kind

    for system in network["systems"]:
        shape(system, ("id", "center", "truck_yard", "disposal_site", "facilities"))
        sid = system["id"]; add(sid, "system"); system_ids.add(sid)
        location(system["center"]); location(system["truck_yard"])
        disposal = system["disposal_site"]
        shape(disposal, ("id", "lat", "lon", "pressure_psi")); add(disposal["id"], "disposal")
        location([disposal["lat"], disposal["lon"]]); number(disposal["pressure_psi"])
        require(type(system["facilities"]) is list and 1 <= len(system["facilities"]) <= 256, "Unsupported facility list.")
        require(sid in ops["systems"], "Missing system operations reference.")
        settings = ops["systems"][sid]
        shape(settings, ("emergency_fleet_size", "facilities"))
        require(type(settings["emergency_fleet_size"]) is int and 0 <= settings["emergency_fleet_size"] <= 256 and type(settings["facilities"]) is dict, "Unsupported fleet/operations map.")
        facility_ids = set()
        for facility in system["facilities"]:
            shape(facility, ("id", "lat", "lon", "wells", "tank", "pump", "pipeline"))
            fid = facility["id"]; add(fid, "facility"); facility_ids.add(fid); facility_count += 1
            require(facility_count <= 256, "Facility limit exceeded.")
            location([facility["lat"], facility["lon"]])
            shape(facility["tank"], ("id", "capacity_bbl")); number(facility["tank"]["capacity_bbl"], positive=True)
            shape(facility["pump"], ("id", "max_flow_bpd", "start_level_pct", "stop_level_pct"))
            number(facility["pump"]["max_flow_bpd"], positive=True)
            stop, start = facility["pump"]["stop_level_pct"], facility["pump"]["start_level_pct"]
            number(stop, maximum=100); number(start, maximum=100)
            require(stop < start < limits["shutdown"], "Unsupported pump setpoint ordering.")
            shape(facility["pipeline"], ("id", "max_bpd", "max_pressure_psi"))
            number(facility["pipeline"]["max_bpd"], positive=True); number(facility["pipeline"]["max_pressure_psi"], positive=True)
            for kind in ("tank", "pump", "pipeline"): add(facility[kind]["id"], kind)
            require(type(facility["wells"]) is list and 1 <= len(facility["wells"]) <= 128, "Unsupported well list.")
            for well in facility["wells"]:
                shape(well, ("id", "baseline_water_bpd", "oil_bpd", "gas_mcfd")); add(well["id"], "well"); well_count += 1
                for key in ("baseline_water_bpd", "oil_bpd", "gas_mcfd"): number(well[key])
            require(fid in settings["facilities"], "Missing facility operations reference.")
            travel = settings["facilities"][fid]
            shape(travel, ("response_min", "cycle_min")); number(travel["response_min"]); number(travel["cycle_min"], positive=True)
        require(set(settings["facilities"]) == facility_ids, "Unknown facility operations reference.")
    require(set(ops["systems"]) == system_ids, "Unknown system operations reference.")
    return ids, assets, {"systems": len(system_ids), "facilities": facility_count, "wells": well_count, "assets": len(assets)}


def telemetry(raw, assets, mapping, removed):
    source = io.BytesIO(raw)
    header = source.readline(MAX_LINE + 1)
    require(header.decode("utf-8-sig").rstrip("\r\n") == "timestamp,ts_unix,asset_id,metric,value", "Unsupported CSV header.")
    output = io.BytesIO(); output.write(header)
    source_count = retained = removed_count = 0
    previous = first = last = None
    output_first = output_last = None
    seen, source_series, output_series = set(), set(), set()
    integer = re.compile(r"0|[1-9][0-9]{0,11}")
    while line := source.readline(MAX_LINE + 1):
        require(len(line) <= MAX_LINE and b"\n" not in line.rstrip(b"\r\n"), "Oversized or multiline CSV record.")
        try:
            row = next(csv.reader([line.decode("utf-8")], strict=True))
        except (UnicodeError, csv.Error) as error:
            raise DemoError("Unsupported CSV record.") from error
        require(len(row) == 5 and integer.fullmatch(row[1]) is not None, "Malformed CSV fields/clock.")
        time = int(row[1]); require(time <= 253402300799, "CSV clock out of range.")
        expected = datetime.fromtimestamp(time, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        require(row[0] == expected and (previous is None or time >= previous), "Unordered or contradictory CSV timestamp.")
        if time != previous:
            require(previous is None or (time - previous) % 300 == 0, "Unsupported CSV sampling cadence.")
            seen.clear()
        asset, metric = row[2], row[3]
        require(asset in assets and metric in METRICS[assets[asset]], "Unknown or unsupported telemetry attachment.")
        require((asset, metric) not in seen, "Duplicate telemetry sample identity."); seen.add((asset, metric))
        try:
            value = float(row[4])
        except ValueError as error:
            raise DemoError("Invalid CSV numeric value.") from error
        require(math.isfinite(value), "Nonfinite CSV value.")
        if metric == "status": require(value in ((0, 1, 2) if assets[asset] == "pump" else (0, 1)), "Unsupported equipment status.")
        source_count += 1; require(source_count <= MAX_ROWS, "CSV row limit exceeded.")
        source_series.add((asset, metric)); require(len(source_series) <= 65536, "CSV series limit exceeded.")
        if first is None: first = time
        previous = last = time
        if asset in removed:
            removed_count += 1; continue
        # Split raw CSV token boundaries, respecting escaped double quotes. Only
        # the asset token changes; all other field text and line endings survive.
        commas, quoted, index = [], False, 0
        while index < len(line):
            if line[index] == 34:
                if quoted and index + 1 < len(line) and line[index + 1] == 34: index += 1
                else: quoted = not quoted
            elif line[index] == 44 and not quoted: commas.append(index)
            index += 1
        require(len(commas) == 4, "Unsupported CSV token boundaries.")
        changed = line[:commas[1] + 1] + mapping[asset].encode("utf-8") + line[commas[2]:]
        require(len(changed) <= MAX_LINE and output.tell() + len(changed) <= CSV_LIMIT, "Renamed CSV exceeds its byte limits.")
        output.write(changed); retained += 1; output_series.add((mapping[asset], metric))
        if output_first is None: output_first = time
        output_last = time
    require(source_count > 0 and retained > 0, "No retained telemetry samples.")
    return output.getvalue(), {"readings": source_count, "series": len(source_series), "first_unix": first, "last_unix": last}, {
        "readings": retained, "series": len(output_series), "first_unix": output_first, "last_unix": output_last}, removed_count


def encoded(document, limit):
    raw = (json.dumps(document, ensure_ascii=False, allow_nan=False, indent=2) + "\n").encode("utf-8")
    require(len(raw) <= limit, "Output JSON exceeds its byte limit.")
    return raw


def write_file(path, raw):
    with path.open("xb") as stream:
        require(stream.write(raw) == len(raw), "Incomplete output write.")
        stream.flush(); os.fsync(stream.fileno())


def prepare_demo(dataset, output):
    dataset, output = Path(dataset), Path(output).absolute()
    require(dataset.is_dir(), "Source must be an existing dataset directory.")
    source = dataset.resolve(strict=True)
    require(not os.path.lexists(output) and output.parent.is_dir(), "Output must be new with an existing parent.")
    parent = output.parent.resolve(strict=True); output = parent / output.name
    require(not output.is_relative_to(source), "Output must be outside the source dataset.")
    raw = {name: read_input(source, name) for name in FILES}
    hashes = {name: sha(data) for name, data in raw.items()}
    network, ops = parse_json(raw["network.json"]), parse_json(raw["ops.json"])
    ids, assets, source_counts = structure(network, ops)
    eligible = sorted((s for s in network["systems"] if len(s["facilities"]) >= 2), key=lambda s: s["id"])
    require(eligible, "A topology variation requires a system containing at least two facilities.")
    system = eligible[0]; removed_facility = max(system["facilities"], key=lambda f: f["id"])
    removed = {removed_facility["id"], *(removed_facility[k]["id"] for k in ("tank", "pump", "pipeline")), *(w["id"] for w in removed_facility["wells"])}
    removed_topology = {"system_id": system["id"], "facility": copy.deepcopy(removed_facility), "asset_ids": sorted(removed),
                        "operations": copy.deepcopy(ops["systems"][system["id"]]["facilities"][removed_facility["id"]])}
    system["facilities"] = [f for f in system["facilities"] if f is not removed_facility]
    del ops["systems"][system["id"]]["facilities"][removed_facility["id"]]
    digest = sha(json.dumps(hashes, sort_keys=True, separators=(",", ":")).encode())
    prefix = "demo-Δ-" + digest[:12]
    mapping = {old: f"{prefix}/{ids[old]}/{index:05d}" for index, old in enumerate(sorted(set(ids) - removed), 1)}
    require(not set(mapping.values()) & set(ids), "Unsupported generated ID collision.")
    csv_output, source_rows, output_rows, removed_rows = telemetry(raw["telemetry.csv"], assets, mapping, removed)
    for s in network["systems"]:
        sid = s["id"]; s["id"] = mapping[sid]; s["disposal_site"]["id"] = mapping[s["disposal_site"]["id"]]
        settings = ops["systems"].pop(sid)
        for f in s["facilities"]:
            fid = f["id"]; f["id"] = mapping[fid]
            settings["facilities"][mapping[fid]] = settings["facilities"].pop(fid)
            for kind in ("tank", "pump", "pipeline"): f[kind]["id"] = mapping[f[kind]["id"]]
            for w in f["wells"]: w["id"] = mapping[w["id"]]
        ops["systems"][mapping[sid]] = settings
    source_dataset_id = ops.get("dataset_id"); ops["dataset_id"] = prefix + "/dataset"
    require(ops["dataset_id"] != source_dataset_id, "Unsupported generated dataset ID collision.")
    _, _, output_counts = structure(network, ops)
    data = {"network.json": encoded(network, JSON_LIMIT), "ops.json": encoded(ops, JSON_LIMIT), "telemetry.csv": csv_output}
    provenance = {"method": METHOD, "qualification": "Derived practice data; not independent held-out accuracy evidence. No detector, fault labels or sensor physics used.",
                  "source_hashes": hashes, "output_hashes": {name: sha(value) for name, value in data.items()},
                  "source_counts": {**source_counts, **source_rows}, "output_counts": {**output_counts, **output_rows},
                  "removed_topology": {**removed_topology, "readings": removed_rows},
                  "dataset_id_mapping": {"source": source_dataset_id, "output": ops["dataset_id"]},
                  "id_mapping": [{"source_id": old, "output_id": new, "kind": ids[old]} for old, new in mapping.items()]}
    metadata = encoded(provenance, PROVENANCE_LIMIT)
    with tempfile.TemporaryDirectory(prefix=".eog-demo-", dir=parent) as staging:
        stage = Path(staging)
        for name in FILES: write_file(stage / name, data[name])
        write_file(stage / "provenance.json", metadata)
        require(all(sha(read_input(source, name)) == hashes[name] for name in FILES), "Source changed before publication.")
        # Windows rename refuses an existing destination, including a directory
        # created concurrently. Never merge staged data into an existing folder.
        require(not os.path.lexists(output), "Output appeared before publication.")
        os.rename(stage, output)
    return provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = prepare_demo(args.dataset, args.output)
    except (DemoError, OSError, UnicodeError, ValueError) as error:
        print(json.dumps({"status": "failed", "error": str(error)}, ensure_ascii=False))
        return 2
    print(json.dumps({"status": "created", "method": METHOD, "output_counts": result["output_counts"],
                      "qualification": result["qualification"]}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
