"""Create bounded, self-authored inputs for production Nemotron evaluation.

This module generates inputs and proposed rubrics only. It runs no model,
simulation, detector or validator. Semantic outcomes require production preflight.
"""
from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import stat


ROOT = Path(__file__).resolve().parents[1]
FORMAT = "breakwater-nemotron-evaluation-fixtures-v1"
QUALIFICATION = "unqualified_requires_production_preflight"
EOG_FILES = ("network.json", "ops.json", "telemetry.csv")
POLICY = {"tool_calls": 12, "refinement_rounds": 3, "sandbox_duration_ms": 120000,
          "eog_duration_ms": 240000, "horizon_h": 3.0, "sample_period_s": 300, "max_age_s": 300}
SANDBOX_IDS = ("canonical-pump-failure", "healthy", "three-tank-chain", "shared-treatment", "balanced-cycle", "no-remedy")
EOG_IDS = ("operations", "renamed-topology", "multiple-failures-shared-fleet", "missing-tank-channel", "stale-tank-channel")
OBJECTIVES = {
    "sandbox": (
        "Investigate this synthetic network for the fixed three-hour horizon. Inspect its current state and available controls, run useful numerical experiments, and select a supported response for operator review when evidence permits. Explain constraints and limits.",
        "Assess whether the captured operating state needs a change over the unchanged forecast horizon. Use the actual network, control catalog and retained numerical validation. Compare useful alternatives and finish with a grounded selection or a bounded explanation when none is supported."),
    "eog": (
        "Investigate this immutable operations capture. Inspect supported evidence, evaluate useful responses and independently validate the response you select. State evidence limitations and remain advisory.",
        "Assess the current recorded operating conditions and bounded response options using the captured data and available tools. Return exact retained evidence references for any supported recommendation; explain missing evidence where it prevents a conclusion.")}
SOURCES = ("scripts/Create-PortfolioExamples.py", "scripts/nemotron_evaluation_fixtures.py",
           "engine/src/scenario.cpp", "desktop/ProjectDocument.cpp", "desktop/tests/agent_orchestrator_tests.cpp",
           "desktop/agent/AgentOrchestrator.hpp", "desktop/eog/EogInvestigator.hpp", "desktop/eog/EogCapture.cpp")


class FixtureError(ValueError):
    pass


def _require(condition, message):
    if not condition:
        raise FixtureError(message)


def _plain(path):
    info = path.lstat()
    _require(not stat.S_ISLNK(info.st_mode) and not (getattr(info, "st_file_attributes", 0) &
             getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)), "Symbolic links and reparse points are unsupported.")
    return info


def _destination(value):
    path = Path(value)
    _require(".." not in path.parts and path.name not in ("", ".", ".."), "Choose a plain new output directory.")
    path = path.absolute()
    _require(not os.path.lexists(path), "Output already exists; choose a new directory.")
    _require(path.parent.is_dir(), "The output parent must already exist.")
    for ancestor in (path.parent, *path.parent.parents):
        _require(stat.S_ISDIR(_plain(ancestor).st_mode), "Output ancestors must be plain directories.")
    return path


def _relative(root, relative):
    name = PurePosixPath(relative)
    _require(not name.is_absolute() and not PureWindowsPath(relative).drive and "\\" not in relative and
             name.parts and all(part not in (".", "..") for part in name.parts), "Output path escapes its root.")
    path = root.joinpath(*name.parts)
    _require(path.is_relative_to(root), "Output path escapes its root.")
    for ancestor in (path.parent, *path.parent.parents):
        if ancestor.is_relative_to(root):
            _require(stat.S_ISDIR(_plain(ancestor).st_mode), "Output directories must be plain.")
    return path


def _encoded(value):
    return (json.dumps(value, ensure_ascii=False, allow_nan=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def _write(root, relative, raw):
    _require(len(raw) <= 2 * 1024 * 1024, "An input exceeds its bounded fixture size.")
    with _relative(root, relative).open("xb") as stream:
        stream.write(raw)


def _file_record(path):
    info = _plain(path)
    _require(stat.S_ISREG(info.st_mode) and 0 < info.st_size <= 2 * 1024 * 1024, "Expected a bounded plain input file.")
    raw = path.read_bytes()
    _require(len(raw) == info.st_size, "An input changed during generation.")
    return {"sha256": hashlib.sha256(raw).hexdigest(), "size_bytes": len(raw)}


def _source_records():
    return {name: _file_record(ROOT / name) for name in SOURCES}


def _portfolio():
    spec = importlib.util.spec_from_file_location("breakwater_portfolio_evaluation_source", ROOT / SOURCES[0])
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _tank(identity, capacity, initial):
    return {"id": identity, "label": identity.replace("-", " ").title(), "kind": "tank", "capacity_m3": capacity, "initial_m3": initial}


def _source(identity, inflow, maximum, cost=250):
    return {"id": identity, "label": identity.replace("-", " ").title(), "kind": "source", "inflow_m3h": inflow,
            "curtailment_m3h": 0.0, "curtailment_max_m3h": maximum, "cost_per_m3": cost}


def _sink(identity, capacity, kind="treatment", cost=0.0):
    return {"id": identity, "label": identity.replace("-", " ").title(), "kind": kind, "capacity_m3h": capacity, "cost_per_m3": cost}


def _route(identity, source, target, capacity, value, pump=False, available=True):
    return {"id": identity, "source_id": source, "target_id": target, "capacity_m3h": capacity,
            "setpoint_m3h": value, "has_pump": pump, "available": available, "cost_per_m3": 0.0}


def sandbox_projects():
    """Serialize supported project-v1 inputs; these are not numerical answers."""
    canonical = {"assets": [
        {"id": "tank_a", "label": "Tank A", "kind": "tank", "capacity_m3": 120.0, "initial_m3": 80.0},
        {"id": "tank_b", "label": "Tank B", "kind": "tank", "capacity_m3": 130.0, "initial_m3": 114.0},
        {**_source("source_a", 52.0, 15.0), "label": "Production A"},
        {**_source("source_b", 16.0, 0.0), "label": "Production B"},
        {**_sink("treatment", 60.0), "label": "Treatment"},
        {**_sink("trucking", 22.0, "trucking", 80.0), "label": "Emergency trucking"}],
        "routes": [_route("feed_a", "source_a", "tank_a", 52.0, 52.0),
                   _route("feed_b", "source_b", "tank_b", 16.0, 16.0),
                   _route("main_pump", "tank_a", "treatment", 55.0, 44.0, True),
                   _route("backup_pump", "tank_b", "treatment", 35.0, 16.0, True),
                   _route("bypass", "tank_a", "tank_b", 25.0, 0.0),
                   _route("truck_route", "tank_a", "trucking", 22.0, 0.0)]}
    incident = copy.deepcopy(canonical)
    incident["routes"][2].update(available=False, setpoint_m3h=0.0)
    chain = {"assets": [_tank("west", 100.0, 50.0), _tank("east", 100.0, 50.0), _tank("north", 100.0, 50.0), _sink("plant", 10.0)],
             "routes": [_route("west-east", "west", "east", 10.0, 1.0), _route("east-north", "east", "north", 10.0, 1.0),
                        _route("north-plant", "north", "plant", 10.0, 1.0)]}
    branch = {"assets": [_tank("buffer-11", 70.0, 64.0), _tank("buffer-24", 80.0, 70.0),
                          _source("feed-11", 16.0, 6.0, 200.0), _source("feed-24", 12.0, 4.0, 230.0), _sink("plant-41", 20.0)],
              "routes": [_route("link-11", "feed-11", "buffer-11", 16.0, 16.0), _route("link-24", "feed-24", "buffer-24", 12.0, 12.0),
                         _route("link-41", "buffer-11", "plant-41", 16.0, 10.0), _route("link-42", "buffer-24", "plant-41", 12.0, 10.0)]}
    cycle = {"assets": [_tank(f"buffer-{i}", 90.0, 45.0) for i in (13, 26, 39)],
             "routes": [_route("link-13", "buffer-13", "buffer-26", 12.0, 4.0), _route("link-26", "buffer-26", "buffer-39", 12.0, 4.0),
                        _route("link-39", "buffer-39", "buffer-13", 12.0, 4.0)]}
    restricted = {"assets": [_tank("buffer-37", 20.0, 18.0), _source("feed-37", 10.0, 0.0), _sink("plant-37", 10.0)],
                  "routes": [_route("link-37", "feed-37", "buffer-37", 10.0, 10.0),
                             _route("link-38", "buffer-37", "plant-37", 10.0, 0.0, True, False)]}
    networks = (incident, canonical, chain, branch, cycle, restricted)
    result = {}
    for identity, network in zip(SANDBOX_IDS, networks, strict=True):
        positions = {asset["id"]: {"x": float((i % 5) * 100), "z": float((i // 5) * 100)} for i, asset in enumerate(network["assets"])}
        if identity in SANDBOX_IDS[:2]:
            positions = {key: {"x": x, "z": z} for key, x, z in (("tank_a", -100, 30), ("tank_b", 80, 30), ("source_a", -240, 30),
                         ("source_b", 80, 190), ("treatment", 100, -130), ("trucking", -120, -150))}
        result[identity] = {"version": 1, "synthetic": True, "network": network, "layout": {"positions": positions,
                            "camera": {"yaw": 35.0, "pitch": -55.0, "distance": 720.0, "x": 0.0, "z": 0.0}}}
    return result


def _csv_bytes(rows):
    stream = io.StringIO(newline="")
    csv.writer(stream, lineterminator="\n").writerows(rows)
    return stream.getvalue().encode("utf-8")


def _transform_eog(folder, identity, cutoff):
    network = json.loads((folder / "network.json").read_bytes())
    ops = json.loads((folder / "ops.json").read_bytes())
    rows = list(csv.reader(io.StringIO((folder / "telemetry.csv").read_text(encoding="utf-8"), newline="")))
    first = network["systems"][0]
    tank = first["facilities"][0]["tank"]["id"]
    changes = []
    if identity == "multiple-failures-shared-fleet":
        facility = first["facilities"][1]
        pump, pipe = facility["pump"]["id"], facility["pipeline"]["id"]
        onset = cutoff - 12 * POLICY["sample_period_s"]
        for row in rows[1:]:
            if int(row[1]) < onset:
                continue
            if row[2] == pump:
                row[4] = {"status": "2", "flow_bpd": "0", "discharge_psi": "1050"}[row[3]]
            elif row[2] == pipe:
                row[4] = {"receive_flow_bpd": "0", "pressure_psi": "1050"}[row[3]]
        ops["systems"][first["id"]]["emergency_fleet_size"] = 1
        changes = [{"kind": "illustrative_channel_corruption", "pump_id": pump, "pipeline_id": pipe,
                    "from_unix": onset, "description": "Added failure-status/zero-delivery channels; original measured tank series is unchanged. This is not a simulated physical fault trajectory."},
                   {"kind": "illustrative_policy_input", "system_id": first["id"], "emergency_fleet_size": 1}]
    elif identity in ("missing-tank-channel", "stale-tank-channel"):
        retained = [rows[0]]
        for row in rows[1:]:
            remove = row[2] == tank and row[3] == "level_pct" and (identity == "missing-tank-channel" or int(row[1]) > cutoff - 900)
            if not remove:
                retained.append(row)
        changes = [{"kind": "illustrative_missing_channel" if identity == "missing-tank-channel" else "illustrative_acquisition_gap",
                    "asset_id": tank, "metric": "level_pct", "removed_readings": len(rows) - len(retained)}]
        rows = retained
    else:
        return changes
    ops["dataset_id"] = "breakwater-evaluation-" + str(EOG_IDS.index(identity) + 1).zfill(2)
    # These files were created exclusively inside this new root; rewrite only the
    # generator's own files, never caller-supplied datasets.
    for name, raw in (("ops.json", _encoded(ops)), ("telemetry.csv", _csv_bytes(rows))):
        path = folder / name
        _plain(path)
        with path.open("wb") as stream:
            stream.write(raw)
    return changes


def generate_fixtures(destination):
    """Write a NEW directory and return its deterministic input/provenance manifest."""
    output = _destination(destination)
    sources = _source_records()
    portfolio = _portfolio()
    output.mkdir(exist_ok=False)
    (output / "inputs").mkdir()
    for mode in ("sandbox", "eog"):
        (output / "inputs" / mode).mkdir()
    inputs = []
    for identity, project in sandbox_projects().items():
        relative = f"inputs/sandbox/{identity}.project.json"
        _write(output, relative, _encoded(project))
        origin = "engine/src/scenario.cpp and desktop/ProjectDocument.cpp" if identity in SANDBOX_IDS[:2] else (
            "desktop/tests/agent_orchestrator_tests.cpp::modelUsesDiscoveredControlIdsOnADifferentGraph" if identity == "three-tank-chain" else "Self-authored project-v1 topology input; no numerical outcome is authored")
        inputs.append({"input_id": identity, "mode": "sandbox", "path": relative, "files": {relative: _file_record(output / relative)},
                       "provenance": {"source": origin, "qualification": QUALIFICATION}, "intended_outcome": "no_supported_plan" if identity == "no-remedy" else "advisory_plan"})
    cutoff = portfolio.START + 36 * POLICY["sample_period_s"]
    for identity in EOG_IDS:
        relative = f"inputs/eog/{identity}"
        folder = _relative(output, relative)
        base = portfolio.create_dataset(folder, renamed=identity == "renamed-topology")
        changes = _transform_eog(folder, identity, cutoff)
        intended = {"missing-tank-channel": "missing_coverage", "stale-tank-channel": "stale_coverage"}.get(identity, "advisory_plan")
        inputs.append({"input_id": identity, "mode": "eog", "path": relative, "cutoff_unix": cutoff,
                       "files": {f"{relative}/{name}": _file_record(folder / name) for name in EOG_FILES}, "intended_outcome": intended,
                       "provenance": {"source": SOURCES[0], "base_source_hashes": base, "illustrative_input_changes": changes,
                                      "qualification": "Self-authored illustrative acquisition/policy inputs; not calibrated field evidence, detector labels or numerical outcome ground truth."}})
    cases = []
    for item in inputs:
        for index, objective in enumerate(OBJECTIVES[item["mode"]], 1):
            case = {"case_id": item["mode"] + "-" + item["input_id"] + f"-p{index}", "mode": item["mode"], "input_id": item["input_id"],
                    "path": item["path"], "objective": objective, "prompt_variant": f"p{index}", "intended_outcome": item["intended_outcome"],
                    "semantic_qualification": QUALIFICATION, "input_files": item["files"],
                    "limits": {"tool_calls": POLICY["tool_calls"], "refinement_rounds": POLICY["refinement_rounds"], "duration_ms": POLICY[item["mode"] + "_duration_ms"]}}
            case["horizon_h" if item["mode"] == "sandbox" else "cutoff_unix"] = POLICY["horizon_h"] if item["mode"] == "sandbox" else cutoff
            cases.append(case)
    _write(output, "cases.json", _encoded({"format": FORMAT, "semantic_qualification": QUALIFICATION, "policy": POLICY, "cases": cases}))
    files = {path.relative_to(output).as_posix(): _file_record(path) for path in sorted(output.rglob("*")) if path.is_file()}
    _require(sum(info["size_bytes"] for info in files.values()) <= 8 * 1024 * 1024, "Fixture output exceeds its total bound.")
    _require(_source_records() == sources, "Source changed during generation; this output is incomplete.")
    manifest = {"format": FORMAT, "semantic_qualification": QUALIFICATION, "source": "Self-authored local illustrative inputs; no official recordings, network access or model execution",
                "policy": POLICY, "source_files": sources, "inputs": inputs, "case_count": len(cases), "files": files,
                "proposed_rubric": "Production preflight must qualify intended outcomes before hosting. Hosted success requires actual current-input tools and matching retained independent validation, or an evidence-grounded refusal. No global infeasibility, automatic approval, dispatch or application is established."}
    _write(output, "manifest.json", _encoded(manifest))
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="New directory under an existing plain parent")
    args = parser.parse_args()
    try:
        manifest = generate_fixtures(args.output)
    except (FixtureError, OSError) as error:
        parser.error(str(error))
    print(json.dumps({"case_count": manifest["case_count"], "semantic_qualification": manifest["semantic_qualification"]}))


if __name__ == "__main__":
    main()
