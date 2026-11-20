"""Stage an immutable Windows/source candidate and a pending manual VM handoff.

No installation, extraction, network request, approval or release publication.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import zipfile


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError("Duplicate JSON key in candidate metadata.")
        value[key] = item
    return value


def read_json(path):
    if path.stat().st_size > 8 * 1024 * 1024:
        raise ValueError("Candidate metadata is oversized.")
    value = json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object)
    if not isinstance(value, dict):
        raise ValueError("Candidate metadata must be a JSON object.")
    return value


def relative_name(value):
    if not isinstance(value, str):
        raise ValueError("Manifest paths must be text.")
    name = value.replace("\\", "/")
    if not name or name.startswith("/") or ":" in name or any(
            part in ("", ".", "..") for part in name.split("/")):
        raise ValueError("Manifest contains an unsafe path.")
    return str(PurePosixPath(name))


def hash_map(value):
    if not isinstance(value, dict) or not value:
        raise ValueError("A nonempty source file manifest is required.")
    result, folded = {}, set()
    for key, checksum in value.items():
        name = relative_name(key)
        if name.casefold() in folded or not isinstance(checksum, str) or len(checksum) != 64 or any(
                char not in "0123456789abcdef" for char in checksum):
            raise ValueError("Manifest has ambiguous paths or invalid SHA-256 values.")
        folded.add(name.casefold())
        result[name] = checksum
    return result


def verify_source_archive(path, expected):
    with zipfile.ZipFile(path) as archive:
        files, folded = {}, set()
        for entry in archive.infolist():
            name = relative_name(entry.filename.rstrip("/"))
            if name != "source" and not name.startswith("source/"):
                raise ValueError("Source archive has an unexpected root.")
            if entry.is_dir():
                continue
            if name.casefold() in folded or not name.startswith("source/"):
                raise ValueError("Source archive has duplicate or unexpected paths.")
            folded.add(name.casefold())
            files[name.removeprefix("source/")] = entry
        if set(files) != set(expected):
            raise ValueError("Source archive does not match its file manifest.")
        if sum(entry.file_size for entry in files.values()) > 512 * 1024 * 1024:
            raise ValueError("Source archive exceeds the handoff size bound.")
        for name, checksum in expected.items():
            with archive.open(files[name]) as stream:
                if hashlib.file_digest(stream, "sha256").hexdigest() != checksum:
                    raise ValueError("Source archive file checksum differs from its manifest.")


def prepare(candidate, destination):
    candidate = candidate.resolve(strict=True)
    destination = destination.resolve()
    if destination.exists() or destination.is_relative_to(candidate) or candidate.is_relative_to(destination):
        raise ValueError("Use a new destination outside the candidate directory.")
    package_path = candidate / "verification/package.json"
    package = read_json(package_path)
    commit = package.get("source_commit", "")
    if package.get("status") != "assembled" or package.get("dirty") is not False or not isinstance(commit, str) or len(commit) != 40 or any(
            char not in "0123456789abcdef" for char in commit):
        raise ValueError("An assembled, clean, commit-qualified candidate is required.")
    windows_zip = candidate / "Breakwater-windows-x64.zip"
    source_zip = candidate / "Breakwater-source.zip"
    if digest(windows_zip) != package.get("zip_sha256"):
        raise ValueError("Windows ZIP checksum differs from the package report.")
    with zipfile.ZipFile(windows_zip) as archive:
        folded = set()
        for entry in archive.infolist():
            name = relative_name(entry.filename.rstrip("/"))
            if name != "Breakwater" and not name.startswith("Breakwater/"):
                raise ValueError("Windows archive has an unexpected root.")
            if name.casefold() in folded:
                raise ValueError("Windows archive has ambiguous paths.")
            folded.add(name.casefold())
        app_info = archive.getinfo("Breakwater/bin/breakwater.exe")
        if app_info.file_size > 256 * 1024 * 1024:
            raise ValueError("Application executable exceeds the handoff size bound.")
        with archive.open(app_info) as stream:
            if hashlib.file_digest(stream, "sha256").hexdigest() != package.get("app_sha256"):
                raise ValueError("Packaged executable checksum differs from its source report.")
    source_manifest_path = candidate / "verification/source-sha256.json"
    verify_source_archive(source_zip, hash_map(read_json(source_manifest_path)))
    documents = Path(__file__).resolve().parents[1] / "docs"
    payload = {
        windows_zip.name: windows_zip, source_zip.name: source_zip,
        "package.json": package_path, "source-sha256.json": source_manifest_path,
        "CLEAN_WINDOWS_CHECK.md": documents / "CLEAN_WINDOWS_CHECK.md",
        "CANDIDATE_READINESS.md": documents / "CANDIDATE_READINESS.md",
        "RELEASE_REVIEW.md": documents / "RELEASE_REVIEW.md",
        "RELEASE_REVIEW_V03.md": documents / "RELEASE_REVIEW_V03.md",
    }
    checksums = {name: digest(path) for name, path in payload.items()}
    summary = (
        "# Current private candidate\n\n"
        f"Application source: `{commit}`.\n\n"
        "These identities were checked during kit preparation. They identify the candidate; "
        "they do not establish CI, hosted qualification, author consent or clean-machine acceptance.\n\n"
        "| Artifact | SHA-256 |\n| --- | --- |\n"
        f"| Windows ZIP | `{checksums[windows_zip.name]}` |\n"
        f"| Matching source ZIP | `{checksums[source_zip.name]}` |\n"
        f"| Application executable | `{package['app_sha256']}` |\n"
        f"| Source file manifest | `{checksums['source-sha256.json']}` |\n\n"
        "The source archive was checked file by file against the supplied manifest. "
        "The Windows archive and executable were checked against package.json.\n\n"
        "Owner review, licensing/notices/source-delivery review and actual clean-Windows "
        "acceptance remain pending. Public release approval is false. Record current engineering "
        "evidence and exact-head human review separately; no historical result is inherited.\n\n"
        "RELEASE_REVIEW.md and RELEASE_REVIEW_V03.md are preserved historical review packets "
        "for their stated revisions. They are context, not acceptance of this candidate. "
        "Read [CANDIDATE_READINESS.md](CANDIDATE_READINESS.md) for the current preparation/review "
        "packet, including any source-qualified pending work. Use CLEAN_WINDOWS_CHECK.md and "
        "a copy of the version-2 result template for new acceptance. "
        "Existing version-1 records stay associated with their original candidates and checks.\n"
    ).encode("utf-8")
    checksums["CANDIDATE_SUMMARY.md"] = hashlib.sha256(summary).hexdigest()
    record = {
        "handoff_version": 1, "status": "prepared", "source_commit": commit,
        "app_sha256": package["app_sha256"], "files": checksums,
        "prepared_utc": datetime.now(timezone.utc).isoformat(),
        "public_release_approved": False, "clean_windows": "pending",
        "owner_review": "pending", "licensing_review": "pending",
    }
    destination.mkdir(parents=True, exist_ok=False)
    try:
        for name, path in payload.items():
            target = destination / name
            with path.open("rb") as source, target.open("xb") as output:
                shutil.copyfileobj(source, output)
            if digest(target) != checksums[name]:
                raise ValueError("A handoff copy changed during preparation.")
        with (destination / "CANDIDATE_SUMMARY.md").open("xb") as output:
            output.write(summary)
        if digest(destination / "CANDIDATE_SUMMARY.md") != checksums["CANDIDATE_SUMMARY.md"]:
            raise ValueError("The candidate summary changed during preparation.")
        (destination / "SHA256SUMS.txt").write_text(
            "".join(f"{checksum}  {name}\n" for name, checksum in checksums.items()), encoding="ascii")
        (destination / "README.md").write_text(
            f"# Breakwater Windows VM handoff\n\nApplication source: `{commit}`.\n\n"
            "This is a review candidate. Owner/license review and actual clean-Windows acceptance are pending.\n\n"
            "Read CANDIDATE_SUMMARY.md for this kit's verified identities. The two RELEASE_REVIEW "
            "packets describe historical revisions; their results do not qualify this candidate.\n\n"
            "1. Verify the ZIP hashes against SHA256SUMS.txt.\n"
            "2. On a fresh Windows x64 VM, extract the Windows ZIP to a path containing spaces and Unicode.\n"
            "3. Run Breakwater/Verify-Breakwater.cmd, then Start-Breakwater.cmd. No PowerShell 7, Python or SDK installation is required.\n"
            "4. Complete CLEAN_WINDOWS_CHECK.md and fill a COPY of clean-windows-result.template.json with actual outcomes.\n"
            "5. Retain the generated verification folder, screenshots and matching source ZIP.\n\n"
            "The automated bundle report deliberately identifies sanitized processes, even when run on a VM. "
            "Record the actual VM identity/manual acceptance separately. Neither this kit nor a process check grants release approval.\n",
            encoding="utf-8")
        template = {
            "acceptance_version": 2, "status": "pending", "clean_machine_verified": False,
            "source_commit": commit, "windows_zip_sha256": checksums[windows_zip.name],
            "source_zip_sha256": checksums[source_zip.name], "app_sha256": package["app_sha256"],
            "reviewer": "", "recorded_utc": "", "vm": {}, "verification_report": "",
            "checks": {name: "not_run" for name in (
                "fresh_windows_no_development_tools", "zip_hashes", "automated_bundle_checks",
                "sandbox_forecast_search_comparison", "explicit_review_edit_reset",
                "operations_import_search_validation", "history_restart_nonactionable_import",
                "missing_optional_science", "file_dialog_cancel_keyboard",
                "windows_scaling_100_125_150_200", "minimum_client_960x540",
                "connections_inspector_route_focus", "scene_focus_shortcut_escape_restore",
                "observed_local_publisher_acquisition", "observed_capture_coverage_quality",
                "observed_native_forecast", "observed_native_search_advisory_review",
                "observed_expiry_replacement_invalidation", "observed_restart_history_nonactionable")},
            "failures": [], "notes": "Manual VM acceptance; never infer it from a developer-process report.",
        }
        (destination / "clean-windows-result.template.json").write_text(json.dumps(template, indent=2) + "\n", encoding="utf-8")
        (destination / "handoff.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    except Exception as error:
        (destination / "preparation-failed.json").write_text(
            json.dumps({"status": "failed", "error_type": type(error).__name__}) + "\n", encoding="utf-8")
        raise
    return record


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    options = parser.parse_args()
    try:
        result = prepare(options.candidate, options.destination)
        print(json.dumps({key: result[key] for key in ("status", "source_commit", "clean_windows", "public_release_approved")}))
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Handoff preparation failed: {error}\n")
