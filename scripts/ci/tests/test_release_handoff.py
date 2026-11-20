"""Small synthetic handoff fixtures; never application or VM qualification."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile


ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("release_handoff", ROOT / "scripts/prepare_release_handoff.py")
handoff = importlib.util.module_from_spec(spec)
spec.loader.exec_module(handoff)


class HandoffTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="breakwater-handoff-fixture-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.candidate = self.root / "candidate"
        self.verification = self.candidate / "verification"
        self.verification.mkdir(parents=True)
        self.destination = self.root / "new handoff"
        self.executable = b"synthetic fixture; not an executable"
        self.source = b"self-authored synthetic source fixture"
        with zipfile.ZipFile(self.candidate / "Breakwater-windows-x64.zip", "w") as archive:
            archive.writestr("Breakwater/bin/breakwater.exe", self.executable)
        self.write_source_zip({"source/README.md": self.source})
        self.package = {"status": "assembled", "dirty": False, "source_commit": "a" * 40,
                        "zip_sha256": handoff.digest(self.candidate / "Breakwater-windows-x64.zip"),
                        "app_sha256": hashlib.sha256(self.executable).hexdigest()}
        self.write_package()
        self.write_manifest({"README.md": hashlib.sha256(self.source).hexdigest()})

    def write_package(self):
        (self.verification / "package.json").write_text(json.dumps(self.package), encoding="utf-8")

    def write_manifest(self, value):
        (self.verification / "source-sha256.json").write_text(json.dumps(value), encoding="utf-8")

    def write_source_zip(self, files):
        with zipfile.ZipFile(self.candidate / "Breakwater-source.zip", "w") as archive:
            for name, data in files.items():
                archive.writestr(name, data)

    def reject(self):
        with self.assertRaises((ValueError, KeyError, zipfile.BadZipFile)):
            handoff.prepare(self.candidate, self.destination)
        self.assertFalse(self.destination.exists())

    def test_preserves_archives_and_keeps_all_external_gates_pending(self):
        original = {name: handoff.digest(self.candidate / name) for name in
                    ("Breakwater-windows-x64.zip", "Breakwater-source.zip")}
        record = handoff.prepare(self.candidate, self.destination)
        self.assertEqual(record["source_commit"], self.package["source_commit"])
        self.assertEqual(record["status"], "prepared")
        self.assertFalse(record["public_release_approved"])
        for name in ("clean_windows", "owner_review", "licensing_review"):
            self.assertEqual(record[name], "pending")
        for name, checksum in original.items():
            self.assertEqual(handoff.digest(self.candidate / name), checksum)
            self.assertEqual(handoff.digest(self.destination / name), checksum)
        template = json.loads((self.destination / "clean-windows-result.template.json").read_text())
        self.assertEqual(template["acceptance_version"], 2)
        self.assertEqual(template["status"], "pending")
        self.assertFalse(template["clean_machine_verified"])
        self.assertEqual(set(template["checks"].values()), {"not_run"})

    def test_current_summary_and_historical_packets_are_distinct_and_hash_checked(self):
        record = handoff.prepare(self.candidate, self.destination)
        summary = (self.destination / "CANDIDATE_SUMMARY.md").read_text(encoding="utf-8")
        for identity in (self.package["source_commit"], self.package["app_sha256"],
                         self.package["zip_sha256"], handoff.digest(self.candidate / "Breakwater-source.zip")):
            self.assertIn(identity, summary)
        self.assertIn("preserved historical review packets", summary)
        self.assertIn("no historical result is inherited", summary)
        self.assertIn("[CANDIDATE_READINESS.md](CANDIDATE_READINESS.md)", summary)
        for name in ("CANDIDATE_READINESS.md", "RELEASE_REVIEW.md", "RELEASE_REVIEW_V03.md"):
            self.assertEqual((self.destination / name).read_bytes(), (ROOT / "docs" / name).read_bytes())
        sums = (self.destination / "SHA256SUMS.txt").read_text(encoding="ascii")
        for name, checksum in record["files"].items():
            self.assertEqual(handoff.digest(self.destination / name), checksum)
            self.assertIn(f"{checksum}  {name}\n", sums)

    def test_new_observed_and_framing_acceptance_cannot_be_inferred_from_packaging(self):
        handoff.prepare(self.candidate, self.destination)
        template = json.loads((self.destination / "clean-windows-result.template.json").read_text())
        for name in ("connections_inspector_route_focus", "scene_focus_shortcut_escape_restore",
                     "observed_local_publisher_acquisition", "observed_capture_coverage_quality",
                     "observed_native_forecast", "observed_native_search_advisory_review",
                     "observed_expiry_replacement_invalidation", "observed_restart_history_nonactionable"):
            self.assertEqual(template["checks"][name], "not_run")
        self.assertEqual(template["reviewer"], "")
        self.assertEqual(template["vm"], {})

    def test_legacy_candidate_and_version_one_results_are_preserved_without_inheriting_acceptance(self):
        legacy_result = {"acceptance_version": 1, "status": "passed", "clean_machine_verified": True,
                         "source_commit": self.package["source_commit"], "checks": {"zip_hashes": "passed"}}
        legacy_path = self.verification / "clean-windows-result.json"
        legacy_bytes = (json.dumps(legacy_result, indent=2) + "\n").encode("utf-8")
        legacy_path.write_bytes(legacy_bytes)
        before = {path.relative_to(self.candidate): path.read_bytes()
                  for path in self.candidate.rglob("*") if path.is_file()}
        record = handoff.prepare(self.candidate, self.destination)
        self.assertEqual({path.relative_to(self.candidate): path.read_bytes()
                          for path in self.candidate.rglob("*") if path.is_file()}, before)
        self.assertEqual(legacy_path.read_bytes(), legacy_bytes)
        self.assertEqual(record["clean_windows"], "pending")
        template = json.loads((self.destination / "clean-windows-result.template.json").read_text())
        self.assertEqual(template["acceptance_version"], 2)
        self.assertFalse(template["clean_machine_verified"])
        self.assertEqual(set(template["checks"].values()), {"not_run"})

    def test_existing_version_one_handoff_is_never_upgraded_in_place(self):
        self.destination.mkdir()
        original = b'{"acceptance_version":1,"status":"pending"}\n'
        result = self.destination / "clean-windows-result.template.json"
        result.write_bytes(original)
        with self.assertRaises(ValueError):
            handoff.prepare(self.candidate, self.destination)
        self.assertEqual(result.read_bytes(), original)
        self.assertEqual({path.name for path in self.destination.iterdir()}, {result.name})

    def test_existing_destination_is_preserved(self):
        self.destination.mkdir()
        marker = self.destination / "keep.txt"
        marker.write_text("keep")
        with self.assertRaises(ValueError):
            handoff.prepare(self.candidate, self.destination)
        self.assertEqual(marker.read_text(), "keep")

    def test_nested_destination_is_rejected(self):
        with self.assertRaises(ValueError):
            handoff.prepare(self.candidate, self.candidate / "handoff")
        self.assertFalse((self.candidate / "handoff").exists())

    def test_dirty_or_unqualified_candidate_is_rejected(self):
        for field, value in (("dirty", True), ("dirty", None), ("status", "running"),
                             ("source_commit", "unknown"), ("source_commit", None)):
            with self.subTest(field=field, value=value):
                old = self.package[field]
                self.package[field] = value
                self.write_package()
                self.reject()
                self.package[field] = old

    def test_changed_windows_zip_is_rejected(self):
        with (self.candidate / "Breakwater-windows-x64.zip").open("ab") as stream:
            stream.write(b"changed")
        self.reject()

    def test_wrong_packaged_executable_is_rejected(self):
        self.package["app_sha256"] = "0" * 64
        self.write_package()
        self.reject()

    def test_changed_source_is_rejected(self):
        self.write_source_zip({"source/README.md": b"changed"})
        self.reject()

    def test_unlisted_source_file_is_rejected(self):
        self.write_source_zip({"source/README.md": self.source, "source/extra.txt": b"extra"})
        self.reject()

    def test_missing_source_file_is_rejected(self):
        self.write_manifest({"README.md": hashlib.sha256(self.source).hexdigest(), "missing.txt": "a" * 64})
        self.reject()

    def test_unsafe_or_ambiguous_manifest_paths_are_rejected(self):
        for paths in ({"../README.md": "a" * 64}, {"C:/README.md": "a" * 64},
                      {"README.md": "a" * 64, "readme.md": "b" * 64},
                      {"README.md": "not-a-hash"}):
            with self.subTest(paths=paths):
                self.write_manifest(paths)
                self.reject()

    def test_unsafe_archive_path_is_rejected_without_extraction(self):
        self.write_source_zip({"source/../escape.txt": b"escape"})
        self.reject()
        self.assertFalse((self.root / "escape.txt").exists())

    def test_duplicate_archive_path_is_rejected(self):
        with zipfile.ZipFile(self.candidate / "Breakwater-source.zip", "a") as archive:
            archive.writestr("source/readme.md", self.source)
        self.reject()

    def test_unsafe_empty_archive_directory_is_rejected(self):
        with zipfile.ZipFile(self.candidate / "Breakwater-source.zip", "a") as archive:
            archive.writestr("source/../../escape/", b"")
        self.reject()

    def test_ambiguous_windows_archive_is_rejected(self):
        windows_zip = self.candidate / "Breakwater-windows-x64.zip"
        with zipfile.ZipFile(windows_zip, "a") as archive:
            archive.writestr("Breakwater/bin/Breakwater.exe", b"conflicting fixture")
        self.package["zip_sha256"] = handoff.digest(windows_zip)
        self.write_package()
        self.reject()

    def test_duplicate_json_keys_and_nonobjects_are_rejected(self):
        for value in ('{"status":"assembled","status":"running"}', '[]'):
            with self.subTest(value=value):
                (self.verification / "package.json").write_text(value)
                self.reject()

    def test_copy_failure_never_produces_a_prepared_record(self):
        with patch.object(handoff.shutil, "copyfileobj", side_effect=OSError("synthetic copy failure")):
            with self.assertRaises(OSError):
                handoff.prepare(self.candidate, self.destination)
        self.assertFalse((self.destination / "handoff.json").exists())
        self.assertEqual(json.loads((self.destination / "preparation-failed.json").read_text())["status"], "failed")


if __name__ == "__main__":
    unittest.main()
