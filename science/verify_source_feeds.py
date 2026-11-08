"""Executed native/Torch/NDJSON check of explicit source allocations.

The four physical fixtures and affine expectations below are independent of the
native arrays. Shares are configuration; only production curtailment is a control.
Every attempt requires a new directory, including failures.
"""
from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
from importlib.metadata import version
import json
import math
from pathlib import Path
import platform
import subprocess
import sys
import time

import torch
from breakwater_science import StorageModel
from verify_rollout import FORWARD_ATOL, FORWARD_RTOL, GRADIENT_ATOL, GRADIENT_RTOL, PERTURBATIONS, _compare
from verify_worker import ROOT, SCIENCE, MAX_LINE, _validators, _request, _identity, _capabilities, project_network

CASES = {
    "balanced": ((5000, 5000), 0., (80., 30.), (8., 8.), 72., (-1.5, -1.5), 4.5),
    "asymmetric": ((7500, 2500), 2., (80., 30.), (10.5, 3.5), 70.5, (-2.25, -.75), 5.25),
    "four_way": ((1000, 2000, 3000, 4000), 4., (0., 0., 0., 0.), (1.2, 2.4, 3.6, 4.8), 168., (-.3, -.6, -.9, -1.2), 6.),
    "zero_share": ((0, 10000), 2., (80., 30.), (0., 14.), 102., (0., -3.), 3.),
}
TIMES = [k * .025 for k in range(120)] + [3.]


def expected_network(name):
    shares, curtailed, initial, *_ = CASES[name]
    return {"assets": [dict(id=f"tank_{i}", label=f"Tank {i+1}", kind="tank", capacity_m3=200., initial_m3=value)
                       for i, value in enumerate(initial)] +
            [dict(id="production", label="Production B", kind="source", inflow_m3h=16., curtailment_m3h=curtailed,
                  curtailment_max_m3h=8., cost_per_m3=5. if len(shares) == 4 else 3.)],
            "routes": [dict(id=f"feed_{i}", source_id="production", target_id=f"tank_{i}", capacity_m3h=0. if name == "zero_share" and i == 0 else 16.,
                            setpoint_m3h=0., has_pump=False, available=True, cost_per_m3=float(i+1), source_share_bps=share)
                       for i, share in enumerate(shares)]}


def validate_case(case):
    name = case["id"]
    if name not in CASES or case["horizon_h"] != 3. or case["step_h"] != .025:
        raise AssertionError("Wrong native fixture identity/configuration")
    network = project_network(case["network"])
    for route, raw in zip(network["routes"], case["network"]["routes"]):
        route["source_share_bps"] = raw["source_share_bps"]
    if network != expected_network(name):
        raise AssertionError("Native physical inputs differ from fixed expectations")
    _, _, initial, rates, cost, *_ = CASES[name]
    volumes = [[v+q*t for t in TIMES] for v, q in zip(initial, rates)]
    if set(case["solvers"]) != {"sparse", "dense", "affine"}:
        raise AssertionError("Missing native solver")
    metrics = {}
    for method, result in case["solvers"].items():
        if result["time_h"] != TIMES or result["tank_ids"] != [f"tank_{i}" for i in range(len(initial))]:
            raise AssertionError("Native timestamps/order differ exactly")
        residual = result["mass_residual_m3"]
        if result["feasible"] is not True or not math.isfinite(residual) or not 0 <= residual <= 1e-9:
            raise AssertionError("Native feasibility/conservation failed")
        metrics[method] = dict(volume_max_error_m3=_compare(torch.tensor(result["volumes_m3"], dtype=torch.float64), volumes, FORWARD_ATOL, FORWARD_RTOL, method),
                               cost_error=_compare(torch.tensor(result["cost"], dtype=torch.float64), cost, FORWARD_ATOL, FORWARD_RTOL, method+" cost"),
                               mass_residual_m3=result["mass_residual_m3"])
    return network, volumes, metrics


def validate_wire(payload, wire):
    """Check exact identities/grid and provenance for every scientific response."""
    tanks = [a["id"] for a in payload["network"]["assets"] if a["kind"] == "tank"]
    if (wire["method"] != "storage_rollout" or wire["method_version"] != 2 or
            wire["identity"] != payload["identity"] or wire["time_h"] != TIMES or
            wire["time_indices"] != list(range(121)) or wire["tank_ids"] != tanks):
        raise AssertionError("Worker changed exact version/identity/grid/ordering")
    expected = {"implementation": "torch_storage_float64", "implementation_version": "torch-constant-flow-0.2.0+worker-v1",
                "dtype": "float64", "device": "cpu", "execution_status": "completed"}
    if wire["provenance"] != expected:
        raise AssertionError("Worker used unexpected implementation")
    if len(wire["volumes_m3"]) != len(tanks) or any(len(row) != 121 or not all(math.isfinite(v) for v in row) for row in wire["volumes_m3"]) or not math.isfinite(wire["cost"]):
        raise AssertionError("Worker fields/cost have wrong shape or nonfinite values")


def verify(native, output):
    started = time.perf_counter()
    report = {"started_utc": datetime.now(timezone.utc).isoformat(), "status": "failed", "command": sys.argv,
              "python": sys.version, "platform": platform.platform(), "cases": []}
    try:
        report["packages"] = {p: version(p) for p in ("torch", "numpy", "nvidia-physicsnemo", "jsonschema")}
        native = native.resolve(strict=True)
        run = subprocess.run([str(native)], capture_output=True, timeout=30, check=False)
        (output/"native.json").write_bytes(run.stdout)
        (output/"native.stderr").write_bytes(run.stderr)
        report.update(native=str(native), native_sha256=hashlib.sha256(native.read_bytes()).hexdigest(), native_exit=run.returncode)
        if run.returncode or run.stderr:
            raise AssertionError("Native exporter failed; raw attempt retained")
        document = json.loads(run.stdout)
        if document["fixture_version"] != 1 or document["solver_version"] != "constant-flow-0.2.0" or [c["id"] for c in document["cases"]] != list(CASES):
            raise AssertionError("Wrong source-feed exporter/provenance/case order")
        requests = [_request("capabilities:before", {"capability_version": 1}, "capabilities")]
        records = []
        for index, case in enumerate(document["cases"]):
            name = case["id"]
            network, expected, metrics = validate_case(case)
            model = StorageModel(case["network"])
            full = model.rollout(horizon_h=3., step_h=.025)
            if full.time_h.tolist() != TIMES or list(model.tank_ids) != [f"tank_{i}" for i in range(len(expected))]:
                raise AssertionError("Standalone Torch changed timestamps/order")
            metrics["torch"] = {"volume_max_error_m3": _compare(full.volumes_m3, expected, FORWARD_ATOL, FORWARD_RTOL, "Torch analytical fields")}
            p = dict(method_version=2, identity=_identity(index, network), network=network, controls=[], horizon_h=3., step_h=.025,
                     derivatives=name != "balanced", preview={"max_times": 256})
            if p["derivatives"]:
                p["controls"] = [dict(control_id="source:production:curtail", value=CASES[name][1])]
            forward_index = len(requests)
            requests.append(_request(name, p))
            differences = []
            if p["derivatives"]:
                for relative in PERTURBATIONS:
                    h = relative*max(1., CASES[name][1])
                    indices = []
                    for sign in (1, -1):
                        altered = copy.deepcopy(p)
                        altered["derivatives"] = False
                        altered["controls"][0]["value"] += sign*h
                        indices.append(len(requests))
                        requests.append(_request(f"{name}:fd:{relative}:{sign}", altered))
                    differences.append((h, *indices))
            records.append((name, p, expected, metrics, forward_index, differences))
        requests.append(_request("capabilities:after", {"capability_version": 1}, "capabilities"))
        req_validator, resp_validator = _validators()
        for request in requests:
            req_validator.validate(request)
        encoded = b"".join(json.dumps(r, ensure_ascii=False, allow_nan=False).encode()+b"\n" for r in requests)
        (output/"requests.ndjson").write_bytes(encoded)
        run = subprocess.run([sys.executable, "-u", str(SCIENCE/"worker.py")], input=encoded, capture_output=True, timeout=180, cwd=SCIENCE)
        (output/"responses.ndjson").write_bytes(run.stdout)
        (output/"worker.stderr").write_bytes(run.stderr)
        report.update(worker_exit=run.returncode, request_count=len(requests))
        if run.returncode or run.stderr:
            raise AssertionError("Actual worker failed or emitted stderr")
        lines = run.stdout.splitlines(keepends=True)
        if len(lines) != len(requests) or any(not line.endswith(b"\n") or len(line)>MAX_LINE for line in lines):
            raise AssertionError("Worker framing/count/byte-limit failure")
        responses = []
        for request, line in zip(requests, lines):
            response = json.loads(line)
            resp_validator.validate(response)
            if response["request_id"] != request["request_id"] or response["status"] != "completed":
                raise AssertionError("Worker response identity/status failure")
            if request["method"] == "storage_rollout":
                validate_wire(request["payload"], response["result"])
            responses.append(response["result"])
        before, after = (_capabilities(responses[i])["storage_rollout"] for i in (0, -1))
        if before["execution_verified"] or not after["execution_verified"] or before["method_version"] != 2:
            raise AssertionError("Capabilities did not track actual split execution")
        for name, payload, expected, metrics, index, differences in records:
            wire = responses[index]
            metrics["worker"] = dict(volume_max_error_m3=_compare(torch.tensor(wire["volumes_m3"], dtype=torch.float64), expected, FORWARD_ATOL, FORWARD_RTOL, "Worker affine fields"),
                                     cost_error=_compare(torch.tensor(wire["cost"], dtype=torch.float64), CASES[name][4], FORWARD_ATOL, FORWARD_RTOL, "Worker analytical cost"))
            if differences:
                deriv = wire["derivatives"]
                if deriv["control_ids"] != ["source:production:curtail"]:
                    raise AssertionError("Derivative control identity mismatch")
                actual = torch.tensor([r[0] for r in deriv["terminal_volume_jacobian_h"]]+deriv["cost_gradient"], dtype=torch.float64)
                expected_deriv = [*CASES[name][5], CASES[name][6]]
                metrics["analytic_gradient_max_error"] = _compare(actual, expected_deriv, GRADIENT_ATOL, GRADIENT_RTOL, "Analytical derivatives")
                metrics["central_differences"] = []
                for h, plus, minus in differences:
                    vectors = [torch.tensor([r[-1] for r in responses[i]["volumes_m3"]]+[responses[i]["cost"]], dtype=torch.float64) for i in (plus, minus)]
                    error = _compare((vectors[0]-vectors[1])/(2*h), expected_deriv, GRADIENT_ATOL, GRADIENT_RTOL, "Worker central differences")
                    metrics["central_differences"].append(dict(step_m3h=h, maximum_error=error))
            report["cases"].append(dict(id=name, metrics=metrics))
        report["status"] = "passed"
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
    finally:
        report["elapsed_s"] = time.perf_counter()-started
        sources = sorted(p for scope in ("contracts", "engine", "science", "desktop") for p in (ROOT/scope).rglob("*")
                         if p.is_file() and p.suffix in {".py", ".cpp", ".hpp", ".qml", ".json", ".txt"}
                         and not any(x in {"out", ".venv", "__pycache__", "evidence"} for x in p.relative_to(ROOT).parts))
        report["source_sha256"] = {str(p.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
        (output/"report.json").write_text(json.dumps(report, indent=2, allow_nan=False)+"\n", encoding="utf-8")
    return report["status"] == "passed"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    passed = verify(args.native, args.output)
    print(f"Source-feed verification {'passed' if passed else 'FAILED'}: {args.output/'report.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
