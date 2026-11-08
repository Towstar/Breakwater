"""Execute current native references, forward comparisons and interior gradients.

This is a standalone science check, not the shared NDJSON worker protocol.
Exit 0 requires every native comparison and every reported perturbation to pass.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import subprocess
import sys

import torch

from breakwater_science import StorageModel
from breakwater_science.rollout import METHOD_VERSION

FORWARD_ATOL = 1e-9
FORWARD_RTOL = 1e-10
GRADIENT_ATOL = 1e-6
GRADIENT_RTOL = 1e-4
PERTURBATIONS = (1e-3, 1e-4, 1e-5)  # relative to max(1, |control in m3/h|)


def _compare(actual, reference, atol, rtol, context):
    expected = torch.as_tensor(reference, dtype=torch.float64, device="cpu")
    value = actual.detach()
    if value.shape != expected.shape or not bool(torch.isfinite(expected).all()):
        raise AssertionError(f"{context}: wrong shape or nonfinite reference")
    error = (value - expected).abs()
    tolerance = atol + rtol * torch.maximum(value.abs(), expected.abs())
    if not bool(torch.isfinite(value).all()) or not bool((error <= tolerance).all()):
        raise AssertionError(f"{context}: outside abs={atol}, rel={rtol} tolerance")
    return float(error.max()) if error.numel() else 0.0


def _objective(run):
    # Dimensionless smooth terminal inventory penalty, plus dimensionless cost.
    # Scales are fixed (100 m3 and 10,000 synthetic units), not data-dependent.
    return (run.volumes_m3[:, -1] / 100.0).square().sum() + run.cost / 10_000.0


def _gradients(model, config):
    # Midpoints give room for all symmetric perturbations, including controls which
    # were zero/boundary-valued in the forward fixture. Fixed controls remain fixed.
    selected = [i for i, c in enumerate(model.controls) if c.maximum > c.minimum]
    if not selected:
        return {"status": "no_variable_controls", "controls": []}
    defaults = model.default_controls()
    mids = torch.tensor([(model.controls[i].minimum + model.controls[i].maximum) / 2
                         for i in selected], dtype=torch.float64, device="cpu", requires_grad=True)

    def run(interior):
        entries = list(defaults.unbind())
        for j, i in enumerate(selected):
            entries[i] = interior[j]
        return model.rollout_vector(torch.stack(entries), **config)

    base = run(mids)
    objective_grad, = torch.autograd.grad(_objective(base), mids)
    cost_grad, = torch.autograd.grad(run(mids).cost, mids)
    terminal_jacobian = torch.autograd.functional.jacobian(
        lambda x: run(x).volumes_m3[:, -1], mids)
    # gradcheck supplies an additional independent central-difference implementation.
    gradcheck = torch.autograd.gradcheck(
        lambda x: torch.cat((run(x).volumes_m3[:, -1] / 100,
                             run(x).cost.reshape(1) / 10_000)),
        (mids,), eps=1e-6, atol=GRADIENT_ATOL, rtol=GRADIENT_RTOL)
    rows = []
    for j, i in enumerate(selected):
        c = model.controls[i]
        checks = []
        for relative_step in PERTURBATIONS:
            step = min(relative_step * max(1.0, abs(float(mids[j].detach()))),
                       (c.maximum - c.minimum) / 8)
            if step <= 0 or float(mids[j].detach()) + step == float(mids[j].detach()):
                raise AssertionError(f"Cannot represent central perturbation: {c.id}")
            plus, minus = mids.detach().clone(), mids.detach().clone()
            plus[j] += step
            minus[j] -= step
            rp, rm = run(plus), run(minus)
            finite_objective = (_objective(rp) - _objective(rm)) / (2 * step)
            finite_cost = (rp.cost - rm.cost) / (2 * step)
            finite_terminal = (rp.volumes_m3[:, -1] - rm.volumes_m3[:, -1]) / (2 * step)
            errors = {
                "objective": _compare(objective_grad[j], finite_objective, GRADIENT_ATOL,
                                      GRADIENT_RTOL, f"{c.id} objective gradient"),
                "cost": _compare(cost_grad[j], finite_cost, GRADIENT_ATOL,
                                 GRADIENT_RTOL, f"{c.id} cost gradient"),
                "terminal": _compare(terminal_jacobian[:, j], finite_terminal, GRADIENT_ATOL,
                                     GRADIENT_RTOL, f"{c.id} terminal Jacobian"),
            }
            checks.append({"relative_step": relative_step, "step_m3h": step,
                           "absolute_errors": errors})
        rows.append({"id": c.id, "value_m3h": float(mids[j].detach()),
                     "objective_gradient_per_m3h": float(objective_grad[j]),
                     "cost_gradient_units_per_m3h": float(cost_grad[j]),
                     "terminal_jacobian_h": terminal_jacobian[:, j].tolist(),
                     "checks": checks})
    return {"status": "passed", "torch_gradcheck": bool(gradcheck), "controls": rows}


def verify(native: Path):
    native = native.resolve(strict=True)
    completed = subprocess.run([str(native)], capture_output=True, text=True,
                               encoding="utf-8", check=False, timeout=30)
    if completed.returncode:
        raise RuntimeError(f"Native exporter failed ({completed.returncode}): {completed.stderr.strip()}")
    document = json.loads(completed.stdout)
    if document.get("fixture_version") != 1 or not document.get("cases"):
        raise ValueError("Unsupported or empty native fixture document")
    rows = []
    for case in document["cases"]:
        model = StorageModel(case["network"])
        pairs = case["overrides"]
        overrides = {c["id"]: c["value"] for c in pairs}
        if len(overrides) != len(pairs):
            raise ValueError("Duplicate fixture control IDs")
        config = case["config"]
        result = model.rollout(overrides, **config)
        times = result.time_h.tolist()
        if (times[0] != 0.0 or times[-1] != config["horizon_h"] or
                not 2 <= len(times) <= 10001 or
                any(a >= b for a, b in zip(times, times[1:]))):
            raise AssertionError(f"{case['name']}: rollout does not cover the exact requested interval")
        references = case["references"]
        if set(references) != {"dense_reference", "sparse", "affine"}:
            raise ValueError("Native fixture must execute all three reference methods")
        errors = {}
        for solver, reference in references.items():
            if tuple(reference["tank_ids"]) != result.tank_ids:
                raise AssertionError(f"{case['name']}: tank IDs/order mismatch")
            _compare(result.time_h, reference["time_h"], 0.0, 0.0, "sample times")
            errors[solver] = {
                "volume_m3": _compare(result.volumes_m3, reference["volumes_m3"],
                                      FORWARD_ATOL, FORWARD_RTOL, "storage trajectory"),
                "cost_units": _compare(result.cost, reference["cost"], FORWARD_ATOL,
                                       FORWARD_RTOL, "synthetic cost"),
            }
        if case["name"] == "grid_requested_capacity_overflow":
            # An ordinary relative trajectory tolerance would hide this .125 m3
            # error at the deliberately extreme, but accepted, inventory scale.
            expected = config["horizon_h"] * math.ldexp(1.0, 50)
            if (result.volumes_m3[0, -1].item() != expected or
                    any(ref["volumes_m3"][0][-1] != expected or ref["feasible"]
                        for ref in references.values())):
                raise AssertionError("Requested endpoint overflow was rounded away or marked feasible")
        elif case["name"].startswith("grid_") and any(not ref["feasible"] for ref in references.values()):
            raise AssertionError("Stationary grid regression was incorrectly rejected")
        payload = json.dumps({"network": case["network"], "overrides": pairs, "config": config},
                             sort_keys=True, separators=(",", ":"), allow_nan=False).encode()
        rows.append({"name": case["name"], "input_sha256": hashlib.sha256(payload).hexdigest(),
                     "samples": result.time_h.numel(), "tank_ids": list(result.tank_ids),
                     "forward_errors": errors,
                     "native_feasible": references["dense_reference"]["feasible"],
                     "final_m3": result.volumes_m3[:, -1].tolist(),
                     "cost_units": float(result.cost),
                     "gradients": _gradients(model, config)})
    return {"status": "passed", "method_version": METHOD_VERSION,
            "python_version": platform.python_version(), "torch_version": torch.__version__,
            "device": "cpu", "dtype": "float64", "native_solver_version": document["solver_version"],
            "native_executable_sha256": hashlib.sha256(native.read_bytes()).hexdigest(),
            "forward_tolerance": {"absolute": FORWARD_ATOL, "relative": FORWARD_RTOL},
            "gradient_tolerance": {"absolute": GRADIENT_ATOL, "relative": GRADIENT_RTOL},
            "objective": "sum((terminal_m3/100 m3)^2) + cost/10000 synthetic units",
            "cases": rows,
            "scope": "interior continuous controls only; native C++ decides feasibility"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True, help="Built breakwater_rollout_reference executable")
    parser.add_argument("--report", type=Path, help="Optional JSON evidence file (use ignored out/)")
    args = parser.parse_args()
    try:
        report = verify(args.native)
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        cases = report["cases"]
        controls = sum(len(c["gradients"]["controls"]) for c in cases)
        max_forward = max(e["volume_m3"] for c in cases for e in c["forward_errors"].values())
        print(f"PASS | {len(cases)} actual native cases | 3 solvers | CPU {torch.__version__} float64")
        print(f"maximum_volume_error_m3={max_forward:.12g}; interior_controls_checked={controls}; steps_per_control=3")
        print("Native feasibility remains independent; no gradients through bounds or discrete search.")
        return 0
    except (ValueError, AssertionError, RuntimeError, OSError, subprocess.SubprocessError) as exc:
        print(f"A3 verification failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
