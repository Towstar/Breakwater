"""Real local walkthrough, manifest identity, native oracles and archive recovery."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXPORT, OFFLINE, ROOT = map(Path, sys.argv[1:4])
sys.argv[1:] = []


class PortfolioExamples(unittest.TestCase):
    def test_native_projects_match_the_committed_oracles(self):
        with tempfile.TemporaryDirectory(prefix="portfolio-native-") as output:
            result = subprocess.run([str(EXPORT), output], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            for fixture in (ROOT / "sandbox").glob("*.json"):
                self.assertEqual(json.loads(fixture.read_text(encoding="utf-8")),
                                 json.loads((Path(output) / fixture.name).read_text(encoding="utf-8")))
            # The producer refuses accidental overwrite of evidence/examples.
            repeated = subprocess.run([str(EXPORT), output], capture_output=True, timeout=30)
            self.assertEqual(repeated.returncode, 2)

    def test_recorded_inputs_search_validate_and_reopen_without_a_model(self):
        manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
        for name, files in manifest["datasets"].items():
            with self.subTest(dataset=name), tempfile.TemporaryDirectory(prefix="portfolio-offline-") as output:
                dataset = ROOT / name
                before = {file: hashlib.sha256((dataset / file).read_bytes()).hexdigest() for file in files}
                self.assertEqual(before, files)
                result = subprocess.run([str(OFFLINE), str(dataset), output], capture_output=True, text=True,
                                        encoding="utf-8", timeout=120)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                report = json.loads(result.stdout)
                self.assertEqual(report["status"], "passed")
                self.assertEqual(report["model_requests"], 0)
                self.assertTrue(report["inputs_unchanged"])
                self.assertTrue(report["history_reopened_nonactionable"])
                self.assertTrue(report["validation"]["independent"])
                self.assertTrue(report["validation"]["feasible"])
                candidate = report["candidate"]
                self.assertEqual(report["validation"]["plan_hash"], candidate["plan"]["plan_hash"])
                self.assertLess(candidate["cost"]["total"], report["baseline"]["cost"]["total"])
                self.assertEqual(before, {file: hashlib.sha256((dataset / file).read_bytes()).hexdigest() for file in files})


if __name__ == "__main__":
    unittest.main(verbosity=2)
