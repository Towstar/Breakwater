"""Bounded float64 storage sample grids, without numerical runtime imports.

Preserve every requested k*step sample strictly before the horizon, then append
the original horizon exactly once. Division is deliberately not used: a rounded
quotient can omit a final interval or invent a duplicate final sample.
"""
from __future__ import annotations

import math
from numbers import Real

MAX_STORAGE_UPDATES = 10_000


class StorageGridError(ValueError):
    """An explicit invalid-parameter or work-budget failure."""

    def __init__(self, code: str, message: str):
        self.code = code
        super().__init__(message)


def _finite_number(value):
    if isinstance(value, bool) or not isinstance(value, Real):
        raise StorageGridError("invalid_parameters", "Storage times must be finite float64 numbers.")
    try:
        value = float(value)
    except (OverflowError, ValueError) as error:
        raise StorageGridError("invalid_parameters", "Storage times must be finite float64 numbers.") from error
    if not math.isfinite(value):
        raise StorageGridError("invalid_parameters", "Storage times must be finite float64 numbers.")
    return value


def storage_sample_times(horizon_h, step_h):
    """Return strictly increasing samples with the exact requested endpoint.

    The bounded work test uses the same rounded multiples as the actual grid.
    A ratio that underflows is harmless: step > horizon simply gives [0,horizon].
    """
    horizon, step = _finite_number(horizon_h), _finite_number(step_h)
    if not 0 < horizon <= 24 or step <= 0:
        raise StorageGridError("invalid_parameters", "Storage horizon must be in (0,24] hours and step must be positive.")
    if horizon > MAX_STORAGE_UPDATES * step:
        raise StorageGridError("work_limit", "Storage forecasts support at most 10,000 updates.")
    times = [0.0]
    for index in range(1, MAX_STORAGE_UPDATES + 1):
        time = index * step
        if time >= horizon:
            times.append(horizon)
            return times
        if time <= times[-1]:
            raise StorageGridError("invalid_parameters", "Storage sample intervals must be representable and strictly increasing.")
        times.append(time)
    raise StorageGridError("work_limit", "Storage forecasts support at most 10,000 updates.")
