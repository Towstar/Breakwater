"""Real fixed-file transformations; no detector, answer-key or model execution."""
import copy
import csv
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "prepare-eog-demo.py"
FIXTURE = ROOT / "desktop" / "tests" / "eog_fixtures" / "render"
spec = importlib.util.spec_from_file_location("eog_demo", SCRIPT)
demo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(demo)
FILES = ("network.json", "ops.json", "telemetry.csv")


def digest(folder):
    return {name: hashlib.sha256((folder / name).read_bytes()).hexdigest() for name in FILES}


def parsed(raw):
    return list(csv.reader(io.StringIO(raw.decode("utf-8-sig"), newline="")))


def entities(network):
    result = {}
    for system in network["systems"]:
        result[system["id"]] = {k: v for k, v in system.items() if k not in ("id", "facilities", "disposal_site")}
        disposal = system["disposal_site"]
        result[disposal["id"]] = {k: v for k, v in disposal.items() if k != "id"}
        for facility in system["facilities"]:
            result[facility["id"]] = {k: v for k, v in facility.items() if k not in ("id", "wells", "tank", "pump", "pipeline")}
            for asset in [facility[k] for k in ("tank", "pump", "pipeline")] + facility["wells"]:
                result[asset["id"]] = {k: v for k, v in asset.items() if k != "id"}
    return result


class EogDemoTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="breakwater-demo-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "source"
        self.source.mkdir()
        network = json.loads((FIXTURE / "network.json").read_bytes())
        ops = json.loads((FIXTURE / "ops.json").read_bytes())
        second = copy.deepcopy(network["systems"][0]["facilities"][0])
        old = second["id"]
        mapping = {old: "zz-Ω/facility"}
        second["id"] = mapping[old]
        for kind in ("tank", "pump", "pipeline"):
            mapping[second[kind]["id"]] = "zz-Ω/" + kind
            second[kind]["id"] = mapping[second[kind]["id"]]
        for index, well in enumerate(second["wells"]):
            mapping[well["id"]] = f"zz-Ω/well:{index}"
            well["id"] = mapping[well["id"]]
        network["systems"][0]["facilities"].append(second)
        sid = network["systems"][0]["id"]
        ops["systems"][sid]["facilities"][second["id"]] = copy.deepcopy(ops["systems"][sid]["facilities"][old])
        # Duplicate existing fixture observations for the second complete facility;
        # these are data-preservation fixtures, not newly modeled sensor readings.
        original = (FIXTURE / "telemetry.csv").read_bytes().splitlines()
        lines = [original[0] + b"\r\n"]
        for line in original[1:]:
            row = next(csv.reader([line.decode("utf-8")]))
            if row[2] == "tank-Z":
                text = b'"-2.0000"' if row[4] == "-2" else b"1.040000000e+02"
                line = line.rsplit(b",", 1)[0] + b"," + text
            elif row[2] == "facility-Z":
                line = line.rsplit(b",", 1)[0] + b",+0017.5000"
            lines.append(line + b"\r\n")
            prefix = line.split(b",", 2)[:2]
            suffix = line.rsplit(b",", 2)[1:]
            lines.append(b",".join([*prefix, mapping[row[2]].encode("utf-8"), *suffix]) + b"\r\n")
        # A real acquisition gap leaves early samples stale at the final clock;
        # the transformation must retain the actual time/value text, not fill it.
        lines.append(b"2026-10-05T00:20:00Z,1791159600,facility-Z,inflow_bpd,17.500000\r\n")
        self.initial = {"network.json": json.dumps(network, ensure_ascii=False).encode(),
                        "ops.json": json.dumps(ops, ensure_ascii=False).encode(), "telemetry.csv": b"".join(lines)}
        for name, raw in self.initial.items(): (self.source / name).write_bytes(raw)
        self.before = digest(self.source)

    def prepare(self, name="variation"):
        target = self.root / name
        provenance = demo.prepare_demo(self.source, target)
        self.assertEqual(digest(self.source), self.before)
        self.assertEqual(set(p.name for p in target.iterdir()), {*FILES, "provenance.json"})
        return target, provenance

    def test_deterministic_topology_rename_preserves_values_economics_and_all_references(self):
        target, provenance = self.prepare()
        again, repeated = self.prepare("same-data-different-output")
        self.assertEqual(provenance, repeated)
        for name in (*FILES, "provenance.json"):
            self.assertEqual((target / name).read_bytes(), (again / name).read_bytes())
        source_network = json.loads(self.initial["network.json"])
        source_ops = json.loads(self.initial["ops.json"])
        network = json.loads((target / "network.json").read_bytes())
        ops = json.loads((target / "ops.json").read_bytes())
        mapping = {item["source_id"]: item["output_id"] for item in provenance["id_mapping"]}
        removed = set(provenance["removed_topology"]["asset_ids"])
        before, after = entities(source_network), entities(network)
        self.assertEqual(set(before) - removed, set(mapping))
        self.assertEqual(set(after), set(mapping.values()))
        self.assertTrue(all(old != new and "Δ" in new for old, new in mapping.items()))
        self.assertEqual(len(set(mapping.values())), len(mapping))
        self.assertEqual({mapping[old]: data for old, data in before.items() if old not in removed}, after)
        self.assertEqual(network["meta"], source_network["meta"])
        self.assertEqual(network["defaults"], source_network["defaults"])
        for key in ("prices", "expected_repair_hours", "emergency_trucking"):
            self.assertEqual(ops[key], source_ops[key])
        self.assertNotEqual(ops["dataset_id"], source_ops["dataset_id"])
        for system in network["systems"]:
            settings = ops["systems"][system["id"]]
            self.assertEqual(set(settings["facilities"]), {f["id"] for f in system["facilities"]})
            source_sid = next(old for old, new in mapping.items() if new == system["id"])
            self.assertEqual(settings["emergency_fleet_size"], source_ops["systems"][source_sid]["emergency_fleet_size"])
            for facility in system["facilities"]:
                source_fid = next(old for old, new in mapping.items() if new == facility["id"])
                self.assertEqual(settings["facilities"][facility["id"]], source_ops["systems"][source_sid]["facilities"][source_fid])
        self.assertEqual(provenance["source_hashes"], self.before)
        self.assertEqual(provenance["output_hashes"], digest(target))
        self.assertEqual(provenance["source_counts"]["facilities"], 2)
        self.assertEqual(provenance["output_counts"]["facilities"], 1)
        self.assertEqual(provenance["removed_topology"]["facility"]["id"], "zz-Ω/facility")
        self.assertIn("not independent held-out", provenance["qualification"])
        source_rows = parsed(self.initial["telemetry.csv"])[1:]
        output_rows = parsed((target / "telemetry.csv").read_bytes())[1:]
        self.assertEqual(len(source_rows), provenance["source_counts"]["readings"])
        self.assertEqual(len(output_rows), provenance["output_counts"]["readings"])
        self.assertEqual(len(source_rows) - len(output_rows), provenance["removed_topology"]["readings"])

    def test_negative_overflow_and_stale_acquisition_field_text_and_line_endings_are_exact(self):
        target, provenance = self.prepare()
        mapping = {item["source_id"]: item["output_id"] for item in provenance["id_mapping"]}
        removed = set(provenance["removed_topology"]["asset_ids"])
        expected = [self.initial["telemetry.csv"].splitlines(keepends=True)[0]]
        for line in self.initial["telemetry.csv"].splitlines(keepends=True)[1:]:
            row = next(csv.reader([line.decode()]))
            if row[2] in removed: continue
            # Independent fixed-format check: timestamp fields have no commas;
            # supported metric/numeric fields have none, even when quoted.
            expected.append(b",".join([*line.split(b",", 2)[:2], mapping[row[2]].encode(), *line.rsplit(b",", 2)[1:]]))
        result = (target / "telemetry.csv").read_bytes()
        self.assertEqual(result, b"".join(expected))
        self.assertIn(b'"-2.0000"', result)
        self.assertIn(b"1.040000000e+02", result)
        self.assertIn(b"+0017.5000", result)
        self.assertIn(b"1791158400", result)
        self.assertIn(b"1791159600", result)
        self.assertTrue(all(line.endswith(b"\r\n") for line in result.splitlines(keepends=True)))

    def test_only_fixed_input_names_are_opened(self):
        (self.source / "answer_key.json").write_bytes(b"POISON: THIS IS NOT A LABEL FILE")
        original = Path.open
        def guard(path, *args, **kwargs):
            if path.parent.resolve() == self.source.resolve():
                self.assertIn(path.name, FILES, "The transformer opened a non-input source file.")
            return original(path, *args, **kwargs)
        with patch.object(Path, "open", guard):
            self.prepare()

    def test_quoted_unicode_source_ids_only_change_the_asset_token(self):
        network = json.loads(self.initial["network.json"])
        source_id = 'tank,"α"'
        network["systems"][0]["facilities"][0]["tank"]["id"] = source_id
        token = io.StringIO(newline="")
        csv.writer(token, lineterminator="").writerow([source_id])
        self.initial["network.json"] = json.dumps(network, ensure_ascii=False).encode()
        self.initial["telemetry.csv"] = self.initial["telemetry.csv"].replace(b",tank-Z,", b"," + token.getvalue().encode() + b",")
        for name, raw in self.initial.items(): (self.source / name).write_bytes(raw)
        self.before = digest(self.source)
        target, provenance = self.prepare()
        mapping = {item["source_id"]: item["output_id"] for item in provenance["id_mapping"]}
        removed = set(provenance["removed_topology"]["asset_ids"])
        expected = [self.initial["telemetry.csv"].splitlines(keepends=True)[0]]
        for line in self.initial["telemetry.csv"].splitlines(keepends=True)[1:]:
            row = next(csv.reader([line.decode()]))
            if row[2] not in removed:
                expected.append(b",".join([*line.split(b",", 2)[:2], mapping[row[2]].encode(), *line.rsplit(b",", 2)[1:]]))
        self.assertEqual((target / "telemetry.csv").read_bytes(), b"".join(expected))
        self.assertIn(source_id, mapping)

    def test_every_system_disposal_and_retained_facility_operations_key_is_renamed(self):
        network, ops = (json.loads(self.initial[name]) for name in ("network.json", "ops.json"))
        source_system = network["systems"][0]
        extra = copy.deepcopy(source_system); extra["id"] = "another-β/system"
        extra["disposal_site"]["id"] = "another-β/disposal"
        extra["facilities"] = [copy.deepcopy(source_system["facilities"][0])]
        facility = extra["facilities"][0]; mapping = {facility["id"]: "another-β/facility"}
        facility["id"] = mapping[facility["id"]]
        for kind in ("tank", "pump", "pipeline"):
            mapping[facility[kind]["id"]] = "another-β/" + kind
            facility[kind]["id"] = mapping[facility[kind]["id"]]
        for index, well in enumerate(facility["wells"]):
            mapping[well["id"]] = f"another-β/well:{index}"; well["id"] = mapping[well["id"]]
        network["systems"].append(extra)
        settings = copy.deepcopy(ops["systems"][source_system["id"]])
        settings["facilities"] = {facility["id"]: copy.deepcopy(settings["facilities"]["facility-Z"])}
        ops["systems"][extra["id"]] = settings
        rows = self.initial["telemetry.csv"].splitlines(keepends=True); result = [rows[0]]
        for line in rows[1:]:
            result.append(line); row = next(csv.reader([line.decode()]))
            if row[2] in mapping:
                result.append(b",".join([*line.split(b",", 2)[:2], mapping[row[2]].encode(), *line.rsplit(b",", 2)[1:]]))
        self.initial = {"network.json": json.dumps(network, ensure_ascii=False).encode(),
                        "ops.json": json.dumps(ops, ensure_ascii=False).encode(), "telemetry.csv": b"".join(result)}
        for name, raw in self.initial.items(): (self.source / name).write_bytes(raw)
        self.before = digest(self.source)
        target, provenance = self.prepare()
        renamed = json.loads((target / "network.json").read_bytes()); operations = json.loads((target / "ops.json").read_bytes())
        self.assertEqual(provenance["output_counts"]["systems"], 2)
        self.assertEqual(provenance["output_counts"]["facilities"], 2)
        self.assertEqual(set(operations["systems"]), {system["id"] for system in renamed["systems"]})
        for system in renamed["systems"]:
            self.assertTrue(system["id"].startswith("demo-Δ-")); self.assertTrue(system["disposal_site"]["id"].startswith("demo-Δ-"))
            self.assertEqual(set(operations["systems"][system["id"]]["facilities"]), {f["id"] for f in system["facilities"]})

    def test_unsupported_shape_references_duplicate_keys_and_nonfinite_samples_fail_without_output(self):
        mutations = {
            "duplicate-key": ("network.json", b'{"meta":0,"meta":1}'),
            "nonfinite": ("telemetry.csv", self.initial["telemetry.csv"].replace(b"+0017.5000", b"NaN", 1)),
            "unknown-channel": ("telemetry.csv", self.initial["telemetry.csv"].replace(b"inflow_bpd", b"unknown_metric", 1)),
            "clock": ("telemetry.csv", self.initial["telemetry.csv"].replace(b"1791158400", b"1791158401", 1)),
        }
        ops = json.loads(self.initial["ops.json"]); ops["systems"]["missing-system"] = {}
        mutations["unknown-ops"] = ("ops.json", json.dumps(ops).encode())
        network = json.loads(self.initial["network.json"])
        network["systems"][0]["facilities"][1]["tank"]["id"] = "tank-Z"
        mutations["duplicate-id"] = ("network.json", json.dumps(network).encode())
        for label, (name, data) in mutations.items():
            with self.subTest(label=label):
                for filename, raw in self.initial.items(): (self.source / filename).write_bytes(raw)
                (self.source / name).write_bytes(data); before = digest(self.source)
                target = self.root / label
                with self.assertRaises(demo.DemoError): demo.prepare_demo(self.source, target)
                self.assertFalse(target.exists()); self.assertEqual(digest(self.source), before)

    def test_single_facility_existing_and_in_source_output_are_refused(self):
        for name in FILES: (self.source / name).write_bytes((FIXTURE / name).read_bytes())
        before = digest(self.source)
        with self.assertRaises(demo.DemoError): demo.prepare_demo(self.source, self.root / "single")
        self.assertFalse((self.root / "single").exists())
        for name, raw in self.initial.items(): (self.source / name).write_bytes(raw)
        existing = self.root / "existing"; existing.mkdir(); marker = existing / "preserve.txt"; marker.write_bytes(b"PRESERVE")
        for output in (existing, self.source / "new-output", self.root / "missing-parent" / "new"):
            with self.subTest(output=output.name), self.assertRaises(demo.DemoError): demo.prepare_demo(self.source, output)
        self.assertEqual(marker.read_bytes(), b"PRESERVE")
        self.assertEqual(digest(self.source), self.before)
        self.assertNotEqual(before, self.before)

    def test_oversize_input_and_failed_writes_or_publication_never_leave_partial_output(self):
        oversized = self.source / "network.json"
        with oversized.open("wb") as stream: stream.truncate(demo.JSON_LIMIT + 1)
        before = digest(self.source)
        with self.assertRaises(demo.DemoError): demo.prepare_demo(self.source, self.root / "oversized")
        self.assertEqual(digest(self.source), before)
        oversized.write_bytes(self.initial["network.json"])
        actual_write = demo.write_file
        def fail_write(path, raw):
            if path.name == "ops.json": raise OSError("Injected write failure")
            return actual_write(path, raw)
        for label, replacement in (("write", patch.object(demo, "write_file", fail_write)),
                                   ("publish", patch.object(demo.os, "rename", side_effect=OSError("Injected publish failure")))):
            with self.subTest(label=label), replacement:
                target = self.root / label
                with self.assertRaises(OSError): demo.prepare_demo(self.source, target)
                self.assertFalse(target.exists()); self.assertEqual(digest(self.source), self.before)
        self.assertFalse(any(path.name.startswith(".eog-demo-") for path in self.root.iterdir()))

    def test_cli_uses_new_fixed_provenance_and_reports_real_exit_codes(self):
        target = self.root / "cli"
        result = subprocess.run([sys.executable, "-B", str(SCRIPT), "--dataset", str(self.source), "--output", str(target)],
                                capture_output=True, text=True, encoding="utf-8", timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout)["status"], "created")
        before = {name: (target / name).read_bytes() for name in (*FILES, "provenance.json")}
        rejected = subprocess.run([sys.executable, "-B", str(SCRIPT), "--dataset", str(self.source), "--output", str(target)],
                                  capture_output=True, text=True, encoding="utf-8", timeout=20)
        self.assertEqual(rejected.returncode, 2)
        self.assertEqual(json.loads(rejected.stdout)["status"], "failed")
        self.assertEqual(before, {name: (target / name).read_bytes() for name in before})
        self.assertEqual(digest(self.source), self.before)

    def test_external_source_change_before_publication_is_detected(self):
        actual_write = demo.write_file
        def external_writer(path, raw):
            actual_write(path, raw)
            if path.name == "provenance.json":
                # This mock represents a separate source writer, not transformer
                # output. Never publish a derived directory from mixed versions.
                (self.source / "telemetry.csv").write_bytes(self.initial["telemetry.csv"] + b" ")
        target = self.root / "changed-source"
        with patch.object(demo, "write_file", external_writer), self.assertRaisesRegex(demo.DemoError, "Source changed"):
            demo.prepare_demo(self.source, target)
        self.assertFalse(target.exists())
        self.assertFalse(any(path.name.startswith(".eog-demo-") for path in self.root.iterdir()))

    def test_destination_appearing_at_publication_is_not_overwritten_or_merged(self):
        actual_rename = os.rename
        target = self.root / "appeared"
        def racing_destination(stage, output):
            output.mkdir()
            (output / "preserve.txt").write_bytes(b"EXTERNAL DESTINATION")
            return actual_rename(stage, output)
        with patch.object(demo.os, "rename", racing_destination), self.assertRaises(OSError):
            demo.prepare_demo(self.source, target)
        self.assertEqual({p.name for p in target.iterdir()}, {"preserve.txt"})
        self.assertEqual((target / "preserve.txt").read_bytes(), b"EXTERNAL DESTINATION")
        self.assertEqual(digest(self.source), self.before)
        self.assertFalse(any(path.name.startswith(".eog-demo-") for path in self.root.iterdir()))


if __name__ == "__main__":
    unittest.main(verbosity=2)
