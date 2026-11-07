"""Run A4's real PhysicsNeMo PINN training/checkpoint/native-reference smoke.

All thresholds are loaded and written to preflight.json before training. Native
fields are evaluation-only; training uses the initial condition and PDE residual.
Exit 0 requires every smoke gate, not just successful model construction/training.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.metadata
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

import torch

from breakwater_science.transport_pinn import (
    CASE, METHOD_VERSION, MODEL_ARGS, exact_cell_averages, exact_concentration,
    exact_ledger, make_model, pde_residual, predict, predicted_cell_averages,
)


def _json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def _rmse(value):
    return float(value.detach().square().mean().sqrt())


def _tensor(value):
    result = torch.tensor(value, dtype=torch.float64, device="cpu")
    if not bool(torch.isfinite(result).all()):
        raise ValueError("Nonfinite reference or model data")
    return result


def collocation_points(count, seed):
    points = torch.quasirandom.SobolEngine(2, scramble=True, seed=seed).draw(count).double()
    points[:, 0] *= CASE.length_m
    points[:, 1] *= CASE.horizon_h
    return points


def _config(path):
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("config_version") != 1:
        raise ValueError("Unsupported smoke config version")
    for key, low, high in (("adam_steps", 1, 10000), ("collocation_points", 8, 4096),
                           ("initial_points", 4, 1024), ("threads", 1, 8),
                           ("seed", 0, 2**32-1)):
        v = value[key]
        if isinstance(v, bool) or not isinstance(v, int) or not low <= v <= high:
            raise ValueError(f"Invalid bounded smoke setting {key}")
    for key, high in (("learning_rate", .1), ("initial_loss_weight", 100),
                      ("training_time_limit_s", 600)):
        v = value[key]
        if isinstance(v, bool) or not isinstance(v, (float, int)) or not math.isfinite(v) or not 0 < v <= high:
            raise ValueError(f"Invalid smoke setting {key}")
    required = {"native_fine_rmse_mg_l", "native_time_refinement_rmse_mg_l",
                "heldout_field_rmse_mg_l", "heldout_field_max_mg_l",
                "heldout_residual_rms_mg_l_h", "outlet_max_mg_l",
                "inventory_relative_error", "minimum_prediction_mg_l",
                "checkpoint_max_abs_mg_l"}
    if set(value["thresholds"]) != required:
        raise ValueError("Missing/unknown smoke thresholds")
    for key, v in value["thresholds"].items():
        if isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v):
            raise ValueError(f"Invalid threshold {key}")
        if key != "minimum_prediction_mg_l" and v <= 0:
            raise ValueError(f"Expected positive threshold {key}")
    return value


def native_reference(executable):
    run = subprocess.run([str(executable)], capture_output=True, text=True,
                         encoding="utf-8", timeout=60)
    if run.returncode:
        raise RuntimeError(f"Native reference failed ({run.returncode}): {run.stderr.strip()}")
    data = json.loads(run.stdout)
    if data.get("fixture_version") != 1:
        raise ValueError("Unsupported native fixture")
    expected = dict(length_m=CASE.length_m, area_m2=CASE.area_m2, flow_m3h=CASE.flow_m3h,
                    dispersion_m2_h=CASE.dispersion_m2_h, decay_per_h=CASE.decay_per_h,
                    horizon_h=CASE.horizon_h, inlet_mg_l=0, a_per_m=CASE.a_per_m,
                    b_per_m=CASE.b_per_m, decay_eigenvalue_per_h=CASE.eigenvalue_per_h)
    for key, value in expected.items():
        if not math.isclose(data["case"][key], value, rel_tol=1e-12, abs_tol=1e-12):
            raise ValueError(f"Native case mismatch: {key}")
    if {(r["cells"], r["cfl_safety"]) for r in data["runs"]} != {(32,.9), (64,.9), (128,.9), (128,.45)}:
        raise ValueError("Native fixture must include all prescribed mesh/time refinements")
    return data


def validate_reference(document, thresholds):
    rows = []
    for run in document["runs"]:
        t = _tensor(run["time_h"])
        field = _tensor(run["concentration_mg_l"])
        n = run["cells"]
        if field.shape != (len(t), n) or len(t) != 41 or t[0] != 0 or t[-1] != CASE.horizon_h:
            raise ValueError("Unexpected native field shape/times")
        if not bool((t[1:] > t[:-1]).all()):
            raise ValueError("Non-increasing native sample times")
        centers = (torch.arange(n, dtype=torch.float64, device="cpu")+.5)*CASE.length_m/n
        if not torch.allclose(_tensor(run["x_m"]), centers, atol=1e-12, rtol=1e-12):
            raise ValueError("Native cell-center coordinates differ from the declared mesh")
        if run["diagnostics"]["max_cfl_number"] > run["cfl_safety"]+1e-12:
            raise ValueError("Native stability limit exceeded")
        exact = exact_cell_averages(n, t)
        error = field-exact
        ledger = exact_ledger(t)
        inventory = _tensor(run["inventory_mg"])
        residual = _tensor(run["mass_balance_residual_mg"])
        if not torch.allclose(field[0], exact[0], atol=1e-12, rtol=1e-12):
            raise ValueError("Native initial state must contain exact cell averages")
        if not torch.allclose(inventory, 1000*CASE.area_m2*CASE.length_m/n*field.sum(dim=1), atol=1e-9, rtol=1e-12):
            raise ValueError("Native inventory and field disagree")
        scale = max(1.0, float(inventory.abs().max()))
        balance = (inventory-inventory[0]-_tensor(run["cumulative_inlet_mg"])
                   +_tensor(run["cumulative_outlet_mg"])-_tensor(run["cumulative_source_mg"])
                   +_tensor(run["cumulative_decay_mg"]))
        if float(balance.abs().max()) > 1e-9*scale or float(residual.abs().max()) > 1e-9*scale:
            raise ValueError("Native signed mass ledger failed")
        if run["cumulative_inlet_mg"][-1] >= 0 or run["cumulative_outlet_mg"][-1] <= 0 or run["cumulative_decay_mg"][-1] <= 0:
            raise ValueError("Native boundary/reaction ledger signs are incorrect")
        rows.append({"cells": n, "cfl_safety": run["cfl_safety"], "rmse_mg_l": _rmse(error),
                     "max_error_mg_l": float(error.abs().max()), "diagnostics": run["diagnostics"],
                     "final_ledger_errors_mg": {key: run[key][-1]-float(value[-1]) for key,value in ledger.items()},
                     "max_mass_residual_mg": float(residual.abs().max())})
    spatial = sorted((r for r in rows if r["cfl_safety"] == .9), key=lambda r:r["cells"])
    fine = next(r for r in document["runs"] if r["cells"] == 128 and r["cfl_safety"] == .9)
    refined = next(r for r in document["runs"] if r["cells"] == 128 and r["cfl_safety"] == .45)
    if fine["time_h"] != refined["time_h"]:
        raise ValueError("Time-refinement output grids differ")
    temporal_rmse = _rmse(_tensor(fine["concentration_mg_l"])-_tensor(refined["concentration_mg_l"]))
    gates = {
        "native_spatial_refinement": all(b["rmse_mg_l"] < a["rmse_mg_l"] for a,b in zip(spatial,spatial[1:])),
        "native_fine_accuracy": spatial[-1]["rmse_mg_l"] <= thresholds["native_fine_rmse_mg_l"],
        "native_time_sensitivity": temporal_rmse <= thresholds["native_time_refinement_rmse_mg_l"],
    }
    orders = [math.log(a["rmse_mg_l"]/b["rmse_mg_l"], 2) for a,b in zip(spatial,spatial[1:])]
    return fine, {"runs": rows, "observed_spatial_orders": orders,
                  "time_refinement_rmse_mg_l": temporal_rmse}, gates


def train(config):
    torch.manual_seed(config["seed"])
    torch.set_num_threads(config["threads"])
    torch.use_deterministic_algorithms(True)
    model = make_model()
    before = torch.cat([p.detach().reshape(-1).clone() for p in model.parameters()])
    # Fixed training set; held-out coordinates are generated separately after training.
    points = collocation_points(config["collocation_points"], config["seed"])
    initial_x = (torch.arange(config["initial_points"], dtype=torch.float64, device="cpu")+.5)/config["initial_points"]*CASE.length_m
    initial_xt = torch.stack((initial_x, torch.zeros_like(initial_x)), dim=1)
    initial_target = exact_concentration(initial_xt).detach()
    optimizer = torch.optim.Adam(model.parameters(), lr=config["learning_rate"])
    history = []
    start = time.perf_counter()
    initial_loss = None
    for step in range(config["adam_steps"]):
        if time.perf_counter()-start > config["training_time_limit_s"]:
            raise RuntimeError(f"Training exceeded its {config['training_time_limit_s']} s budget")
        optimizer.zero_grad(set_to_none=True)
        residual = pde_residual(lambda xt: predict(model, xt), points)
        pde_loss = residual.square().mean()
        ic_loss = (predict(model, initial_xt)-initial_target).square().mean()
        loss = pde_loss + config["initial_loss_weight"]*ic_loss
        if not bool(torch.isfinite(loss.detach())):
            raise RuntimeError("Nonfinite training loss")
        if initial_loss is None:
            initial_loss = float(loss.detach())
        loss.backward()
        optimizer.step()
        if step % 100 == 0 or step+1 == config["adam_steps"]:
            row = {"step": step, "total_loss": float(loss.detach()),
                   "pde_loss": float(pde_loss.detach()), "initial_loss": float(ic_loss.detach())}
            history.append(row)
            print(f"A4 step {step+1}/{config['adam_steps']}: loss={row['total_loss']:.6g}", flush=True)
    duration = time.perf_counter()-start
    if duration > config["training_time_limit_s"]:
        raise RuntimeError("Training finished outside its declared time budget")
    model.eval()
    after = torch.cat([p.detach().reshape(-1) for p in model.parameters()])
    return model, {"steps": config["adam_steps"], "runtime_s": duration,
                   "parameter_count": before.numel(), "parameter_update_l2": float((after-before).norm()),
                   "first_loss": initial_loss, "history": history,
                   "training_data": "IC point samples and PDE residual only; no native/transient field targets"}


def evaluate(model, fine, thresholds, checkpoint, config):
    t = _tensor(fine["time_h"])
    native = _tensor(fine["concentration_mg_l"])
    prediction = predicted_cell_averages(model, fine["cells"], t)
    # t=0 is excluded from the held-out field gate because IC data trained the model.
    field_error = prediction[1:]-native[1:]
    xt = collocation_points(512, (config["seed"]+1) % (2**32))
    training_points = collocation_points(config["collocation_points"], config["seed"])
    if bool((xt[:, None, :] == training_points[None, :, :]).all(dim=-1).any()):
        raise ValueError("Held-out residual coordinates overlap training coordinates")
    heldout_residual = pde_residual(lambda p: predict(model, p), xt)
    with torch.no_grad():
        direct = predict(model, xt)
        outlet_xt = torch.stack((torch.full_like(t, CASE.length_m), t), dim=1)
        outlet = predict(model, outlet_xt)[:, 0]
    inventory = prediction.sum(dim=1)*1000*CASE.area_m2*CASE.length_m/fine["cells"]
    native_inventory = _tensor(fine["inventory_mg"])
    relative_mass = ((inventory-native_inventory).abs()/native_inventory.abs()).max()
    analytic_outlet = exact_concentration(outlet_xt)[:, 0]
    model.save(checkpoint)
    restored = make_model()  # double before .load avoids a float32 checkpoint round trip
    restored.load(checkpoint, map_location="cpu", strict=True)
    restored.eval()
    with torch.no_grad():
        reloaded = predict(restored, xt)
    reload_error = float((reloaded-direct).abs().max())
    metrics = {
        "heldout_field_rmse_mg_l": _rmse(field_error),
        "heldout_field_max_mg_l": float(field_error.abs().max()),
        "heldout_residual_rms_mg_l_h": _rmse(heldout_residual),
        "outlet_max_mg_l": float((outlet-analytic_outlet).abs().max()),
        "inventory_relative_error": float(relative_mass),
        "minimum_prediction_mg_l": min(float(prediction.min()), float(direct.min())),
        "checkpoint_max_abs_mg_l": reload_error,
    }
    gates = {key: value <= thresholds[key] for key,value in metrics.items()
             if key != "minimum_prediction_mg_l"}
    gates["minimum_prediction_mg_l"] = metrics["minimum_prediction_mg_l"] >= thresholds["minimum_prediction_mg_l"]
    if not all(math.isfinite(x) for x in metrics.values()):
        raise RuntimeError("Nonfinite model evaluation")
    return metrics, gates, prediction


def execute(native, config_path, output):
    config = _config(config_path)
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    # PhysicsNeMo reads this supported setting at import, even for local checkpoints.
    os.environ.setdefault("LOCAL_CACHE", str(output/"physicsnemo-cache"))
    import physicsnemo
    native = native.resolve(strict=True)
    preflight = {"method_version": METHOD_VERSION, "config": config,
                 "config_sha256": hashlib.sha256(config_path.read_bytes()).hexdigest(),
                 "started_utc": datetime.now(timezone.utc).isoformat(),
                 "python": platform.python_version(), "platform": platform.platform(),
                 "torch": torch.__version__, "physicsnemo": physicsnemo.__version__,
                 "numpy": importlib.metadata.version("numpy"), "device": "cpu", "dtype": "float64",
                 "cuda_build": torch.version.cuda, "model_args": MODEL_ARGS,
                 "heldout_residual_seed": (config["seed"]+1) % (2**32),
                 "heldout_residual_points": 512,
                 "physicsnemo_cache": os.environ["LOCAL_CACHE"],
                 "native_executable_sha256": hashlib.sha256(native.read_bytes()).hexdigest()}
    _json(output/"preflight.json", preflight)
    report = {**preflight, "status": "failed", "scope": "fixed-case experimental smoke; no volume approval or cross-setting generalization"}
    try:
        document = native_reference(native)
        _json(output/"native-reference.json", document)
        fine, native_metrics, native_gates = validate_reference(document, config["thresholds"])
        report.update(native_solver_version=document["solver_version"], native=native_metrics, gates=native_gates)
        if not all(native_gates.values()):
            raise RuntimeError("Native refinement/accuracy smoke gates failed")
        model, training = train(config)
        report["training"] = training
        checkpoint = output/"transport.mdlus"
        metrics, model_gates, prediction = evaluate(model, fine, config["thresholds"], checkpoint, config)
        model_gates["training_loss_decreased"] = training["history"][-1]["total_loss"] < training["first_loss"]
        model_gates["parameters_updated"] = training["parameter_update_l2"] > 0
        report.update(model_metrics=metrics, gates={**native_gates, **model_gates},
                      checkpoint_sha256=hashlib.sha256(checkpoint.read_bytes()).hexdigest(),
                      model_class=f"{type(model).__module__}.{type(model).__name__}")
        _json(output/"prediction.json", {"time_h": fine["time_h"], "x_m": fine["x_m"],
              "concentration_mg_l": prediction.tolist(), "cell_average_quadrature_points": 8})
        report["status"] = "passed" if all(report["gates"].values()) else "failed"
    except (ValueError, RuntimeError, OSError, subprocess.SubprocessError) as exc:
        report["error"] = str(exc)
    _json(output/"report.json", report)
    print(f"A4 {report['status'].upper()}: {output/'report.json'}", flush=True)
    if "model_metrics" in report:
        print(json.dumps(report["model_metrics"], sort_keys=True), flush=True)
    return 0 if report["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--config", type=Path, default=Path(__file__).with_name("a4_config.json"))
    parser.add_argument("--output", type=Path, default=Path("out/a4-smoke"))
    args = parser.parse_args()
    try:
        return execute(args.native, args.config, args.output)
    except (ValueError, RuntimeError, ImportError, OSError, subprocess.SubprocessError) as exc:
        print(f"A4 setup failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
