"""F1 physical references, constructed without native initial/result buffers.

Pure-advection cell averages, point observations and continuous first crossings.
The initial-pulse semigroup is a Poisson convolution, including physical escape.
Numerical cross-checks/roundoff estimates are not formal interval enclosures.
"""
from __future__ import annotations

from decimal import Decimal, localcontext
from functools import lru_cache
import math

import numpy as np

from .transport_reference import continuum as a9_continuum


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value):
    require(isinstance(value, (int, float)) and not isinstance(value, bool) and
            math.isfinite(value), "Reference values must be finite numbers")
    return float(value)


def geometry(case):
    p = case["pipe"]
    n = p["cells"]
    require(isinstance(n, int) and not isinstance(n, bool) and 2 <= n <= 4096,
            "Reference mesh must contain 2..4096 cells")
    length, area = number(p["length_m"]), number(p["area_m2"])
    require(length > 0 and area > 0 and number(p["dispersion_m2_h"]) == 0 and
            number(p["decay_per_h"]) == 0, "F1 references require pure advection")
    require(case["config"]["boundary"] == "dirichlet_inlet" and
            len(case["source_mg_l_h"]) == n and
            all(number(v) == 0 for v in case["source_mg_l_h"]), "F1 requires zero source/open inlet")
    return n, length, area


def schedule(case, name, key):
    points = [(number(k["time_h"]), number(k[key])) for k in case[name]]
    require(points and points[0][0] == 0 and all(v >= 0 for _, v in points) and
            all(b[0] > a[0] for a, b in zip(points, points[1:])), "Invalid reference schedule")
    return points


def time_value(case, time):
    time = number(time)
    require(0 <= time <= number(case["config"]["horizon_h"]), "Reference time outside forecast")
    return time


def flow_segments(case, time):
    """Positive or paused intervals with cumulative advective distance at start."""
    _, _, area = geometry(case)
    flow = schedule(case, "flow", "flow_m3h")
    clock, segments = 0., []
    for i, (left, q) in enumerate(flow):
        right = min(time, flow[i+1][0] if i+1 < len(flow) else time)
        if right <= left:
            continue
        velocity = q/area
        segments.append((left, right, velocity, clock))
        clock += (right-left)*velocity
    return segments, clock


def displacement(case, time):
    return flow_segments(case, time_value(case, time))[1]


def outlet_flow(case, time):
    time = time_value(case, time)
    return next(q for start, q in reversed(schedule(case, "flow", "flow_m3h")) if start <= time)


def _pulse_parameters(case):
    _, length, _ = geometry(case)
    p = case["reference_parameters"]
    start, width = number(p["bump_start_m"]), number(p["bump_width_m"])
    require(start >= 0 and width > 0 and start+width <= length, "Invalid pulse support")
    require(all(value == 0 for _, value in schedule(case, "inlet", "concentration_mg_l")),
            "Initial-pulse reference requires zero inlet")
    return start, width


def point_values(case, x, time):
    """Point continuum observations; interfaces use right-continuous arrival time."""
    time = time_value(case, time)
    _, length, _ = geometry(case)
    x = np.asarray(x, dtype=np.float64)
    require(np.isfinite(x).all() and np.all((0 <= x) & (x <= length)), "Point outside pipe")
    segments, distance = flow_segments(case, time)
    if case["regime"] in ("front_smooth", "front_box"):
        start, width = _pulse_parameters(case)
        entered = x-distance-start
        # Compare arrival distances before subtracting near a wave interface:
        # x-distance can round a just-before-arrival point onto the jump.
        inside = (distance >= x-start-width) & (distance < x-start)
        if case["regime"] == "front_box":
            return inside.astype(np.float64)
        return np.where(inside, .5*(1-np.cos(2*math.pi*entered/width)), 0.)
    require(case["regime"] == "scheduled_inlet" and
            number(case["reference_parameters"]["initial_mg_l"]) == 0, "Unsupported reference family")
    inlet = schedule(case, "inlet", "concentration_mg_l")
    target = distance-x
    values, assigned = np.zeros_like(x), np.zeros_like(x, dtype=bool)
    # Latest inverse at a pause interface selects the post-pause emitted value.
    for left, right, velocity, clock in reversed(segments):
        if velocity == 0:
            continue
        mask = (~assigned) & (target >= clock) & (target <= clock+(right-left)*velocity)
        entered = left+(target[mask]-clock)/velocity
        values[mask] = np.interp(entered, [t for t, _ in inlet], [v for _, v in inlet])
        assigned[mask] = True
    return values


def continuum(case, time):
    time = time_value(case, time)
    n, length, area = geometry(case)
    if case["regime"] == "scheduled_inlet":
        return a9_continuum(case, time)
    start, width = _pulse_parameters(case)
    require(case["regime"] in ("front_smooth", "front_box"), "Unsupported reference family")
    dx, shift = length/n, displacement(case, time)
    edges = np.arange(n+1, dtype=np.float64)*dx
    # Intersection with analytic support only; predicted fields are never clipped.
    z = np.minimum(width, np.maximum(0., edges-shift-start))
    primitive = z if case["regime"] == "front_box" else .5*z-width*np.sin(2*math.pi*z/width)/(4*math.pi)
    field = np.diff(primitive)/dx
    mass = 1000*area*float(primitive[-1]-primitive[0])
    initial_mass = 1000*area*width*(1. if case["regime"] == "front_box" else .5)
    return dict(concentration_mg_l=field, inventory_mg=mass, cumulative_inlet_mg=0.,
                cumulative_outlet_mg=initial_mass-mass, cumulative_source_mg=0., cumulative_decay_mg=0.)


def poisson_weights(z, count):
    z = number(z)
    require(z >= 0 and isinstance(count, int) and not isinstance(count, bool) and count > 0,
            "Invalid Poisson clock or count")
    if z == 0:
        values = np.zeros(count); values[0] = 1.
        return values
    # A recurrence avoids independent lgamma cancellation at every weight. For
    # large clocks start at the mode, so an underflowing zero-order probability
    # cannot erase its representable neighbors. Never normalize the saved tail.
    values = np.zeros(count)
    if z <= 700:
        values[0] = math.exp(-z)
        for k in range(1, count):
            values[k] = values[k-1]*z/k
    else:
        mode = min(count-1, int(z))
        values[mode] = math.exp(-z+mode*math.log(z)-math.lgamma(mode+1))
        for k in range(mode, 0, -1):
            values[k-1] = values[k]*k/z
        for k in range(mode+1, count):
            values[k] = values[k-1]*z/k
    return values


def semidiscrete(case, time):
    if case["regime"] == "scheduled_inlet":
        return None
    n, length, _ = geometry(case)
    initial = continuum(case, 0.)["concentration_mg_l"]
    weights = poisson_weights(displacement(case, time)*n/length, n)
    return np.convolve(initial, weights)[:n]


def poisson_decimal_disagreement(z, count):
    """Independent 80-digit recurrence check, including clocks above exp underflow."""
    weights = poisson_weights(z, count)
    with localcontext() as ctx:
        ctx.prec = 80
        value = (-Decimal.from_float(float(z))).exp()
        clock = Decimal.from_float(float(z))
        errors = []
        for k in range(count):
            errors.append(abs(weights[k]-float(value)))
            value = value*clock/Decimal(k+1)
    return max(errors)


@lru_cache(maxsize=2)
def _quadrature(order):
    return np.polynomial.legendre.leggauss(order)


def quadrature_averages(case, time, order=32):
    """Independent inverse-characteristic quadrature, split at every wave interface."""
    n, length, _ = geometry(case)
    dx, distance = length/n, displacement(case, time)
    edges = np.arange(n+1, dtype=np.float64)*dx
    if case["regime"] in ("front_smooth", "front_box"):
        start, width = _pulse_parameters(case)
        breaks = [start+distance, start+width+distance]
    else:
        knots = {t for t, _ in schedule(case, "flow", "flow_m3h")}
        knots.update(t for t, _ in schedule(case, "inlet", "concentration_mg_l"))
        breaks = [distance-displacement(case, t) for t in knots if t <= time]
    cuts = np.unique(np.r_[edges, [x for x in breaks if 0 < x < length]])
    left, right = cuts[:-1], cuts[1:]
    middle, half = (left+right)/2, (right-left)/2
    nodes, weights = _quadrature(order)
    values = point_values(case, middle[:, None]+half[:, None]*nodes, time)
    integrals = half*(values@weights)
    field = np.zeros(n)
    # Assign segments geometrically; midpoint indexing does not alter concentrations.
    indices = np.minimum(n-1, (middle/dx).astype(int))
    np.add.at(field, indices, integrals/dx)
    return field


def reference_check(case, times, multiplier=128):
    """Check all saved continuum frames, plus selected semigroup clocks."""
    n, _, _ = geometry(case)
    maximum = 0.
    for t in times:
        exact = continuum(case, float(t))["concentration_mg_l"]
        for order in (32, 64):
            maximum = max(maximum, float(np.max(np.abs(exact-quadrature_averages(case, float(t), order)))))
    kernel_error = 0.
    if case["regime"] != "scheduled_inlet":
        _, length, _ = geometry(case)
        for t in (times[0], times[len(times)//2], times[-1]):
            # L1 kernel disagreement bounds its convolution against a unit-height profile.
            kernel_error = max(kernel_error, n*poisson_decimal_disagreement(displacement(case, float(t))*n/length, n))
    observed = max(maximum, kernel_error)
    roundoff = multiplier*np.finfo(float).eps*(n+len(case["flow"])+len(case["inlet"])+1)
    return dict(maximum_quadrature_disagreement_mg_l=maximum,
                semigroup_l1_disagreement_bound_mg_l=kernel_error,
                crosscheck_disagreement_mg_l=observed,
                uncertainty_estimate_mg_l=max(observed, float(roundoff)),
                scope="Floating-point cross-check/roundoff scale; not a formal enclosure")


def _arrival(case, emitted_time):
    """Arrival of an emitted characteristic at the physical outlet, or None."""
    _, length, _ = geometry(case)
    horizon = case["config"]["horizon_h"]
    goal = displacement(case, emitted_time)+length
    for left, right, velocity, clock in flow_segments(case, horizon)[0]:
        if velocity > 0 and clock <= goal <= clock+(right-left)*velocity:
            arrival = left+(goal-clock)/velocity
            if arrival >= emitted_time:
                return arrival
    return None


def outlet_peak_reference(case):
    """Continuous outlet supremum over the horizon, not a saved-frame peak."""
    horizon = case["config"]["horizon_h"]
    _, length, _ = geometry(case)
    segments, distance = flow_segments(case, horizon)
    if not any(v > 0 for _, _, v, _ in segments):
        return None
    if case["regime"] in ("front_smooth", "front_box"):
        start, width = _pulse_parameters(case)
        if distance < length-start-width:
            return 0.
        if case["regime"] == "front_box" or distance >= length-start-width/2:
            return 1.
        return float(point_values(case, [length], horizon)[0])
    require(case["regime"] == "scheduled_inlet", "Unsupported outlet peak family")
    inlet = schedule(case, "inlet", "concentration_mg_l")
    # Emissions can arrive by H only if their advective clock is <= d(H)-L.
    # The signal is affine on each flow/inlet interval, so endpoints/knots suffice.
    emitted_clock_limit, peak = distance-length, 0.
    for left, right, velocity, clock in segments:
        if velocity == 0 or clock > emitted_clock_limit:
            continue
        end = min(right, left+(emitted_clock_limit-clock)/velocity)
        candidates = [left, end, *(t for t, _ in inlet if left < t < end)]
        peak = max(peak, float(np.max(np.interp(candidates, [t for t, _ in inlet], [v for _, v in inlet]))))
    return peak


def threshold_reference(case, threshold):
    """Continuous first upward outlet crossings, independent of sampled output."""
    threshold = number(threshold)
    require(0 < threshold < 1, "F1 thresholds must be strictly between zero and unit peak")
    _, length, _ = geometry(case)
    crossings = []
    if case["regime"] in ("front_smooth", "front_box"):
        start, width = _pulse_parameters(case)
        flow = schedule(case, "flow", "flow_m3h")
        require(len(flow) == 1 and flow[0][1] > 0, "Pulse crossing requires constant positive flow")
        velocity = flow[0][1]/case["pipe"]["area_m2"]
        offset = 0. if case["regime"] == "front_box" else width*math.acos(1-2*threshold)/(2*math.pi)
        value = (length-start-width+offset)/velocity
        if value <= case["config"]["horizon_h"]:
            crossings.append(value)
    else:
        require(case["regime"] == "scheduled_inlet", "Unsupported threshold family")
        inlet = schedule(case, "inlet", "concentration_mg_l")
        flow = schedule(case, "flow", "flow_m3h")
        end = case["config"]["horizon_h"]
        cuts = sorted({0., end, *(t for t, _ in inlet if t < end), *(t for t, _ in flow if t < end)})
        previous = 0.
        for left, right in zip(cuts, cuts[1:]):
            q = next(q for t, q in reversed(flow) if t <= left)
            if q == 0:
                continue
            a, b = np.interp([left, right], [t for t, _ in inlet], [v for _, v in inlet])
            candidate = None
            if previous < threshold <= a:
                candidate = left  # A resumed stream may carry a concentration jump.
            elif a < threshold <= b:
                candidate = left+(right-left)*(threshold-a)/(b-a)
            if candidate is not None:
                arrival = _arrival(case, float(candidate))
                if arrival is not None:
                    crossings.append(arrival)
            previous = b
    return dict(threshold_mg_l=threshold, first_upward_h=crossings[0] if crossings else None,
                upward_crossings_h=crossings, upward_crossing_count=len(crossings),
                status="reached" if crossings else "not_reached")
