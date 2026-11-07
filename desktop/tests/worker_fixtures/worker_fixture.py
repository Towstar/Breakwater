"""Deliberately controlled protocol peer for the native process-boundary tests."""
import json
import os
import sys
import time
import uuid

mode = os.environ.get("BREAKWATER_WORKER_FIXTURE_MODE", "valid")
request = json.loads(sys.stdin.readline())
assert request["protocol_version"] == 1
assert request["method"] == "health"
assert request["payload"] == {}
uuid.UUID(request["request_id"])

response = {
    "protocol_version": 1,
    "request_id": request["request_id"],
    "status": "completed",
    "result": {
        "python_version": "3.12.14",
        "methods": ["health"],
        "torch_detected": True,
        "physicsnemo_detected": True,
        "scientific_execution": "not_implemented",
    },
}

if mode == "hang":
    time.sleep(30)
elif mode == "crash":
    os._exit(7)
elif mode == "empty":
    pass
elif mode == "oversized_stdout":
    sys.stdout.write("x" * (1024 * 1024 + 1))
    sys.stdout.flush()
elif mode == "oversized_stderr":
    sys.stderr.write("private-secret-marker" * 4000)
    sys.stderr.flush()
    print(json.dumps(response), flush=True)
elif mode == "malformed":
    print("not JSON: private-secret-marker", flush=True)
else:
    if mode == "wrong_id":
        response["request_id"] = "old-or-foreign-request"
    elif mode == "wrong_protocol":
        response["protocol_version"] = 2
    elif mode == "wrong_status":
        response["status"] = "running"
    elif mode == "scientific_claim":
        response["result"]["scientific_execution"] = "executed"
    elif mode == "wrong_boolean":
        response["result"]["torch_detected"] = "true"
    elif mode == "worker_error":
        response.pop("result")
        response["status"] = "error"
        response["error"] = {"code": "private-secret-marker", "message": "private-secret-marker"}
    elif mode == "extra_private_fields":
        response["result"]["private"] = "private-secret-marker"
        response["private"] = "private-secret-marker"
        sys.stderr.write("private-secret-marker diagnostic\n")
        sys.stderr.flush()
    elif mode == "credential_environment" and "NVIDIA_API_KEY" in os.environ:
        response["result"]["scientific_execution"] = "credential_inherited"
    encoded = json.dumps(response)
    if mode == "partial":
        for start in range(0, len(encoded), 7):
            sys.stdout.write(encoded[start:start + 7])
            sys.stdout.flush()
            time.sleep(0.003)
        sys.stdout.write("\n")
        sys.stdout.flush()
    elif mode == "no_newline":
        sys.stdout.write(encoded)
        sys.stdout.flush()
    elif mode == "extra_line":
        sys.stdout.write(encoded + "\nprivate-secret-marker\n")
        sys.stdout.flush()
    elif mode == "crash_after_reply":
        print(encoded, flush=True)
        os._exit(3)
    else:
        print(encoded, flush=True)
