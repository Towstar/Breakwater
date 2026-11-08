"""Offline checks of the real live-check executable; never enable hosted calls.

Usage: python -B eog_live_cli_tests.py EXECUTABLE RENDER_FIXTURE_DIRECTORY
Each process receives a private temporary history path. These tests exercise
native CLI/readiness/input protection, not model or numerical qualification.
"""
import csv
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

FILES = ("network.json", "ops.json", "telemetry.csv")
EXECUTABLE = None
FIXTURE = None


class EogLiveCliTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="breakwater-eog-live-cli-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.dataset = self.root / "data"
        self.dataset.mkdir()
        for name in FILES:
            shutil.copyfile(FIXTURE / name, self.dataset / name)
        self.before = self.hashes()
        self.invocations = 0
        with (self.dataset / "telemetry.csv").open(encoding="utf-8-sig", newline="") as stream:
            times = [int(row["ts_unix"]) for row in csv.DictReader(stream)]
        self.first, self.now = min(times), max(times)

    def hashes(self):
        return {name: hashlib.sha256((self.dataset / name).read_bytes()).hexdigest()
                for name in FILES}

    def invoke(self, *arguments, archive=None, database=None, expected=2, code=None):
        self.invocations += 1
        archive = archive if archive is not None else self.root / f"trace-{self.invocations}.eog-session.json"
        database = database if database is not None else self.root / f"history-{self.invocations}.sqlite"
        command = [str(EXECUTABLE), "--dataset", str(self.dataset), "--archive", str(archive),
                   "--db", str(database), *map(str, arguments)]
        self.assertNotIn("--live", command, "Offline tests must never enable hosted calls.")
        # Read only known process/runtime settings; no credential variable is
        # inspected or forwarded, and no provider/model can be invoked here.
        names = ("SystemRoot", "WINDIR", "PATH", "PATHEXT", "TEMP", "TMP", "USERPROFILE",
                 "LOCALAPPDATA", "APPDATA", "ASAN_OPTIONS", "UBSAN_OPTIONS", "QT_LOGGING_RULES")
        environment = {name: os.environ[name] for name in names if name in os.environ}
        try:
            completed = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                                       errors="replace", timeout=30, env=environment)
        finally:
            self.assertEqual(self.hashes(), self.before, "A CLI path/readiness check changed source bytes.")
        self.assertEqual(completed.returncode, expected, completed.stdout + completed.stderr)
        lines = completed.stdout.strip().splitlines()
        self.assertEqual(len(lines), 1, "CLI stdout must be one JSON summary: " + completed.stdout)
        summary = json.loads(lines[0])
        self.assertIsInstance(summary, dict)
        self.assertEqual(summary.get("exit_code"), expected)
        if code is not None:
            self.assertEqual(summary.get("code"), code)
        for key in ("model_requests", "successful_model_replies", "model_issued_tool_calls",
                    "executed_tools", "numeric_exchanges", "numeric_completed"):
            if key in summary:
                self.assertEqual(summary[key], 0, "Offline CLI unexpectedly started work: " + key)
        if "live_enabled" in summary:
            self.assertIs(summary["live_enabled"], False)
        if "archive_written" in summary:
            self.assertIs(summary["archive_written"], False)
        return summary, Path(archive), Path(database)

    def test_native_executable_and_inputs_are_required_without_skips(self):
        self.assertTrue(EXECUTABLE.is_file(), "Build the actual live-check executable before this test.")
        self.assertTrue(all((FIXTURE / name).is_file() for name in FILES))
        summary, archive, _ = self.invoke(expected=3, code="live_not_enabled")
        self.assertEqual(summary["status"], "incomplete")
        self.assertEqual(summary["model_requests"], 0)
        self.assertFalse(archive.exists())

    def test_readiness_at_current_and_historical_cutoffs_never_runs_or_archives(self):
        for cutoff in (None, self.first, self.first + (self.now - self.first) // 2):
            with self.subTest(cutoff=cutoff):
                arguments = () if cutoff is None else ("--cutoff", cutoff)
                summary, archive, database = self.invoke(*arguments, expected=3, code="live_not_enabled")
                self.assertEqual(summary["status"], "incomplete")
                self.assertEqual(summary["model_requests"], 0)
                self.assertEqual(summary["numeric_exchanges"], 0)
                self.assertEqual(summary["session_id"], "")
                self.assertFalse(archive.exists())
                self.assertTrue(database.is_file())
                with closing(sqlite3.connect(database)) as connection:
                    self.assertEqual(connection.execute("SELECT count(*) FROM eog_sessions").fetchone()[0], 0)

    def test_invalid_existing_and_in_source_archives_are_rejected(self):
        existing = self.root / "existing.eog-session.json"
        sentinel = b"PRESERVE EXISTING ARCHIVE BYTES\n"
        existing.write_bytes(sentinel)
        directory = self.root / "folder.eog-session.json"
        directory.mkdir()
        file_parent = self.root / "file-parent"
        file_parent.write_bytes(b"PRESERVE NONDIRECTORY PARENT")
        targets = [self.root / "wrong-extension.json", existing, directory,
                   file_parent / "new.eog-session.json", self.dataset / "new.eog-session.json"]
        targets += [self.dataset / name for name in FILES]
        for target in targets:
            with self.subTest(archive=target.name):
                self.invoke(archive=target, code="invalid_input_or_output_paths")
        self.assertEqual(existing.read_bytes(), sentinel)
        self.assertEqual(file_parent.read_bytes(), b"PRESERVE NONDIRECTORY PARENT")
        self.assertFalse((self.dataset / "new.eog-session.json").exists())

    def test_source_database_paths_and_archive_database_collision_are_rejected(self):
        for target in [*(self.dataset / name for name in FILES), self.dataset / "new.sqlite"]:
            with self.subTest(database=target.name):
                _, archive, _ = self.invoke(database=target, code="invalid_input_or_output_paths")
                self.assertFalse(archive.exists())
        self.assertFalse((self.dataset / "new.sqlite").exists())
        collision = self.root / "collision.eog-session.json"
        self.invoke(archive=collision, database=collision, code="invalid_input_or_output_paths")
        self.assertFalse(collision.exists())

    def test_database_sidecar_names_cannot_be_used_as_archives(self):
        database = self.root / "history.sqlite"
        for suffix in ("-wal", "-shm", "-journal", ".lock"):
            with self.subTest(suffix=suffix):
                target = Path(str(database) + suffix)
                self.invoke(archive=target, database=database, code="invalid_input_or_output_paths")
                self.assertFalse(target.exists())
        self.assertFalse(database.exists())

    def test_malformed_cutoffs_and_future_cutoffs_cannot_write_an_archive(self):
        for cutoff in ("not-a-time", "-1", "1.5", "01", "1e9", "253402300800"):
            with self.subTest(cutoff=cutoff):
                _, archive, database = self.invoke("--cutoff", cutoff, code="invalid_cutoff")
                self.assertFalse(archive.exists())
                self.assertFalse(database.exists())
        for cutoff in (self.first - 1, self.now + 1):
            with self.subTest(out_of_window=cutoff):
                _, archive, _ = self.invoke("--cutoff", cutoff, code="cutoff_out_of_range")
                self.assertFalse(archive.exists())

    def test_outside_hardlinked_input_databases_fail_without_modifying_any_input(self):
        # Hard links are mandatory real filesystem operations, not mocked or
        # skipped. SQLite must refuse these non-database bytes before claiming it.
        for name in FILES:
            with self.subTest(input=name):
                database = self.root / (name + ".sqlite")
                os.link(self.dataset / name, database)
                original = database.read_bytes()
                self.assertTrue(os.path.samefile(database, self.dataset / name))
                summary, archive, _ = self.invoke(database=database, expected=6, code="history_failed")
                self.assertEqual(summary["status"], "failed")
                self.assertEqual(summary["model_requests"], 0)
                self.assertFalse(archive.exists())
                self.assertEqual(database.read_bytes(), original)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: eog_live_cli_tests.py EXECUTABLE RENDER_FIXTURE_DIRECTORY")
    EXECUTABLE, FIXTURE = (Path(value).resolve() for value in sys.argv[1:])
    unittest.main(argv=[sys.argv[0]], verbosity=2)
