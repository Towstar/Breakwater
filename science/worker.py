"""Minimal versioned NDJSON worker boundary. Scientific methods are future work."""
import importlib.util
import json
import platform
import sys

PROTOCOL_VERSION = 1
MAX_LINE = 1024 * 1024


def handle(request):
    if not isinstance(request, dict):
        raise ValueError("Request must be an object")
    request_id = request.get("request_id")
    if not isinstance(request_id, str) or not request_id:
        raise ValueError("A nonempty string request_id is required")
    if request.get("protocol_version") != PROTOCOL_VERSION:
        raise ValueError("Unsupported protocol_version")
    if request.get("method") != "health":
        return {"protocol_version": 1, "request_id": request_id, "status": "error",
                "error": {"code": "not_implemented", "message": "Only health is implemented in the minimum base"}}
    return {"protocol_version": 1, "request_id": request_id, "status": "completed", "result": {
        "python_version": platform.python_version(), "methods": ["health"],
        "torch_detected": importlib.util.find_spec("torch") is not None,
        "physicsnemo_detected": importlib.util.find_spec("physicsnemo") is not None,
        "scientific_execution": "not_implemented"}}


def main():
    while line := sys.stdin.buffer.readline(MAX_LINE + 1):
        try:
            if len(line) > MAX_LINE:
                raise ValueError("Request exceeds 1 MiB")
            request = json.loads(line)
            response = handle(request)
        except (ValueError, UnicodeError) as error:
            response = {"protocol_version": 1, "request_id": None, "status": "error",
                        "error": {"code": "invalid_request", "message": str(error)}}
        print(json.dumps(response, allow_nan=False), flush=True)
        if len(line) > MAX_LINE:
            break


if __name__ == "__main__":
    main()
