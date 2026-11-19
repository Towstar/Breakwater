"""Verify a relocated bundle with its own standard-library Python, no SDK/key."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def validate_live_cli_readiness(summary, provider_configured):
    if (summary.get("code") != "live_not_enabled" or summary.get("exit_code") != 3
            or summary.get("provider_configured") is not provider_configured
            or summary.get("live_enabled") is not False or summary.get("archive_written") is not False
            or any(summary.get(key) != 0 for key in ("model_requests", "numeric_exchanges", "executed_tools"))):
        raise RuntimeError("Packaged live CLI runtime/explicit-override readiness check did not pass.")


def verify(bundle, evidence):
    bundle = bundle.resolve(strict=True)
    evidence = evidence.resolve()
    if evidence.exists() or evidence == bundle or evidence.is_relative_to(bundle):
        raise ValueError("Use a new evidence directory outside the bundle.")
    evidence.mkdir(parents=True)
    report = {"status": "running", "clean_machine": False,
              "environment": "sanitized local Windows process, not a clean VM",
              "api_key_supplied": False, "steps": [],
              "started_utc": datetime.now(timezone.utc).isoformat()}
    try:
        report["bundle_manifest_sha256"] = digest(bundle / "files-sha256.json")
        report["app_sha256"] = digest(bundle / "bin/breakwater.exe")
        hashes = json.loads((bundle / "files-sha256.json").read_text(encoding="utf-8-sig"))
        def check_manifest():
            for relative, expected in hashes.items():
                path = (bundle / relative).resolve(strict=True)
                if not path.is_relative_to(bundle) or not path.is_file() or digest(path) != expected:
                    raise ValueError("Bundle file manifest did not match.")
        check_manifest()
        child_env = {name: os.environ[name] for name in ("SystemRoot", "WINDIR", "TEMP", "TMP", "USERPROFILE") if name in os.environ}
        windows = child_env.get("SystemRoot", "C:\\Windows")
        child_env.update(PATH=windows + "\\System32;" + windows,
                         QT_FORCE_STDERR_LOGGING="1", QSG_RHI_BACKEND="d3d11",
                         QSG_RHI_PREFER_SOFTWARE_RENDERER="1", PYTHONDONTWRITEBYTECODE="1",
                         PYTHONNOUSERSITE="1")
        def run(name, executable, arguments, *, expected_exit=0, environment=None):
            start = time.monotonic()
            completed = subprocess.run([str(executable), *map(str, arguments)], cwd=evidence,
                                       env=child_env if environment is None else environment, capture_output=True, timeout=180,
                                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            (evidence / (name + ".log")).write_bytes(completed.stdout)
            (evidence / (name + "-stderr.log")).write_bytes(completed.stderr)
            report["steps"].append({"name": name, "exit_code": completed.returncode,
                                    "expected_exit_code": expected_exit,
                                    "elapsed_s": time.monotonic() - start})
            if completed.returncode != expected_exit:
                raise RuntimeError(f"Bundle check {name} failed ({completed.returncode}); inspect its retained logs.")
            return completed
        python = bundle / "runtime/python/python.exe"
        run("isolated-python", python, ["-B", "-I", "-c",
            'import importlib.util,json,jsonschema,sys; assert sys.version_info[:3]==(3,12,13); assert importlib.util.find_spec("torch") is None; print(json.dumps({"python":"3.12.13","operations_ready":True,"torch_bundled":False}))'])
        app = bundle / "bin/breakwater.exe"
        run("sandbox-render", app, ["--smoke", "--screenshot", evidence / "sandbox.png", "--history-db", evidence / "sandbox.sqlite"])
        run("recorded-ui", app, ["--workspace", "workbench", "--ui-overhaul-check"])
        for demo in ("operations", "renamed-topology"):
            dataset = bundle / "share/breakwater/examples" / demo
            run(demo + "-render", app, ["--workspace", "eog", "--eog-dataset", dataset, "--smoke",
                "--screenshot", evidence / (demo + ".png"), "--eog-report", evidence / (demo + "-render.json"),
                "--history-db", evidence / (demo + ".sqlite")])
            run(demo + "-offline", bundle / "bin/breakwater_offline_check.exe", [dataset, evidence])
            result = json.loads((evidence / (demo + "-offline.log")).read_text(encoding="utf-8"))
            if result.get("model_requests") != 0 or not result.get("history_reopened_nonactionable"):
                raise RuntimeError("Offline authority/model-request check did not pass.")
        # No --live flag and no key: verify the installed runner's runtime setup
        # without starting a provider/model operation or restoring any authority.
        live_cli = bundle / "bin/breakwater_eog_live_check.exe"
        dataset = bundle / "share/breakwater/examples/operations"
        for name, environment, configured in (
                ("live-cli-readiness", child_env, True),
                ("live-cli-explicit-override", dict(child_env, BREAKWATER_PYTHON=str(evidence / "missing-python.exe")), False)):
            archive = evidence / (name + ".eog-session.json")
            completed = run(name, live_cli, ["--dataset", dataset, "--archive", archive,
                "--db", evidence / (name + ".sqlite")], expected_exit=3, environment=environment)
            validate_live_cli_readiness(json.loads(completed.stdout), configured)
            if archive.exists():
                raise RuntimeError("Non-live readiness check unexpectedly wrote an investigation archive.")
        check_manifest()
        report.update(status="passed", inputs="bundle manifest unchanged", hosted_model_requests=0)
        print("Sanitized relocation, rendering, local search, independent validation and historical reopening passed.")
        print("A clean Windows account/VM remains a separate release gate.")
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        (evidence / "verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("evidence", type=Path)
    options = parser.parse_args()
    try:
        verify(options.bundle, options.evidence)
    except Exception as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
