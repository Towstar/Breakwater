"""Credential-free preflight against the production controller and provider.

Usage: python -B eog_offline_preflight_tests.py OFFLINE_EXECUTABLE PORTFOLIO_OPERATIONS
These checks require the actual executable and configured local numerical runtime.
They never construct a hosted transport or forward NVIDIA credentials.
"""
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

FILES = ("network.json", "ops.json", "telemetry.csv")
EXECUTABLE = None
FIXTURE = None


def hashes(directory):
    return {name: hashlib.sha256((directory / name).read_bytes()).hexdigest() for name in FILES}


class EogOfflinePreflightTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(EXECUTABLE.is_file(), "Build the real offline-check executable before running this suite.")
        self.original = hashes(FIXTURE)
        self.directory = tempfile.TemporaryDirectory(prefix="breakwater-preflight-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.dataset, self.output = self.root / "dataset", self.root / "output"
        self.dataset.mkdir()
        self.output.mkdir()
        for name in FILES:
            shutil.copyfile(FIXTURE / name, self.dataset / name)

    def environment(self):
        # Only runtime/provider configuration crosses the subprocess boundary.
        # In particular no model key, endpoint, model override or .env is read.
        names = ("SystemRoot", "WINDIR", "PATH", "PATHEXT", "TEMP", "TMP", "USERPROFILE",
                 "LOCALAPPDATA", "APPDATA", "ASAN_OPTIONS", "UBSAN_OPTIONS", "QT_LOGGING_RULES",
                 "BREAKWATER_PYTHON", "BREAKWATER_EOG_WORKER", "BREAKWATER_EOG_CONTRACTS", "BREAKWATER_EOG_NATIVE")
        return {name: os.environ[name] for name in names if name in os.environ}

    def invoke(self, *flags, expected=0, overrides=None):
        before = hashes(self.dataset)
        environment = self.environment()
        environment.update(overrides or {})
        try:
            result = subprocess.run([str(EXECUTABLE), str(self.dataset), str(self.output), *flags],
                                    capture_output=True, text=True, encoding="utf-8", errors="replace",
                                    timeout=150, env=environment, cwd=self.root)
        finally:
            self.assertEqual(hashes(self.dataset), before, "Preflight changed its source dataset.")
            self.assertEqual(hashes(FIXTURE), self.original, "A test changed the committed source fixture.")
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        if expected == 2:
            self.assertEqual(result.stdout, "")
            return None
        summary = json.loads(result.stdout)
        self.assertIs(summary["preflight"], True)
        self.assertEqual(summary["source_hashes"], before)
        self.assertEqual(summary["model_requests"], 0)
        self.assertIs(summary["zero_model_requests"], True)
        self.assertIs(summary["actionable"], False)
        self.assertIn("no global", summary["limitation"])
        saved = Path(summary["artifact_directory"]) / "verification.json"
        self.assertEqual(json.loads(saved.read_text(encoding="utf-8")), summary)
        if expected == 0:
            self.assertEqual(summary["status"], "passed")
            self.assertIs(summary["inputs_unchanged"], True)
            self.assertIs(summary["history_reopened_nonactionable"], True)
            self.assertIs(summary["provider_configured"], True)
            self.assertIs(summary["provider_exchange_verified"], True)
            responses = self.assert_archive(summary, before)
            self.assertIs(summary["provider_verified"], responses[-1]["status"] == "completed")
        else:
            self.assertEqual(summary["status"], "failed")
        return summary

    def assert_archive(self, summary, before):
        path = Path(summary["archive_path"])
        self.assertTrue(path.is_file())
        self.assertEqual(path.parent, Path(summary["artifact_directory"]))
        archive = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(archive["session_id"], summary["session_id"])
        self.assertEqual(archive["status"], "completed")
        self.assertIs(archive["historical"], True)
        self.assertIs(archive["actionable"], False)
        self.assertEqual(archive["operator_records"], [])
        self.assertEqual(archive["agent_events"], [])
        manifest = archive["capture_manifest"]
        self.assertEqual(manifest["source_hashes"], before)
        for key in ("capture_id", "dataset_identity", "cutoff_unix"):
            self.assertEqual(manifest[key], summary[key])
        responses = []
        for row in archive["analyses"]:
            request, response = row["request"], row["response"]
            self.assertEqual(request["eog_version"], 1)
            self.assertEqual(response["eog_version"], 1)
            self.assertEqual(request["kind"], "analysis_request")
            self.assertEqual(response["kind"], "analysis_response")
            self.assertEqual(request["method_version"], 1)
            self.assertEqual(response["method_version"], 1)
            self.assertEqual(request["request_id"], response["request_id"])
            self.assertEqual(request["operation"], response["operation"])
            for key in ("capture_id", "dataset_identity", "cutoff_unix"):
                self.assertEqual(request[key], manifest[key])
                self.assertEqual(response[key], manifest[key])
            responses.append(response)
        self.assertEqual([row["request_id"] for row in responses], summary["analysis_request_ids"])
        self.assertEqual(len(set(summary["analysis_request_ids"])), len(responses))
        self.assertGreaterEqual(len(responses), 2)
        self.assertEqual(responses[0]["operation"], "detect")
        self.assertEqual(responses[0]["status"], "completed")
        self.assertEqual(responses[1]["operation"], "search_plans")
        self.assertEqual(responses[1]["status"], summary["search_response_status"])
        return responses

    def test_valid_portfolio_fixture_retains_exact_independently_validated_advice(self):
        summary = self.invoke("--preflight")
        self.assertEqual(summary["outcome"], "validated_advice")
        self.assertEqual(summary["coverage"], "available")
        self.assertIs(summary["provider_verified"], True)
        responses = self.assert_archive(summary, hashes(self.dataset))
        self.assertEqual([row["operation"] for row in responses], ["detect", "search_plans", "validate_plan"])
        validation = responses[-1]["result"]["validation"]
        self.assertEqual(validation, summary["validation"])
        self.assertTrue(validation["independent"])
        self.assertTrue(validation["feasible"])
        self.assertEqual(validation["coverage"], "available")
        self.assertEqual(validation["plan_id"], summary["selected_plan_id"])
        self.assertEqual(validation["validation_id"], summary["validation_id"])
        self.assertEqual(validation["plan_hash"], summary["candidate"]["plan"]["plan_hash"])
        self.assertTrue(summary["baseline"]["plan"])

    def remove_failure_status_coverage(self, stale):
        path = self.dataset / "telemetry.csv"
        with path.open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            fields, rows = reader.fieldnames, list(reader)
        now = max(int(row["ts_unix"]) for row in rows)
        asset = next(row["asset_id"] for row in rows if row["metric"] == "status" and row["value"] == "2")
        transformed = [row for row in rows if not (row["asset_id"] == asset and row["metric"] == "status" and
                      (not stale or int(row["ts_unix"]) > now - 600))]
        self.assertLess(len(transformed), len(rows))
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
            writer.writeheader()
            writer.writerows(transformed)

    def assert_coverage_refusal(self):
        summary = self.invoke("--preflight")
        self.assertEqual(summary["outcome"], "coverage_unavailable")
        self.assertEqual(summary["coverage"], "unavailable")
        self.assertIs(summary["provider_verified"], False)
        self.assertEqual(summary["selected_plan_id"], "")
        self.assertEqual(summary["validation_id"], "")
        responses = self.assert_archive(summary, hashes(self.dataset))
        self.assertEqual(len(responses), 2)
        self.assertEqual(responses[-1]["status"], "unavailable")
        self.assertIn("failure_context_incomplete", [row["code"] for row in responses[-1]["diagnostics"]])
        self.assertNotIn("validation", summary)

    def test_missing_failure_channel_is_completed_nonactionable_coverage_evidence(self):
        self.remove_failure_status_coverage(stale=False)
        self.assert_coverage_refusal()

    def test_stale_failure_channel_is_completed_nonactionable_coverage_evidence(self):
        self.remove_failure_status_coverage(stale=True)
        self.assert_coverage_refusal()

    def test_no_current_failure_retains_provider_refusal_without_global_claim(self):
        network = json.loads((self.dataset / "network.json").read_text(encoding="utf-8"))
        pumps = {facility["pump"]["id"]: facility["pump"]["max_flow_bpd"]
                 for system in network["systems"] for facility in system["facilities"]}
        path = self.dataset / "telemetry.csv"
        with path.open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            fields, rows = reader.fieldnames, list(reader)
        failed = {row["asset_id"] for row in rows if row["asset_id"] in pumps and
                  row["metric"] == "status" and row["value"] == "2"}
        self.assertTrue(failed)
        # Replace fault feedback with coherent running feedback. Numerical
        # interpretation remains entirely in the real detector/provider.
        for row in rows:
            if row["asset_id"] in failed:
                if row["metric"] == "status":
                    row["value"] = "1"
                elif row["metric"] == "flow_bpd":
                    row["value"] = str(pumps[row["asset_id"]])
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)
        summary = self.invoke("--preflight")
        self.assertEqual(summary["outcome"], "no_validated_plan")
        self.assertEqual(summary["coverage"], "available")
        self.assertIs(summary["provider_verified"], False)
        self.assertEqual(summary["selected_plan_id"], "")
        self.assertEqual(summary["validation_id"], "")
        responses = self.assert_archive(summary, hashes(self.dataset))
        self.assertEqual(len(responses), 2)
        self.assertEqual(responses[-1]["status"], "unavailable")
        self.assertEqual([row["code"] for row in responses[-1]["diagnostics"]], ["no_active_failure"])

    def test_unavailable_provider_fails_instead_of_becoming_no_plan(self):
        summary = self.invoke("--preflight", expected=1,
                              overrides={"BREAKWATER_PYTHON": str(self.root / "absent-python.exe")})
        self.assertIs(summary["provider_configured"], False)
        self.assertIs(summary["provider_exchange_verified"], False)
        self.assertNotIn("outcome", summary)
        self.assertNotIn("archive_path", summary)

    def test_unknown_duplicate_and_misplaced_flags_fail_before_artifact_creation(self):
        for flags in (("--prefight",), ("--preflight=true",), ("--preflight", "--preflight"),
                      ("--unknown", "--preflight")):
            with self.subTest(flags=flags):
                self.invoke(*flags, expected=2)
                self.assertEqual(list(self.output.iterdir()), [])
        result = subprocess.run([str(EXECUTABLE), "--preflight", str(self.dataset), str(self.output)],
                                capture_output=True, text=True, timeout=15, env=self.environment(), cwd=self.root)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(list(self.output.iterdir()), [])
        self.assertEqual(hashes(FIXTURE), self.original)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: eog_offline_preflight_tests.py OFFLINE_EXECUTABLE PORTFOLIO_OPERATIONS")
    EXECUTABLE, FIXTURE = (Path(value).resolve() for value in sys.argv[1:])
    unittest.main(argv=[sys.argv[0]], verbosity=2)
