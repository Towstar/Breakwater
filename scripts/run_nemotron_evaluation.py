"""Run one declared, bounded Nemotron evaluation round; never apply controls.

Default execution performs native preflight only. --live explicitly enables the
production hosted CLIs. GUI observations must be witnessed and attached later.
Journal files and case results are exclusive, append-only artifacts, not signed
execution attestations. No resume, automatic retry or replacement is supported.
"""
from __future__ import annotations

import argparse
from collections import Counter
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import signal
import stat
import subprocess
import threading
import time
import uuid

from nemotron_evaluation_fixtures import generate_fixtures
from nemotron_evaluation_audit import audit_case, retained_tool_metrics
from audit_candidate_evidence import audit_observed
from verify_v03_evidence import inspect, transport_diagnostics_valid


ROOT = Path(__file__).resolve().parents[1]
FORMAT = "breakwater-nemotron-evaluation-v1"
MODEL = "nvidia/nemotron-3.5-lightning-30b-a3b"
ENDPOINT = "https://integrate.api.nvidia.com/v1/chat/completions"
SEMANTIC = {"advisory_plan", "no_supported_plan", "missing_coverage", "stale_coverage"}
MAX_PROCESS_SECONDS, DEFAULT_ROUND_SECONDS, MAX_ROUND_SECONDS, MAX_CLI_CASES = 450, 1800, 7200, 32
DEFAULT_CASE_GAP_SECONDS = 30
STDOUT_LIMIT, STDERR_LIMIT, ARTIFACT_LIMIT = 8 * 1024 * 1024, 1024 * 1024, 64 * 1024 * 1024
OBSERVED = {"observed-pump": False, "observed-missing": True}
OBSERVED_METRICS = {"logical_requests", "model_requests", "completed_model_requests", "http_attempts", "http_attempts_basis",
                    "request_counts_complete", "retries", "retry_count", "total_tool_calls", "tool_calls", "duplicate_calls",
                    "read_repeats", "duplicate_expensive_calls", "duplicate_expensive_executions", "unknown_argument_calls",
                    "duplicate_counts_complete", "successful_tools", "validation_reuse", "actual_tool_executions", "model_duration_ms",
                    "dispatched_tool_calls", "predispatch_rejections", "stored_runs", "selected_cost", "provider_exchanges"}
ENVIRONMENT = ("SYSTEMROOT", "WINDIR", "COMSPEC", "TEMP", "TMP", "USERPROFILE", "LOCALAPPDATA", "APPDATA",
               "PROGRAMDATA", "PATHEXT", "PATH", "PROCESSOR_ARCHITECTURE", "NUMBER_OF_PROCESSORS",
               "BREAKWATER_PYTHON", "BREAKWATER_EOG_WORKER", "BREAKWATER_EOG_CONTRACTS", "BREAKWATER_EOG_NATIVE",
               "BREAKWATER_NEMOTRON_INTERVAL_MS")
METRICS = ("logical_requests", "http_attempts", "retries", "total_tool_calls", "duplicate_calls", "successful_tools",
           "validation_reuse", "stored_runs", "model_duration_ms", "read_repeats", "duplicate_expensive_calls",
           "duplicate_expensive_executions", "actual_tool_executions", "provider_exchanges", "predispatch_rejections",
           "dispatched_tool_calls", "unknown_argument_calls")


class EvaluationError(ValueError):
    pass


class ArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        raise EvaluationError("Invalid command arguments.")


def require(condition, message):
    if not condition:
        raise EvaluationError(message)


def utc():
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def plain(path, *, directory=False):
    path = Path(path).absolute()
    require(".." not in path.parts and all(":" not in part for part in path.parts[1:]), "Path traversal and alternate streams are unsupported.")
    for item in (path, *path.parents):
        info = item.lstat()
        require(not stat.S_ISLNK(info.st_mode) and not (getattr(info, "st_file_attributes", 0) & 0x400),
                "Symbolic links and reparse points are unsupported.")
    require(path.is_dir() if directory else path.is_file(), "Expected a plain directory or regular file.")
    return path


def relative(root, name):
    require(isinstance(name, str), "Artifact path must be text.")
    parts = PurePosixPath(name)
    require(not parts.is_absolute() and not PureWindowsPath(name).drive and ":" not in name and "\\" not in name and parts.parts and
            all(part not in (".", "..") for part in parts.parts), "Artifact path escapes its round.")
    path = root.joinpath(*parts.parts)
    require(path.is_relative_to(root), "Artifact path escapes its round.")
    return path


def bytes_at(path, limit=ARTIFACT_LIMIT):
    path = plain(path)
    before = path.stat()
    require(0 <= before.st_size <= limit, "Artifact exceeds its byte bound.")
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    after = path.stat()
    require(len(raw) == before.st_size == after.st_size and before.st_mtime_ns == after.st_mtime_ns,
            "Artifact changed during inspection.")
    return raw


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON key.")
        result[key] = value
    return result


def document(raw):
    try:
        value = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=unique,
                           parse_constant=lambda _: (_ for _ in ()).throw(EvaluationError("Nonfinite JSON constant.")))
        require(isinstance(value, dict), "Expected a JSON object.")
        inspect(value)
        return value
    except (UnicodeError, json.JSONDecodeError, RecursionError, OverflowError) as error:
        raise EvaluationError("Invalid bounded JSON artifact.") from error


def encoded(value):
    inspect(value)
    return (json.dumps(value, ensure_ascii=False, allow_nan=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def write_new(path, raw):
    plain(path.parent, directory=True)
    require(not os.path.lexists(path), "Artifact already exists; replacement is unsupported.")
    with path.open("xb") as stream:
        stream.write(raw)
        stream.flush()
        os.fsync(stream.fileno())


def reference(root, path):
    raw = bytes_at(path)
    return {"path": path.relative_to(root).as_posix(), "bytes": len(raw), "sha256": digest(raw)}


def read_reference(root, entry):
    require(isinstance(entry, dict) and set(entry) == {"path", "bytes", "sha256"}, "Malformed artifact reference.")
    raw = bytes_at(relative(root, entry["path"]))
    require(type(entry["bytes"]) is int and len(raw) == entry["bytes"] and digest(raw) == entry["sha256"], "Artifact hash or size differs.")
    return raw


def source_identity():
    # Git is read-only. Diff bytes are hashed in memory, never logged.
    def git(*args):
        result = subprocess.run(["git", "-C", str(ROOT), *args], stdin=subprocess.DEVNULL, capture_output=True,
                                timeout=30, check=False, env=child_environment(os.environ, live=False))
        require(result.returncode == 0 and len(result.stdout) <= ARTIFACT_LIMIT, "Source identity is unavailable.")
        return result.stdout
    head = git("rev-parse", "HEAD").decode("ascii").strip()
    tree = git("rev-parse", "HEAD^{tree}").decode("ascii").strip()
    require(re.fullmatch(r"[0-9a-f]{40}", head) and re.fullmatch(r"[0-9a-f]{40}", tree), "Invalid source identity.")
    status = git("status", "--porcelain=v1", "--untracked-files=all")
    worktree = hashlib.sha256(git("diff", "--binary", "HEAD"))
    for name in git("ls-files", "--others", "--exclude-standard", "-z").split(b"\0"):
        if name:
            path = relative(ROOT, os.fsdecode(name).replace("\\", "/"))
            worktree.update(name + b"\0" + bytes.fromhex(digest(bytes_at(path))))
    return {"head": head, "tree": tree, "dirty": bool(status), "status_sha256": digest(status), "worktree_sha256": worktree.hexdigest()}


def executable_identity(path):
    path = plain(path)
    raw = bytes_at(path, 512 * 1024 * 1024)
    require(raw, "Executable is empty.")
    return {"path": str(path), "sha256": digest(raw), "bytes": len(raw)}


def child_environment(environment, *, live):
    # Do not even read credential/model configuration during native preflight.
    result = {name: environment[name] for name in ENVIRONMENT if name in environment}
    result.update(PYTHONUTF8="1", PYTHONDONTWRITEBYTECODE="1", PYTHONNOUSERSITE="1")
    if live:
        require(environment.get("NVIDIA_BASE_URL", ENDPOINT) in (ENDPOINT, "https://integrate.api.nvidia.com/v1"),
                "Live evaluation requires the official NVIDIA endpoint.")
        require(environment.get("NEMOTRON_MODEL", MODEL) == MODEL, "Live evaluation requires the declared model.")
        key = environment.get("NVIDIA_API_KEY", "")
        require(isinstance(key, str) and 0 < len(key) <= 4096 and "\r" not in key and "\n" not in key,
                "Live evaluation requires a process-supplied NVIDIA key.")
        result.update(NVIDIA_API_KEY=key, NVIDIA_BASE_URL=ENDPOINT, NEMOTRON_MODEL=MODEL)
    return result


def terminate_owned(process):
    if process.poll() is not None:
        return
    if os.name == "nt":
        result = subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], stdin=subprocess.DEVNULL,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15, check=False,
                                creationflags=subprocess.CREATE_NO_WINDOW, env=child_environment(os.environ, live=False))
        if result.returncode and process.poll() is None:
            process.kill()
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass


def run_helper(arguments, folder, environment, timeout):
    started = time.monotonic()
    require(0 < timeout <= MAX_PROCESS_SECONDS, "Invalid helper deadline.")
    result = {"exit_code": None, "failure": None, "duration_s": 0.0, "stdout": b"", "stderr": b""}
    options = {"creationflags": subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == "nt" else {"start_new_session": True}
    try:
        process = subprocess.Popen([str(value) for value in arguments], cwd=folder, env=environment,
                                   stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False, **options)
    except OSError:
        result.update(failure="helper_launch_failed", duration_s=time.monotonic() - started)
        return result
    exceeded = threading.Event()
    captures = {}
    def collect(name, stream, limit):
        data, count, checksum = bytearray(), 0, hashlib.sha256()
        try:
            while block := stream.read(65536):
                count += len(block); checksum.update(block)
                data.extend(block[:max(0, limit - len(data))])
                if count > limit:
                    exceeded.set()
        except OSError:
            exceeded.set()
        captures[name] = {"raw": bytes(data), "total_bytes": count, "sha256": checksum.hexdigest()}
    readers = [threading.Thread(target=collect, args=(name, getattr(process, name), limit), daemon=True)
               for name, limit in (("stdout", STDOUT_LIMIT), ("stderr", STDERR_LIMIT))]
    for reader in readers:
        reader.start()
    try:
        while process.poll() is None:
            if exceeded.is_set() or time.monotonic() - started >= timeout:
                result["failure"] = "output_limit" if exceeded.is_set() else "helper_timeout"
                terminate_owned(process)
                break
            try:
                process.wait(timeout=min(0.05, max(0.001, timeout - (time.monotonic() - started))))
            except subprocess.TimeoutExpired:
                pass
        process.wait(timeout=15)
    except KeyboardInterrupt:
        result["failure"] = "interrupted"; terminate_owned(process); process.wait(timeout=15)
    except (OSError, subprocess.TimeoutExpired):
        result["failure"] = "helper_termination_failed"; terminate_owned(process)
    finally:
        for reader in readers:
            reader.join(timeout=2)
        for name in ("stdout", "stderr"):
            captured = captures.get(name)
            if captured is None:
                result["failure"] = "helper_output_incomplete"
                captured = {"raw": b"", "total_bytes": 0, "sha256": digest(b"")}
            result[name] = captured["raw"]
            result[name + "_total_bytes"] = captured["total_bytes"]
            result[name + "_sha256"] = captured["sha256"]
        if exceeded.is_set() and not result["failure"]:
            result["failure"] = "output_limit"
        result.update(exit_code=process.poll(), duration_s=time.monotonic() - started)
    return result


def journal(root, manifest):
    sequence = manifest.get("journal_sequence", 0) + 1
    previous = root / f"manifest-{sequence - 1:04d}.json"
    manifest["previous_manifest"] = reference(root, previous) if sequence > 1 else None
    manifest["journal_sequence"] = sequence; manifest["updated_utc"] = utc()
    path = root / f"manifest-{sequence:04d}.json"
    write_new(path, encoded(manifest))
    return path


def delay_policy(gap):
    require(type(gap) is int and 0 <= gap <= 60, "Case gap must be an integer from zero to sixty seconds.")
    return {"kind": "hosted_completion_to_next_start", "gap_seconds": gap, "first_hosted_gap_seconds": 0,
            "preflight_gap_seconds": 0, "included_in_round_deadline": True}


def provider_deferral_policy():
    return {"version": 1, "basis": "latest_request_remaining_cooldown", "capped_wait": "stop",
            "missing_diagnostics": "stop", "included_in_round_deadline": True}


def provider_cooldown(root, result):
    """Latest logical request only; earlier cooldowns may already have elapsed."""
    if "archive" not in result:
        return None
    archive = document(read_reference(root, result["archive"]))
    if result["mode"] == "sandbox":
        requests = [row["request"] for row in archive.get("events", []) if row.get("type") == "model_request_completed"]
    else:
        requests = [row["event"] for row in archive.get("agent_events", []) if row.get("event", {}).get("type") == "model_request"]
    if not requests or "transport_diagnostics" not in requests[-1]:
        return None
    diagnostic = requests[-1]["transport_diagnostics"]
    require(transport_diagnostics_valid(diagnostic), "Provider cooldown diagnostics are invalid.")
    return diagnostic["cooldown_remaining_ms"], diagnostic["cooldown_capped"]


def validate_provider_following_delays(root, manifest, result):
    cooldown = provider_cooldown(root, result)
    following = [row for row in manifest["wait_records"] if
                 row["kind"] == "hosted_delay_started" and row["previous_case_id"] == result["case_id"]]
    if cooldown is None or cooldown[1]:
        require(not following, "A case started after unknown or capped provider cooldown.")
        return False
    for row in following:
        require(row["provider_not_before_elapsed_s"] == result["completed_elapsed_s"] + cooldown[0] / 1000,
                "Provider eligibility differs from retained request diagnostics.")
    return True


def validate_delay_history(manifest):
    provider = manifest.get("provider_deferral_policy")
    if provider is not None:
        expected = provider_deferral_policy()
        require(isinstance(provider, dict) and set(provider) == set(expected) and
                all(type(provider[key]) is type(value) and provider[key] == value for key, value in expected.items()),
                "Provider deferral policy differs.")
    policy = manifest.get("delay_policy")
    if policy is None:
        require("delay_policy" not in manifest and "wait_records" not in manifest, "Legacy delay policy differs.")
        return {}
    require(isinstance(policy, dict), "Delay policy differs.")
    expected = delay_policy(policy.get("gap_seconds"))
    require(set(policy) == set(expected) and all(type(policy[key]) is type(value) and policy[key] == value for key, value in expected.items()),
            "Delay policy differs.")
    records = manifest.get("wait_records")
    cases = [case for case in manifest["cases"] if case["mode"] != "observed"]
    require(isinstance(records, list) and len(records) <= 2 * len(cases), "Invalid delay records.")
    completed, pending = {}, None
    start_keys = {"kind", "case_id", "previous_case_id", "previous_completed_elapsed_s", "started_elapsed_s", "eligible_elapsed_s", "planned_delay_s"}
    if provider is not None:
        start_keys.add("provider_not_before_elapsed_s")
    finish_keys = {"kind", "case_id", "finished_elapsed_s", "delayed_s", "status"}
    def number(value):
        return type(value) in (int, float) and math.isfinite(value) and value >= 0
    for record in records:
        require(isinstance(record, dict), "Malformed delay record.")
        if record.get("kind") == "hosted_delay_started":
            index = len(completed)
            require(pending is None and index < len(cases) and set(record) == start_keys and record["case_id"] == cases[index]["case_id"],
                    "Delay order differs from declared cases.")
            started, eligible, planned = (record[key] for key in ("started_elapsed_s", "eligible_elapsed_s", "planned_delay_s"))
            require(all(number(value) for value in (started, eligible, planned)), "Invalid delay timing.")
            previous = record["previous_completed_elapsed_s"]
            provider_eligible = record.get("provider_not_before_elapsed_s", 0)
            require(number(provider_eligible), "Invalid provider eligibility timing.")
            if index == 0:
                require(record["previous_case_id"] is None and previous is None and eligible == started and planned == 0 and provider_eligible == 0,
                        "First hosted case gained an undeclared delay.")
            else:
                previous_id = cases[index - 1]["case_id"]
                require(record["previous_case_id"] == previous_id and number(previous) and
                        cases[index - 1]["attempted"] is True and
                        completed[previous_id]["finish"]["status"] == "completed" and
                        previous >= completed[previous_id]["finish"]["finished_elapsed_s"] and started >= previous and
                        eligible == max(previous + policy["gap_seconds"], provider_eligible) and planned == max(0, eligible - started),
                        "Completion-to-next-start delay differs from policy.")
            pending = record
        else:
            require(pending is not None and set(record) == finish_keys and record.get("kind") == "hosted_delay_finished" and
                    record["case_id"] == pending["case_id"] and record["status"] in {"completed", "round_deadline", "interrupted"},
                    "Delay completion has no matching start.")
            finished, delayed = record["finished_elapsed_s"], record["delayed_s"]
            require(number(finished) and number(delayed) and finished >= pending["started_elapsed_s"] and
                    delayed == finished - pending["started_elapsed_s"], "Delay duration differs.")
            if record["status"] == "completed":
                require(finished >= pending["eligible_elapsed_s"] and finished < manifest["limits"]["round_seconds"],
                        "Hosted delay completed before eligibility or after the round deadline.")
            elif record["status"] == "round_deadline":
                require(finished >= manifest["limits"]["round_seconds"], "Delay deadline was not reached.")
            completed[pending["case_id"]] = {"policy": policy, "start": pending, "finish": record}
            pending = None
    for case in cases:
        if case["attempted"]:
            require(case["case_id"] in completed and completed[case["case_id"]]["finish"]["status"] == "completed",
                    "Attempted case has no completed declared delay.")
    return completed


def wait_for_hosted_case(root, manifest, case_id, previous, round_started, deadline, provider_not_before=0):
    started = time.monotonic()
    previous_id, previous_completed = previous if previous is not None else (None, None)
    started_elapsed = started - round_started
    eligible_elapsed = started_elapsed if previous is None else max(previous_completed + manifest["delay_policy"]["gap_seconds"], provider_not_before)
    begin = {"kind": "hosted_delay_started", "case_id": case_id, "previous_case_id": previous_id,
             "previous_completed_elapsed_s": previous_completed, "started_elapsed_s": started_elapsed,
             "eligible_elapsed_s": eligible_elapsed, "planned_delay_s": max(0, eligible_elapsed - started_elapsed)}
    if "provider_deferral_policy" in manifest:
        begin["provider_not_before_elapsed_s"] = provider_not_before
    manifest["wait_records"].append(begin)
    status = "completed"
    try:
        if begin["planned_delay_s"] > 0:
            journal(root, manifest)  # Preserve the declared wait even if interrupted.
        while True:
            now = time.monotonic()
            if now >= deadline:
                status = "round_deadline"; break
            if now - round_started >= eligible_elapsed:
                break
            time.sleep(min(0.25, eligible_elapsed - (now - round_started), deadline - now))
    except KeyboardInterrupt:
        status = "interrupted"
    finished = time.monotonic()
    if status == "completed" and finished >= deadline:
        status = "round_deadline"
    end = {"kind": "hosted_delay_finished", "case_id": case_id, "finished_elapsed_s": finished - round_started,
           "delayed_s": (finished - round_started) - started_elapsed, "status": status}
    manifest["wait_records"].append(end)
    if status != "completed":
        manifest["stop_reason"] = status
        return None
    return {"policy": manifest["delay_policy"], "start": begin, "finish": end}


def load_round(directory):
    root = plain(directory, directory=True)
    paths = sorted(root.glob("manifest-[0-9][0-9][0-9][0-9].json"))
    require(paths, "Round has no journal.")
    previous, prior_value = None, None
    for sequence, path in enumerate(paths, 1):
        require(path.name == f"manifest-{sequence:04d}.json", "Journal sequence has a gap.")
        value = document(bytes_at(path))
        require(value.get("format") == FORMAT and value.get("journal_sequence") == sequence and
                value.get("previous_manifest") == previous, "Journal chain differs.")
        validate_delay_history(value)
        if prior_value is not None:
            require(value.get("delay_policy") == prior_value.get("delay_policy") and
                    value.get("provider_deferral_policy") == prior_value.get("provider_deferral_policy") and
                    value.get("wait_records", [])[:len(prior_value.get("wait_records", []))] == prior_value.get("wait_records", []),
                    "Delay policy or earlier wait records changed.")
            for group in ("cases", "preflights"):
                require(len(value[group]) == len(prior_value[group]), "Declared case membership changed.")
                for before, after in zip(prior_value[group], value[group], strict=True):
                    key = "case_id" if group == "cases" else "input_id"
                    require(before[key] == after[key] and (not before["attempted"] or after["attempted"]), "An attempted case was hidden.")
                    if "result" in before:
                        require(after.get("result") == before["result"] and after["status"] == before["status"], "A completed result was changed or hidden.")
        previous = reference(root, path)
        prior_value = value
    require(len(value.get("cases", [])) == 24 and len({case["case_id"] for case in value["cases"]}) == 24,
            "Round declaration differs.")
    return root, value


def verify_fixtures(root, manifest):
    fixture = document(read_reference(root, manifest["fixture_manifest"]))
    catalog = document(read_reference(root, manifest["case_catalog"]))
    require(len(catalog["cases"]) == 22 <= MAX_CLI_CASES and fixture["case_count"] == 22, "CLI case declaration differs.")
    expected = {case["case_id"]: case for case in catalog["cases"]}
    declarations = {case["case_id"]: case for case in manifest["cases"] if case["mode"] != "observed"}
    require(set(expected) == set(declarations), "CLI case membership changed.")
    for identity, case in expected.items():
        require(declarations[identity]["definition"] == case, "Declared objective, budget or input changed.")
    require({case["case_id"] for case in manifest["cases"] if case["mode"] == "observed"} == set(OBSERVED), "Observed declarations differ.")
    for name, info in fixture["files"].items():
        raw = bytes_at(relative(root / "fixtures", name))
        require(len(raw) == info["size_bytes"] and digest(raw) == info["sha256"], "Fixture input hash differs.")
    return fixture, catalog


def check_binding(manifest, *, live):
    current = source_identity()
    require(current == manifest["source"], "Source changed during the evaluation round.")
    if live:
        require(not current["dirty"] and current["head"] == manifest["expected_source"], "Hosted evidence requires the exact clean source.")
    for identity in manifest["executables"].values():
        require(executable_identity(identity["path"]) == identity, "An executable changed during the round.")


def input_binding(root, definition, archive):
    if definition["mode"] == "sandbox":
        project = document(bytes_at(relative(root / "fixtures", definition["path"])))
        require(archive["snapshot"]["network"] == project["network"] and archive["snapshot"]["horizon_h"] == definition["horizon_h"],
                "Sandbox evidence differs from its declared input or horizon.")
    else:
        capture = archive["capture_manifest"]
        hashes = {PurePosixPath(name).name: value["sha256"] for name, value in definition["input_files"].items()}
        require(capture["source_hashes"] == hashes and capture["cutoff_unix"] == definition["cutoff_unix"],
                "Operations evidence differs from its declared input or cutoff.")


def unsafe_output(raw, environment):
    text = raw.decode("utf-8", errors="replace")
    if re.search(r"nvapi-[A-Za-z0-9_-]{16,}|Bearer\s+[A-Za-z0-9_-]{16,}|</?think>|reasoning_content|raw_reasoning", text, re.I):
        return True
    key = environment.get("NVIDIA_API_KEY")
    return bool(key and key in text)


def horizon_binding(mode, archive, summary):
    if mode == "sandbox":
        return {"mode": mode, "horizon_h": archive["snapshot"]["horizon_h"]}
    selected = summary.get("selected_plan_id")
    plans = []
    for analysis in archive.get("analyses", []):
        response = analysis.get("response", {}).get("result") or {}
        evaluations = list(response.get("evaluations", []))
        if response.get("evaluation"):
            evaluations.append(response["evaluation"])
        plans.extend(evaluation.get("plan", {}) for evaluation in evaluations)
    chosen = [plan for plan in plans if selected and plan.get("plan_id") == selected and isinstance(plan.get("repair_deadlines"), list)]
    if not chosen:
        return None
    plan = chosen[-1]
    return {"mode": mode, "start_unix": plan.get("start_unix"), "context": plan.get("context"),
            "repair_deadlines": sorted(plan["repair_deadlines"], key=lambda item: item["facility_id"])}


def attempt(root, definition, folder, executable, environment, timeout, *, hosted, binding, cadence=None, round_started=None):
    folder.mkdir(exist_ok=False)
    started = utc()
    require(executable_identity(executable) == binding["executable"], "Executable changed before helper launch.")
    source = relative(root / "fixtures", definition["path"])
    mode = definition["mode"]
    archive = folder / ("evidence.session.json" if mode == "sandbox" else "evidence.eog-session.json")
    if mode == "eog" and not hosted:
        arguments = [executable, str(source), str(folder), "--preflight"]
    else:
        arguments = [executable, "--project" if mode == "sandbox" else "--dataset", str(source), "--archive", str(archive),
                     "--db", str(folder / "history.sqlite"), "--objective", definition["objective"], "--live" if hosted else "--local"]
        if mode == "eog":
            arguments.extend(["--cutoff", str(definition["cutoff_unix"])])
    helper_started = time.monotonic()
    process = run_helper(arguments, folder, environment, timeout)
    result = {"case_id": definition["case_id"], "mode": mode, "hosted": hosted, "objective": definition["objective"],
              "input_files": definition["input_files"], "expected": definition["expected"], "started_utc": started,
              "binding": binding,
              "process": {key: value for key, value in process.items() if key not in ("stdout", "stderr")},
              "arguments": arguments, "status": "failed", "audit": {"passed": False, "classification": process["failure"] or "incomplete_execution"}}
    summary = None
    try:
        require(not unsafe_output(process["stdout"], environment) and not unsafe_output(process["stderr"], environment), "Unsafe helper output was withheld.")
        summary = document(process["stdout"])
    except (ValueError, KeyError, TypeError):
        result["audit"] = {"passed": False, "classification": process["failure"] or "unsafe_or_malformed_output"}
        result["raw_output_retained"] = False
        for name in ("stdout", "stderr"):
            raw = process[name]
            result["process"].setdefault(name + "_total_bytes", len(raw))
            result["process"].setdefault(name + "_sha256", digest(raw))
    else:
        result["raw_output_retained"] = True
        for name, suffix in (("stdout", ".json"), ("stderr", ".log")):
            path = folder / (name + suffix)
            write_new(path, process[name]); result[name] = reference(root, path)
    if summary is not None and result["raw_output_retained"] and not process["failure"]:
        try:
            if mode == "eog" and not hosted:
                archive = plain(summary["archive_path"])
                require(archive.is_relative_to(folder), "Preflight archive escapes its attempt directory.")
            if archive.exists():
                result["archive"] = reference(root, archive)
                value = document(read_reference(root, result["archive"]))
                input_binding(root, definition, value)
                audited = audit_case(mode, value, summary, definition["expected"], hosted=hosted)
                require(isinstance(audited, dict) and type(audited.get("passed")) is bool, "Auditor returned no supported verdict.")
                result["audit"] = audited
                result["horizon_binding"] = horizon_binding(mode, value, summary)
                exits = {0, 3} if mode == "sandbox" or hosted else {0}
                if process["exit_code"] not in exits or (hosted and summary.get("exit_code") != process["exit_code"]):
                    result["audit"] = {**audited, "passed": False, "classification": "process_exit_mismatch"}
                if not hosted and (audited.get("classification") not in SEMANTIC):
                    result["audit"] = {**audited, "passed": False, "classification": "unqualified_preflight"}
            else:
                result["audit"] = {"passed": False, "classification": "missing_archive"}
        except (ValueError, KeyError, TypeError, OSError):
            result["audit"] = {"passed": False, "classification": "evidence_audit_rejected"}
    result["status"] = "passed" if result["audit"].get("passed") is True else "failed"
    partial = sorted(path for path in folder.rglob("*") if path.is_file())
    require(len(partial) <= 1024, "Attempt produced too many artifacts.")
    result["partial_artifacts"] = [reference(root, path) for path in partial]
    result["finished_utc"] = utc()
    if cadence is not None:
        result["cadence"] = cadence
        result["helper_started_elapsed_s"] = helper_started - round_started
        result["completed_elapsed_s"] = time.monotonic() - round_started
    path = folder / "result.json"; write_new(path, encoded(result))
    return result, reference(root, path)


def run_round(args):
    round_started = time.monotonic()
    deadline = round_started + args.duration_seconds
    policy = delay_policy(args.case_gap_seconds)
    require(type(args.duration_seconds) is int and 0 < args.duration_seconds <= MAX_ROUND_SECONDS, "Round duration exceeds its bound.")
    require(args.expected_source is None or re.fullmatch(r"[0-9a-f]{40}", args.expected_source), "Expected source must be a full commit ID.")
    output = Path(args.output).absolute()
    require(".." not in output.parts and not os.path.lexists(output), "Choose a new output directory; resume and replacement are unsupported.")
    plain(output.parent, directory=True)
    source = source_identity()
    if args.live:
        require(re.fullmatch(r"[0-9a-f]{40}", args.expected_source or "") and source["head"] == args.expected_source and not source["dirty"],
                "--live requires --expected-source at the exact clean HEAD.")
    executables = {name: executable_identity(getattr(args, name + "_executable")) for name in ("sandbox", "eog", "offline", "desktop")}
    # Credentials are checked only after explicit live authority and clean-source admission.
    live_environment = child_environment(os.environ, live=True) if args.live else None
    offline_environment = child_environment(os.environ, live=False)
    output.mkdir(exist_ok=False)
    generate_fixtures(output / "fixtures")
    catalog = document(bytes_at(output / "fixtures/cases.json"))
    cases = catalog["cases"]
    require(len(cases) == 22 <= MAX_CLI_CASES, "Unexpected fixture case count.")
    manifest = {"format": FORMAT, "round_id": str(uuid.uuid4()), "started_utc": utc(), "stage": "preflight", "live_enabled": args.live,
                "source": source, "expected_source": args.expected_source, "executables": executables,
                "model": {"identifier": MODEL, "endpoint": ENDPOINT}, "binding_kind": "Recorded source and executable hashes; not signed build or execution attestation",
                "delay_policy": policy, "wait_records": [], "provider_deferral_policy": provider_deferral_policy(),
                "limits": {"process_seconds": MAX_PROCESS_SECONDS, "round_seconds": args.duration_seconds, "maximum_cli_cases": MAX_CLI_CASES,
                           "stdout_bytes": STDOUT_LIMIT, "stderr_bytes": STDERR_LIMIT},
                "fixture_manifest": reference(output, output / "fixtures/manifest.json"), "case_catalog": reference(output, output / "fixtures/cases.json"),
                "cases": [{"case_id": case["case_id"], "mode": case["mode"], "definition": case, "status": "pending", "attempted": False} for case in cases] +
                         [{"case_id": name, "mode": "observed", "status": "pending", "attempted": False} for name in OBSERVED],
                "preflights": [{"input_id": name, "status": "pending", "attempted": False} for name in dict.fromkeys(case["input_id"] for case in cases)],
                "public_release_approved": False}
    journal(output, manifest)
    for name in ("preflight", "hosted", "observed"):
        (output / name).mkdir()
    by_input = {case["input_id"]: case for case in reversed(cases)}
    try:
        for index, preflight in enumerate(manifest["preflights"], 1):
            if time.monotonic() >= deadline:
                manifest["stop_reason"] = "round_deadline"; break
            check_binding(manifest, live=False)
            verify_fixtures(output, manifest)
            if time.monotonic() >= deadline:
                manifest["stop_reason"] = "round_deadline"; break
            definition = {**by_input[preflight["input_id"]], "expected": by_input[preflight["input_id"]]["intended_outcome"]}
            preflight.update(status="running", attempted=True); journal(output, manifest)
            executable = executables["sandbox" if definition["mode"] == "sandbox" else "offline"]["path"]
            result, ref = attempt(output, definition, output / "preflight" / f"p{index:02d}", executable, offline_environment,
                                  min(MAX_PROCESS_SECONDS, max(0.001, deadline - time.monotonic())), hosted=False,
                                  binding={"source": source, "executable": executables["sandbox" if definition["mode"] == "sandbox" else "offline"]})
            preflight.update(status=result["status"], result=ref)
            if result["status"] == "passed":
                preflight["qualified_expected"] = result["audit"]["classification"]
            journal(output, manifest)
            check_binding(manifest, live=False); verify_fixtures(output, manifest)
            if result["process"]["failure"] == "interrupted":
                manifest["stop_reason"] = "interrupted"; break
        qualified = all(item["status"] == "passed" for item in manifest["preflights"])
        if not qualified or not args.live:
            check_binding(manifest, live=False); verify_fixtures(output, manifest)
            manifest["stage"] = "preflight_complete" if qualified else "incomplete"
            journal(output, manifest)
            return (0 if qualified else 2), manifest
        expectations = {item["input_id"]: item["qualified_expected"] for item in manifest["preflights"]}
        manifest["stage"] = "hosted"; journal(output, manifest)
        previous_completed = None
        provider_not_before = 0
        for index, case in enumerate(manifest["cases"], 1):
            if case["mode"] == "observed":
                continue
            if time.monotonic() >= deadline:
                manifest["stop_reason"] = "round_deadline"; break
            check_binding(manifest, live=True); verify_fixtures(output, manifest)
            if provider_not_before >= manifest["limits"]["round_seconds"]:
                manifest["stop_reason"] = "provider_cooldown_exceeds_round"; break
            cadence = wait_for_hosted_case(output, manifest, case["case_id"], previous_completed, round_started, deadline, provider_not_before)
            if cadence is None:
                break
            check_binding(manifest, live=True); verify_fixtures(output, manifest)
            if time.monotonic() >= deadline:
                manifest["stop_reason"] = "round_deadline"; break
            definition = {**case["definition"], "expected": expectations[case["definition"]["input_id"]]}
            case.update(status="running", attempted=True); journal(output, manifest)
            result, ref = attempt(output, definition, output / "hosted" / f"c{index:02d}", executables[case["mode"]]["path"], live_environment,
                                  min(MAX_PROCESS_SECONDS, max(0.001, deadline - time.monotonic())), hosted=True,
                                  binding={"source": source, "executable": executables[case["mode"]]}, cadence=cadence, round_started=round_started)
            case.update(status=result["status"], result=ref); journal(output, manifest)
            previous_completed = (case["case_id"], result["completed_elapsed_s"])
            cooldown = provider_cooldown(output, result)
            if cooldown is None:
                manifest["stop_reason"] = "provider_cooldown_unavailable"; break
            if cooldown[1]:
                manifest["stop_reason"] = "provider_cooldown_exceeds_diagnostic_bound"; break
            provider_not_before = result["completed_elapsed_s"] + cooldown[0] / 1000
            check_binding(manifest, live=True); verify_fixtures(output, manifest)
            if result["process"]["failure"] == "interrupted":
                manifest["stop_reason"] = "interrupted"; break
        passed = all(case["status"] == "passed" for case in manifest["cases"] if case["mode"] != "observed")
        check_binding(manifest, live=True); verify_fixtures(output, manifest)
        manifest["stage"] = "awaiting_observed" if passed else "incomplete"
        journal(output, manifest)
        return (0 if passed else 2), manifest
    except (ValueError, KeyError, TypeError, OSError):
        manifest.update(stage="invalid", stop_reason="round_integrity_or_binding_failure"); journal(output, manifest)
        return 1, manifest


def observed_metrics(value, audited):
    events = value.get("events", [])
    calls = [event for event in events if event.get("type") == "tool_requested"]
    starts = [event for event in events if event.get("type") == "tool_started"]
    results = [event for event in events if event.get("type") == "tool_completed"]
    requests = [event["request"] for event in events if event.get("type") == "model_request_completed"]
    model_starts = sum(event.get("type") == "model_request_started" for event in events)
    known_calls = len(calls) == audited["tool_calls"]
    by_id = {event["tool_call_id"]: event for event in starts}
    prepared = []
    for call in calls:
        start = by_id.get(call["tool_call_id"], {})
        arguments = start.get("input_summary")
        prepared_call = {"tool_name": call["tool_name"], "tool_call_id": call["tool_call_id"]}
        if start.get("tool_name") == call.get("tool_name") and isinstance(arguments, dict):
            prepared_call["arguments"] = arguments
        prepared.append(prepared_call)  # Unknown arguments are omitted, never replaced with {}.
    metrics = retained_tool_metrics(requests, prepared, results, logical=model_starts, name="tool_name", identity="tool_call_id")
    request_ids = [request.get("model_request_id") for request in requests]
    metrics["duplicate_counts_complete"] &= known_calls
    metrics["request_counts_complete"] &= len(requests) == audited["model_requests"] and all(
        isinstance(identity, str) and identity for identity in request_ids) and len(set(request_ids)) == len(request_ids)
    recommendation = value.get("recommendation", {})
    metrics.update(dispatched_tool_calls=len(starts), stored_runs=len(value.get("runs", [])), provider_exchanges=0,
                   selected_cost=recommendation.get("verified_cost") if recommendation.get("selected_run_id") else None)
    if known_calls:
        metrics["predispatch_rejections"] = len(calls) - sum(call["tool_call_id"] in by_id for call in calls)
    return metrics


def attach_observed(args):
    root, manifest = load_round(args.directory)
    require(not (root / "SEALED.json").exists() and manifest["stage"] not in ("invalid", "sealed"), "Round is sealed or invalid.")
    require(manifest["live_enabled"] is True and args.case in OBSERVED, "Observed evidence requires a declared live round.")
    check_binding(manifest, live=True); verify_fixtures(root, manifest)
    case = next(item for item in manifest["cases"] if item["case_id"] == args.case)
    require(case["status"] == "pending" and not case["attempted"], "Observed case already has an attempt.")
    raw = bytes_at(args.archive)
    audited, accepted, safe = {"passed": False, "classification": "evidence_audit_rejected"}, False, False
    try:
        value = document(raw)
        safe = True
        audited = audit_observed(value, missing=OBSERVED[args.case], passed=True)
        accepted = True
    except (ValueError, KeyError, TypeError):
        pass
    folder = root / "observed" / args.case; folder.mkdir(exist_ok=False)
    result = {"case_id": args.case, "mode": "observed", "hosted": True, "status": "passed" if accepted else "failed",
              "source_archive_bytes": len(raw), "source_archive_sha256": digest(raw),
              "raw_archive_retained": safe,
              "audit": {**audited, "passed": accepted, "classification": ("missing_coverage" if OBSERVED[args.case] else "advisory_plan") if accepted else "evidence_audit_rejected"},
              "binding": {"source": manifest["source"], "executable": manifest["executables"]["desktop"]},
              "binding_kind": "Operator-witnessed GUI attachment to the recorded current desktop binary; not execution attestation", "attached_utc": utc()}
    if safe:
        path = folder / "evidence.session.json"; write_new(path, raw); result["archive"] = reference(root, path)
    if accepted:
        result["audit"].update(observed_metrics(value, audited))
    result_path = folder / "result.json"; write_new(result_path, encoded(result))
    case.update(status=result["status"], attempted=True, result=reference(root, result_path))
    manifest["stage"] = "ready_to_seal" if all(item["status"] == "passed" for item in manifest["cases"]) else "incomplete"
    journal(root, manifest)
    return (0 if accepted else 2), manifest


def reaudit(root, item, *, cadence=None, observed_metrics_required=False):
    result = document(read_reference(root, item["result"]))
    require(result["case_id"] == item["case_id"] and result["status"] == item["status"], "Case/result identity differs.")
    if cadence is not None:
        require(result.get("cadence") == cadence and cadence["finish"]["status"] == "completed" and
                type(result.get("helper_started_elapsed_s")) in (int, float) and
                type(result.get("completed_elapsed_s")) in (int, float) and
                math.isfinite(result["helper_started_elapsed_s"]) and math.isfinite(result["completed_elapsed_s"]) and
                result["helper_started_elapsed_s"] >= cadence["finish"]["finished_elapsed_s"] and
                result["completed_elapsed_s"] >= result["helper_started_elapsed_s"], "Case delay differs from its declared wait.")
    else:
        require("cadence" not in result, "Case delay lacks its manifest binding.")
    if item["mode"] != "observed":
        require(result["mode"] == item["mode"] and result["input_files"] == item["definition"]["input_files"] and
                result["objective"] == item["definition"]["objective"], "Result objective or input binding differs.")
    for artifact in result.get("partial_artifacts", []):
        read_reference(root, artifact)
    if "archive" not in result:
        require(result["status"] == "failed", "Passing case has no raw archive.")
        if result.get("raw_output_retained"):
            document(read_reference(root, result["stdout"])); read_reference(root, result["stderr"])
        return result
    value = document(read_reference(root, result["archive"]))
    if item["mode"] == "observed":
        require(result["source_archive_sha256"] == result["archive"]["sha256"] and
                result["source_archive_bytes"] == result["archive"]["bytes"], "Observed attachment differs from the supplied exact bytes.")
        try:
            audit = audit_observed(value, missing=OBSERVED[item["case_id"]], passed=True)
        except (ValueError, KeyError, TypeError):
            require(result["status"] == "failed" and result["audit"].get("classification") == "evidence_audit_rejected",
                    "Passing observed evidence failed re-audit.")
            return result
        supplemental = observed_metrics_required or bool(set(result["audit"]) & (OBSERVED_METRICS - {"logical_requests", "total_tool_calls", "model_requests", "tool_calls"}))
        require(result["status"] == "passed" and all(result["audit"].get(key) == val for key, val in audit.items()
                if key != "model_requests" or not supplemental), "Observed re-audit differs.")
        if supplemental:
            require(result["audit"].get("completed_model_requests") == audit["model_requests"], "Observed completed request count differs.")
            require({key: val for key, val in result["audit"].items() if key in OBSERVED_METRICS} == observed_metrics(value, audit),
                    "Observed supplemental metrics differ from raw evidence.")
    elif result.get("raw_output_retained"):
        summary = document(read_reference(root, result["stdout"]))
        read_reference(root, result["stderr"])
        input_binding(root, item["definition"], value)
        try:
            audited = audit_case(item["mode"], value, summary, result["expected"], hosted=result["hosted"])
        except (ValueError, KeyError, TypeError):
            require(result["status"] == "failed" and result["audit"].get("classification") == "evidence_audit_rejected",
                    "A passing case failed its semantic re-audit.")
            return result
        require(result["audit"] == audited or (result["status"] == "failed" and result["audit"].get("classification") == "process_exit_mismatch"), "Semantic re-audit differs.")
        if result["status"] == "passed":
            require(audited.get("passed") is True and result["process"]["failure"] is None and result["process"]["exit_code"] in (0, 3), "Failed execution cannot pass sealing.")
            require(result.get("horizon_binding") == horizon_binding(item["mode"], value, summary), "Cost horizon binding differs.")
    else:
        require(result["status"] == "failed", "Uninspected output cannot pass sealing.")
    return result


def seal_round(args):
    root, manifest = load_round(args.directory)
    require(not (root / "SEALED.json").exists() and manifest["stage"] != "invalid", "Round is already sealed or invalid.")
    check_binding(manifest, live=manifest["live_enabled"]); verify_fixtures(root, manifest)
    cadence = validate_delay_history(manifest)
    results, sessions, preflight_results = [], set(), {}
    provider_verified = True if "provider_deferral_policy" in manifest else None
    for preflight in manifest["preflights"]:
        if "result" in preflight:
            definition = next(case["definition"] for case in manifest["cases"] if case.get("definition", {}).get("input_id") == preflight["input_id"])
            original = document(read_reference(root, preflight["result"]))
            checked = reaudit(root, {"case_id": original["case_id"], "mode": definition["mode"], "definition": definition, "status": preflight["status"], "result": preflight["result"]})
            require(checked["hosted"] is False and checked["binding"] == {"source": manifest["source"], "executable": manifest["executables"]["sandbox" if definition["mode"] == "sandbox" else "offline"]}, "Preflight source/executable binding differs.")
            if checked["status"] == "passed":
                require(preflight["qualified_expected"] == checked["audit"]["classification"] in SEMANTIC, "Preflight expectation is not qualified.")
            preflight_results[preflight["input_id"]] = checked
        else:
            require(preflight["status"] in ("pending", "running"), "Completed preflight has no result.")
    for case in manifest["cases"]:
        if "result" in case:
            result = reaudit(root, case, cadence=cadence.get(case["case_id"]), observed_metrics_required="delay_policy" in manifest); results.append(result)
            require(result["hosted"] is True and result["binding"] == {"source": manifest["source"], "executable": manifest["executables"]["desktop" if case["mode"] == "observed" else case["mode"]]}, "Case source/executable binding differs.")
            if case["mode"] != "observed":
                native = preflight_results[case["definition"]["input_id"]]
                require(native["status"] == "passed" and result["expected"] == native["audit"]["classification"], "Hosted expectation lacks passing preflight evidence.")
                for following in cadence.values():
                    if following["start"]["previous_case_id"] == case["case_id"]:
                        require(following["start"]["previous_completed_elapsed_s"] == result.get("completed_elapsed_s"),
                                "Following delay differs from the actual prior case completion.")
                if "provider_deferral_policy" in manifest:
                    verified = validate_provider_following_delays(root, manifest, result)
                    provider_verified = provider_verified and verified
            if "archive" in result:
                session = document(read_reference(root, result["archive"])).get("session_id")
                require(session and session not in sessions, "A session was reused across declared cases.")
                sessions.add(session)
        else:
            require(case["status"] in ("pending", "running"), "Completed case has no result.")
    passed = sum(case["status"] == "passed" for case in manifest["cases"])
    cli = [case for case in manifest["cases"] if case["mode"] != "observed"]
    all_passed = passed == 24 and all(item["status"] == "passed" for item in manifest["preflights"]) and provider_verified is not False
    totals = {key: sum(result["audit"].get(key, 0) for result in results if type(result["audit"].get(key, 0)) in (int, float)) for key in METRICS}
    cost_gaps = []
    for result in results:
        if result["mode"] == "observed":
            continue
        case = next(item for item in manifest["cases"] if item["case_id"] == result["case_id"])
        native = preflight_results[case["definition"]["input_id"]]
        a, b = native["audit"].get("selected_cost"), result["audit"].get("selected_cost")
        if (native["status"] == result["status"] == "passed" and type(a) in (int, float) and type(b) in (int, float) and
                math.isfinite(a) and math.isfinite(b) and native["input_files"] == result["input_files"] and
                native.get("horizon_binding") is not None and native["horizon_binding"] == result.get("horizon_binding")):
            cost_gaps.append({"case_id": result["case_id"], "native_selected_cost": a, "hosted_selected_cost": b, "difference": b - a,
                              "scope": "Descriptive comparison for matching input hashes and horizons only; no global optimality claim."})
    summary = {"format": FORMAT, "round_id": manifest["round_id"], "source": manifest["source"], "overall_passed": all_passed,
               "status": "passed" if all_passed else "incomplete", "declared_cases": 24, "passed_cases": passed,
               "attempted_cases": sum(case["attempted"] for case in manifest["cases"]), "completion_rate": passed / 24,
               "cli_passed": sum(case["status"] == "passed" for case in cli), "cli_declared": 22,
               "pending_cases": [case["case_id"] for case in manifest["cases"] if case["status"] in ("pending", "running")],
               "failed_cases": [case["case_id"] for case in manifest["cases"] if case["status"] == "failed"],
               "preflight_passed": sum(item["status"] == "passed" for item in manifest["preflights"]), "preflight_declared": 11,
               "failed_preflights": [item["input_id"] for item in manifest["preflights"] if item["status"] == "failed"],
               "pending_preflights": [item["input_id"] for item in manifest["preflights"] if item["status"] in ("pending", "running")],
               "refusal_categories": dict(Counter(result["audit"]["classification"] for result in results if result["audit"]["classification"] in SEMANTIC - {"advisory_plan"})),
               "metrics": totals, "selected_costs": [{"case_id": result["case_id"], "selected_cost": result["audit"].get("selected_cost")} for result in results],
               "native_preflight_cost_gaps": cost_gaps,
               "delay_policy": manifest.get("delay_policy"),
               "provider_deferral_policy": manifest.get("provider_deferral_policy"), "provider_deferrals_verified": provider_verified,
               "delayed_s": sum(record["finish"]["delayed_s"] for record in cadence.values()),
               "request_counts_complete": all(result["audit"].get("request_counts_complete") is True for result in results),
               "duplicate_counts_complete": all(result["audit"].get("duplicate_counts_complete") is True for result in results),
               "cases": [{"case_id": case["case_id"], "status": case["status"], "result": case.get("result")} for case in manifest["cases"]],
               "limitations": "All declared attempts are counted. Grounded completion/refusal is not prose accuracy, global optimality, field qualification, execution attestation or release approval. Native/hosted cost comparisons require matching input hashes and horizons.",
               "public_release_approved": False}
    text = ("# Nemotron evaluation round\n\n" + f"Status: {summary['status']}. Passed {passed}/24 declared cases ({summary['cli_passed']}/22 CLI cases).\n\n" +
            f"Pending: {', '.join(summary['pending_cases']) or 'none'}.\n\nFailed: {', '.join(summary['failed_cases']) or 'none'}.\n\n" + summary["limitations"] + "\n")
    manifest["stage"] = "sealed"; latest = journal(root, manifest)
    write_new(root / "summary.json", encoded(summary)); write_new(root / "summary.md", text.encode("utf-8"))
    sealed = {"format": FORMAT, "overall_passed": all_passed, "sealed_utc": utc(), "manifest": reference(root, latest),
              "summary": reference(root, root / "summary.json"), "markdown_summary": reference(root, root / "summary.md")}
    write_new(root / "SEALED.json", encoded(sealed))
    return (0 if all_passed else 2), summary


def main(argv=None):
    parser = ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run")
    run.add_argument("--output", type=Path, required=True)
    for name in ("sandbox", "eog", "offline", "desktop"):
        run.add_argument("--" + name + "-executable", type=Path, required=True)
    run.add_argument("--expected-source"); run.add_argument("--live", action="store_true")
    run.add_argument("--duration-seconds", type=int, default=DEFAULT_ROUND_SECONDS)
    run.add_argument("--case-gap-seconds", type=int, default=DEFAULT_CASE_GAP_SECONDS,
                     help="Hosted completion-to-next-start courtesy gap (0..60 seconds), within the round deadline")
    attach = commands.add_parser("attach-observed")
    attach.add_argument("--directory", type=Path, required=True); attach.add_argument("--case", choices=OBSERVED, required=True)
    attach.add_argument("--archive", type=Path, required=True)
    seal = commands.add_parser("seal"); seal.add_argument("--directory", type=Path, required=True)
    try:
        args = parser.parse_args(argv)
        code, result = {"run": run_round, "attach-observed": attach_observed, "seal": seal_round}[args.command](args)
        print(json.dumps({"status": result.get("status", result.get("stage")), "overall_passed": result.get("overall_passed", False), "exit_code": code}))
        return code
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError):
        # Raw helper output, rejected evidence, environment and secrets never enter diagnostics.
        print(json.dumps({"status": "invalid", "code": "evaluation_rejected", "exit_code": 1}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
