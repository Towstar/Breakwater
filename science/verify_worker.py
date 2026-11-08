"""Verify real NDJSON storage execution against current native fixtures and gradients.

Requires the pinned science environment plus jsonschema. Every scientific result is
returned by an actual worker subprocess; the native exporter executes independently.
Fixture identity labels are test-only, not application run IDs or approval evidence.
"""
from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
from importlib.metadata import version as package_version
import json
from pathlib import Path
import platform
import subprocess
import sys

import torch

from breakwater_science import StorageModel
from verify_rollout import FORWARD_ATOL, FORWARD_RTOL, GRADIENT_ATOL, GRADIENT_RTOL, PERTURBATIONS, _compare

SCIENCE = Path(__file__).resolve().parent
ROOT = SCIENCE.parent
MAX_LINE = 1024 * 1024


def project_network(native):
    """Translate native fixture member names to the published project-v1 format."""
    assets = []
    for asset in native["assets"]:
        value = {key: asset[key] for key in ("id", "label", "kind")}
        if asset["kind"] == "tank":
            value.update(capacity_m3=asset["capacity_m3"], initial_m3=asset["initial_m3"])
        elif asset["kind"] == "source":
            value.update({key: asset[key] for key in
                          ("inflow_m3h", "curtailment_m3h", "curtailment_max_m3h", "cost_per_m3")})
        else:
            value.update(capacity_m3h=asset["capacity_m3"], cost_per_m3=asset["cost_per_m3"])
        assets.append(value)
    route_keys = ("id", "source_id", "target_id", "capacity_m3h", "setpoint_m3h", "has_pump", "available", "cost_per_m3")
    return {"assets": assets, "routes": [{key: route[key] for key in route_keys} for route in native["routes"]]}


def _validators():
    try:
        import jsonschema
        from referencing import Registry, Resource
    except ImportError as error:
        raise RuntimeError("Verification requires jsonschema and referencing; schema checking is mandatory.") from error
    schemas = [json.loads(path.read_text(encoding="utf-8"))
               for path in sorted((ROOT / "contracts/science").glob("*.schema.json"))]
    registry = Registry().with_resources((schema["$id"], Resource.from_contents(schema)) for schema in schemas)
    by_name = {schema["$id"].split("/")[-1]: schema for schema in schemas}
    return (jsonschema.Draft202012Validator(by_name["request-v1.schema.json"], registry=registry),
            jsonschema.Draft202012Validator(by_name["response-v1.schema.json"], registry=registry))


def _request(identifier, payload, method="storage_rollout"):
    return {"protocol_version": 1, "request_id": identifier, "method": method, "payload": payload}


def _identity(index, network):
    digest = hashlib.sha256(json.dumps(network, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()).hexdigest()
    return {"snapshot_id": f"verification-fixture:{index}", "volume_run_id": f"native-reference-fixture:{index}",
            "numerical_revision": index, "physical_hash": digest}


def _exchange(requests, request_validator, response_validator):
    lines = []
    for request in requests:
        request_validator.validate(request)
        line = json.dumps(request, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode("utf-8") + b"\n"
        if len(line) > MAX_LINE:
            raise AssertionError("Verification request exceeds the published byte budget")
        lines.append(line)
    completed = subprocess.run([sys.executable, "-u", str(SCIENCE / "worker.py")], input=b"".join(lines),
                               capture_output=True, check=False, timeout=180, cwd=SCIENCE)
    if completed.returncode or completed.stderr:
        raise AssertionError(f"Worker failed or emitted unexpected stderr: exit {completed.returncode}")
    response_lines = completed.stdout.splitlines(keepends=True)
    if len(response_lines) != len(requests):
        raise AssertionError("Worker response count does not match requests")
    responses = []
    for request, line in zip(requests, response_lines):
        if not line.endswith(b"\n") or len(line) > MAX_LINE:
            raise AssertionError("Worker violated newline/byte framing")
        response = json.loads(line.decode("utf-8"))
        response_validator.validate(response)
        if response["request_id"] != request["request_id"] or response["status"] != "completed":
            raise AssertionError(f"Worker request {request['request_id']} did not complete with matching identity: {response}")
        responses.append(response["result"])
    return responses, max(map(len, response_lines))


def _capabilities(result):
    return {item["name"]: item for item in result["methods"]}


def verify(native: Path):
    started = datetime.now(timezone.utc).isoformat()
    native = native.resolve(strict=True)
    completed = subprocess.run([str(native)], capture_output=True, encoding="utf-8", text=True,
                               check=False, timeout=30)
    if completed.returncode or completed.stderr:
        raise RuntimeError(f"Actual native exporter failed with exit {completed.returncode}")
    document = json.loads(completed.stdout)
    if document.get("fixture_version") != 1 or len(document.get("cases", [])) != 18:
        raise ValueError("Expected the current 18-case executed native fixture document")
    request_validator, response_validator = _validators()
    requests = [_request("capabilities:before", {"capability_version": 1}, "capabilities")]
    records = []
    for index, case in enumerate(document["cases"]):
        model = StorageModel(case["network"])
        network = project_network(case["network"])
        payload = {"method_version": 1, "identity": _identity(index, network), "network": network,
                   "controls": [{"control_id": item["id"], "value": item["value"]} for item in case["overrides"]],
                   **case["config"], "derivatives": False, "preview": {"max_times": 17}}
        forward_index = len(requests)
        requests.append(_request(f"fixture:{index}:forward", payload))
        selected = [control for control in model.controls if control.maximum > control.minimum]
        mids = [(control.minimum + control.maximum) / 2 for control in selected]
        gradient_payload = copy.deepcopy(payload)
        gradient_payload["controls"] = [{"control_id": control.id, "value": midpoint}
                                        for control, midpoint in zip(selected, mids)]
        gradient_payload["derivatives"] = True
        gradient_payload["preview"] = {"max_times": 2}
        gradient_index = len(requests)
        requests.append(_request(f"fixture:{index}:gradient", gradient_payload))
        differences = []
        for column, (control, midpoint) in enumerate(zip(selected, mids)):
            for relative_step in PERTURBATIONS:
                step = min(relative_step * max(1.0, abs(midpoint)), (control.maximum - control.minimum) / 8)
                if step <= 0 or midpoint + step == midpoint:
                    raise AssertionError("Central-difference step is not representable")
                indices = []
                for sign in (1, -1):
                    perturbed = copy.deepcopy(gradient_payload)
                    perturbed["derivatives"] = False
                    perturbed["controls"][column]["value"] += sign * step
                    indices.append(len(requests))
                    requests.append(_request(f"fixture:{index}:fd:{column}:{relative_step}:{sign}", perturbed))
                differences.append((column, relative_step, step, *indices))
        records.append((case, model, payload, selected, mids, forward_index, gradient_index, differences))
    requests.append(_request("capabilities:after", {"capability_version": 1}, "capabilities"))
    responses, maximum_line = _exchange(requests, request_validator, response_validator)
    before, after = _capabilities(responses[0]), _capabilities(responses[-1])
    if before["storage_rollout"]["execution_verified"] or not after["storage_rollout"]["execution_verified"]:
        raise AssertionError("Capabilities did not track actual successful session execution")
    if after["pipe_transport"]["implemented"] or responses[-1]["artifacts_supported"]:
        raise AssertionError("Worker advertised an unsupported transport/artifact method")
    rows = []
    for case, model, payload, selected, mids, forward_index, gradient_index, differences in records:
        wire = responses[forward_index]
        if wire["identity"] != payload["identity"] or wire["tank_ids"] != list(model.tank_ids):
            raise AssertionError("Scientific result identity/tank ordering mismatch")
        indices = wire["time_indices"]
        references = case["references"]
        if set(references) != {"dense_reference", "sparse", "affine"}:
            raise AssertionError("Exporter omitted an actual native solver")
        full = model.rollout({item["id"]: item["value"] for item in case["overrides"]}, **case["config"])
        if (full.time_h[0].item() != 0.0 or full.time_h[-1].item() != case["config"]["horizon_h"] or
                wire["time_h"][0] != 0.0 or wire["time_h"][-1] != case["config"]["horizon_h"]):
            raise AssertionError("Native/scientific comparison lost the exact requested endpoints")
        if len(indices) > 17 or indices != sorted(set(indices)) or indices[0] != 0 or indices[-1] != full.time_h.numel() - 1:
            raise AssertionError("Preview lost original indices or interval endpoints")
        _compare(torch.tensor(wire["time_h"], dtype=torch.float64), full.time_h[indices], 0, 0, "wire sample times")
        errors = {}
        for method, reference in references.items():
            if reference["time_h"] != full.time_h.tolist():
                raise AssertionError("Native and scientific full timestamp identities differ")
            _compare(full.volumes_m3, reference["volumes_m3"], FORWARD_ATOL, FORWARD_RTOL, "standalone/native full rollout")
            expected = [[series[i] for i in indices] for series in reference["volumes_m3"]]
            errors[method] = {
                "volume_m3": _compare(torch.tensor(wire["volumes_m3"], dtype=torch.float64), expected,
                                      FORWARD_ATOL, FORWARD_RTOL, "worker/native retained samples"),
                "cost_units": _compare(torch.tensor(wire["cost"], dtype=torch.float64), reference["cost"],
                                       FORWARD_ATOL, FORWARD_RTOL, "worker/native cost")}
        if case["name"] == "grid_requested_capacity_overflow":
            expected_endpoint = case["config"]["horizon_h"] * (2.0 ** 50)
            if (wire["volumes_m3"][0][-1] != expected_endpoint or
                    full.volumes_m3[0, -1].item() != expected_endpoint or
                    any(ref["volumes_m3"][0][-1] != expected_endpoint or ref["feasible"]
                        for ref in references.values())):
                raise AssertionError("High-flow requested endpoint mismatch/false native feasibility")
        elif case["name"].startswith("grid_") and any(not ref["feasible"] for ref in references.values()):
            raise AssertionError("Stationary grid regression was incorrectly rejected")
        gradient_wire = responses[gradient_index]["derivatives"]
        control_ids = [control.id for control in selected]
        if gradient_wire["control_ids"] != control_ids:
            raise AssertionError("Requested derivative IDs/order changed across the wire")
        values = torch.tensor(mids, dtype=torch.float64, requires_grad=True)

        def interior_run(vector):
            return model.rollout({control.id: vector[j] for j, control in enumerate(selected)}, **case["config"])

        if selected:
            expected_jacobian = torch.autograd.functional.jacobian(lambda vector: interior_run(vector).volumes_m3[:, -1], values)
            expected_cost_gradient = torch.autograd.functional.jacobian(lambda vector: interior_run(vector).cost, values)
        else:
            expected_jacobian = torch.empty((len(model.tank_ids), 0), dtype=torch.float64)
            expected_cost_gradient = torch.empty(0, dtype=torch.float64)
        actual_jacobian = torch.tensor(gradient_wire["terminal_volume_jacobian_h"], dtype=torch.float64)
        actual_cost_gradient = torch.tensor(gradient_wire["cost_gradient"], dtype=torch.float64)
        ad_jacobian_error = _compare(actual_jacobian, expected_jacobian, GRADIENT_ATOL, GRADIENT_RTOL, "wire/AD terminal Jacobian")
        ad_cost_error = _compare(actual_cost_gradient, expected_cost_gradient, GRADIENT_ATOL, GRADIENT_RTOL, "wire/AD cost gradient")
        checks = []
        for column, relative_step, step, plus_index, minus_index in differences:
            plus, minus = responses[plus_index], responses[minus_index]
            finite_terminal = (torch.tensor([series[-1] for series in plus["volumes_m3"]], dtype=torch.float64) -
                               torch.tensor([series[-1] for series in minus["volumes_m3"]], dtype=torch.float64)) / (2 * step)
            finite_cost = torch.tensor((plus["cost"] - minus["cost"]) / (2 * step), dtype=torch.float64)
            checks.append({"control_id": control_ids[column], "relative_step": relative_step, "step_m3h": step,
                           "terminal_jacobian_error_h": _compare(actual_jacobian[:, column], finite_terminal,
                               GRADIENT_ATOL, GRADIENT_RTOL, "actual worker central difference terminal"),
                           "cost_gradient_error": _compare(actual_cost_gradient[column], finite_cost,
                               GRADIENT_ATOL, GRADIENT_RTOL, "actual worker central difference cost")})
        rows.append({"name": case["name"], "tank_ids": list(model.tank_ids), "preview_indices": indices,
                     "native_samples": full.time_h.numel(), "native_feasible": references["dense_reference"]["feasible"],
                     "final_m3": [series[-1] for series in wire["volumes_m3"]], "cost_units": wire["cost"],
                     "forward_errors": errors, "requested_control_ids": control_ids,
                     "ad_jacobian_error_h": ad_jacobian_error, "ad_cost_gradient_error": ad_cost_error,
                     "central_difference_checks": checks, "provenance": wire["provenance"]})
    sources = [SCIENCE / name for name in ("worker.py", "worker_storage.py", "storage_grid.py",
                                          "verify_worker.py", "breakwater_science/rollout.py")]
    sources.extend(ROOT / name for name in ("engine/src/storage_grid.hpp", "engine/src/engine.cpp",
                                            "engine/examples/rollout_reference.cpp"))
    sources.extend(sorted((ROOT / "contracts/science").glob("*.schema.json")))
    sources.extend(path for path in (SCIENCE / "requirements-worker-tests.in", SCIENCE / "requirements-worker-tests-win-py312.txt") if path.is_file())
    return {"status": "passed", "started_utc": started, "finished_utc": datetime.now(timezone.utc).isoformat(),
            "python_version": platform.python_version(), "torch_version": torch.__version__, "device": "cpu", "dtype": "float64",
            "jsonschema_version": package_version("jsonschema"), "referencing_version": package_version("referencing"),
            "native_executable_sha256": hashlib.sha256(native.read_bytes()).hexdigest(),
            "native_solver_version": document["solver_version"], "schema_validation": "Draft 2020-12, offline pinned repository schemas",
            "request_count": len(requests), "maximum_response_line_bytes": maximum_line, "response_line_limit_bytes": MAX_LINE,
            "capabilities_before": responses[0], "capabilities_after": responses[-1],
            "forward_tolerance": {"absolute": FORWARD_ATOL, "relative": FORWARD_RTOL},
            "gradient_tolerance": {"absolute": GRADIENT_ATOL, "relative": GRADIENT_RTOL},
            "source_hashes": [{"path": path.relative_to(ROOT).as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()} for path in sources],
            "cases": rows, "scope": "Actual native fixtures and worker execution; interior selected continuous controls only. Native C++ remains feasibility authority.",
            "identity_scope": "Verification-only opaque fixture IDs and input fingerprints; not application run-store records or the Qt physical-hash algorithm."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        report = verify(args.native)
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        controls = sum(len(case["requested_control_ids"]) for case in report["cases"])
        checks = sum(len(case["central_difference_checks"]) for case in report["cases"])
        maximum = max(error["volume_m3"] for case in report["cases"] for error in case["forward_errors"].values())
        print(f"PASS | {len(report['cases'])} actual native cases | 3 solvers | {report['request_count']} real NDJSON requests")
        print(f"interior_controls={controls}; central_difference_checks={checks}; maximum_volume_error_m3={maximum:.12g}")
        print("Offline schema checks passed. Fixture identities are not approval evidence; native C++ decides feasibility.")
        return 0
    except (ValueError, AssertionError, RuntimeError, OSError, subprocess.SubprocessError) as error:
        print(f"Worker verification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
