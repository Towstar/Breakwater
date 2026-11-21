"""Offline guards and real local-mode checks for the Sandbox verification CLI.

Usage: python -B sandbox_live_cli_tests.py EXECUTABLE HEALTHY_PROJECT_JSON
No test enables --live. The explicit --local check uses production numerics,
recorder and SQLite export with no provider credentials or model requests.
"""
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest

EXECUTABLE = None
PROJECT = None


class SandboxLiveCliTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="breakwater-sandbox-cli-α#-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.inputs = self.root / "input"
        self.inputs.mkdir()
        self.project = self.inputs / "healthy.json"
        shutil.copyfile(PROJECT, self.project)
        self.before = self.project.read_bytes()
        self.invocations = 0

    def invoke(self, *arguments, project=None, archive=None, database=None,
               expected=2, code=None, unchanged=True):
        self.invocations += 1
        archive = Path(archive) if archive is not None else self.root / f"trace-{self.invocations}.session.json"
        database = Path(database) if database is not None else self.root / f"history-{self.invocations}.sqlite"
        command = [str(EXECUTABLE), "--project", str(project or self.project),
                   "--archive", str(archive), "--db", str(database), *map(str, arguments)]
        self.assertNotIn("--live", command, "Offline CLI tests never authorize hosted calls.")
        names = ("SystemRoot", "WINDIR", "PATH", "PATHEXT", "TEMP", "TMP", "USERPROFILE",
                 "LOCALAPPDATA", "APPDATA", "ASAN_OPTIONS", "UBSAN_OPTIONS", "QT_LOGGING_RULES")
        environment = {name: os.environ[name] for name in names if name in os.environ}
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                                errors="replace", timeout=60, env=environment, cwd=self.root)
        if unchanged:
            self.assertEqual(self.project.read_bytes(), self.before, "The CLI changed the source project.")
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        lines = result.stdout.strip().splitlines()
        self.assertEqual(len(lines), 1, "CLI must emit exactly one sanitized JSON summary: " + result.stdout)
        value = json.loads(lines[0])
        self.assertEqual(value["exit_code"], expected)
        if code:
            self.assertEqual(value["code"], code)
        self.assertIs(value["live_enabled"], False)
        self.assertIs(value["controls_applied"], False)
        for field in ("model_requests", "completed_model_requests", "successful_model_replies", "http_attempts", "retry_count"):
            if field in value:
                self.assertEqual(value[field], 0, "An offline invocation made model work: " + field)
        return value, archive, database

    def test_valid_readiness_is_keyless_and_creates_no_history_or_archive(self):
        self.assertTrue(EXECUTABLE.is_file(), "Build the actual Sandbox live-check target before this test.")
        value, archive, database = self.invoke(expected=3, code="live_not_enabled")
        self.assertIs(value["inputs_unchanged"], True)
        self.assertIs(value["controls_unchanged"], True)
        self.assertIs(value["archive_written"], False)
        self.assertFalse(archive.exists())
        self.assertFalse(database.exists())

    def test_local_mode_uses_production_numerics_and_unchanged_durable_export(self):
        value, archive, database = self.invoke("--local", expected=0, code="verified_rehearsal")
        self.assertEqual(value["source_mode"], "local_native")
        self.assertIs(value["archive_written"], True)
        self.assertIs(value["grounded_validated_final"], True)
        self.assertIs(value["controls_unchanged"], True)
        self.assertIs(value["inputs_unchanged"], True)
        retained = json.loads(archive.read_text(encoding="utf-8"))
        self.assertEqual(retained["archive_version"], 1)
        self.assertEqual(retained["status"], "completed")
        self.assertEqual(retained["approvals"], [])
        recommendation = retained["recommendation"]
        self.assertEqual(recommendation["source_mode"], "local_native")
        self.assertEqual(recommendation["status"], "validated_recommendation")
        self.assertNotEqual(recommendation["approval_status"], "operator_applied")
        self.assertEqual(recommendation["selected_run_id"], value["selected_run_id"])
        self.assertEqual(recommendation["validation_id"], value["validation_id"])
        self.assertFalse(any(event["type"] == "model_request_started" for event in retained["events"]))
        selected = next(detail["run"] for detail in retained["runs"]
                        if detail["run"]["run_id"] == value["selected_run_id"])
        self.assertEqual(selected["validation_id"], recommendation["validation_id"])
        self.assertTrue(selected["validation"]["feasible"])
        self.assertEqual(selected["controls"], recommendation["controls"])
        # Export keeps the stored terminal evidence, including its real pending
        # review state; no helper rewrites a passing archive or approval record.
        with closing(sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)) as connection:
            stored, = connection.execute("SELECT archive FROM sessions WHERE session_id = ?",
                                         (retained["session_id"],)).fetchone()
        final_checkpoint = json.loads(stored)
        self.assertGreaterEqual(final_checkpoint["checkpoint_sequence"], retained["checkpoint_sequence"])
        # The production controller flushes once more during destruction. That
        # may advance only checkpoint metadata after the export transaction.
        for field in ("session_id", "created_at", "status", "objective", "snapshot", "model",
                      "runs", "events", "recommendation", "approvals"):
            self.assertEqual(final_checkpoint[field], retained[field])

    def test_bad_paths_existing_outputs_and_in_input_outputs_are_rejected(self):
        existing = self.root / "existing.session.json"
        sentinel = b"PRESERVE EXISTING EVIDENCE\n"
        existing.write_bytes(sentinel)
        folder = self.root / "folder.session.json"
        folder.mkdir()
        bad_parent = self.root / "file-parent"
        bad_parent.write_bytes(b"PRESERVE PARENT")
        for archive in (existing, folder, self.root / "wrong.json", bad_parent / "new.session.json",
                        self.inputs / "new.session.json", self.project):
            with self.subTest(archive=archive.name):
                self.invoke(archive=archive, code="invalid_input_or_output_paths")
        self.assertEqual(existing.read_bytes(), sentinel)
        self.assertEqual(bad_parent.read_bytes(), b"PRESERVE PARENT")
        for database in (self.project, self.inputs / "new.sqlite", existing):
            with self.subTest(database=database.name):
                self.invoke(database=database, code="invalid_input_or_output_paths")
        collision = self.root / "collision.session.json"
        self.invoke(archive=collision, database=collision, code="invalid_input_or_output_paths")
        self.assertFalse(collision.exists())

    def test_local_no_plan_retains_actual_completed_search_without_claiming_infeasibility(self):
        no_remedy = self.inputs / "finite-search-no-plan.json"
        document = {"version": 1, "synthetic": True, "layout": {}, "network": {
            "assets": [{"id": "store", "label": "Store", "kind": "tank", "capacity_m3": 10, "initial_m3": 9},
                       {"id": "feed", "label": "Feed", "kind": "source", "inflow_m3h": 1,
                        "curtailment_m3h": 0, "curtailment_max_m3h": 0, "cost_per_m3": 0}],
            "routes": [{"id": "input", "source_id": "feed", "target_id": "store", "capacity_m3h": 1,
                        "setpoint_m3h": 1, "has_pump": False, "available": True, "cost_per_m3": 0}]}}
        no_remedy.write_text(json.dumps(document), encoding="utf-8")
        source = no_remedy.read_bytes()
        value, archive, _ = self.invoke("--local", project=no_remedy, expected=3, code="no_validated_plan")
        self.assertTrue(value["archive_written"])
        self.assertFalse(value["grounded_validated_final"])
        self.assertEqual(value["model_issued_tool_calls"], 0)
        retained = json.loads(archive.read_text(encoding="utf-8"))
        self.assertEqual(retained["status"], "completed")
        self.assertEqual(retained["recommendation"]["status"], "no_validated_plan")
        self.assertIsNone(retained["recommendation"]["selected_run_id"])
        self.assertEqual(retained["approvals"], [])
        self.assertTrue(any(event["type"] == "search_completed" for event in retained["events"]))
        self.assertEqual(no_remedy.read_bytes(), source)

    def test_sidecars_and_preexisting_database_cannot_be_overwritten(self):
        for suffix in ("-wal", "-shm", "-journal", ".lock"):
            with self.subTest(suffix=suffix):
                database = self.root / ("history" + suffix + ".sqlite")
                sidecar = Path(str(database) + suffix)
                sidecar.write_bytes(b"PRESERVE SIDECAR")
                self.invoke(database=database, code="output_path_collision")
                self.assertEqual(sidecar.read_bytes(), b"PRESERVE SIDECAR")
                self.assertFalse(database.exists())
        database = self.root / "existing.sqlite"
        database.write_bytes(b"PRESERVE DATABASE")
        self.invoke(database=database, code="invalid_input_or_output_paths")
        self.assertEqual(database.read_bytes(), b"PRESERVE DATABASE")

    def test_missing_nonfile_malformed_duplicate_and_invalid_project_are_rejected(self):
        for project in (self.inputs / "absent.json", self.inputs):
            with self.subTest(path=project.name):
                self.invoke(project=project, code="invalid_input_or_output_paths")
        payloads = [b"{", b"[]", b'{"version":1,"version":1,"network":{}}',
                    b'{"version":99,"network":{"assets":[],"routes":[]}}',
                    b'{"version":1,"network":{"assets":[],"routes":[]}}',
                    b'{"version":1,"network":{"assets":[{"id":"a","kind":"tank","capacity_m3":-1}],"routes":[]}}']
        for index, payload in enumerate(payloads):
            path = self.inputs / f"invalid-{index}.json"
            path.write_bytes(payload)
            with self.subTest(payload=index):
                self.invoke(project=path, code="project_invalid")
                self.assertEqual(path.read_bytes(), payload)
        large = self.inputs / "oversized.json"
        large.write_bytes(b" " * (2 * 1024 * 1024 + 1))
        self.invoke(project=large, code="invalid_input_or_output_paths")

    def test_objective_and_unknown_arguments_cannot_start_work(self):
        for objective in ("", " \t\n", "x" * 4001):
            with self.subTest(length=len(objective)):
                self.invoke("--objective", objective, code="invalid_input_or_output_paths")
        self.invoke("--unexpected", code="invalid_arguments")
        self.invoke("extra-positional", code="invalid_input_or_output_paths")

    def test_hardlinked_source_as_new_database_is_rejected_without_changing_source(self):
        database = self.root / "linked.sqlite"
        os.link(self.project, database)
        self.assertTrue(os.path.samefile(self.project, database))
        before_hash = hashlib.sha256(self.before).hexdigest()
        self.invoke(database=database, code="invalid_input_or_output_paths")
        self.assertEqual(hashlib.sha256(database.read_bytes()).hexdigest(), before_hash)

    @unittest.skipUnless(os.name == "nt", "Windows 8.3 aliases only")
    def test_short_spelled_outputs_cannot_write_inside_long_spelled_input(self):
        import ctypes
        from ctypes import wintypes

        short_path = ctypes.WinDLL("kernel32", use_last_error=True).GetShortPathNameW
        short_path.argtypes = (wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD)
        short_path.restype = wintypes.DWORD
        buffer = ctypes.create_unicode_buffer(32768)
        size = short_path(str(self.inputs), buffer, len(buffer))
        if not size or size >= len(buffer) or buffer.value.casefold() == str(self.inputs).casefold():
            self.skipTest("This volume does not supply a distinct Windows 8.3 alias")
        alias = Path(buffer.value)
        self.assertTrue(os.path.samefile(alias, self.inputs))
        self.assertEqual(alias.resolve(), self.inputs)
        before_names = sorted(path.relative_to(self.inputs) for path in self.inputs.rglob("*"))
        for arguments in ((), ("--local",)):
            for kind in ("archive", "database"):
                with self.subTest(mode=arguments, destination=kind):
                    destination = alias / "fresh-evidence" / ("trace.session.json" if kind == "archive" else "history.sqlite")
                    _, archive, database = self.invoke(*arguments, **{kind: destination},
                        code="invalid_input_or_output_paths")
                    self.assertFalse(archive.exists())
                    self.assertFalse(database.exists())
                    self.assertFalse((self.inputs / "fresh-evidence").exists())
        self.assertEqual(sorted(path.relative_to(self.inputs) for path in self.inputs.rglob("*")), before_names)
        # Normalize a legitimate outside alias too; do not reject every short
        # spelling just to make the in-input regression pass.
        outside = self.root / "outside-evidence-output"
        outside.mkdir()
        size = short_path(str(outside), buffer, len(buffer))
        self.assertTrue(0 < size < len(buffer))
        outside_alias = Path(buffer.value)
        self.assertTrue(os.path.samefile(outside_alias, outside))
        self.invoke(archive=outside_alias / "new.session.json", database=outside_alias / "new.sqlite",
                    expected=3, code="live_not_enabled")
        self.assertEqual(list(outside.iterdir()), [])


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: sandbox_live_cli_tests.py EXECUTABLE HEALTHY_PROJECT_JSON")
    EXECUTABLE, PROJECT = (Path(argument).resolve() for argument in sys.argv[1:])
    unittest.main(argv=[sys.argv[0]], verbosity=2)
