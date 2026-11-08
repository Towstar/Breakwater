"""Independent analytic and semidiscrete oracles for the bounded A9 fixtures.

Only fixture parameters are consumed: native initial fields and results are never
used to construct a reference. Concentrations are physical cell averages (mg/L),
times are hours, geometry is metres/m2, and ledger quantities are milligrams.
"""
from __future__ import annotations

from bisect import bisect_right
from dataclasses import dataclass
from functools import lru_cache
import math

import numpy as np


@dataclass(frozen=True)
class _Geometry:
    cells: int
    length: float
    area: float
    diffusion: float
    decay: float

    @property
    def dx(self):
        return self.length / self.cells

    @property
    def edges(self):
        return np.linspace(0.0, self.length, self.cells + 1, dtype=np.float64)

    @property
    def centers(self):
        return (np.arange(self.cells, dtype=np.float64) + .5) * self.dx


def _number(value):
    if isinstance(value, bool):
        raise ValueError("Reference parameters must be finite numbers")
    value = float(value)
    if not math.isfinite(value):
        raise ValueError("Reference parameters must be finite numbers")
    return value


def _geometry(case):
    pipe = case["pipe"]
    cells = pipe["cells"]
    if isinstance(cells, bool) or not isinstance(cells, int) or not 2 <= cells <= 4096:
        raise ValueError("Reference mesh needs 2..4096 cells")
    g = _Geometry(cells, *(_number(pipe[key]) for key in
        ("length_m", "area_m2", "dispersion_m2_h", "decay_per_h")))
    if g.length <= 0 or g.area <= 0 or g.diffusion < 0 or g.decay < 0:
        raise ValueError("Invalid reference geometry or rates")
    return g


def _time(case, time_h):
    time = _number(time_h)
    if not 0 <= time <= _number(case["config"]["horizon_h"]):
        raise ValueError("Reference time is outside the captured forecast")
    return time


def _schedule(case, name, value_key, *, optional=False):
    values = [(_number(item["time_h"]), _number(item[value_key])) for item in case[name]]
    if not values:
        if optional:
            return values
        raise ValueError("Reference schedule is empty")
    if (values[0][0] != 0.0 or any(value < 0 for _, value in values) or
            any(right[0] <= left[0] for left, right in zip(values, values[1:]))):
        raise ValueError("Invalid reference schedule")
    return values


def _flow(case):
    return _schedule(case, "flow", "flow_m3h")


def _constant_flow(case, expected, *, area):
    flow = _flow(case)
    if any(not math.isclose(q / area, expected, rel_tol=2e-14, abs_tol=2e-15)
           for _, q in flow):
        raise ValueError("Flow does not match the analytic reference family")


def _require_source(case, g, value=0.0):
    source = np.asarray(case["source_mg_l_h"], dtype=np.float64)
    if source.shape != (g.cells,) or not np.all(source == value):
        raise ValueError("Source does not match the analytic reference family")


def _require_boundary(case, value):
    if case["config"]["boundary"] != value:
        raise ValueError("Boundary does not match the analytic reference family")


def _zero_inlet(case):
    if any(value != 0.0 for _, value in
           _schedule(case, "inlet", "concentration_mg_l")):
        raise ValueError("Reference family requires zero inlet concentration")


def _displacement(case, g, time):
    flow = _flow(case)
    return math.fsum((min(time, flow[i + 1][0] if i + 1 < len(flow) else time) - start) * q / g.area
                    for i, (start, q) in enumerate(flow) if start < time)


def _sinc(value):
    return 1.0 if value == 0.0 else math.sin(value) / value


def _decay_integral(rate, time):
    return time if rate == 0.0 else -math.expm1(-rate * time) / rate


def _pulse_primitive(points, start, width):
    # Clipping integration limits to the compact support is geometric, not a
    # correction or clipping of a predicted concentration.
    z = np.clip(np.asarray(points, dtype=np.float64) - start, 0.0, width)
    return .5 * z - width * np.sin(2 * math.pi * z / width) / (4 * math.pi)


def _pulse(case, g, shift=0.0):
    parameters = case["reference_parameters"]
    start = _number(parameters["bump_start_m"])
    width = _number(parameters["bump_width_m"])
    if start < 0 or width <= 0 or start + width > g.length:
        raise ValueError("The initial pulse must be contained in the pipe")
    integrals = _pulse_primitive(g.edges - shift, start, width)
    return np.diff(integrals) / g.dx, float(integrals[-1] - integrals[0])


def _mixed_parameters(case, g):
    fraction = _number(case["reference_parameters"]["mode_fraction"])
    if not .5 < fraction < 1.0 or g.diffusion <= 0:
        raise ValueError("Mixed reference needs .5 < mode_fraction < 1 and positive diffusion")
    b = math.pi * fraction / g.length
    a = -b * math.cos(b * g.length) / math.sin(b * g.length)
    velocity = 2 * g.diffusion * a
    _constant_flow(case, velocity, area=g.area)
    return a, b, velocity, g.decay + g.diffusion * (a*a + b*b)


def _mixed_initial(g, a, b):
    edges = g.edges
    primitive = (np.exp(a*(edges-g.length)) *
                 (a*np.sin(b*edges)-b*np.cos(b*edges)) /
                 ((a*a+b*b)*math.sin(b*g.length)))
    return np.diff(primitive)/g.dx, float(primitive[-1]-primitive[0])


def _ledger(field, mass, *, inlet=0.0, outlet=0.0, source=0.0, decay=0.0):
    return {"concentration_mg_l": np.asarray(field, dtype=np.float64),
            "inventory_mg": float(mass), "cumulative_inlet_mg": float(inlet),
            "cumulative_outlet_mg": float(outlet),
            "cumulative_source_mg": float(source), "cumulative_decay_mg": float(decay)}


def _inlet_value(inlet, time):
    index = max(0, bisect_right([point[0] for point in inlet], time)-1)
    if index + 1 == len(inlet):
        return inlet[index][1]
    left, right = inlet[index], inlet[index+1]
    weight = (time-left[0])/(right[0]-left[0])
    return (1-weight)*left[1] + weight*right[1]


def _scheduled_inlet(case, g, time):
    if _number(case["reference_parameters"]["initial_mg_l"]) != 0.0:
        raise ValueError("Scheduled-inlet reference requires an initially empty pipe")
    flow = _flow(case)
    inlet = _schedule(case, "inlet", "concentration_mg_l")
    knots = sorted({0.0, time, *(t for t, _ in flow if 0 < t < time),
                    *(t for t, _ in inlet if 0 < t < time)})
    total_displacement = _displacement(case, g, time)
    field = np.zeros(g.cells, dtype=np.float64)
    injected, departed = [], []
    for left, right in zip(knots, knots[1:]):
        index = bisect_right([point[0] for point in flow], left)-1
        velocity = flow[index][1]/g.area
        if velocity == 0.0:
            continue

        def integral(start, end):
            if end <= start:
                return 0.0
            return velocity*(end-start)*.5*(_inlet_value(inlet, start)+_inlet_value(inlet, end))

        injected.append(integral(left, right))
        oldest_position = total_displacement - _displacement(case, g, left)
        departure_end = min(right, left+(oldest_position-g.length)/velocity)
        departed.append(integral(left, departure_end))
        for cell in range(g.cells):
            start = max(left, left+(oldest_position-(cell+1)*g.dx)/velocity)
            end = min(right, left+(oldest_position-cell*g.dx)/velocity)
            field[cell] += integral(start, end)/g.dx
    inlet_mass = 1000*g.area*math.fsum(injected)
    outlet_mass = 1000*g.area*math.fsum(departed)
    return _ledger(field, inlet_mass-outlet_mass, inlet=inlet_mass, outlet=outlet_mass)


def continuum(case, time_h):
    """Analytic continuum cell averages and independently integrated mass ledger."""
    g, time = _geometry(case), _time(case, time_h)
    regime, parameters = case["regime"], case["reference_parameters"]
    if regime in ("advection", "scheduled_advection", "scheduled_inlet"):
        _require_boundary(case, "dirichlet_inlet")
        _require_source(case, g)
        if g.diffusion != 0.0 or g.decay != 0.0:
            raise ValueError("Pure-advection references require zero diffusion and decay")
        if regime == "scheduled_inlet":
            return _scheduled_inlet(case, g, time)
        _zero_inlet(case)
        field, integral = _pulse(case, g, _displacement(case, g, time))
        _, initial_integral = _pulse(case, g)
        return _ledger(field, 1000*g.area*integral,
                       outlet=1000*g.area*(initial_integral-integral))
    if regime == "closed_diffusion":
        _require_boundary(case, "closed_no_flux")
        _require_source(case, g)
        _constant_flow(case, 0.0, area=g.area)
        if g.decay != 0.0 or g.diffusion <= 0.0:
            raise ValueError("Closed diffusion requires positive diffusion and zero decay")
        mean, amplitude = (_number(parameters[key]) for key in ("mean_mg_l", "amplitude_mg_l"))
        if amplitude < 0 or mean < amplitude:
            raise ValueError("Closed cosine profile must be nonnegative")
        beta = math.pi/g.length
        field = mean + amplitude*_sinc(beta*g.dx/2)*np.cos(beta*g.centers)*math.exp(-g.diffusion*beta*beta*time)
        return _ledger(field, 1000*g.area*g.length*mean)
    if regime == "open_diffusion":
        _require_boundary(case, "dirichlet_inlet")
        _require_source(case, g)
        _constant_flow(case, 0.0, area=g.area)
        _zero_inlet(case)
        if g.decay != 0.0 or g.diffusion <= 0.0:
            raise ValueError("Open diffusion requires positive diffusion and zero decay")
        amplitude = _number(parameters["amplitude_mg_l"])
        if amplitude < 0:
            raise ValueError("Open sine amplitude must be nonnegative")
        beta = math.pi/(2*g.length)
        rate = g.diffusion*beta*beta
        field = amplitude*_sinc(beta*g.dx/2)*np.sin(beta*g.centers)*math.exp(-rate*time)
        initial_mass = 1000*g.area*amplitude/beta
        return _ledger(field, initial_mass*math.exp(-rate*time),
                       inlet=initial_mass*math.expm1(-rate*time))
    if regime == "mixed_adr":
        _require_boundary(case, "dirichlet_inlet")
        _require_source(case, g)
        _zero_inlet(case)
        a, b, velocity, rate = _mixed_parameters(case, g)
        initial, integral = _mixed_initial(g, a, b)
        initial_mass = 1000*g.area*integral
        integrated_decay = _decay_integral(rate, time)
        inlet_flux = -g.diffusion*math.exp(-a*g.length)*b/math.sin(b*g.length)
        return _ledger(initial*math.exp(-rate*time), initial_mass*math.exp(-rate*time),
                       inlet=1000*g.area*inlet_flux*integrated_decay,
                       outlet=1000*g.area*velocity*integrated_decay,
                       decay=g.decay*initial_mass*integrated_decay)
    if regime in ("reaction_source", "source_only"):
        _require_boundary(case, "closed_no_flux")
        _constant_flow(case, 0.0, area=g.area)
        initial, source = (_number(parameters[key]) for key in ("initial_mg_l", "source_mg_l_h"))
        _require_source(case, g, source)
        if initial < 0 or source < 0 or g.diffusion != 0.0 or (regime == "source_only" and g.decay != 0.0):
            raise ValueError("Uniform ODE reference parameters do not match the family")
        integral = _decay_integral(g.decay, time)
        value = initial*math.exp(-g.decay*time) + source*integral
        decay = initial*(-math.expm1(-g.decay*time)) + source*(time-integral)
        liters = 1000*g.area*g.length
        return _ledger(np.full(g.cells, value), liters*value,
                       source=liters*source*time, decay=liters*decay)
    raise ValueError(f"Unsupported reference regime: {regime}")


@lru_cache(maxsize=64)
def _mixed_system(cells, length, diffusion, decay, velocity):
    dx = length/cells
    lower, upper = velocity/dx+diffusion/(dx*dx), diffusion/(dx*dx)
    diagonal = np.full(cells, -(velocity/dx+2*upper+decay))
    diagonal[0] -= upper
    diagonal[-1] += upper
    symmetric = np.diag(diagonal)
    off_diagonal = math.sqrt(lower*upper)
    indices = np.arange(cells-1)
    symmetric[indices, indices+1] = off_diagonal
    symmetric[indices+1, indices] = off_diagonal
    eigenvalues, vectors = np.linalg.eigh(symmetric)
    scale = np.power(math.sqrt(lower/upper), np.arange(cells, dtype=np.float64))
    if not np.isfinite(scale).all():
        raise ValueError("Reference similarity transform is nonfinite")
    return symmetric, eigenvalues, vectors, scale


def semidiscrete(case, time_h):
    """Exact semidiscrete formulas, or checked float64 eigensolution for mixed ADR.

    These isolate temporal integration error from continuum spatial truncation.
    Scheduled inlet forcing has no semidiscrete oracle in this bounded matrix.
    """
    g, time = _geometry(case), _time(case, time_h)
    initial = continuum(case, 0.0)["concentration_mg_l"]
    regime = case["regime"]
    if regime == "scheduled_inlet":
        return None
    if time == 0.0:
        return initial.copy()
    if regime in ("advection", "scheduled_advection"):
        z = _displacement(case, g, time)/g.dx
        weights = [math.exp(-z)]
        for index in range(1, g.cells):
            weights.append(weights[-1]*z/index)
        return np.array([math.fsum(weights[index]*initial[cell-index] for index in range(cell+1))
                         for cell in range(g.cells)], dtype=np.float64)
    if regime in ("closed_diffusion", "open_diffusion"):
        beta = math.pi/g.length if regime == "closed_diffusion" else math.pi/(2*g.length)
        rate = 4*g.diffusion*math.sin(beta*g.dx/2)**2/(g.dx*g.dx)
        mean = _number(case["reference_parameters"]["mean_mg_l"]) if regime == "closed_diffusion" else 0.0
        return mean + (initial-mean)*math.exp(-rate*time)
    if regime == "mixed_adr":
        _, _, velocity, _ = _mixed_parameters(case, g)
        _, values, vectors, scale = _mixed_system(g.cells, g.length, g.diffusion, g.decay, velocity)
        transformed = vectors.T @ (initial/scale)
        return scale*(vectors @ (np.exp(values*time)*transformed))
    return continuum(case, time)["concentration_mg_l"]


def reference_diagnostics(case):
    """Numerical eigendecomposition checks; not a formal floating-point bound."""
    if case["regime"] != "mixed_adr":
        return {}
    g = _geometry(case)
    initial = continuum(case, 0.0)["concentration_mg_l"]
    _, _, velocity, _ = _mixed_parameters(case, g)
    matrix, values, vectors, scale = _mixed_system(g.cells, g.length, g.diffusion, g.decay, velocity)
    norm = float(np.linalg.norm(matrix, ord=np.inf))
    residual = matrix@vectors - vectors*values[None, :]
    reconstruction = (vectors*values[None, :])@vectors.T
    initial_reconstruction = scale*(vectors@(vectors.T@(initial/scale)))
    condition = float(scale.max()/scale.min())
    # A conservative conditioning scale, not an interval enclosure or proof of
    # the LAPACK implementation. Expose its ingredients for the evidence report.
    uncertainty = (np.finfo(np.float64).eps*condition*g.cells*max(1.0, norm)*
                   _number(case["config"]["horizon_h"])*max(1.0, float(np.max(np.abs(initial)))))
    return {"normalized_eigen_residual": float(np.linalg.norm(residual, ord=np.inf))/(norm*float(np.linalg.norm(vectors, ord=np.inf))),
            "orthogonality_error": float(np.linalg.norm(vectors.T@vectors-np.eye(g.cells), ord=np.inf)),
            "normalized_reconstruction_error": float(np.linalg.norm(matrix-reconstruction, ord=np.inf))/norm,
            "initial_reconstruction_maximum_mg_l": float(np.max(np.abs(initial_reconstruction-initial))),
            "similarity_condition": condition, "matrix_inf_norm_per_h": norm,
            "floating_reference_uncertainty_mg_l": float(uncertainty),
            "min_eigenvalue_per_h": float(values.min()), "max_eigenvalue_per_h": float(values.max())}
