"""Bounded NDJSON-v1 worker with legacy health and optional storage execution.

Only the standard library is needed at startup. An installed desktop may deploy
worker.py alone; scientific imports occur only for a storage request. Native C++
remains the authority for volume feasibility and approval.
"""
from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import importlib
import importlib.util
import json
import math
from pathlib import Path
import platform
import sys

PROTOCOL_VERSION = 1
MAX_LINE = 1024 * 1024  # UTF-8 bytes, including the terminating newline.
MAX_MESSAGE = 2000
ERROR_CODES = frozenset({
    "not_implemented", "invalid_request", "unsupported_version", "unsupported_method",
    "invalid_network", "invalid_controls", "invalid_parameters", "identity_mismatch",
    "runtime_unavailable", "output_too_large", "artifact_unavailable", "work_limit",
    "numerical_failure", "cancelled",
})


class _WorkerError(ValueError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code, self.message = code, message


class _Discard:
    """Discard library text without retaining an unbounded in-memory log."""
    encoding = "utf-8"
    errors = "strict"

    def write(self, value):
        return len(value)

    def flush(self):
        pass

    def isatty(self):
        return False


def _integer(value):
    return isinstance(value, int) and not isinstance(value, bool)


def _version_one(value):
    # JSON Schema's numeric const 1 also accepts 1.0, but never a JSON boolean.
    return _number(value) and value == 1


def _text(value, maximum, *, empty=False):
    if not isinstance(value, str) or len(value) > maximum or (not empty and not value):
        return False
    try:
        value.encode("utf-8", errors="strict")
        return True
    except UnicodeError:
        return False


def _number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        return math.isfinite(float(value))
    except (ValueError, OverflowError):
        return False


def _keys(value, required, optional=()):
    return (isinstance(value, dict) and set(required) <= value.keys() and
            value.keys() <= set(required) | set(optional))


def _error(request_id, code, message):
    if not isinstance(code, str) or code not in ERROR_CODES:
        code, message = "numerical_failure", "Scientific execution failed."
    message = str(message).encode("utf-8", errors="replace").decode("utf-8")[:MAX_MESSAGE]
    return {"protocol_version": PROTOCOL_VERSION, "request_id": request_id,
            "status": "error", "error": {"code": code, "message": message}}


def _encode(response):
    return (json.dumps(response, ensure_ascii=False, separators=(",", ":"),
                       allow_nan=False) + "\n").encode("utf-8", errors="strict")


def _detected(name):
    try:
        return importlib.util.find_spec(name) is not None
    except (ImportError, AttributeError, ValueError, OSError):
        return False


def _mapper_deployed():
    try:
        return Path(__file__).with_name("worker_storage.py").is_file()
    except OSError:
        return False


def _finite_json(value):
    if isinstance(value, dict):
        return all(_text(key, MAX_LINE, empty=True) and _finite_json(item)
                   for key, item in value.items())
    if isinstance(value, list):
        return all(_finite_json(item) for item in value)
    if isinstance(value, str):
        return _text(value, MAX_LINE, empty=True)
    if value is None or isinstance(value, bool):
        return True
    return _number(value)


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON object key.")
        result[key] = value
    return result


def _constant(_):
    raise ValueError("JSON numbers must be finite.")


def _decode(line):
    request = json.loads(line.decode("utf-8", errors="strict"),
                         object_pairs_hook=_pairs, parse_constant=_constant)
    # Limit framing depth before the recursive semantic checks. Syntactically
    # valid out-of-range numbers are checked after extracting the request ID.
    pending = [(request, 0)]
    while pending:
        value, depth = pending.pop()
        if depth > 128:
            raise ValueError("JSON request nesting exceeds the framing limit.")
        if isinstance(value, dict):
            pending.extend((item, depth + 1) for item in value.values())
        elif isinstance(value, list):
            pending.extend((item, depth + 1) for item in value)
    return request


def _storage_result(result, payload):
    """Validate publication semantics independently of the numerical mapper."""
    required = {"method", "method_version", "identity", "provenance", "time_h",
                "time_indices", "tank_ids", "volumes_m3", "cost", "warnings"}
    if not _keys(result, required, {"derivatives"}):
        raise _WorkerError("numerical_failure", "Storage result has an invalid shape.")
    if result["identity"] != payload.get("identity"):
        raise _WorkerError("identity_mismatch", "Storage result identity does not match the request.")
    if (result["method"] != "storage_rollout" or
            isinstance(result["method_version"], bool) or
            not isinstance(result["method_version"], (int,float)) or
            result["method_version"] not in (1,2) or result["method_version"] != payload.get("method_version")):
        raise _WorkerError("numerical_failure", "Storage result method does not match the request.")
    provenance = result["provenance"]
    if (not _keys(provenance, {"implementation", "implementation_version", "dtype",
                               "device", "execution_status"}) or
            provenance["implementation"] != "torch_storage_float64" or
            not _text(provenance["implementation_version"], 128) or
            provenance["dtype"] != "float64" or provenance["device"] != "cpu" or
            provenance["execution_status"] != "completed"):
        raise _WorkerError("numerical_failure", "Storage result provenance is invalid.")
    times, indices = result["time_h"], result["time_indices"]
    try:
        horizon, step = float(payload["horizon_h"]), float(payload["step_h"])
        if (not math.isfinite(horizon) or not math.isfinite(step) or
                not 0 < horizon <= 24 or step <= 0 or horizon > 10000 * step):
            raise ValueError("Unsupported captured sample grid")
        # Independent publication check: reconstruct rounded requested multiples,
        # then the exact horizon. Do not trust the mapper's returned last index or
        # a rounded quotient, which can omit a one-ULP terminal interval.
        expected_times = [0.0]
        for ordinal in range(1, 10001):
            sample = ordinal * step
            if sample >= horizon:
                expected_times.append(horizon)
                break
            if sample <= expected_times[-1]:
                raise ValueError("Nonincreasing captured sample grid")
            expected_times.append(sample)
        else:
            raise ValueError("Captured sample grid exceeds the update limit")
        last_index = len(expected_times) - 1
        expected_tanks = [asset["id"] for asset in payload["network"]["assets"]
                          if asset["kind"] == "tank"]
        maximum = payload["preview"]["max_times"]
    except (KeyError, TypeError, ValueError, OverflowError, ZeroDivisionError) as exc:
        raise _WorkerError("numerical_failure", "Storage result cannot bind to its request.") from exc
    if (not isinstance(times, list) or not isinstance(indices, list) or
            not 2 <= len(times) <= min(256, maximum) or len(indices) != len(times) or
            any(not _number(value) for value in times) or
            any(not _integer(index) or not 0 <= index <= last_index for index in indices) or
            indices[0] != 0 or indices[-1] != last_index or
            times[0] != 0.0 or times[-1] != horizon or
            any(left >= right for left, right in zip(indices, indices[1:])) or
            any(left >= right for left, right in zip(times, times[1:])) or
            any(time != expected_times[index] for time, index in zip(times, indices))):
        raise _WorkerError("numerical_failure", "Storage preview times/indices do not cover the requested horizon.")
    if result["tank_ids"] != expected_tanks:
        raise _WorkerError("identity_mismatch", "Storage result tank IDs do not match captured order.")
    volumes = result["volumes_m3"]
    if (not isinstance(volumes, list) or len(volumes) != len(expected_tanks) or
            any(not isinstance(row, list) or len(row) != len(times) or
                any(not _number(value) for value in row) for row in volumes) or
            not _number(result["cost"]) or result["cost"] < 0):
        raise _WorkerError("numerical_failure", "Storage result contains invalid numeric outputs.")
    warnings = result["warnings"]
    if (not isinstance(warnings, list) or len(warnings) > 32 or
            any(not _text(warning, 1000, empty=True) for warning in warnings)):
        raise _WorkerError("numerical_failure", "Storage result warnings are invalid.")
    if payload.get("derivatives"):
        derivative = result.get("derivatives")
        ids = [control["control_id"] for control in payload["controls"]]
        if (not _keys(derivative, {"control_ids", "terminal_volume_jacobian_h", "cost_gradient"}) or
                derivative["control_ids"] != ids):
            raise _WorkerError("numerical_failure", "Storage derivatives do not match selected controls.")
        matrix, gradient = derivative["terminal_volume_jacobian_h"], derivative["cost_gradient"]
        if (not isinstance(matrix, list) or len(matrix) != len(expected_tanks) or
                any(not isinstance(row, list) or len(row) != len(ids) or
                    any(not _number(value) for value in row) for row in matrix) or
                not isinstance(gradient, list) or len(gradient) != len(ids) or
                any(not _number(value) for value in gradient)):
            raise _WorkerError("numerical_failure", "Storage derivative dimensions/values are invalid.")
    elif "derivatives" in result:
        raise _WorkerError("numerical_failure", "Storage result contains unrequested derivatives.")
    if not _finite_json(result):
        raise _WorkerError("numerical_failure", "Storage result is not finite JSON data.")


def _fit_storage(response):
    """Keep all tanks/derivatives; reduce only explicitly indexed preview times."""
    result = response["result"]
    warned = False
    while len(_encode(response)) > MAX_LINE:
        count = len(result["time_h"])
        if count <= 2:
            raise _WorkerError("output_too_large", "Requested storage result exceeds the 1 MiB protocol limit.")
        target = max(2, count // 2)
        positions = [index * (count - 1) // (target - 1) for index in range(target)]
        result["time_h"] = [result["time_h"][index] for index in positions]
        result["time_indices"] = [result["time_indices"][index] for index in positions]
        result["volumes_m3"] = [[row[index] for index in positions] for row in result["volumes_m3"]]
        if not warned and len(result["warnings"]) < 32:
            result["warnings"].append("Preview times reduced to fit the 1 MiB protocol budget; indices retain original samples.")
        warned = True


class WorkerSession:
    def __init__(self):
        self.storage_execution_verified = False

    def handle(self, request):
        request_id = request.get("request_id") if isinstance(request, dict) else None
        if not _text(request_id, 128):
            request_id = None
        try:
            if not _keys(request, {"protocol_version", "request_id", "method"}, {"payload"}) or request_id is None:
                raise _WorkerError("invalid_request", "Request needs a valid envelope and nonempty request_id of at most 128 characters.")
            if not _finite_json(request):
                raise _WorkerError("invalid_request", "Request values must be finite and valid Unicode.")
            if not _version_one(request["protocol_version"]):
                raise _WorkerError("unsupported_version", "Unsupported protocol_version.")
            method = request["method"]
            if method not in ("health", "capabilities", "storage_rollout", "pipe_transport"):
                raise _WorkerError("unsupported_method", "Unsupported worker method.")
            payload = request.get("payload", {})
            if not isinstance(payload, dict) or (method != "health" and "payload" not in request):
                raise _WorkerError("invalid_request", "A payload object is required.")
            if method == "health":
                if payload:
                    raise _WorkerError("invalid_request", "Health payload must be empty.")
                result = {"python_version": platform.python_version(), "methods": ["health"],
                          "torch_detected": _detected("torch"),
                          "physicsnemo_detected": _detected("physicsnemo"),
                          "scientific_execution": "not_implemented"}
            elif method == "capabilities":
                if not _keys(payload, {"capability_version"}):
                    raise _WorkerError("invalid_request", "Capabilities payload needs only capability_version.")
                if not _version_one(payload["capability_version"]):
                    raise _WorkerError("unsupported_version", "Unsupported capability_version.")
                deployed = _mapper_deployed()
                result = {"capability_version": 1, "methods": [
                    {"name": "health", "method_version": 1, "implemented": True, "execution_verified": True},
                    {"name": "capabilities", "method_version": 1, "implemented": True, "execution_verified": True},
                    {"name": "storage_rollout", "method_version": 2, "implemented": deployed,
                     "execution_verified": deployed and self.storage_execution_verified},
                    {"name": "pipe_transport", "method_version": 1, "implemented": False, "execution_verified": False}],
                    "max_line_bytes": MAX_LINE, "max_preview_times": 256,
                    "max_preview_cells": 128, "artifacts_supported": False}
            elif method == "pipe_transport":
                raise _WorkerError("not_implemented", "Python pipe_transport is unavailable; v1 requires authoritative native FVM.")
            else:
                if not _mapper_deployed():
                    raise _WorkerError("runtime_unavailable", "Storage rollout support is not deployed with this worker.")
                sink = _Discard()
                try:
                    with redirect_stdout(sink), redirect_stderr(sink):
                        mapper = importlib.import_module("worker_storage")
                        result = mapper.storage_rollout(payload)
                except (ImportError, OSError) as exc:
                    raise _WorkerError("runtime_unavailable", "The storage scientific runtime is unavailable.") from exc
                except MemoryError as exc:
                    raise _WorkerError("work_limit", "Storage calculation exceeded available work resources.") from exc
                except Exception as exc:
                    if isinstance(exc, getattr(locals().get("mapper"), "StorageError", ())):
                        raise _WorkerError(exc.code, exc.message) from exc
                    raise _WorkerError("numerical_failure", "Storage scientific execution failed.") from exc
                _storage_result(result, payload)
            response = {"protocol_version": PROTOCOL_VERSION, "request_id": request_id,
                        "status": "completed", "result": result}
            if method == "storage_rollout":
                _fit_storage(response)
                _storage_result(response["result"], payload)
            if len(_encode(response)) > MAX_LINE:
                raise _WorkerError("output_too_large", "Response exceeds the 1 MiB protocol limit.")
            if method == "storage_rollout":
                self.storage_execution_verified = True
            return response
        except _WorkerError as exc:
            return _error(request_id, exc.code, exc.message)
        except (ValueError, TypeError, OverflowError, RecursionError):
            return _error(request_id, "numerical_failure", "Scientific response could not be represented safely.")


_session = WorkerSession()


def handle(request):
    """Compatibility entry point; verification belongs to this worker process."""
    return _session.handle(request)


def main():
    while line := sys.stdin.buffer.readline(MAX_LINE + 1):
        oversized = len(line) > MAX_LINE
        try:
            if oversized:
                raise ValueError("Request exceeds the 1 MiB protocol limit.")
            if not line.endswith(b"\n"):
                raise ValueError("Request must end with a newline.")
            response = handle(_decode(line))
        except (ValueError, UnicodeError, RecursionError):
            response = _error(None, "invalid_request", "Invalid UTF-8 JSON frame; require one finite object and a newline within 1 MiB.")
        try:
            sys.stdout.buffer.write(_encode(response))
            sys.stdout.buffer.flush()
        except BrokenPipeError:
            return
        if oversized:
            return


if __name__ == "__main__":
    main()
