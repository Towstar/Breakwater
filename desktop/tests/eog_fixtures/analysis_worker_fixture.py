"""Protocol-only EOG process fixture. It implements no detector, optimizer or
independent numerical validator. Wire fields exercise the desktop boundary only."""
import csv
import ctypes
import json
import os
from pathlib import Path
import signal
import sys
import time


def abrupt_exit():
    if os.name == "nt":
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
        ctypes.windll.kernel32.GetCurrentProcess.restype = ctypes.c_void_p
        ctypes.windll.kernel32.TerminateProcess.argtypes = [ctypes.c_void_p, ctypes.c_uint]
        ctypes.windll.kernel32.TerminateProcess(ctypes.windll.kernel32.GetCurrentProcess(), 0xC0000005)
        os._exit(27)
    else:
        os.kill(os.getpid(), signal.SIGKILL)


def source_sample(directory, mode):
    """Return original evidence or one deliberate, shape-valid grounding error."""
    with (directory / "telemetry.csv").open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    selected = mode.removeprefix("validation_")
    samples = [row for row in rows if row["metric"] == "level_pct"]
    sample = samples[0] if selected in ("actual_negative", "actual_stale") else samples[-1]
    evidence = {"evidence_id": "protocol-only-sample", "asset_id": sample["asset_id"],
                "metric": sample["metric"], "timestamp_unix": int(sample["ts_unix"]),
                "value": float(sample["value"]), "unit": "%", "quality": "good"}
    if selected == "wrong_source_value":
        evidence["value"] += 1
    elif selected == "wrong_source_time":
        evidence["timestamp_unix"] -= 1
    elif selected == "absent_source_channel":
        evidence.update(asset_id="pump-Z", metric="discharge_psi", unit="psi", value=1123)
    elif selected == "actual_stale":
        evidence["quality"] = "stale"
    elif selected == "missing_sample":
        evidence.update(value=None, quality="missing")
    return evidence


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "valid"
    gate = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    # Never inspect or print a credential value. The client's whitelist must omit
    # all of these names, including the harmless test-only environment canary.
    for name in ("NVIDIA_API_KEY", "OPENAI_API_KEY", "AWS_SECRET_ACCESS_KEY",
                 "BREAKWATER_EOG_TEST_SECRET_CANARY"):
        if name in os.environ:
            return 40
    raw = sys.stdin.buffer.readline(1024 * 1024 + 1)
    if not raw.endswith(b"\n") or len(raw) > 1024 * 1024 or sys.stdin.buffer.read(1):
        return 41
    request = json.loads(raw)
    response = {key: request[key] for key in (
        "eog_version", "request_id", "operation", "method_version",
        "dataset_identity", "capture_id", "cutoff_unix")}
    response.update(kind="analysis_response", status="completed", diagnostics=[])
    if request["operation"] == "validate_plan":
        plan = request["arguments"]["plan"]
        response["result"] = {"analysis_id": "protocol-only-validation", "validation": {
            "validation_id": "protocol-only-validation-record",
            "plan_id": plan["plan_id"], "plan_hash": plan["plan_hash"],
            "evaluator_method": request["arguments"]["evaluator_method"],
            "validator_method": "protocol-only-validator", "independent": True,
            "feasible": True, "coverage": "available", "outcome": "avoids_shutdown",
            "cost": {"truck_cost": 0, "lost_revenue": 0, "total": 0},
            "violations": [], "minimum_shutdown_margin_bbl": 0,
            "evidence_ids": [], "evidence": [], "shutdowns": [],
            "validated_horizons": plan["repair_deadlines"]}}
        if mode.startswith("validation_"):
            validation = response["result"]["validation"]
            sample = source_sample(Path(request["capture_directory"]), mode)
            validation.update(evidence_ids=[sample["evidence_id"]], evidence=[sample])
    else:
        response["result"] = {
            "analysis_id": "protocol-only-detection", "events": [], "assumptions": [],
            "coverage": "available", "limitations": ["Protocol-only fixture; no detector is implemented."],
            "evidence": [source_sample(Path(request["capture_directory"]), mode)]}
    if mode == "unavailable":
        response.update(status="unavailable", result=None, diagnostics=[
            {"code": "provider_unavailable", "message": "Protocol-only fixture intentionally unavailable."}])
    if mode == "wrong_id":
        response["request_id"] += "-wrong"
    if mode == "wrong_cutoff":
        response["cutoff_unix"] += 1
    if mode == "wrong_capture":
        response["capture_id"] = "0" * 64
    if mode == "wrong_plan_hash":
        response["result"]["validation"]["plan_hash"] = "0" * 64
    if mode == "unknown_asset":
        response["result"]["evidence"][0]["asset_id"] = "invented-asset"
    if mode == "future_evidence":
        response["result"]["evidence"][0]["timestamp_unix"] = request["cutoff_unix"] + 1
    if mode == "tamper_capture":
        with (Path(request["capture_directory"]) / "ops.json").open("ab") as stream:
            stream.write(b" ")
    if mode == "crash_once" and gate is not None and not gate.exists():
        gate.write_text("first attempt", encoding="utf-8")
        abrupt_exit()
    if mode == "crash":
        abrupt_exit()
    if mode == "timeout":
        time.sleep(300)
    if mode == "oversized_stdout":
        sys.stdout.buffer.write(b"x" * (4 * 1024 * 1024 + 1)); sys.stdout.buffer.flush()
        return 0
    if mode == "oversized_stderr":
        sys.stderr.buffer.write(b"x" * (64 * 1024 + 1)); sys.stderr.buffer.flush()
    if mode == "stderr_private":
        sys.stderr.write("PRIVATE_PROVIDER_TRACE C:/private/provider/path DO_NOT_ARCHIVE\n"); sys.stderr.flush()
    data = json.dumps(response, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8") + b"\n"
    if mode == "duplicate_key":
        data = data.replace(b'{"eog_version":1,', b'{"eog_version":1,"eog_version":1,', 1)
    if mode == "malformed":
        data = b"{not-json}\n"
    if mode == "invalid_utf8":
        data = b'{"invalid":"\xff"}\n'
    if mode == "no_newline":
        data = data[:-1]
    if mode == "fragmented":
        for offset in range(0, len(data), 31):
            sys.stdout.buffer.write(data[offset:offset + 31]); sys.stdout.buffer.flush()
            time.sleep(0.008)
    else:
        sys.stdout.buffer.write(data); sys.stdout.buffer.flush()
    if mode == "extra_output":
        sys.stdout.buffer.write(b"EXTRA\n"); sys.stdout.buffer.flush()
    if mode == "gated_exit" and gate is not None:
        Path(str(gate) + ".ready").write_text("response emitted", encoding="utf-8")
        while not gate.exists():
            time.sleep(0.005)
    if mode == "nonzero_after_valid":
        return 19
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
