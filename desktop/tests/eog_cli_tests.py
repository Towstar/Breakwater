"""Actual executable CLI rejections; no numerical provider or model call."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

EXECUTABLE = Path(sys.argv[1]).resolve()
FIXTURE = Path(sys.argv[2]).resolve()
FILES = ("network.json", "ops.json", "telemetry.csv")


class EogCliTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.dataset = self.root / "data"
        self.dataset.mkdir()
        for name in FILES:
            shutil.copyfile(FIXTURE / name, self.dataset / name)
        self.before = self.hashes()

    def hashes(self):
        return {name: hashlib.sha256((self.dataset / name).read_bytes()).hexdigest()
                for name in FILES}

    def reject(self, *arguments, dataset=True):
        command = [str(EXECUTABLE), "--workspace", "eog", "--smoke"]
        if dataset:
            command += ["--eog-dataset", str(self.dataset)]
        result = subprocess.run(command + list(arguments), capture_output=True,
                                text=True, timeout=25)
        self.assertEqual(result.returncode, 8, result.stdout + result.stderr)
        self.assertEqual(self.hashes(), self.before)

    def test_inputs_cannot_be_report_screenshot_or_database(self):
        for name in FILES:
            for flag in ("--eog-report", "--screenshot", "--history-db"):
                with self.subTest(file=name, writer=flag):
                    self.reject(flag, str(self.dataset / name))

    def test_reports_screenshot_and_database_are_distinct(self):
        target = str(self.root / "collision")
        for left, right in (("--eog-report", "--screenshot"),
                            ("--eog-report", "--history-db"),
                            ("--screenshot", "--history-db")):
            with self.subTest(left=left, right=right):
                self.reject(left, target, right, target)

    def test_reports_cannot_overwrite_database_journals(self):
        database = str(self.root / "history.sqlite")
        for suffix in ("-wal", "-shm", "-journal"):
            with self.subTest(suffix=suffix):
                self.reject("--history-db", database, "--eog-report", database + suffix)

    def test_legacy_report_writers_are_rejected(self):
        for flag in ("--health-report", "--science-report"):
            with self.subTest(flag=flag):
                self.reject(flag, str(self.dataset / "network.json"))

    def test_conflicting_workbench_operations_are_rejected(self):
        self.reject("--sensor-editor-check")
        self.reject("--workspace", "workbench")

    def test_facility_and_report_need_a_dataset(self):
        self.reject("--eog-facility", "facility-Z", dataset=False)
        self.reject("--eog-report", str(self.root / "report.json"), dataset=False)

    def test_unknown_workspace_and_facility_are_rejected(self):
        self.reject("--workspace", "unknown")
        self.reject("--eog-facility", "missing-facility")

    def test_screenshot_replaces_hardlink_without_mutating_input(self):
        screenshot = self.root / "capture.png"
        os.link(self.dataset / "network.json", screenshot)
        result = subprocess.run([str(EXECUTABLE), "--workspace", "eog", "--smoke",
                                 "--eog-dataset", str(self.dataset), "--screenshot", str(screenshot),
                                 "--history-db", str(self.root / "history.sqlite")],
                                capture_output=True, text=True, timeout=25)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.hashes(), self.before)
        self.assertTrue(screenshot.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=2)

