"""Runner bookkeeping with mocked helpers/auditors only; never live evidence."""
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import uuid


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
spec = importlib.util.spec_from_file_location("evaluation_runner", ROOT / "scripts/run_nemotron_evaluation.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def observed_archive(session):
    events = [{"type": "model_request_started"}, {"type": "model_request_completed", "request": {
        "model_request_id": "request-" + session, "duration_ms": 1, "retry_count": 0}}]
    for index, name in enumerate(("get_network_state", "get_control_catalog")):
        identity = f"call-{index}"
        events.extend([{"type": "tool_requested", "tool_call_id": identity, "tool_name": name},
                       {"type": "tool_started", "tool_call_id": identity, "tool_name": name, "input_summary": {}},
                       {"type": "tool_completed", "tool_call_id": identity, "tool_name": name, "result": {"ok": True}}])
    return {"session_id": session, "events": events}


class RunnerTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="breakwater-runner-mocks-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.executables = {}
        for name in ("sandbox", "eog", "offline", "desktop"):
            path = self.root / (name + ".exe"); path.write_bytes(("mock-" + name).encode())
            self.executables[name] = path
        self.source = {"head": "a" * 40, "tree": "b" * 40, "dirty": False,
                       "status_sha256": "c" * 64, "worktree_sha256": "d" * 64}
        self.calls = []
        self.behavior = None
        self.clock = None
        self.secret = "nvapi-" + "x" * 30
        self.stack = ExitStack(); self.addCleanup(self.stack.close)
        self.stack.enter_context(patch.object(runner, "source_identity", side_effect=lambda: copy.deepcopy(self.source)))
        self.stack.enter_context(patch.object(runner.os, "fsync"))  # Durability is not the bookkeeping behavior under test.
        self.stack.enter_context(patch.object(runner, "run_helper", side_effect=self.fake_helper))
        self.stack.enter_context(patch.object(runner, "audit_case", side_effect=self.fake_audit))
        self.stack.enter_context(patch.dict(os.environ, {"NVIDIA_API_KEY": self.secret, "OTHER_SECRET": "do-not-forward",
                                                      "PYTHONPATH": "do-not-forward", "BREAKWATER_PYTHON": "configured-python"}, clear=True))

    def args(self, name="round", live=False):
        return SimpleNamespace(output=self.root / name, live=live, expected_source="a" * 40 if live else None,
                               duration_seconds=1800, case_gap_seconds=0,
                               **{key + "_executable": value for key, value in self.executables.items()})

    @staticmethod
    def fake_audit(mode, archive, summary, expected, *, hosted=True):
        classification = summary["mock_classification"]
        return {"classification": classification, "passed": classification == expected and not summary.get("mock_failure", False),
                "logical_requests": int(hosted), "retries": 0, "total_tool_calls": 2, "duplicate_calls": 0,
                "validation_reuse": 0, "selected_cost": 123.0 if classification == "advisory_plan" else None}

    def fake_helper(self, arguments, folder, environment, timeout):
        arguments = list(map(str, arguments)); self.calls.append((arguments, folder, dict(environment), timeout))
        offline_eog = arguments[-1] == "--preflight"
        hosted = "--live" in arguments
        source = Path(arguments[1] if offline_eog else arguments[arguments.index("--project" if "--project" in arguments else "--dataset") + 1])
        identity = source.name.removesuffix(".project.json")
        classification = {"no-remedy": "no_supported_plan", "missing-tank-channel": "missing_coverage", "stale-tank-channel": "stale_coverage"}.get(identity, "advisory_plan")
        archive_path = folder / "breakwater-offline-mock" / "local-analysis.eog-session.json" if offline_eog else Path(arguments[arguments.index("--archive") + 1])
        archive_path.parent.mkdir(exist_ok=True)
        if "--project" in arguments:
            archive = {"session_id": str(uuid.uuid4()), "snapshot": {"network": json.loads(source.read_bytes())["network"], "horizon_h": 3.0}}
        else:
            archive = {"session_id": str(uuid.uuid4()), "capture_manifest": {
                "source_hashes": {name: runner.digest((source / name).read_bytes()) for name in ("network.json", "ops.json", "telemetry.csv")},
                "cutoff_unix": 1794823200}}
            # Read the generated declaration, rather than introducing another clock.
            catalog = json.loads((source.parents[2] / "cases.json").read_bytes())
            archive["capture_manifest"]["cutoff_unix"] = next(case["cutoff_unix"] for case in catalog["cases"] if case["input_id"] == identity)
        archive_path.write_bytes(json.dumps(archive).encode())
        code = 0 if offline_eog or classification == "advisory_plan" else 3
        summary = {"status": "passed" if code == 0 else "incomplete", "exit_code": code,
                   "archive_path": str(archive_path), "mock_classification": classification}
        process = {"exit_code": code, "duration_s": 0.01, "failure": None, "stdout": json.dumps(summary).encode(), "stderr": b""}
        if self.clock is not None:
            self.clock[0] += 500
        if self.behavior:
            self.behavior(arguments, folder, archive_path, process)
        return process

    def test_pacing_policy_is_forwarded_without_forwarding_unrelated_settings(self):
        environment = {"PATH": "runtime", "NVIDIA_API_KEY": "fixture-key", "BREAKWATER_NEMOTRON_INTERVAL_MS": "9000", "OTHER_SECRET": "private"}
        child = runner.child_environment(environment, live=True)
        self.assertEqual(child["BREAKWATER_NEMOTRON_INTERVAL_MS"], "9000")
        self.assertNotIn("OTHER_SECRET", child)
        native = runner.child_environment(environment, live=False)
        self.assertNotIn("NVIDIA_API_KEY", native)

    def test_default_preflights_all_inputs_without_reading_or_forwarding_credentials(self):
        self.source["dirty"] = True
        args = self.args()
        code, manifest = runner.run_round(args)
        self.assertEqual(code, 0)
        self.assertEqual(manifest["stage"], "preflight_complete")
        self.assertTrue(manifest["source"]["dirty"])
        self.assertEqual(len(self.calls), 11)
        self.assertTrue(all(item["status"] == "passed" for item in manifest["preflights"]))
        self.assertTrue(all(item["status"] == "pending" for item in manifest["cases"]))
        for arguments, _, environment, timeout in self.calls:
            self.assertNotIn("--live", arguments)
            self.assertFalse({"NVIDIA_API_KEY", "NVIDIA_BASE_URL", "NEMOTRON_MODEL", "OTHER_SECRET", "PYTHONPATH"} & environment.keys())
            self.assertEqual(environment["BREAKWATER_PYTHON"], "configured-python")
            self.assertLessEqual(timeout, 450)
        class NoCredentialReads(dict):
            def get(self, key, default=None):
                if key.startswith("NVIDIA") or key == "NEMOTRON_MODEL":
                    raise AssertionError("credential inspected during native preflight")
                return super().get(key, default)
        self.assertNotIn("NVIDIA_API_KEY", runner.child_environment(NoCredentialReads(PATH="runtime-path"), live=False))

    def test_live_requires_exact_clean_source_and_fixed_endpoint_model(self):
        for change in ({"dirty": True}, {"head": "e" * 40}):
            self.source.update(change)
            with self.assertRaises(runner.EvaluationError):
                runner.run_round(self.args(live=True))
            self.source.update(head="a" * 40, dirty=False)
        self.assertFalse(self.calls)
        self.assertFalse(self.args().output.exists())
        for name, value in (("NVIDIA_BASE_URL", "https://another.invalid/v1"), ("NEMOTRON_MODEL", "another-model")):
            with patch.dict(os.environ, {name: value}), self.assertRaises(runner.EvaluationError):
                runner.run_round(self.args(live=True))
        args = self.args(live=True); args.expected_source = None
        with self.assertRaises(runner.EvaluationError):
            runner.run_round(args)

    def test_invalid_arguments_use_fixed_redacted_diagnostics_and_exit_one(self):
        stdout, stderr = io.StringIO(), io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = runner.main(["run", "--accidental-unknown-" + self.secret])
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(stdout.getvalue()), {"status": "invalid", "code": "evaluation_rejected", "exit_code": 1})
        self.assertEqual(stderr.getvalue(), "")
        self.assertNotIn(self.secret, stdout.getvalue())
        self.assertFalse(self.calls)

    def test_live_is_once_per_declared_case_after_all_preflights_and_awaits_observed(self):
        args = self.args(live=True)
        code, manifest = runner.run_round(args)
        self.assertEqual(code, 0)
        self.assertEqual(manifest["stage"], "awaiting_observed")
        self.assertEqual(len(self.calls), 33)
        self.assertTrue(all("--live" not in call[0] for call in self.calls[:11]))
        self.assertTrue(all("--live" in call[0] for call in self.calls[11:]))
        self.assertEqual(sum(case["status"] == "passed" for case in manifest["cases"]), 22)
        self.assertEqual({case["case_id"] for case in manifest["cases"] if case["status"] == "pending"}, set(runner.OBSERVED))
        databases = [call[0][call[0].index("--db") + 1] for call in self.calls[11:]]
        self.assertEqual(len(set(databases)), 22)
        for arguments, _, environment, _ in self.calls[11:]:
            self.assertEqual(environment["NVIDIA_BASE_URL"], runner.ENDPOINT)
            self.assertEqual(environment["NEMOTRON_MODEL"], runner.MODEL)
            self.assertNotIn(self.secret, json.dumps(arguments))
        loaded_root, loaded = runner.load_round(args.output)
        self.assertEqual(loaded, manifest)
        self.assertEqual(len(loaded["cases"]), 24)
        self.assertTrue((loaded_root / "manifest-0001.json").is_file())

    def test_unknown_setup_failure_never_becomes_a_refusal_or_allows_hosting(self):
        def fail(arguments, folder, archive, process):
            if len(self.calls) == 1:
                archive.unlink()
                process["exit_code"] = 3
                process["stdout"] = json.dumps({"status": "failed", "exit_code": 3, "code": "setup_failed"}).encode()
        self.behavior = fail
        code, manifest = runner.run_round(self.args(live=True))
        self.assertEqual(code, 2)
        self.assertEqual(len(self.calls), 11)
        self.assertEqual(manifest["preflights"][0]["status"], "failed")
        self.assertTrue(all(case["status"] == "pending" for case in manifest["cases"]))

    def test_source_and_binary_changes_abort_remaining_hosted_cases(self):
        def source_change(arguments, folder, archive, process):
            if len(self.calls) == 11:
                self.source["dirty"] = True
        self.behavior = source_change
        code, manifest = runner.run_round(self.args(live=True))
        self.assertEqual(code, 1)
        self.assertEqual(len(self.calls), 11)
        self.assertEqual(manifest["stage"], "invalid")
        self.source["dirty"] = False; self.calls.clear()
        def binary_change(arguments, folder, archive, process):
            if len(self.calls) == 12:
                self.executables["eog"].write_bytes(b"changed-binary")
        self.behavior = binary_change
        code, manifest = runner.run_round(self.args("binary-change", live=True))
        self.assertEqual(code, 1)
        self.assertEqual(len(self.calls), 12)
        self.assertEqual(sum(case["attempted"] for case in manifest["cases"]), 1)

    def test_preflight_source_or_binary_mutation_invalidates_standalone_round(self):
        for mutation in ("source", "binary", "input"):
            with self.subTest(mutation=mutation):
                self.calls.clear(); self.source["dirty"] = False
                self.executables["eog"].write_bytes(b"original-mock-eog")
                def change(arguments, folder, archive, process):
                    if mutation == "source":
                        self.source["dirty"] = True
                    elif mutation == "binary":
                        self.executables["eog"].write_bytes(b"changed-during-preflight")
                    else:
                        project = Path(arguments[arguments.index("--project") + 1])
                        project.write_bytes(project.read_bytes() + b" ")
                self.behavior = change
                args = self.args("preflight-" + mutation)
                code, manifest = runner.run_round(args)
                self.assertEqual(code, 1); self.assertEqual(len(self.calls), 1)
                self.assertEqual(manifest["stage"], "invalid")
                self.assertEqual(manifest["preflights"][0]["status"], "passed")
                self.assertIn("result", manifest["preflights"][0])
                self.assertEqual(sum(item["status"] == "pending" for item in manifest["preflights"]), 10)
                self.assertTrue(all(case["status"] == "pending" for case in manifest["cases"]))

    def test_timeout_retains_partial_files_and_failed_result_without_retry(self):
        def timeout(arguments, folder, archive, process):
            if len(self.calls) == 1:
                (folder / "partial.txt").write_bytes(b"partial evidence")
                process.update(failure="helper_timeout", exit_code=-9, stdout=b"{}")
        self.behavior = timeout
        args = self.args()
        code, manifest = runner.run_round(args)
        self.assertEqual(code, 2)
        preflight = manifest["preflights"][0]
        result = runner.document(runner.read_reference(args.output, preflight["result"]))
        self.assertEqual(result["audit"]["classification"], "helper_timeout")
        self.assertTrue(any(item["path"].endswith("partial.txt") for item in result["partial_artifacts"]))
        self.assertEqual(len(self.calls), 11)
        before = (args.output / "manifest-0001.json").read_bytes()
        with self.assertRaises(runner.EvaluationError):
            runner.run_round(args)
        self.assertEqual((args.output / "manifest-0001.json").read_bytes(), before)

    def test_unsafe_output_is_withheld_and_cannot_pass(self):
        def unsafe(arguments, folder, archive, process):
            if len(self.calls) == 1:
                process["stderr"] = ("Bearer " + "a" * 24).encode()
        self.behavior = unsafe
        args = self.args()
        code, manifest = runner.run_round(args)
        self.assertEqual(code, 2)
        result = runner.document(runner.read_reference(args.output, manifest["preflights"][0]["result"]))
        self.assertFalse(result["raw_output_retained"])
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["audit"]["classification"], "unsafe_or_malformed_output")
        self.assertFalse(any(("Bearer " + "a" * 24).encode() in path.read_bytes() for path in args.output.rglob("*") if path.is_file()))

    def test_semantic_rejection_is_retained_and_can_only_seal_incomplete(self):
        args = self.args(live=True)
        def reject(mode, archive, summary, expected, *, hosted=True):
            if summary.get("mock_failure"):
                raise ValueError("Synthetic semantic rejection")
            return self.fake_audit(mode, archive, summary, expected, hosted=hosted)
        def mark(arguments, folder, archive, process):
            if len(self.calls) == 1:
                summary = json.loads(process["stdout"]); summary["mock_failure"] = True
                process["stdout"] = json.dumps(summary).encode()
        self.behavior = mark
        with patch.object(runner, "audit_case", side_effect=reject):
            code, manifest = runner.run_round(args)
            self.assertEqual(code, 2); self.assertEqual(len(self.calls), 11)
            failed = runner.document(runner.read_reference(args.output, manifest["preflights"][0]["result"]))
            self.assertEqual(failed["audit"]["classification"], "evidence_audit_rejected")
            self.assertIn("archive", failed)
            code, summary = runner.seal_round(SimpleNamespace(directory=args.output))
            self.assertEqual(code, 2); self.assertFalse(summary["overall_passed"])
            self.assertEqual(summary["failed_preflights"], ["canonical-pump-failure"])
            self.assertEqual(summary["passed_cases"], 0)

    def test_round_deadline_keeps_unrun_cases_pending(self):
        self.clock = [0.0]
        with patch.object(runner.time, "monotonic", side_effect=lambda: self.clock[0]):
            code, manifest = runner.run_round(self.args(live=True))
        self.assertEqual(code, 2)
        self.assertEqual(len(self.calls), 4)
        self.assertEqual(manifest["stop_reason"], "round_deadline")
        self.assertTrue(all(case["status"] == "pending" for case in manifest["cases"]))
        self.assertEqual(sum(item["attempted"] for item in manifest["preflights"]), 4)

    def test_pending_observations_cannot_seal_as_full_pass(self):
        args = self.args(live=True); runner.run_round(args)
        code, summary = runner.seal_round(SimpleNamespace(directory=args.output))
        self.assertEqual(code, 2)
        self.assertFalse(summary["overall_passed"])
        self.assertEqual((summary["passed_cases"], summary["completion_rate"]), (22, 22 / 24))
        self.assertEqual(set(summary["pending_cases"]), set(runner.OBSERVED))
        self.assertFalse(runner.document((args.output / "SEALED.json").read_bytes())["overall_passed"])
        with self.assertRaises(runner.EvaluationError):
            runner.seal_round(SimpleNamespace(directory=args.output))

    def test_attached_exact_bytes_seal_only_with_all_twenty_four_cases(self):
        args = self.args(live=True); runner.run_round(args)
        with patch.object(runner, "audit_observed", side_effect=lambda value, **_: {"session_id": value["session_id"], "tool_calls": 2, "model_requests": 1}):
            for identity in runner.OBSERVED:
                path = self.root / (identity + ".json")
                raw = (json.dumps(observed_archive(identity)) + "\n").encode(); path.write_bytes(raw)
                code, manifest = runner.attach_observed(SimpleNamespace(directory=args.output, case=identity, archive=path))
                self.assertEqual(code, 0)
                copied = args.output / "observed" / identity / "evidence.session.json"
                self.assertEqual(copied.read_bytes(), raw)
                with self.assertRaises(runner.EvaluationError):
                    runner.attach_observed(SimpleNamespace(directory=args.output, case=identity, archive=path))
            self.assertEqual(manifest["stage"], "ready_to_seal")
            code, summary = runner.seal_round(SimpleNamespace(directory=args.output))
            self.assertEqual(code, 0); self.assertTrue(summary["overall_passed"])
            self.assertEqual(summary["passed_cases"], 24)
            self.assertTrue((args.output / "summary.md").is_file())

    def test_raw_hash_or_path_changes_prevent_sealing(self):
        args = self.args(live=True); _, manifest = runner.run_round(args)
        case = manifest["cases"][0]
        result = runner.document(runner.read_reference(args.output, case["result"]))
        archive = runner.relative(args.output, result["archive"]["path"])
        archive.write_bytes(archive.read_bytes() + b" ")
        with self.assertRaises(runner.EvaluationError):
            runner.seal_round(SimpleNamespace(directory=args.output))
        self.assertFalse((args.output / "SEALED.json").exists())
        for name in ("../outside", "/outside", "C:/outside", "nested\\outside"):
            with self.subTest(name=name), self.assertRaises(runner.EvaluationError):
                runner.relative(args.output, name)

    def test_failed_observed_attachment_is_an_attempt_and_cannot_be_replaced(self):
        args = self.args(live=True); runner.run_round(args)
        archive = self.root / "failed-observation.json"
        raw = (json.dumps(observed_archive("synthetic-failed-observed")) + "\n").encode(); archive.write_bytes(raw)
        request = SimpleNamespace(directory=args.output, case="observed-pump", archive=archive)
        with patch.object(runner, "audit_observed", side_effect=ValueError("Synthetic observed rejection")):
            code, manifest = runner.attach_observed(request)
            self.assertEqual(code, 2)
            case = next(item for item in manifest["cases"] if item["case_id"] == request.case)
            self.assertEqual(case["status"], "failed"); self.assertTrue(case["attempted"])
            self.assertEqual((args.output / "observed/observed-pump/evidence.session.json").read_bytes(), raw)
            with self.assertRaises(runner.EvaluationError):
                runner.attach_observed(request)
            code, summary = runner.seal_round(SimpleNamespace(directory=args.output))
            self.assertEqual(code, 2); self.assertFalse(summary["overall_passed"])
            self.assertIn("observed-pump", summary["failed_cases"])

    def test_helper_timeout_and_oversize_terminate_only_the_owned_process(self):
        class Process:
            def __init__(self, stdout=b"", stderr=b""):
                self.stdout, self.stderr = io.BytesIO(stdout), io.BytesIO(stderr)
                self.pid, self.returncode = 12345, None
            def poll(self):
                return self.returncode
            def wait(self, timeout=None):
                if self.returncode is None:
                    raise subprocess.TimeoutExpired("mock-helper", timeout)
                return self.returncode
        # Remove only our run_helper mock for this local process-adapter test.
        original = spec.loader.get_code("evaluation_runner")
        namespace = {"__name__": "runner_adapter_test", "__file__": str(ROOT / "scripts/run_nemotron_evaluation.py")}
        exec(original, namespace)
        for oversized in (False, True):
            process = Process(stdout=b"x" * 33 if oversized else b"")
            ticks = iter([0.0, 0.0, 2.0, 2.0, 2.0, 2.0])
            def kill(owned):
                self.assertIs(owned, process); owned.returncode = -9
            with patch.object(subprocess, "Popen", return_value=process), patch.object(runner.time, "monotonic", side_effect=lambda: next(ticks, 2.0)), \
                    patch.dict(namespace, STDOUT_LIMIT=16), patch.dict(namespace, terminate_owned=kill):
                result = namespace["run_helper"](["mock-helper"], self.root, {}, 1)
            self.assertEqual(result["failure"], "output_limit" if oversized else "helper_timeout")
            self.assertLessEqual(len(result["stdout"]), 16)
            self.assertEqual(result["exit_code"], -9)

    def test_cadence_integrates_deadline_without_delaying_native_or_first_hosted_case(self):
        clock, sleeps = [0.0], []
        def sleep(seconds):
            sleeps.append((len(self.calls), seconds)); clock[0] += seconds
        args = self.args(live=True); args.case_gap_seconds = 30; args.duration_seconds = 60
        with patch.object(runner.time, "monotonic", side_effect=lambda: clock[0]), patch.object(runner.time, "sleep", side_effect=sleep):
            code, manifest = runner.run_round(args)
        self.assertEqual(code, 2); self.assertEqual(len(self.calls), 13)
        self.assertEqual(sum(item["status"] == "passed" for item in manifest["preflights"]), 11)
        self.assertEqual(sum(item["attempted"] for item in manifest["cases"]), 2)
        self.assertEqual(manifest["stop_reason"], "round_deadline")
        self.assertTrue(all(count in (12, 13) and 0 < delay <= .25 for count, delay in sleeps))
        self.assertEqual(sum(delay for _, delay in sleeps), 60)
        history = runner.validate_delay_history(manifest)
        self.assertEqual([entry["start"]["planned_delay_s"] for entry in history.values()], [0, 30, 30])
        self.assertEqual(list(history.values())[-1]["finish"]["status"], "round_deadline")
        sealed_code, summary = runner.seal_round(SimpleNamespace(directory=args.output))
        self.assertEqual(sealed_code, 2); self.assertFalse(summary["overall_passed"])
        self.assertEqual(summary["delayed_s"], 60); self.assertEqual(summary["delay_policy"], runner.delay_policy(30))

    def test_cadence_rechecks_binding_after_the_wait_before_launch(self):
        clock = [0.0]
        def sleep(seconds):
            clock[0] += seconds
            self.source["dirty"] = True
        args = self.args(live=True); args.case_gap_seconds = 30
        with patch.object(runner.time, "monotonic", side_effect=lambda: clock[0]), patch.object(runner.time, "sleep", side_effect=sleep):
            code, manifest = runner.run_round(args)
        self.assertEqual(code, 1); self.assertEqual(len(self.calls), 12)
        self.assertEqual(manifest["stage"], "invalid")
        self.assertEqual(sum(case["attempted"] for case in manifest["cases"]), 1)
        self.assertEqual(manifest["cases"][0]["status"], "passed")
        self.assertIn("result", manifest["cases"][0])
        self.assertTrue(all(case["status"] == "pending" for case in manifest["cases"][1:]))


class CadenceTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="breakwater-cadence-")
        self.addCleanup(temporary.cleanup); self.root = Path(temporary.name)
        self.clock, self.sleeps, self.snapshots = 0.0, [], []
        self.real_journal = runner.journal
        self.stack = ExitStack(); self.addCleanup(self.stack.close)
        self.stack.enter_context(patch.object(runner.time, "monotonic", side_effect=lambda: self.clock))
        self.stack.enter_context(patch.object(runner.time, "sleep", side_effect=self.sleep))
        self.stack.enter_context(patch.object(runner, "journal", side_effect=lambda _, manifest: self.snapshots.append(copy.deepcopy(manifest))))

    def sleep(self, seconds):
        self.sleeps.append(seconds); self.clock += seconds

    def manifest(self, gap=30, duration=1800):
        return {"format": runner.FORMAT, "delay_policy": runner.delay_policy(gap), "wait_records": [], "preflights": [],
                "limits": {"round_seconds": duration}, "cases": [
                    {"case_id": f"case-{index:02d}", "mode": "sandbox", "attempted": False, "status": "pending"} for index in range(1, 23)] +
                    [{"case_id": name, "mode": "observed", "attempted": False, "status": "pending"} for name in runner.OBSERVED]}

    def test_cadence_defaults_override_and_policy_types(self):
        captured = []
        arguments = ["run", "--output", "new", "--sandbox-executable", "s", "--eog-executable", "e", "--offline-executable", "o", "--desktop-executable", "d"]
        with patch.object(runner, "run_round", side_effect=lambda args: (captured.append(args.case_gap_seconds) or 0, {"stage": "preflight_complete"})), redirect_stdout(io.StringIO()):
            self.assertEqual(runner.main(arguments), 0)
            self.assertEqual(runner.main(arguments + ["--case-gap-seconds", "0"]), 0)
            self.assertEqual(runner.main(arguments + ["--case-gap-seconds", "60"]), 0)
        self.assertEqual(captured, [30, 0, 60])
        for invalid in (-1, 61, True, 30.0):
            with self.subTest(invalid=invalid), self.assertRaises(runner.EvaluationError):
                runner.delay_policy(invalid)

    def test_cadence_uses_prior_completion_and_credits_already_elapsed_time(self):
        manifest = self.manifest()
        first = runner.wait_for_hosted_case(self.root, manifest, "case-01", None, 0, 1800)
        self.assertEqual(first["start"]["planned_delay_s"], 0); self.assertEqual(self.sleeps, [])
        manifest["cases"][0]["attempted"] = True; self.clock = 105.0
        second = runner.wait_for_hosted_case(self.root, manifest, "case-02", ("case-01", 100.0), 0, 1800)
        self.assertEqual(second["start"]["eligible_elapsed_s"], 130)
        self.assertEqual(second["start"]["planned_delay_s"], 25)
        self.assertEqual(second["finish"]["delayed_s"], 25); self.assertEqual(self.clock, 130)
        manifest["cases"][1]["attempted"] = True; self.clock = 175.0; before = len(self.sleeps)
        third = runner.wait_for_hosted_case(self.root, manifest, "case-03", ("case-02", 140.0), 0, 1800)
        self.assertEqual(third["start"]["planned_delay_s"], 0); self.assertEqual(len(self.sleeps), before)
        self.assertEqual(runner.validate_delay_history(manifest)["case-02"], second)

    def test_cadence_deadline_and_interrupt_leave_the_next_case_unattempted(self):
        for interrupt in (False, True):
            with self.subTest(interrupt=interrupt):
                self.clock = 0.0; manifest = self.manifest(duration=20)
                runner.wait_for_hosted_case(self.root, manifest, "case-01", None, 0, 20)
                manifest["cases"][0]["attempted"] = True; self.clock = 1.0
                def sleep(seconds):
                    if interrupt:
                        raise KeyboardInterrupt
                    self.sleep(seconds)
                with patch.object(runner.time, "sleep", side_effect=sleep):
                    outcome = runner.wait_for_hosted_case(self.root, manifest, "case-02", ("case-01", 1.0), 0, 20)
                self.assertIsNone(outcome)
                self.assertEqual(manifest["stop_reason"], "interrupted" if interrupt else "round_deadline")
                self.assertFalse(manifest["cases"][1]["attempted"])
                self.assertEqual(manifest["cases"][1]["status"], "pending")
                self.assertEqual(runner.validate_delay_history(manifest)["case-02"]["finish"]["status"], manifest["stop_reason"])

    def test_cadence_final_clock_sample_crossing_deadline_keeps_valid_pending_evidence(self):
        manifest = self.manifest(duration=20)
        samples = iter((19.5, 19.75, 20.0))
        with patch.object(runner.time, "monotonic", side_effect=lambda: next(samples)):
            result = runner.wait_for_hosted_case(self.root, manifest, "case-01", None, 0, 20)
        self.assertIsNone(result); self.assertFalse(manifest["cases"][0]["attempted"])
        self.assertEqual(manifest["stop_reason"], "round_deadline")
        record = runner.validate_delay_history(manifest)["case-01"]
        self.assertEqual(record["finish"]["status"], "round_deadline")
        self.assertEqual(record["finish"]["finished_elapsed_s"], 20)

    def test_cadence_policy_wait_history_and_attempts_cannot_be_rewritten(self):
        for mutation in ("policy", "wait", "attempt"):
            with self.subTest(mutation=mutation):
                directory = self.root / mutation; directory.mkdir()
                self.clock = 0.0; manifest = self.manifest()
                runner.wait_for_hosted_case(directory, manifest, "case-01", None, 0, 1800)
                manifest["cases"][0].update(attempted=True, status="failed", result={"path": "failed.json", "bytes": 1, "sha256": "a" * 64})
                self.real_journal(directory, manifest)
                if mutation == "policy":
                    manifest["delay_policy"] = runner.delay_policy(0)
                elif mutation == "wait":
                    manifest["wait_records"][0].update(started_elapsed_s=1.0, eligible_elapsed_s=1.0)
                    manifest["wait_records"][1]["finished_elapsed_s"] = 1.0
                else:
                    manifest["cases"][0].update(attempted=False, status="pending"); del manifest["cases"][0]["result"]
                self.real_journal(directory, manifest)
                with self.assertRaises(runner.EvaluationError):
                    runner.load_round(directory)

    def observed(self):
        events = [{"type": "model_request_started"}, {"type": "model_request_completed", "request": {
            "model_request_id": "request-1", "retry_count": 2, "duration_ms": 25}}]
        for index in range(1, 5):
            identity = f"call-{index}"; name = "get_network_state" if index < 4 else "validate_run"
            arguments = {} if index < 4 else {"snapshot_id": "snapshot-1", "run_id": "run-1"}
            events.extend([{"type": "tool_requested", "tool_call_id": identity, "tool_name": name},
                           {"type": "tool_started", "tool_call_id": identity, "tool_name": name, "input_summary": arguments},
                           {"type": "tool_completed", "tool_call_id": identity, "tool_name": name,
                            "result": {"ok": True, "reused_validation": index == 4}}])
        return {"session_id": "observed-unit-only", "events": events, "runs": [{"id": "r1"}, {"id": "r2"}],
                "recommendation": {"selected_run_id": "r2", "verified_cost": 12.5}}, {"session_id": "observed-unit-only", "tool_calls": 4, "model_requests": 1}

    def test_cadence_observed_metrics_keep_unretained_arguments_unknown(self):
        value, audited = self.observed()
        metrics = runner.observed_metrics(value, audited)
        self.assertEqual(set(metrics), runner.OBSERVED_METRICS)
        self.assertTrue(metrics["duplicate_counts_complete"]); self.assertTrue(metrics["request_counts_complete"])
        self.assertEqual((metrics["duplicate_calls"], metrics["unknown_argument_calls"], metrics["dispatched_tool_calls"], metrics["predispatch_rejections"]), (2, 0, 4, 0))
        self.assertEqual((metrics["successful_tools"], metrics["validation_reuse"]), (4, 1))
        self.assertEqual((metrics["http_attempts"], metrics["retries"], metrics["model_duration_ms"], metrics["actual_tool_executions"]), (3, 2, 25, 3))
        self.assertEqual((metrics["read_repeats"], metrics["stored_runs"], metrics["selected_cost"]), (2, 2, 12.5))
        del next(event for event in value["events"] if event.get("type") == "tool_started" and event["tool_call_id"] == "call-3")["input_summary"]
        metrics = runner.observed_metrics(value, audited)
        self.assertFalse(metrics["duplicate_counts_complete"])
        self.assertEqual((metrics["duplicate_calls"], metrics["unknown_argument_calls"]), (1, 1))
        value["events"] = [event for event in value["events"] if event["type"] != "model_request_started"]
        self.assertFalse(runner.observed_metrics(value, audited)["request_counts_complete"])

    def test_cadence_observed_metric_mutations_fail_reaudit(self):
        value, audited = self.observed()
        archive = self.root / "observed.json"; runner.write_new(archive, runner.encoded(value))
        archived = runner.reference(self.root, archive)
        base = {"case_id": "observed-pump", "mode": "observed", "hosted": True, "status": "passed", "archive": archived,
                "source_archive_sha256": archived["sha256"], "source_archive_bytes": archived["bytes"],
                "audit": {**audited, **runner.observed_metrics(value, audited), "passed": True, "classification": "advisory_plan"}}
        item = {"case_id": "observed-pump", "mode": "observed", "status": "passed"}
        with patch.object(runner, "audit_observed", return_value=audited):
            path = self.root / "original-result.json"; runner.write_new(path, runner.encoded(base))
            runner.reaudit(self.root, {**item, "result": runner.reference(self.root, path)}, observed_metrics_required=True)
            for key in sorted(runner.OBSERVED_METRICS):
                changed = copy.deepcopy(base)
                old = changed["audit"][key]
                changed["audit"][key] = not old if type(old) is bool else old + " changed" if isinstance(old, str) else old + 1
                path = self.root / (key + ".json"); runner.write_new(path, runner.encoded(changed))
                with self.subTest(metric=key), self.assertRaises(runner.EvaluationError):
                    runner.reaudit(self.root, {**item, "result": runner.reference(self.root, path)}, observed_metrics_required=True)
            changed = copy.deepcopy(base)
            for key in runner.OBSERVED_METRICS:
                changed["audit"].pop(key, None)
            path = self.root / "removed-metrics.json"; runner.write_new(path, runner.encoded(changed))
            with self.assertRaises(runner.EvaluationError):
                runner.reaudit(self.root, {**item, "result": runner.reference(self.root, path)}, observed_metrics_required=True)


if __name__ == "__main__":
    unittest.main()
