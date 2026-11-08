"""Private, pure EOG observation index; no files, wire protocol or model APIs.

This is evidence selection, not a detector, physics forecast or capture verifier.
All public view queries are causal. A view never fills absent readings with zero
or a nominal command. The fingerprint is private evidence identity, not the public
dataset/capture hash. See EOG_OBSERVATIONS.md for policy and limitations.
"""
from __future__ import annotations

from bisect import bisect_left, bisect_right
from dataclasses import dataclass
import hashlib
from itertools import islice
import math
import struct
from types import MappingProxyType
from typing import Iterable

MAX_UNIX = 253402300799
MAX_SAMPLES = 2_000_000
MAX_CHANNELS = 8192
MAX_RETURNED_SAMPLES = 8192
METHOD = "eog-observations-0.1.1"


class ObservationError(ValueError):
    def __init__(self, code: str, message: str):
        self.code = code
        super().__init__(message)


def _integer(value, name, low=0, high=MAX_UNIX):
    if type(value) is not int or not low <= value <= high:
        raise ObservationError("invalid_parameter", f"{name} must be an integer in [{low}, {high}].")
    return value


def _text(value, name):
    if not isinstance(value, str) or not value or "\x00" in value:
        raise ObservationError("invalid_identity", f"{name} must be a nonempty string without NUL.")
    try:
        length = len(value.encode("utf-16-le")) // 2
    except UnicodeError as error:
        raise ObservationError("invalid_identity", f"{name} must contain valid Unicode.") from error
    if length > 256:
        raise ObservationError("limit_exceeded", f"{name} exceeds 256 UTF-16 units.")
    return value


def _finite(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ObservationError("invalid_value", "Observation values must be numeric, not coerced strings/bools.")
    try:
        number = float(value)
    except (ValueError, OverflowError) as error:
        raise ObservationError("invalid_value", "Observation is not finite binary64.") from error
    if not math.isfinite(number):
        raise ObservationError("invalid_value", "Observation is not finite binary64.")
    return number


@dataclass(frozen=True, slots=True)
class Channel:
    asset_id: str
    metric: str
    unit: str
    status_codes: tuple[int, ...] | None = None

    def __post_init__(self):
        _text(self.asset_id, "Asset ID")
        _text(self.metric, "Metric")
        _text(self.unit, "Unit")
        if self.status_codes is not None:
            codes = self.status_codes
            if (type(codes) is not tuple or not 1 <= len(codes) <= 16 or
                    any(type(code) is not int or not 0 <= code <= 255 for code in codes) or
                    len(set(codes)) != len(codes)):
                raise ObservationError("invalid_parameter", "Status codes must be 1-16 unique integers from 0 to 255.")

    @property
    def key(self):
        return self.asset_id, self.metric


@dataclass(frozen=True, slots=True)
class Sample:
    unix: int
    asset_id: str
    metric: str
    value: float

    def __post_init__(self):
        _integer(self.unix, "Sample timestamp")
        _text(self.asset_id, "Asset ID")
        _text(self.metric, "Metric")
        object.__setattr__(self, "value", _finite(self.value))

    @property
    def key(self):
        return self.asset_id, self.metric


@dataclass(frozen=True, slots=True)
class Policy:
    max_age_s: int = 300
    max_gap_s: int = 300
    max_skew_s: int = 0

    def __post_init__(self):
        _integer(self.max_age_s, "Maximum age", high=86400)
        _integer(self.max_gap_s, "Maximum gap", low=1, high=86400)
        _integer(self.max_skew_s, "Maximum acquisition skew", high=86400)


@dataclass(frozen=True, slots=True)
class Reading:
    channel: Channel
    sample: Sample | None
    age_s: int | None
    state: str


@dataclass(frozen=True, slots=True)
class Cohort:
    readings: tuple[Reading, ...]
    state: str


@dataclass(frozen=True, slots=True)
class Window:
    samples: tuple[Sample, ...]
    state: str


@dataclass(frozen=True, slots=True, init=False)
class Archive:
    """Bounded immutable index of already-parsed, globally time-ordered rows.

    Construction checks the complete supplied sequence. Use a separately parsed
    capture for production: this class does not authenticate captured file bytes.
    """
    _channels: object
    _series: object
    _times: object
    first_unix: int
    last_unix: int
    capture_cutoff_unix: int
    sample_count: int

    def __init__(self, channels: Iterable[Channel], samples: Iterable[Sample], *,
                 capture_cutoff_unix=None, max_samples=MAX_SAMPLES, max_channels=MAX_CHANNELS):
        _integer(max_samples, "Sample budget", low=1, high=MAX_SAMPLES)
        _integer(max_channels, "Channel budget", low=1, high=MAX_CHANNELS)
        if capture_cutoff_unix is not None:
            _integer(capture_cutoff_unix, "Verified capture cutoff")
        catalog = {}
        for channel in channels:
            if type(channel) is not Channel:
                raise ObservationError("invalid_parameter", "Expected immutable Channel values.")
            if channel.key in catalog:
                raise ObservationError("duplicate_channel", "Channel catalog contains duplicate identity.")
            catalog[channel.key] = channel
            if len(catalog) > max_channels:
                raise ObservationError("limit_exceeded", "Channel budget exceeded.")
        if not catalog:
            raise ObservationError("invalid_parameter", "An explicit channel catalog is required.")
        grouped = {key: [] for key in catalog}
        previous = first = None
        count = 0
        for sample in samples:
            if type(sample) is not Sample:
                raise ObservationError("invalid_parameter", "Expected immutable Sample values.")
            count += 1
            if count > max_samples:
                raise ObservationError("limit_exceeded", "Sample budget exceeded.")
            if previous is not None and sample.unix < previous:
                raise ObservationError("unordered_samples", "Rows must be ordered by acquisition time.")
            if capture_cutoff_unix is not None and sample.unix > capture_cutoff_unix:
                raise ObservationError("future_sample", "A supplied row exceeds the declared capture cutoff.")
            if sample.key not in catalog:
                raise ObservationError("unknown_channel", "Sample identity is absent from the explicit catalog.")
            values = grouped[sample.key]
            if values and values[-1].unix == sample.unix:
                raise ObservationError("duplicate_sample", "Duplicate acquisition time/channel identity.")
            values.append(sample)
            if first is None:
                first = sample.unix
            previous = sample.unix
        if not count:
            raise ObservationError("empty_input", "No observed data window is available.")
        series = {key: tuple(values) for key, values in grouped.items()}
        object.__setattr__(self, "_channels", MappingProxyType(catalog))
        object.__setattr__(self, "_series", MappingProxyType(series))
        object.__setattr__(self, "_times", MappingProxyType({key: tuple(s.unix for s in values)
                                                          for key, values in series.items()}))
        object.__setattr__(self, "first_unix", first)
        object.__setattr__(self, "last_unix", previous)
        object.__setattr__(self, "capture_cutoff_unix", previous if capture_cutoff_unix is None else capture_cutoff_unix)
        object.__setattr__(self, "sample_count", count)

    def at(self, cutoff_unix: int, policy: Policy = Policy()):
        _integer(cutoff_unix, "Cutoff")
        if not self.first_unix <= cutoff_unix <= self.capture_cutoff_unix:
            raise ObservationError("invalid_cutoff", "Cutoff must lie within the supplied capture clock.")
        if type(policy) is not Policy:
            raise ObservationError("invalid_parameter", "Expected an explicit observation Policy.")
        return View(self, cutoff_unix, policy)


@dataclass(frozen=True, slots=True)
class View:
    _archive: Archive
    cutoff_unix: int
    policy: Policy

    def __post_init__(self):
        if type(self._archive) is not Archive or type(self.policy) is not Policy:
            raise ObservationError("invalid_parameter", "View requires Archive and Policy.")
        _integer(self.cutoff_unix, "Cutoff")
        if not self._archive.first_unix <= self.cutoff_unix <= self._archive.capture_cutoff_unix:
            raise ObservationError("invalid_cutoff", "Cutoff lies outside the supplied capture clock.")

    def _key(self, asset_id, metric):
        key = _text(asset_id, "Asset ID"), _text(metric, "Metric")
        if key not in self._archive._channels:
            raise ObservationError("unknown_channel", "Requested channel is absent from the catalog.")
        return key

    def latest(self, asset_id: str, metric: str) -> Reading:
        key = self._key(asset_id, metric)
        channel = self._archive._channels[key]
        end = bisect_right(self._archive._times[key], self.cutoff_unix)
        if not end:
            return Reading(channel, None, None, "missing")
        sample = self._archive._series[key][end - 1]
        age = self.cutoff_unix - sample.unix
        state = "stale" if age > self.policy.max_age_s else "available"
        if state == "available" and channel.status_codes is not None and sample.value not in channel.status_codes:
            state = "unsupported_status"
        return Reading(channel, sample, age, state)

    def series(self, asset_id: str, metric: str, *, start_unix=None,
               max_points=MAX_RETURNED_SAMPLES) -> tuple[Sample, ...]:
        key = self._key(asset_id, metric)
        _integer(max_points, "Returned sample budget", low=1, high=MAX_RETURNED_SAMPLES)
        times = self._archive._times[key]
        left = 0
        if start_unix is not None:
            _integer(start_unix, "Series start", high=self.cutoff_unix)
            left = bisect_left(times, start_unix)
        end = bisect_right(times, self.cutoff_unix)
        if end - left > max_points:
            raise ObservationError("limit_exceeded", "Query exceeds retained sample budget; choose a smaller window.")
        return self._archive._series[key][left:end]

    def cohort(self, keys: Iterable[tuple[str, str]]) -> Cohort:
        selected = []
        seen = set()
        for key in keys:
            if type(key) is not tuple or len(key) != 2:
                raise ObservationError("invalid_parameter", "Cohort keys must be (asset ID, metric) tuples.")
            key = self._key(*key)
            if key in seen:
                raise ObservationError("duplicate_channel", "Cohort repeats a channel.")
            seen.add(key)
            selected.append(self.latest(*key))
            if len(selected) > MAX_CHANNELS:
                raise ObservationError("limit_exceeded", "Cohort channel budget exceeded.")
        if not selected:
            raise ObservationError("invalid_parameter", "Cohort must request at least one channel.")
        readings = tuple(selected)
        # Keep every reading and its individual reason; this summary never grants
        # authority to the remaining channels when one is unavailable.
        if any(r.state != "available" for r in readings):
            return Cohort(readings, "unavailable")
        stamps = [r.sample.unix for r in readings]
        state = "incoherent" if max(stamps) - min(stamps) > self.policy.max_skew_s else "available"
        return Cohort(readings, state)

    def window(self, asset_id: str, metric: str, start_unix: int, *,
               max_points=MAX_RETURNED_SAMPLES) -> Window:
        """Require uninterrupted support from start through cutoff.

        Retain one predecessor at/before start when present, so a boundary gap
        cannot disappear merely because it falls outside the displayed window.
        """
        key = self._key(asset_id, metric)
        _integer(start_unix, "Window start", high=self.cutoff_unix)
        _integer(max_points, "Returned sample budget", low=1, high=MAX_RETURNED_SAMPLES)
        times = self._archive._times[key]
        end = bisect_right(times, self.cutoff_unix)
        if not end:
            return Window((), "missing")
        left = max(0, bisect_right(times, start_unix) - 1)
        if end - left > max_points:
            raise ObservationError("limit_exceeded", "Window exceeds retained sample budget.")
        values = self._archive._series[key][left:end]
        if values[0].unix > start_unix:
            return Window(values, "insufficient_history")
        latest = self.latest(asset_id, metric)
        if latest.state != "available":
            return Window(values, latest.state)
        if (start_unix - values[0].unix > self.policy.max_gap_s or
                self.cutoff_unix - values[-1].unix > self.policy.max_gap_s or any(
                b.unix - a.unix > self.policy.max_gap_s for a, b in zip(values, values[1:]))):
            return Window(values, "gap")
        codes = self._archive._channels[key].status_codes
        if codes is not None and any(s.value not in codes for s in values):
            return Window(values, "unsupported_status")
        return Window(values, "available")

    def evidence_fingerprint(self) -> str:
        """Hash only the causal prefix and policy, independent of future rows.

        This deliberately differs from the public full-source dataset identity.
        Never substitute it for capture integrity verification.
        """
        digest = hashlib.sha256(b"breakwater-private-eog-observations-v1\n")
        digest.update(struct.pack(">QIII", self.cutoff_unix, self.policy.max_age_s,
                                  self.policy.max_gap_s, self.policy.max_skew_s))
        for key, channel in sorted(self._archive._channels.items()):
            for text in (*key, channel.unit):
                raw = text.encode("utf-8")
                digest.update(struct.pack(">I", len(raw)))
                digest.update(raw)
            codes = channel.status_codes
            raw_codes = repr(codes).encode("ascii")
            digest.update(struct.pack(">I", len(raw_codes)))
            digest.update(raw_codes)
            end = bisect_right(self._archive._times[key], self.cutoff_unix)
            digest.update(struct.pack(">I", end))
            for sample in islice(self._archive._series[key], end):
                digest.update(struct.pack(">Qd", sample.unix, sample.value))
        return digest.hexdigest()
