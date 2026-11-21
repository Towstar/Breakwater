"""Input-generation checks only; no model, physics or semantic qualification."""
import csv
import hashlib
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import re
import stat
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("nemotron_evaluation_fixtures", ROOT / "scripts/nemotron_evaluation_fixtures.py")
fixtures = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixtures)


def read_json(path):
    return json.loads(path.read_bytes())


def readings(folder):
    return list(csv.DictReader(io.StringIO((folder / "telemetry.csv").read_text(encoding="utf-8"), newline="")))


def entities(network):
    result = set()
    for system in network["systems"]:
        result.add(system["disposal_site"]["id"])
        for facility in system["facilities"]:
            result.add(facility["id"])
            result.update(facility[key]["id"] for key in ("tank", "pump", "pipeline"))
            result.update(well["id"] for well in facility["wells"])
    return result


class NemotronEvaluationFixtureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="breakwater-nemotron-inputs-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.output = self.root / "new inputs α"
        self.manifest = fixtures.generate_fixtures(self.output)
        self.inputs = {item["input_id"]: item for item in self.manifest["inputs"]}

    def test_determinism_and_every_input_and_catalog_hash(self):
        second = self.root / "repeat"
        self.assertEqual(self.manifest, fixtures.generate_fixtures(second))
        first_files = {p.relative_to(self.output).as_posix(): p.read_bytes() for p in self.output.rglob("*") if p.is_file()}
        second_files = {p.relative_to(second).as_posix(): p.read_bytes() for p in second.rglob("*") if p.is_file()}
        self.assertEqual(first_files, second_files)
        self.assertEqual(set(first_files), {*self.manifest["files"], "manifest.json"})
        for relative, info in self.manifest["files"].items():
            raw = first_files[relative]
            self.assertEqual(info, {"sha256": hashlib.sha256(raw).hexdigest(), "size_bytes": len(raw)})
        for item in self.inputs.values():
            self.assertTrue(item["files"])
            for relative, info in item["files"].items():
                self.assertEqual(info, self.manifest["files"][relative])
        for relative, info in self.manifest["source_files"].items():
            raw = (ROOT / relative).read_bytes()
            self.assertEqual(info, {"sha256": hashlib.sha256(raw).hexdigest(), "size_bytes": len(raw)})
        self.assertLess(sum(info["size_bytes"] for info in self.manifest["files"].values()), 8 * 1024 * 1024)

    def test_catalog_has_exact_membership_two_neutral_prompts_and_no_qualified_outcome(self):
        catalog = read_json(self.output / "cases.json")
        cases = catalog["cases"]
        self.assertEqual(len(self.inputs), 11)
        self.assertEqual(set(self.inputs), set(fixtures.SANDBOX_IDS) | set(fixtures.EOG_IDS))
        self.assertEqual(len(cases), self.manifest["case_count"])
        self.assertEqual(len(cases), 22)
        self.assertEqual(len({case["case_id"] for case in cases}), 22)
        for identity, item in self.inputs.items():
            matching = [case for case in cases if case["input_id"] == identity]
            self.assertEqual({case["prompt_variant"] for case in matching}, {"p1", "p2"})
            self.assertEqual(len({case["objective"] for case in matching}), 2)
            for case in matching:
                self.assertEqual(case["mode"], item["mode"])
                self.assertEqual(case["path"], item["path"])
                self.assertEqual(case["input_files"], item["files"])
                self.assertEqual(case["semantic_qualification"], fixtures.QUALIFICATION)
                self.assertNotIn("result", case)
                self.assertNotIn("expected_plan", case)
                self.assertEqual(case["objective"], fixtures.OBJECTIVES[item["mode"]][int(case["prompt_variant"][1]) - 1])
        self.assertEqual(self.inputs["no-remedy"]["intended_outcome"], "no_supported_plan")
        self.assertEqual(self.inputs["missing-tank-channel"]["intended_outcome"], "missing_coverage")
        self.assertEqual(self.inputs["stale-tank-channel"]["intended_outcome"], "stale_coverage")

    def test_projects_keep_canonical_source_values_and_custom_topologies(self):
        projects = {key: read_json(self.output / self.inputs[key]["path"]) for key in fixtures.SANDBOX_IDS}
        healthy, incident = projects["healthy"], projects["canonical-pump-failure"]
        self.assertEqual(healthy["network"]["assets"], incident["network"]["assets"])
        self.assertEqual([a["initial_m3"] for a in healthy["network"]["assets"] if a["kind"] == "tank"], [80, 114])
        main = next(r for r in healthy["network"]["routes"] if r["id"] == "main_pump")
        failed = next(r for r in incident["network"]["routes"] if r["id"] == "main_pump")
        self.assertEqual((main["capacity_m3h"], main["setpoint_m3h"], main["available"]), (55, 44, True))
        self.assertEqual(failed, {**main, "setpoint_m3h": 0.0, "available": False})
        self.assertEqual(healthy["layout"]["positions"]["source_a"], {"x": -240, "z": 30})
        source = (ROOT / "engine/src/scenario.cpp").read_text(encoding="utf-8")
        # Read the small literal initializer, rather than silently letting our
        # canonical projection drift when a source value or label changes.
        def initializers(member):
            block = source.split("n." + member + " = {", 1)[1].split("};", 1)[0]
            return [next(csv.reader([record], skipinitialspace=True)) for record in re.findall(r"\{([^{}]+)\}", block)]
        expected_assets = []
        for fields in initializers("assets"):
            kind = fields[2].split("::")[1]
            values = [float(value) for value in fields[3:]] + [0.0] * (6 - len(fields[3:]))
            capacity, initial, inflow, curtailment, maximum, cost = values
            item = {"id": fields[0], "label": fields[1], "kind": kind}
            if kind == "tank":
                item.update(capacity_m3=capacity, initial_m3=initial)
            elif kind == "source":
                item.update(inflow_m3h=inflow, curtailment_m3h=curtailment, curtailment_max_m3h=maximum, cost_per_m3=cost)
            else:
                item.update(capacity_m3h=capacity, cost_per_m3=cost)
            expected_assets.append(item)
        expected_routes = []
        for fields in initializers("routes"):
            expected_routes.append({"id": fields[0], "source_id": fields[1], "target_id": fields[2],
                                    "capacity_m3h": float(fields[3]), "setpoint_m3h": float(fields[4]),
                                    "has_pump": fields[5] == "true" if len(fields) > 5 else False,
                                    "available": fields[6] == "true" if len(fields) > 6 else True,
                                    "cost_per_m3": float(fields[7]) if len(fields) > 7 else 0.0})
        self.assertEqual(healthy["network"], {"assets": expected_assets, "routes": expected_routes})
        shapes = set()
        for identity, project in projects.items():
            self.assertEqual(project["version"], 1)
            self.assertIs(project["synthetic"], True)
            assets, routes = project["network"]["assets"], project["network"]["routes"]
            ids = {asset["id"] for asset in assets}
            self.assertEqual(len(ids) + len(routes), len(ids | {route["id"] for route in routes}))
            self.assertEqual(set(project["layout"]["positions"]), ids)
            for route in routes:
                self.assertIn(route["source_id"], ids)
                self.assertIn(route["target_id"], ids)
                self.assertGreaterEqual(route["capacity_m3h"], route["setpoint_m3h"])
            if identity not in fixtures.SANDBOX_IDS[:2]:
                shapes.add((len(assets), len(routes)))
                self.assertNotIn("tank_a", ids)
                self.assertFalse(any("feasible" in a["label"].lower() or "remedy" in a["label"].lower() for a in assets))
        self.assertEqual(shapes, {(4, 3), (5, 4), (3, 3), (3, 2)})
        chain = projects["three-tank-chain"]["network"]
        self.assertEqual([(r["source_id"], r["target_id"], r["setpoint_m3h"]) for r in chain["routes"]],
                         [("west", "east", 1), ("east", "north", 1), ("north", "plant", 1)])
        cycle = projects["balanced-cycle"]["network"]
        self.assertEqual({r["source_id"] for r in cycle["routes"]}, {r["target_id"] for r in cycle["routes"]})

    def test_eog_reuses_base_inputs_and_renamed_topology_has_only_known_references(self):
        for identity in fixtures.EOG_IDS:
            item = self.inputs[identity]
            folder = self.output / item["path"]
            network, ops = read_json(folder / "network.json"), read_json(folder / "ops.json")
            ids = entities(network)
            self.assertEqual(set(ops["systems"]), {system["id"] for system in network["systems"]})
            for system in network["systems"]:
                self.assertEqual(set(ops["systems"][system["id"]]["facilities"]), {f["id"] for f in system["facilities"]})
            self.assertTrue({row["asset_id"] for row in readings(folder)} <= ids)
            base = item["provenance"]["base_source_hashes"]
            self.assertEqual(item["files"][item["path"] + "/network.json"]["sha256"], base["network.json"])
            if identity in ("operations", "renamed-topology"):
                for name in fixtures.EOG_FILES:
                    self.assertEqual(item["files"][item["path"] + "/" + name]["sha256"], base[name])
        original = read_json(self.output / self.inputs["operations"]["path"] / "network.json")
        renamed = read_json(self.output / self.inputs["renamed-topology"]["path"] / "network.json")
        self.assertTrue(entities(original).isdisjoint(entities(renamed)))
        self.assertEqual((len(original["systems"]), len(renamed["systems"])), (2, 1))
        self.assertEqual(sum(len(s["facilities"]) for s in original["systems"]), 3)
        self.assertEqual(sum(len(s["facilities"]) for s in renamed["systems"]), 2)
        self.assertTrue(any("α" in value for value in entities(renamed)))

    def test_dense_csv_clock_missing_channel_and_stale_gap_are_explicit_inputs(self):
        base_folder = self.output / self.inputs["operations"]["path"]
        base = readings(base_folder)
        network = read_json(base_folder / "network.json")
        tank = network["systems"][0]["facilities"][0]["tank"]["id"]
        cutoff = self.inputs["operations"]["cutoff_unix"]
        self.assertEqual(len(base), 37 * 3 * 13)
        clock = sorted({int(row["ts_unix"]) for row in base})
        self.assertEqual(clock[-1], cutoff)
        self.assertEqual({b - a for a, b in zip(clock, clock[1:])}, {300})
        for identity in fixtures.EOG_IDS:
            rows = readings(self.output / self.inputs[identity]["path"])
            keys = [(row["ts_unix"], row["asset_id"], row["metric"]) for row in rows]
            self.assertEqual(len(keys), len(set(keys)))
            self.assertEqual([int(row["ts_unix"]) for row in rows], sorted(int(row["ts_unix"]) for row in rows))
            self.assertTrue(all(math.isfinite(float(row["value"])) for row in rows))
        missing = readings(self.output / self.inputs["missing-tank-channel"]["path"])
        stale = readings(self.output / self.inputs["stale-tank-channel"]["path"])
        self.assertFalse(any(row["asset_id"] == tank and row["metric"] == "level_pct" for row in missing))
        last = max(int(row["ts_unix"]) for row in stale if row["asset_id"] == tank and row["metric"] == "level_pct")
        self.assertEqual(cutoff - last, 900)
        self.assertGreater(cutoff - last, fixtures.POLICY["max_age_s"])
        unaffected = lambda rows: [row for row in rows if not (row["asset_id"] == tank and row["metric"] == "level_pct")]
        self.assertEqual(unaffected(base), unaffected(missing))
        self.assertEqual(unaffected(base), unaffected(stale))
        for identity, removed in (("missing-tank-channel", 37), ("stale-tank-channel", 3)):
            change = self.inputs[identity]["provenance"]["illustrative_input_changes"][0]
            self.assertEqual(change["removed_readings"], removed)
            self.assertTrue(change["kind"].startswith("illustrative_"))

    def test_multiple_failure_channels_share_policy_without_inventing_tank_truth(self):
        folder = self.output / self.inputs["multiple-failures-shared-fleet"]["path"]
        base = self.output / self.inputs["operations"]["path"]
        network, ops = read_json(folder / "network.json"), read_json(folder / "ops.json")
        system = network["systems"][0]
        self.assertEqual(ops["systems"][system["id"]]["emergency_fleet_size"], 1)
        pumps = {facility["pump"]["id"] for facility in system["facilities"]}
        current = [row for row in readings(folder) if int(row["ts_unix"]) == self.inputs["operations"]["cutoff_unix"]]
        self.assertEqual({row["asset_id"] for row in current if row["metric"] == "status" and row["value"] == "2"}, pumps)
        tank_rows = lambda directory: [row for row in readings(directory) if row["metric"] == "level_pct"]
        self.assertEqual(tank_rows(folder), tank_rows(base))
        self.assertIn("not a simulated physical fault trajectory", self.inputs["multiple-failures-shared-fleet"]["provenance"]["illustrative_input_changes"][0]["description"])

    def test_exclusive_output_rejects_existing_files_directories_and_path_escapes(self):
        marker = self.output / "keep.txt"
        marker.write_bytes(b"preserve")
        before = (self.output / "manifest.json").read_bytes()
        for target in (self.output, marker, self.root / "missing-parent" / "new", self.root / "child" / ".." / "escape"):
            with self.subTest(target=target), self.assertRaises(fixtures.FixtureError):
                fixtures.generate_fixtures(target)
        self.assertEqual(marker.read_bytes(), b"preserve")
        self.assertEqual((self.output / "manifest.json").read_bytes(), before)
        self.assertFalse((self.root / "missing-parent").exists())
        for relative in ("../escape", "/escape", "C:/escape", "inputs\\escape"):
            with self.subTest(relative=relative), self.assertRaises(fixtures.FixtureError):
                fixtures._write(self.output, relative, b"escape")
        self.assertFalse((self.root / "escape").exists())

    def test_output_symlink_and_symlink_ancestor_are_rejected(self):
        target, link = self.root / "real", self.root / "link"
        target.mkdir()
        try:
            link.symlink_to(target, target_is_directory=True)
        except OSError as error:
            self.skipTest("Directory symlinks are unavailable: " + type(error).__name__)
        for output in (link, link / "new"):
            with self.subTest(output=output), self.assertRaises(fixtures.FixtureError):
                fixtures.generate_fixtures(output)
        self.assertEqual(list(target.iterdir()), [])

    def test_windows_reparse_attribute_is_rejected_even_without_symlink_mode(self):
        info = SimpleNamespace(st_mode=stat.S_IFDIR | 0o700, st_file_attributes=0x400)
        with patch.object(Path, "lstat", return_value=info), self.assertRaises(fixtures.FixtureError):
            fixtures._plain(self.root)

    def test_configured_policy_matches_production_bounds(self):
        native = (ROOT / "desktop/agent/AgentOrchestrator.hpp").read_text(encoding="utf-8")
        eog = (ROOT / "desktop/eog/EogInvestigator.hpp").read_text(encoding="utf-8")
        for source in (native, eog):
            self.assertEqual(int(re.search(r"int toolCalls = (\d+);", source)[1]), fixtures.POLICY["tool_calls"])
            self.assertEqual(int(re.search(r"int refinementRounds = (\d+);", source)[1]), fixtures.POLICY["refinement_rounds"])
        self.assertEqual(int(re.search(r"int durationMs = (\d+);", native)[1]), fixtures.POLICY["sandbox_duration_ms"])
        self.assertEqual(int(re.search(r"maximum_eog_investigation_duration_ms = (\d+);", eog)[1]), fixtures.POLICY["eog_duration_ms"])
        gateway = (ROOT / "desktop/agent/SimulationGateway.hpp").read_text(encoding="utf-8")
        self.assertEqual(float(re.search(r"double horizonH = ([\d.]+)", gateway)[1]), fixtures.POLICY["horizon_h"])
        capture = (ROOT / "desktop/eog/EogCapture.cpp").read_text(encoding="utf-8")
        self.assertIn('{"sample_period_s", 300}, {"max_age_s", 300}', capture)
        self.assertEqual(read_json(self.output / "cases.json")["policy"], fixtures.POLICY)
        for case in read_json(self.output / "cases.json")["cases"]:
            self.assertEqual(case["limits"], {"tool_calls": 12, "refinement_rounds": 3, "duration_ms": fixtures.POLICY[case["mode"] + "_duration_ms"]})


if __name__ == "__main__":
    unittest.main()
