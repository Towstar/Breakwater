"""Controlled scientific protocol peer. Values are test fixtures, not science evidence."""
import ctypes
import json
import os
from pathlib import Path
import sys
import time

MODE = os.environ.get("BREAKWATER_STORAGE_FIXTURE_MODE", "valid")
LIMIT = 1024 * 1024
pid_file = os.environ.get("BREAKWATER_STORAGE_FIXTURE_PIDFILE")
if pid_file:
    path = Path(pid_file)
    if path.is_file():
        prior = int(path.read_text())
        if os.name == "nt":
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.OpenProcess.restype = ctypes.c_void_p
            handle = kernel.OpenProcess(0x00100000, False, prior)
            alive = bool(handle and kernel.WaitForSingleObject(ctypes.c_void_p(handle), 0) == 0x102)
            if handle:
                kernel.CloseHandle(ctypes.c_void_p(handle))
        else:
            try:
                os.kill(prior, 0)
                alive = True
            except ProcessLookupError:
                alive = False
        if alive:
            os._exit(19)
    path.write_text(str(os.getpid()))


def caps(verified):
    result = {"capability_version": 1, "methods": [
        {"name": name, "method_version": 1,
         "implemented": name != "pipe_transport", "execution_verified": name in ("health", "capabilities") or (name == "storage_rollout" and verified)}
        for name in ("health", "capabilities", "storage_rollout", "pipe_transport")],
        "max_line_bytes": LIMIT, "max_preview_times": 256, "max_preview_cells": 128, "artifacts_supported": False}
    if MODE == "not_deployed":
        result["methods"][2]["implemented"] = False
    elif MODE == "bad_capability_version":
        result["capability_version"] = 2
    elif MODE == "duplicate_methods":
        result["methods"][3] = result["methods"][0]
    elif MODE == "bad_limits":
        result["max_line_bytes"] = LIMIT + 1
    elif MODE == "artifacts":
        result["artifacts_supported"] = True
    elif MODE == "preverified":
        result["methods"][2]["execution_verified"] = True
    elif MODE == "post_unverified":
        result["methods"][2]["execution_verified"] = False
    return result


def rollout(payload):
    assets = payload["network"]["assets"]
    by_id = {item["id"]: item for item in assets}
    selected = {item["control_id"]: item["value"] for item in payload["controls"]}
    tanks = [item for item in assets if item["kind"] == "tank"]
    rates = {item["id"]: 0.0 for item in tanks}
    cost = 0.0
    for source in assets:
        if source["kind"] == "source":
            cost += selected.get("source:" + source["id"] + ":curtail", source.get("curtailment_m3h", 0.0)) * source.get("cost_per_m3", 0.0)
    for route in payload["network"]["routes"]:
        source, target = by_id[route["source_id"]], by_id[route["target_id"]]
        if source["kind"] == "source":
            flow = source.get("inflow_m3h", 0.0) - selected.get("source:" + source["id"] + ":curtail", source.get("curtailment_m3h", 0.0))
        else:
            flow = selected.get("route:" + route["id"] + ":flow", route["setpoint_m3h"])
            rates[source["id"]] -= flow
        if target["kind"] == "tank":
            rates[target["id"]] += flow
        else:
            cost += flow * target.get("cost_per_m3", 0.0)
        cost += flow * route["cost_per_m3"]
    horizon, step = payload["horizon_h"], payload["step_h"]
    grid = [0.0]
    for index in range(1, 10001):
        rounded_multiple = index * step
        if rounded_multiple >= horizon:
            grid.append(horizon)
            break
        assert rounded_multiple > grid[-1]
        grid.append(rounded_multiple)
    else:
        raise ValueError("Controlled fixture exceeded 10,000 updates")
    updates = len(grid) - 1
    count = min(updates + 1, payload["preview"]["max_times"])
    indices = [index * updates // (count - 1) for index in range(count)]
    times = [grid[index] for index in indices]
    return {"method": "storage_rollout", "method_version": 1, "identity": payload["identity"],
            "provenance": {"implementation": "torch_storage_float64", "implementation_version": "test-fixture-only",
                           "dtype": "float64", "device": "cpu", "execution_status": "completed"},
            "time_h": times, "time_indices": indices, "tank_ids": [item["id"] for item in tanks],
            "volumes_m3": [[item.get("initial_m3", 0.0) + rates[item["id"]] * value for value in times] for item in tanks],
            "cost": cost * horizon, "warnings": ["Controlled test fixture; no scientific execution claimed."]}


def emit(response):
    raw = json.dumps(response, separators=(",", ":"), ensure_ascii=False).encode("utf-8") + b"\n"
    if MODE in ("large_frames", "aggregate_limit"):
        raw = b" " * (LIMIT - len(raw)) + raw
    if MODE == "partial":
        for start in range(0, len(raw), 11):
            sys.stdout.buffer.write(raw[start:start + 11])
            sys.stdout.buffer.flush()
            time.sleep(0.002)
    else:
        sys.stdout.buffer.write(raw)
        sys.stdout.buffer.flush()


verified = False
for raw in sys.stdin.buffer:
    request = json.loads(raw)
    method = request["method"]
    if MODE == "hang":
        time.sleep(30)
    if MODE == "oversized_frame":
        sys.stdout.buffer.write(b" " * LIMIT + b"\n")
        sys.stdout.buffer.flush()
        break
    if MODE == "oversized_stderr":
        sys.stderr.write("private-secret-marker" * 4000)
        sys.stderr.flush()
    if MODE == "invalid_utf8":
        sys.stdout.buffer.write(b"\xff\n")
        sys.stdout.buffer.flush()
        break
    response = {"protocol_version": 1, "request_id": request["request_id"], "status": "completed"}
    if method == "capabilities":
        response["result"] = caps(verified)
    else:
        assert method == "storage_rollout"
        if MODE == "runtime_error":
            response["status"] = "error"
            response["error"] = {"code": "runtime_unavailable", "message": "private-secret-marker"}
        else:
            result = rollout(request["payload"])
            if MODE == "wrong_identity":
                result["identity"] = dict(result["identity"], volume_run_id="foreign-run")
            elif MODE == "wrong_tank":
                result["tank_ids"][0] = "foreign-tank"
            elif MODE == "wrong_index":
                result["time_indices"][-1] -= 1
            elif MODE == "wrong_time":
                result["time_h"][-1] -= 0.01
            elif MODE == "finite_disagreement":
                result["volumes_m3"][1][-1] += 1.0
            elif MODE == "null_sample":
                result["volumes_m3"][0][0] = None
            elif MODE == "wrong_shape":
                result["volumes_m3"][0].pop()
            elif MODE == "wrong_provenance":
                result["provenance"]["dtype"] = "float32"
            elif MODE == "derivatives":
                result["derivatives"] = {}
            elif MODE == "result_artifact":
                result["artifact"] = {}
            elif MODE == "private_result":
                result["private"] = "private-secret-marker"
            elif MODE == "unicode_warning":
                result["warnings"] = ["\U0001f30a" * 1000]
            elif MODE == "oversized_warning":
                result["warnings"] = ["\U0001f30a" * 1001]
            response["result"] = result
            verified = True
    if MODE == "wrong_id":
        response["request_id"] = "foreign-request"
    elif MODE == "wrong_protocol":
        response["protocol_version"] = True
    elif MODE == "credential_environment" and "NVIDIA_API_KEY" in os.environ:
        response["request_id"] = "credential-was-inherited"
    emit(response)
    if MODE == "duplicate_response":
        emit(response)
    if method == "storage_rollout" and MODE == "crash_after_storage":
        os._exit(7)
phase_file = os.environ.get("BREAKWATER_STORAGE_FIXTURE_PHASEFILE")
if phase_file:
    Path(phase_file).write_text("exchange-complete")
if MODE == "crash_after_exchange":
    os._exit(8)
elif MODE == "hang_after_exchange":
    time.sleep(30)
elif MODE == "valid_linger":
    time.sleep(0.15)  # Let the lifecycle regression retain this process's original handle.
elif MODE == "aggregate_limit":
    sys.stdout.buffer.write(b"\n")
    sys.stdout.buffer.flush()
elif MODE == "trailing_noise":
    sys.stdout.buffer.write(b"private-secret-marker\n")
    sys.stdout.buffer.flush()
