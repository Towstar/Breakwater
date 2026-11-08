"""Bounded fixed-file EOG reader for offline evaluation and future capture loading.

This does not replace the public capture verifier or provide a worker protocol.
Only the three explicit allowed names are opened; directory contents are never
enumerated. Source bytes are checked again after parsing to reject mutation.
"""
from __future__ import annotations

import csv
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import re

from eog_model import Model, ModelError, compile_model
from eog_observations import Archive, MAX_SAMPLES, ObservationError, Sample

FILES = ("network.json", "ops.json", "telemetry.csv")
JSON_LIMIT = 4 * 1024 * 1024
CSV_LIMIT = 128 * 1024 * 1024


class InputError(ValueError):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


@dataclass(frozen=True, slots=True)
class Inputs:
    model: Model
    observations: Archive
    file_hashes: tuple[tuple[str, str], ...]
    raw_input_set_identity: str


def _json(raw):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise InputError("duplicate_json_key", "Duplicate configuration JSON key.")
            result[key] = value
        return result

    def finite(text):
        import math
        value = float(text)
        if not math.isfinite(value):
            raise InputError("nonfinite_input", "Nonfinite JSON number.")
        return value

    def no_constant(_):
        raise InputError("nonfinite_input", "Nonfinite JSON constant.")

    try:
        text = raw.decode("utf-8-sig")
        depth, quoted, escape = 0, False, False
        for char in text:
            if quoted:
                if escape:
                    escape = False
                elif char == "\\":
                    escape = True
                elif char == '"':
                    quoted = False
            elif char == '"':
                quoted = True
            elif char in "[{":
                depth += 1
                if depth > 64:
                    raise InputError("limit_exceeded", "Configuration nesting exceeds 64.")
            elif char in "]}":
                depth -= 1
        return json.loads(text, object_pairs_hook=unique, parse_float=finite, parse_constant=no_constant)
    except (UnicodeError, json.JSONDecodeError, RecursionError, ValueError) as error:
        if isinstance(error, InputError):
            raise
        raise InputError("invalid_json", "Invalid bounded UTF-8 configuration JSON.") from error


def _rows(raw):
    try:
        reader = csv.reader(io.StringIO(raw.decode("utf-8-sig"), newline=""), strict=True)
        if next(reader, None) != ["timestamp", "ts_unix", "asset_id", "metric", "value"]:
            raise InputError("invalid_csv", "Unexpected telemetry CSV header.")
        count, previous_unix, expected_timestamp = 0, None, None
        for row in reader:
            count += 1
            if count > MAX_SAMPLES:
                raise InputError("limit_exceeded", "Telemetry exceeds the row budget.")
            if len(row) != 5 or not re.fullmatch(r"0|[1-9][0-9]{0,11}", row[1]):
                raise InputError("invalid_csv", "Malformed telemetry row or exact Unix timestamp.")
            unix = int(row[1])
            if unix != previous_unix:
                try:
                    expected_timestamp = datetime.fromtimestamp(unix, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
                except (OverflowError, OSError, ValueError) as error:
                    raise InputError("invalid_csv", "Unsupported telemetry timestamp.") from error
                previous_unix = unix
            if row[0] != expected_timestamp:
                raise InputError("invalid_csv", "Telemetry UTC text disagrees with exact Unix timestamp.")
            try:
                value = float(row[4])
            except (ValueError, OverflowError) as error:
                raise InputError("invalid_csv", "Malformed telemetry numeric value.") from error
            yield Sample(unix, row[2], row[3], value)
    except (UnicodeError, csv.Error) as error:
        raise InputError("invalid_csv", "Malformed UTF-8 telemetry CSV.") from error


def _read(path, limit):
    if (not path.is_file() or path.is_symlink() or
            (hasattr(path, "is_junction") and path.is_junction())):
        raise InputError("invalid_path", "Allowed input must be a regular local file.")
    stat = path.stat()
    if stat.st_size > limit:
        raise InputError("limit_exceeded", "Allowed input exceeds its byte budget.")
    with path.open("rb") as handle:
        raw = handle.read(limit + 1)
    if len(raw) > limit:
        raise InputError("limit_exceeded", "Allowed input grew beyond its byte budget.")
    return raw, (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns)


def load_inputs(directory, *, capture_cutoff_unix=None) -> Inputs:
    """Read fixed files, compile semantics, index observations and reject mutation.

    An optional cutoff is a caller claim, not authenticated by this function.
    A production caller must first verify the published capture contract. The
    raw-input identity below hashes actual loaded files, not a source manifest.
    """
    folder = Path(directory)
    if (not folder.is_dir() or folder.is_symlink() or str(folder).startswith(("\\\\", "//")) or
            (hasattr(folder, "is_junction") and folder.is_junction())):
        raise InputError("invalid_path", "Expected a plain local input directory.")
    try:
        originals = {name: _read(folder / name, CSV_LIMIT if name == "telemetry.csv" else JSON_LIMIT) for name in FILES}
        model = compile_model(_json(originals["network.json"][0]), _json(originals["ops.json"][0]))
        archive = Archive(model.channels(), _rows(originals["telemetry.csv"][0]), capture_cutoff_unix=capture_cutoff_unix)
        hashes = tuple((name, hashlib.sha256(originals[name][0]).hexdigest()) for name in FILES)
        for name, digest in hashes:
            reread, stat = _read(folder / name, CSV_LIMIT if name == "telemetry.csv" else JSON_LIMIT)
            if stat != originals[name][1] or hashlib.sha256(reread).hexdigest() != digest:
                raise InputError("input_changed", "Allowed input changed while the snapshot was being read.")
        identity_bytes = ("breakwater-eog-inputs-v1\n" + "".join(f"{name}:{digest}\n" for name, digest in hashes)).encode("ascii")
        return Inputs(model, archive, hashes, hashlib.sha256(identity_bytes).hexdigest())
    except (ModelError, ObservationError) as error:
        raise InputError(error.code, str(error)) from error
    except OSError as error:
        raise InputError("input_io_failed", "Could not read the fixed local inputs.") from error
