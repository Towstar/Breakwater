"""A4 fixed-case PhysicsNeMo PINN; no volume-feasibility or worker integration.

The network learns from initial data and the PDE. Its ansatz enforces the physical
boundaries but does not contain the analytic transient used for reference evaluation.
All derivatives are with respect to physical metres and hours.
"""
from __future__ import annotations

from dataclasses import dataclass
import math
from collections.abc import Callable

import numpy as np
import torch

METHOD_VERSION = "physicsnemo-pipe-pinn-0.1.0"
MODEL_ARGS = dict(in_features=2, out_features=1, layer_size=32, num_layers=3,
                  activation_fn="tanh", weight_norm=False)


@dataclass(frozen=True)
class Case:
    length_m: float = 1.0
    area_m2: float = 1.0
    dispersion_m2_h: float = 0.1
    decay_per_h: float = 0.1
    horizon_h: float = 1.0

    @property
    def b_per_m(self):
        return 3 * math.pi / (4 * self.length_m)

    @property
    def a_per_m(self):
        return self.b_per_m

    @property
    def velocity_m_h(self):
        return 2 * self.dispersion_m2_h * self.a_per_m

    @property
    def flow_m3h(self):
        return self.area_m2 * self.velocity_m_h

    @property
    def eigenvalue_per_h(self):
        return self.decay_per_h + 2 * self.dispersion_m2_h * self.b_per_m**2


CASE = Case()


def _coordinates(xt):
    if (not isinstance(xt, torch.Tensor) or xt.ndim != 2 or xt.shape[1] != 2 or
            xt.dtype != torch.float64 or xt.device.type != "cpu" or
            not bool(torch.isfinite(xt.detach()).all())):
        raise ValueError("Coordinates must be finite CPU float64 [point,(x_m,time_h)]")


def _times(time_h):
    if (not isinstance(time_h, torch.Tensor) or time_h.ndim != 1 or
            time_h.dtype != torch.float64 or time_h.device.type != "cpu" or
            not bool(torch.isfinite(time_h.detach()).all())):
        raise ValueError("Times must be a finite CPU float64 vector")


def exact_concentration(xt, case=CASE):
    """Positive eigenmode used only for IC/independent reference checks."""
    _coordinates(xt)
    x, t = xt[:, :1], xt[:, 1:2]
    a, b, length = case.a_per_m, case.b_per_m, case.length_m
    return torch.exp(a * (x - length) - case.eigenvalue_per_h * t) * torch.sin(b*x) / math.sin(b*length)


def _primitive(x, case=CASE):
    a, b, length = case.a_per_m, case.b_per_m, case.length_m
    return (torch.exp(a*(x-length)) * (a*torch.sin(b*x)-b*torch.cos(b*x)) /
            ((a*a+b*b)*math.sin(b*length)))


def exact_cell_averages(cells, time_h, case=CASE):
    _times(time_h)
    if isinstance(cells, bool) or not isinstance(cells, int) or not 2 <= cells <= 4096:
        raise ValueError("Expected 2..4096 cells")
    edges = torch.linspace(0, case.length_m, cells+1, dtype=torch.float64, device="cpu")
    integral = _primitive(edges, case)
    initial = (integral[1:] - integral[:-1]) / (case.length_m/cells)
    return torch.exp(-case.eigenvalue_per_h*time_h[:, None]) * initial[None, :]


def exact_ledger(time_h, case=CASE):
    _times(time_h)
    endpoints = torch.tensor([0.0, case.length_m], dtype=torch.float64, device="cpu")
    primitive = _primitive(endpoints, case)
    mass0 = 1000*case.area_m2*(primitive[1]-primitive[0])
    factor = -torch.expm1(-case.eigenvalue_per_h*time_h)/case.eigenvalue_per_h
    inlet_flux = -case.dispersion_m2_h * math.exp(-case.a_per_m*case.length_m) * case.b_per_m / math.sin(case.b_per_m*case.length_m)
    return {
        "inventory_mg": mass0 * torch.exp(-case.eigenvalue_per_h*time_h),
        "cumulative_inlet_mg": 1000*case.area_m2*inlet_flux*factor,
        "cumulative_outlet_mg": 1000*case.area_m2*case.velocity_m_h*factor,
        "cumulative_decay_mg": case.decay_per_h*mass0*factor,
        "cumulative_source_mg": torch.zeros_like(time_h),
    }


def make_model():
    # Lazy import keeps A3 and analytic tests usable in the smaller A3 environment.
    from physicsnemo.models.mlp import FullyConnected
    return FullyConnected(**MODEL_ARGS).to(device="cpu", dtype=torch.float64)


def predict(model, xt, case=CASE):
    _coordinates(xt)
    h = torch.sin(math.pi*xt[:, :1]/(2*case.length_m))
    features = torch.cat((2*h-1, 2*xt[:, 1:2]/case.horizon_h-1), dim=1)
    return h * model(features)


def pde_residual(field: Callable, xt, case=CASE):
    _coordinates(xt)
    coordinates = xt.detach().clone().requires_grad_(True)
    c = field(coordinates)
    if c.shape != (coordinates.shape[0], 1):
        raise ValueError("Field must return [point,1] concentrations")
    gradient, = torch.autograd.grad(c, coordinates, torch.ones_like(c), create_graph=True)
    cx, ct = gradient[:, :1], gradient[:, 1:2]
    second, = torch.autograd.grad(cx, coordinates, torch.ones_like(cx), create_graph=True)
    cxx = second[:, :1]
    return ct + case.velocity_m_h*cx - case.dispersion_m2_h*cxx + case.decay_per_h*c


def predicted_cell_averages(model, cells, time_h, case=CASE):
    _times(time_h)
    if isinstance(cells, bool) or not isinstance(cells, int) or not 2 <= cells <= 4096:
        raise ValueError("Expected 2..4096 cells")
    nodes, weights = np.polynomial.legendre.leggauss(8)
    nodes = torch.tensor(nodes, dtype=torch.float64, device="cpu")
    weights = torch.tensor(weights/2, dtype=torch.float64, device="cpu")
    dx = case.length_m/cells
    centers = (torch.arange(cells, dtype=torch.float64, device="cpu") + .5)*dx
    x = centers[:, None]+nodes[None, :]*dx/2
    xt = torch.stack((x[None, :, :].expand(time_h.numel(), -1, -1),
                      time_h[:, None, None].expand(-1, cells, 8)), dim=-1)
    with torch.no_grad():
        c = predict(model, xt.reshape(-1, 2), case).reshape(time_h.numel(), cells, 8)
    return (c*weights).sum(dim=-1)
