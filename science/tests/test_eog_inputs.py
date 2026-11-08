import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import eog_inputs
from eog_inputs import InputError, load_inputs
from eog_model import compile_model
from eog_test_fixtures import frame, inputs


def write_fixture(folder, rows=None):
    n, o = inputs()
    model = compile_model(n, o)
    (folder / "network.json").write_text(json.dumps(n), encoding="utf-8")
    (folder / "ops.json").write_text(json.dumps(o), encoding="utf-8")
    if rows is None:
        rows = frame(model, 0, status=0) + frame(model, 300, status=2)
    text = io.StringIO(newline="")
    writer = csv.writer(text, lineterminator="\n")
    writer.writerow(("timestamp", "ts_unix", "asset_id", "metric", "value"))
    for s in rows:
        stamp = datetime.fromtimestamp(s.unix, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        writer.writerow((stamp, s.unix, s.asset_id, s.metric, s.value))
    (folder / "telemetry.csv").write_bytes(text.getvalue().encode())
    return model


class EogInputTests(unittest.TestCase):
    def reject(self, folder, code):
        with self.assertRaises(InputError) as caught:
            load_inputs(folder)
        self.assertEqual(caught.exception.code, code)

    def test_only_allowed_names_are_read_even_if_answer_key_exists(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            (folder / "answer_key.json").write_text("NEVER READ THIS", encoding="utf-8")
            opened = []
            original = Path.open
            def checked(path, *args, **kwargs):
                self.assertIn(path.name, eog_inputs.FILES)
                opened.append(path.name)
                return original(path, *args, **kwargs)
            with patch.object(Path, "open", checked):
                loaded = load_inputs(folder)
            self.assertEqual(set(opened), set(eog_inputs.FILES))
            self.assertEqual(len(opened), 6)
            self.assertEqual((loaded.observations.first_unix, loaded.observations.last_unix), (0, 300))

    def test_raw_hashes_and_bom_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            csv_path = folder / "telemetry.csv"
            csv_path.write_bytes(b"\xef\xbb\xbf" + csv_path.read_bytes())
            loaded = load_inputs(folder)
            for name, digest in loaded.file_hashes:
                self.assertEqual(digest, hashlib.sha256((folder / name).read_bytes()).hexdigest())

    def test_mutation_during_compilation_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            original = eog_inputs.compile_model
            def changing(n, o):
                (folder / "ops.json").write_bytes((folder / "ops.json").read_bytes() + b"\n")
                return original(n, o)
            with patch.object(eog_inputs, "compile_model", changing):
                self.reject(folder, "input_changed")

    def test_duplicate_json_and_nonfinite_input_are_rejected(self):
        for data, code in ((b'{"meta":{},"meta":{}}', "duplicate_json_key"),
                           (b'{"value":NaN}', "nonfinite_input"), (b'{"value":1e999}', "nonfinite_input"),
                           (b"{", "invalid_json")):
            with self.subTest(data=data), tempfile.TemporaryDirectory() as directory:
                folder = Path(directory)
                write_fixture(folder)
                (folder / "network.json").write_bytes(data)
                self.reject(folder, code)

    def test_json_depth_limit_is_not_hidden_by_unknown_shape(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            (folder / "network.json").write_bytes(b"[" * 65 + b"0" + b"]" * 65)
            self.reject(folder, "limit_exceeded")

    def test_csv_timestamp_value_header_and_identity_corruption(self):
        mutations = ((lambda s: s.replace("ts_unix", "time"), "invalid_csv"),
                     (lambda s: s.replace("1970-01-01T00:00:00Z", "1970-01-01T00:00:01Z"), "invalid_csv"),
                     (lambda s: s.replace(",0,", ",0.0,", 1), "invalid_csv"),
                     (lambda s: s.replace("inflow_bpd,1800.0", "inflow_bpd,NaN", 1), "invalid_value"),
                     (lambda s: s + s.splitlines()[1] + "\n", "unordered_samples"))
        for change, code in mutations:
            with self.subTest(code=code), tempfile.TemporaryDirectory() as directory:
                folder = Path(directory)
                write_fixture(folder)
                path = folder / "telemetry.csv"
                path.write_text(change(path.read_text(encoding="utf-8")), encoding="utf-8")
                self.reject(folder, code)

    def test_duplicate_samples_and_unknown_channels_are_not_dropped(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            m = compile_model(*inputs())
            rows = frame(m, 0)
            write_fixture(folder, rows + [rows[0]])
            self.reject(folder, "duplicate_sample")
            write_fixture(folder, rows)
            path = folder / "telemetry.csv"
            path.write_text(path.read_text(encoding="utf-8").replace("inflow_bpd", "unknown_metric"), encoding="utf-8")
            self.reject(folder, "unknown_channel")

    def test_explicit_capture_gap_is_allowed_but_future_rows_reject(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            result = load_inputs(folder, capture_cutoff_unix=450)
            self.assertEqual(result.observations.at(450).cutoff_unix, 450)
            with self.assertRaises(InputError) as caught:
                load_inputs(folder, capture_cutoff_unix=299)
            self.assertEqual(caught.exception.code, "future_sample")

    def test_byte_limit_is_checked_before_full_read(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            with patch.object(eog_inputs, "JSON_LIMIT", 16):
                self.reject(folder, "limit_exceeded")

    def test_missing_files_fail_without_enumerating_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            with patch.object(Path, "iterdir", side_effect=AssertionError("No directory enumeration")):
                self.reject(folder, "invalid_path")

    def test_offline_command_has_explicit_success_failure_and_historical_cutoff(self):
        script = Path(__file__).resolve().parents[1] / "inspect_eog.py"
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            for cutoff in (300, 299, 301):
                report = folder / f"report-{cutoff}.json"
                completed = subprocess.run([sys.executable, "-S", str(script), "--dataset-folder", str(folder),
                                            "--cutoff-unix", str(cutoff), "--report", str(report)],
                                           capture_output=True, text=True, timeout=15)
                data = json.loads(report.read_text(encoding="utf-8"))
                self.assertEqual(completed.returncode, 1 if cutoff == 301 else 0)
                self.assertEqual(data["status"], "failed" if cutoff == 301 else "completed")
                if cutoff != 301:
                    self.assertEqual(data["actual_cutoff_unix"], cutoff)
                    self.assertTrue(all(s["unix"] <= cutoff for e in data["detection"]["events"] for s in e["evidence"]))

    def assert_report_preserves_inputs(self, folder, report, *, cwd=None):
        before = {name: (folder / name).read_bytes() for name in eog_inputs.FILES}
        script = Path(__file__).resolve().parents[1] / "inspect_eog.py"
        completed = subprocess.run(
            [sys.executable, "-S", str(script), "--dataset-folder", str(folder), "--report", str(report)],
            cwd=cwd, capture_output=True, text=True, timeout=15)
        self.assertEqual(completed.returncode, 2, completed.stderr)
        self.assertIn("Report destination must not", completed.stderr)
        self.assertEqual(completed.stdout, "")
        self.assertEqual(before, {name: (folder / name).read_bytes() for name in eog_inputs.FILES})
        self.assertEqual(list(folder.glob("eog-*.tmp")), [])

    def test_report_collision_preserves_all_sources_for_success_and_failure(self):
        for invalid_input in (False, True):
            for name in eog_inputs.FILES:
                with self.subTest(invalid_input=invalid_input, name=name), tempfile.TemporaryDirectory() as directory:
                    folder = Path(directory)
                    write_fixture(folder)
                    if invalid_input:
                        (folder / "network.json").write_bytes(b"{")
                    self.assert_report_preserves_inputs(folder, folder / name)

    def test_relative_report_alias_is_resolved_before_analysis(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            (folder / "child").mkdir()
            for name in eog_inputs.FILES:
                with self.subTest(name=name):
                    self.assert_report_preserves_inputs(folder, Path("child") / ".." / name, cwd=folder)

    def test_hard_link_report_alias_preserves_both_directory_entries(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            write_fixture(folder)
            for name in eog_inputs.FILES:
                alias = folder / (name + ".report")
                os.link(folder / name, alias)
                before = alias.read_bytes()
                self.assert_report_preserves_inputs(folder, alias)
                self.assertEqual(alias.read_bytes(), before)
                self.assertTrue(alias.samefile(folder / name))


if __name__ == "__main__":
    unittest.main()
