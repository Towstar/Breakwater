"""Private desktop tool: estimate first-order decay from a closed tracer test.

PhysicsNeMo parameterizes the bounded unknown coefficient. The characteristic
solution supplies the forward model; this is parameter inference, not a general
neural PDE surrogate. A separately calculated least-squares coefficient and raw
concentration residual gate every usable estimate. Native transport stays separate.
"""
from __future__ import annotations

import contextlib
import hashlib
import json
import math
import sys

METHOD = "physicsnemo-sparse-decay-calibration-0.1.0"
MAX_BYTES = 65536


def validate(payload):
    if not isinstance(payload, dict) or set(payload) != {"initial_mg_l", "observations"}:
        raise ValueError("Provide initial_mg_l and observations only.")
    def number(value):
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError("Tracer measurements must be finite numbers.")
        return float(value)
    initial = number(payload["initial_mg_l"])
    rows = payload["observations"]
    if not 1e-9 <= initial <= 1e9 or not isinstance(rows, list) or not 3 <= len(rows) <= 64:
        raise ValueError("Use a positive initial concentration and 3–64 observations.")
    observations = []
    previous = 0.0
    for row in rows:
        if not isinstance(row, list) or len(row) != 2:
            raise ValueError("Each observation is [elapsed_hours, concentration_mg_l].")
        elapsed, concentration = map(number, row)
        if not previous < elapsed <= 1000 or not initial * 1e-12 <= concentration <= initial:
            raise ValueError("Use increasing positive hours ≤1000 and positive concentrations no greater than the initial value.")
        observations.append((elapsed, concentration))
        previous = elapsed
    if observations[-1][0] - observations[0][0] < 1e-3:
        raise ValueError("Observations need at least 0.001 hours of time spread.")
    return initial, observations


def reference(initial, observations):
    """Independent scalar log-space least squares; no torch/model calls."""
    denominator = math.fsum(t * t for t, _ in observations)
    coefficient = -math.fsum(t * math.log(c / initial) for t, c in observations) / denominator
    if not math.isfinite(coefficient) or not 0 <= coefficient < 10:
        raise ValueError("The inferred decay is outside the supported [0,10) per-hour range.")
    return coefficient


def fit(payload):
    initial, observations = validate(payload)
    expected = reference(initial, observations)
    import torch
    import physicsnemo
    from physicsnemo.models.mlp import FullyConnected
    torch.set_num_threads(1)
    torch.manual_seed(20261108)
    model = FullyConnected(in_features=1, out_features=1, layer_size=8,
                          num_layers=1, activation_fn="tanh", weight_norm=False).to(dtype=torch.float64, device="cpu")
    coordinate = torch.zeros((1, 1), dtype=torch.float64)
    times = torch.tensor([t for t, _ in observations], dtype=torch.float64)
    target = torch.tensor([math.log(c / initial) for _, c in observations], dtype=torch.float64)
    optimizer = torch.optim.LBFGS(model.parameters(), lr=1.0, max_iter=100,
                                 tolerance_grad=1e-20, tolerance_change=1e-24,
                                 line_search_fn="strong_wolfe")
    calls = 0
    def closure():
        nonlocal calls
        calls += 1
        optimizer.zero_grad()
        decay = 10 * torch.sigmoid(model(coordinate).reshape(()))
        loss = torch.mean(((-decay * times - target) / times[-1]) ** 2)
        loss.backward()
        return loss
    optimizer.step(closure)
    with torch.no_grad():
        decay = float((10 * torch.sigmoid(model(coordinate).reshape(()))).item())
    relative_rmse = math.sqrt(math.fsum(((initial * math.exp(-decay * t) - c) / initial) ** 2
                                      for t, c in observations) / len(observations))
    difference = abs(decay - expected)
    accepted = (math.isfinite(decay) and 0 <= decay < 10 and
                difference <= 1e-6 + 1e-5 * abs(expected) and relative_rmse <= 0.05)
    digest = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()).hexdigest()
    return {"status": "ok" if accepted else "rejected", "method": METHOD,
            "decay_per_h": decay, "reference_decay_per_h": expected,
            "reference_difference_per_h": difference, "relative_rmse": relative_rmse,
            "observations": len(observations), "optimizer_evaluations": calls,
            "input_sha256": digest, "physicsnemo_version": str(physicsnemo.__version__),
            "assumption": "Closed well-mixed tracer sample; known initial concentration; first-order decay only.",
            "quality_gate": "Independent log-space least-squares agreement and concentration RMSE ≤5% of initial.",
            "forecast_authority": "Native finite-volume solver; estimate requires explicit application and a new forecast."}


class _Discard:
    def write(self, value): return len(value)
    def flush(self): pass


def main():
    try:
        line = sys.stdin.buffer.readline(MAX_BYTES + 1)
        if len(line) > MAX_BYTES or not line.endswith(b"\n") or sys.stdin.buffer.read(1):
            raise ValueError("Send one JSON line no larger than 64 KiB.")
        payload = json.loads(line, parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON is unsupported.")))
        with contextlib.redirect_stdout(_Discard()), contextlib.redirect_stderr(_Discard()):
            result = fit(payload)
    except (ImportError, ModuleNotFoundError):
        result = {"status": "unavailable", "message": "PhysicsNeMo and PyTorch are required in BREAKWATER_PYTHON."}
    except ValueError as error:
        result = {"status": "error", "message": str(error)[:500]}
    except Exception:
        result = {"status": "error", "message": "The bounded PhysicsNeMo calibration could not complete."}
    print(json.dumps(result, allow_nan=False, separators=(",", ":")))


if __name__ == "__main__":
    main()
