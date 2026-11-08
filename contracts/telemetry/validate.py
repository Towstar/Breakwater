"""Offline G1 contract validation. This module performs no acquisition or physics.

Public APIs validate evidence/configuration and classify fixed-clock fixtures only.
They never mutate a project, create a forecast or authorize operating controls.
"""
from __future__ import annotations

import argparse
import copy
from datetime import datetime
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys

from jsonschema import Draft202012Validator
from referencing import Registry, Resource
from referencing.exceptions import NoSuchResource

ROOT = Path(__file__).resolve().parent
MAX_DOCUMENT_BYTES = 4 * 1024 * 1024
MAX_FRAME_BYTES = 256 * 1024
MAX_DEPTH = 64
MAX_SENSORS = 256
MAX_READINGS = 256
LIMITS = {"frame_bytes": MAX_FRAME_BYTES, "sensors": MAX_SENSORS, "readings_per_batch": MAX_READINGS,
          "reorder_batches": 16, "reorder_bytes": 4 * 1024 * 1024,
          "trace_bytes": 32 * 1024 * 1024, "trace_readings": 32768,
          "ingress_readings_per_s": 1024, "ingress_bytes_per_s": 1024 * 1024}
DEFAULT_POLICY = {"max_skew_s": 1.0, "reorder_window_s": 2.0, "future_allowance_s": 0.25,
                  "heartbeat_period_s": 1.0, "source_timeout_s": 5.0}
SCHEMAS = {"sensor_configuration": "sensor-configuration-v1.schema.json",
           "stream_frame": "stream-frame-v1.schema.json", "stream_command": "stream-command-v1.schema.json",
           "observed_snapshot": "observed-snapshot-v1.schema.json", "detection_event": "detection-event-v1.schema.json",
           "project_v2": "project-v2.schema.json"}
UNITS = {"inventory": "m3", "level": "m", "source_flow": "m3/h", "route_flow": "m3/h",
         "pump_state": "state", "concentration": "mg/L", "pressure": "kPa", "temperature": "degC"}
PUMP_STATES = {"commanded_off", "running", "delivery_loss", "unavailable", "unknown"}
ID_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}")
HASH_PATTERN = re.compile(r"[0-9a-f]{64}")


class ContractError(ValueError):
    def __init__(self, code: str, message: str):
        self.code = code
        super().__init__(message)


def require(condition, code, message):
    if not condition:
        raise ContractError(code, message)


def _pairs(items):
    result = {}
    for key, value in items:
        require(key not in result, "duplicate_key", "JSON contains a duplicate object key.")
        result[key] = value
    return result


def _finite(value, depth=0):
    require(depth <= MAX_DEPTH, "limit_exceeded", "JSON nesting exceeds the contract limit.")
    if isinstance(value, str):
        try:
            value.encode("utf-8", errors="strict")
        except UnicodeEncodeError as error:
            raise ContractError("invalid_utf8", "Unpaired Unicode surrogate is not valid UTF-8 text.") from error
    elif isinstance(value, (int, float)) and not isinstance(value, bool):
        try:
            require(math.isfinite(float(value)), "nonfinite", "All numbers must be finite binary64 values.")
        except OverflowError as error:
            raise ContractError("nonfinite", "Number cannot be represented as finite binary64.") from error
    elif isinstance(value, list):
        for item in value:
            _finite(item, depth + 1)
    elif isinstance(value, dict):
        for key, item in value.items():
            _finite(key, depth + 1)
            _finite(item, depth + 1)


def decode_document(data: bytes | str):
    if isinstance(data, str):
        try:
            data = data.encode("utf-8", errors="strict")
        except UnicodeEncodeError as error:
            raise ContractError("invalid_utf8", "Document is not valid UTF-8.") from error
    require(len(data) <= MAX_DOCUMENT_BYTES, "limit_exceeded", "Document exceeds 4 MiB.")
    try:
        value = json.loads(data.decode("utf-8", errors="strict"), object_pairs_hook=_pairs,
                           parse_constant=lambda _: (_ for _ in ()).throw(ContractError("nonfinite", "Nonfinite JSON is forbidden.")))
    except ContractError:
        raise
    except (ValueError, UnicodeError, RecursionError) as error:
        raise ContractError("invalid_json", "Expected one bounded UTF-8 JSON document.") from error
    require(isinstance(value, dict), "invalid_shape", "A contract document must be an object.")
    _finite(value)
    if value.get("kind") in ("stream_frame", "stream_command"):
        require(len(data) <= MAX_FRAME_BYTES, "limit_exceeded", "Stream frame exceeds 256 KiB including its newline.")
    return value


def _local_only(uri):
    raise NoSuchResource(ref=uri)


def _validators():
    documents = {}
    for path in ROOT.glob("*.schema.json"):
        document = json.loads(path.read_text(encoding="utf-8"))
        Draft202012Validator.check_schema(document)
        documents[path.name] = document
    registry = Registry(retrieve=_local_only).with_resources(
        (document["$id"], Resource.from_contents(document)) for document in documents.values())
    return {kind: Draft202012Validator(documents[name], registry=registry) for kind, name in SCHEMAS.items()}


_VALIDATORS = None


def _shape(value, kind):
    global _VALIDATORS
    if _VALIDATORS is None:
        _VALIDATORS = _validators()
    require(isinstance(kind, str) and kind in _VALIDATORS, "unsupported_kind", "Unsupported G1 document kind.")
    errors = list(_VALIDATORS[kind].iter_errors(value))
    require(not errors, "invalid_shape", "Document violates the strict local G1 schema.")


def _unique(items, field, message):
    keys = [item[field] for item in items]
    require(len(keys) == len(set(keys)), "duplicate_id", message)


def _utf16_reference(value):
    return len(value.encode("utf-16-le")) // 2 <= 256


def _context_network(context):
    if "network" in context:
        return context["network"]
    return context.get("project", {}).get("network")


def _context_pipes(context):
    if "pipe_parameters" in context:
        return context["pipe_parameters"]
    return context.get("project", {}).get("pipe_transport", {}).get("routes", {})


def _json_equal(left, right):
    """Compare JSON types with native binary64 numeric semantics, not bool == 1."""
    if isinstance(left, bool) or isinstance(right, bool):
        return isinstance(left, bool) and isinstance(right, bool) and left == right
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return float(left) == float(right)
    if isinstance(left, dict) and isinstance(right, dict):
        return left.keys() == right.keys() and all(_json_equal(left[key], right[key]) for key in left)
    if isinstance(left, list) and isinstance(right, list):
        return len(left) == len(right) and all(_json_equal(a, b) for a, b in zip(left, right))
    return type(left) is type(right) and left == right


def _validate_context(context, kind):
    require(isinstance(context, dict), "invalid_context", "Validation context must be an object.")
    _finite(context)
    for field in ("network", "project", "configuration", "pipe_parameters", "pipe_parameter_hashes", "command", "previous_frame", "clock"):
        require(field not in context or isinstance(context[field], dict), "invalid_context", "Provided context records must be objects.")
    if "physical_hash" in context:
        require(isinstance(context["physical_hash"], str) and HASH_PATTERN.fullmatch(context["physical_hash"]) is not None,
                "invalid_context", "Registered physical hash is invalid.")
    if "network" in context:
        _legacy_project({"version": 1, "network": context["network"]})
    if "project" in context:
        _legacy_project(context["project"])
        project = context["project"]
        if "network" in context:
            require(_json_equal(context["network"], project["network"]), "invalid_context", "Registered network and project contradict each other.")
        if "pipe_parameters" in context and "pipe_transport" in project:
            require(_json_equal(context["pipe_parameters"], project["pipe_transport"]["routes"]), "invalid_context", "Registered pipe parameters and project contradict each other.")
    if "clock" in context:
        clock = context["clock"]
        require(set(clock) == {"stream_id", "epoch_id", "now_event_time_s", "watermark_event_time_s", "mode"},
                "invalid_clock", "Receiver clock requires stream, epoch, now, watermark and mode.")
        require(all(isinstance(clock[field], str) and ID_PATTERN.fullmatch(clock[field]) is not None for field in ("stream_id", "epoch_id")),
                "invalid_clock", "Receiver clock identities are invalid.")
        require(all(isinstance(clock[field], (int, float)) and not isinstance(clock[field], bool) and clock[field] >= 0
                    for field in ("now_event_time_s", "watermark_event_time_s")), "invalid_clock", "Receiver clock times must be finite nonnegative numbers.")
        require(clock["mode"] in ("synthetic_live", "replay", "historical"), "invalid_clock", "Receiver clock mode is invalid.")


def _legacy_project(project):
    """Mirror existing ProjectDocument parse requirements, not engine physics."""
    require(isinstance(project, dict) and project.get("version") == 1 and not isinstance(project.get("version"), bool),
            "invalid_project", "Embedded/context project must use existing version 1.")
    network = project.get("network")
    require(isinstance(network, dict) and isinstance(network.get("assets"), list) and isinstance(network.get("routes"), list),
            "invalid_project", "The existing v1 network needs asset/route arrays.")
    require(len(network["assets"]) <= 128 and len(network["routes"]) <= 256, "limit_exceeded", "Network exceeds existing size limits.")

    def text(item, key):
        value = item.get(key)
        require(isinstance(value, str) and _utf16_reference(value), "invalid_project", "Existing v1 text field is invalid.")
        return value

    def numeric(item, key, fallback=0.0):
        value = item.get(key, fallback)
        require(isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(float(value)),
                "invalid_project", "Existing v1 numeric field is invalid.")
        return value

    identities = []
    for asset in network["assets"]:
        require(isinstance(asset, dict), "invalid_project", "Existing v1 asset must be an object.")
        identities.append(text(asset, "id")); text(asset, "label")
        kind = text(asset, "kind")
        require(kind in ("tank", "source", "treatment", "trucking"), "invalid_project", "Unknown existing asset kind.")
        numeric(asset, "capacity_m3" if kind == "tank" else "capacity_m3h")
        for field in ("initial_m3", "inflow_m3h", "curtailment_m3h", "curtailment_max_m3h", "cost_per_m3"):
            numeric(asset, field)
    for route in network["routes"]:
        require(isinstance(route, dict), "invalid_project", "Existing v1 route must be an object.")
        identities.append(text(route, "id")); text(route, "source_id"); text(route, "target_id")
        for field in ("capacity_m3h", "setpoint_m3h", "cost_per_m3"):
            numeric(route, field)
        for field in ("has_pump", "available"):
            require(field not in route or isinstance(route[field], bool), "invalid_project", "Existing v1 route flag must be Boolean.")
    require(all(identities) and len(identities) == len(set(identities)), "duplicate_id", "Existing project IDs must be nonempty and globally unique.")
    if "pipe_transport" in project:
        transport = project["pipe_transport"]
        require(isinstance(transport, dict) and set(transport) == {"version", "routes"} and transport["version"] == 1 and
                not isinstance(transport["version"], bool) and isinstance(transport["routes"], dict),
                "invalid_project", "Invalid existing pipe-transport envelope.")
        route_ids = {route["id"] for route in network["routes"]}
        require(set(transport["routes"]) <= route_ids, "invalid_project", "Pipe parameters reference an absent route.")
        schema = json.loads((ROOT.parent / "science/pipe-parameters-v1.schema.json").read_text(encoding="utf-8"))
        validator = Draft202012Validator(schema)
        require(all(validator.is_valid(parameters) for parameters in transport["routes"].values()), "invalid_project", "Existing pipe parameters violate their published schema.")
    layout = project.get("layout", {})
    layout = layout if isinstance(layout, dict) else {}
    positions = layout.get("positions", {})
    positions = positions if isinstance(positions, dict) else {}
    for index, asset in enumerate(network["assets"]):
        position = positions.get(asset["id"], {})
        position = position if isinstance(position, dict) else {}
        require(abs(numeric(position, "x", index % 5 * 100)) <= 10000 and abs(numeric(position, "z", index // 5 * 100)) <= 10000,
                "invalid_project", "Existing layout coordinate exceeds its bound.")
    camera = layout.get("camera", {})
    camera = camera if isinstance(camera, dict) else {}
    numeric(camera, "yaw", 35)
    require(-85 <= numeric(camera, "pitch", -55) <= -15 and 250 <= numeric(camera, "distance", 720) <= 1600 and
            abs(numeric(camera, "x")) <= 10000 and abs(numeric(camera, "z")) <= 10000,
            "invalid_project", "Existing camera settings exceed supported bounds.")


def _sensor_maps(configuration):
    return {sensor["sensor_id"]: sensor for sensor in configuration["sensors"]}


def _bound_identity(document, context):
    configuration = context.get("configuration")
    if configuration is not None:
        validate_document(configuration, "sensor_configuration", {key: value for key, value in context.items() if key != "configuration"})
        require(document.get("configuration_hash") == sensor_semantic_hash(configuration), "configuration_mismatch", "Configuration hash differs from the supplied configuration.")
        require(document.get("physical_hash") == configuration["physical_hash"], "physical_mismatch", "Network identity differs from the supplied configuration.")
    if "physical_hash" in context:
        require(document.get("physical_hash") == context["physical_hash"], "physical_mismatch", "Network identity differs from the supplied context.")


def _validate_configuration(value, context):
    _unique(value["sensors"], "sensor_id", "Sensor IDs must be unique.")
    require(value["policy"]["source_timeout_s"] >= value["policy"]["heartbeat_period_s"], "invalid_clock", "Source timeout must cover a heartbeat period.")
    if "physical_hash" in context:
        require(value["physical_hash"] == context["physical_hash"], "physical_mismatch", "Configuration network identity differs from context.")
    network = _context_network(context)
    assets = {asset["id"]: asset for asset in network["assets"]} if network is not None else None
    routes = {route["id"]: route for route in network["routes"]} if network is not None else None
    channels, groups = set(), {}
    for sensor in value["sensors"]:
        quantity, attachment = sensor["quantity"], sensor["attachment"]
        require(_utf16_reference(attachment["id"]), "invalid_id", "Attachment exceeds the existing 256 UTF-16-unit reference bound.")
        require(sensor["max_age_s"] >= sensor["expected_period_s"], "invalid_clock", "Maximum age must cover an expected sampling period.")
        if quantity != "custom_numeric":
            require(sensor["unit"] == UNITS[quantity], "invalid_unit", "Sensor unit is not its canonical quantity unit.")
        if "instrument_min" in sensor and "instrument_max" in sensor:
            require(sensor["instrument_min"] < sensor["instrument_max"], "invalid_range", "Instrument minimum must be below maximum.")
        channel = (sensor["source_id"], sensor["channel_id"])
        require(channel not in channels, "duplicate_id", "Source/channel pairs must identify distinct measurements.")
        channels.add(channel)
        require(attachment["kind"] == ("asset" if quantity in ("inventory", "level", "source_flow") else "route")
                if quantity not in ("pressure", "temperature", "custom_numeric") else True,
                "invalid_attachment", "Sensor attachment kind is incompatible with its quantity.")
        if network is not None:
            target = (assets if attachment["kind"] == "asset" else routes).get(attachment["id"])
            require(target is not None, "invalid_attachment", "Sensor attachment does not exist in the supplied network.")
            if quantity in ("inventory", "level", "source_flow"):
                require(target["kind"] == ("source" if quantity == "source_flow" else "tank"), "invalid_attachment", "Asset kind is incompatible with sensor quantity.")
            if quantity == "pump_state":
                require(target.get("has_pump") is True, "invalid_attachment", "Pump feedback requires an actual pumped route.")
        calibration, support = sensor["calibration"], sensor["physical_support"]
        if quantity == "level":
            require(calibration["kind"] == "level_volume_piecewise_linear", "invalid_calibration", "Level needs an explicit level-volume calibration.")
            knots = calibration["knots"]
            require(all(left["level_m"] < right["level_m"] and left["volume_m3"] < right["volume_m3"]
                        for left, right in zip(knots, knots[1:])), "invalid_calibration", "Calibration levels and volumes must be strictly increasing.")
        else:
            require(calibration["kind"] == "identity", "invalid_calibration", "This quantity requires identity calibration in G1.")
        if quantity != "concentration":
            require("pipe_parameter_hash" not in sensor, "invalid_support", "Only concentration refers to physical pipe parameters.")
            require(support["kind"] == "none", "invalid_support", "Spatial pipe support belongs only to concentration sensors.")
        else:
            require(support["kind"] != "none", "invalid_support", "Concentration requires explicit spatial support.")
            if support["kind"] in ("interval_average", "point_unavailable"):
                require(support["start_m"] < support["end_m"] if support["kind"] == "interval_average" else support["start_m"] == support["end_m"],
                        "invalid_support", "Interval support must have positive extent; unsupported point support has one coordinate.")
            if network is not None:
                pipe = _context_pipes(context).get(attachment["id"])
                require(pipe is not None, "invalid_support", "Concentration needs explicitly supplied physical pipe parameters.")
                expected_pipe_hash = context.get("pipe_parameter_hashes", {}).get(attachment["id"])
                require(expected_pipe_hash is not None and sensor["pipe_parameter_hash"] == expected_pipe_hash,
                        "parameter_mismatch", "Concentration parameter identity differs from the registered physical pipe.")
                require(support.get("end_m", 0) <= pipe["length_m"] and support.get("cell_index", 0) < pipe["cells"],
                        "invalid_support", "Concentration support lies outside its physical pipe.")
        groups.setdefault(sensor["group_id"], []).append(sensor)
    for group in groups.values():
        first = group[0]
        require(sum(sensor["role"] == "primary" for sensor in group) == 1, "invalid_redundancy", "Each sensor group needs exactly one primary.")
        require(all((sensor["quantity"], sensor["unit"], sensor["attachment"]) == (first["quantity"], first["unit"], first["attachment"])
                    for sensor in group), "invalid_redundancy", "Redundant group members must observe the same quantity and attachment.")


def _canonical(sensor, value, unit):
    if sensor["quantity"] == "pump_state":
        require(isinstance(value, str) and value in PUMP_STATES and unit == "state", "invalid_value", "Pump readings require typed feedback states.")
        return value
    require(isinstance(value, (int, float)) and not isinstance(value, bool), "invalid_value", "Numeric sensors require finite numeric readings.")
    conversions = {"inventory": {"m3": 1.0, "L": 0.001}, "level": {"m": 1.0, "cm": 0.01},
                   "source_flow": {"m3/h": 1.0, "L/s": 3.6, "L/h": 0.001},
                   "route_flow": {"m3/h": 1.0, "L/s": 3.6, "L/h": 0.001}}
    factors = conversions.get(sensor["quantity"], {sensor["unit"]: 1.0})
    require(unit in factors, "invalid_unit", "Reading unit has no declared G1 conversion.")
    converted = float(value) * factors[unit]
    require(math.isfinite(converted), "nonfinite", "Canonical conversion overflowed.")
    return converted


def _validate_frame(value, context):
    _bound_identity(value, context)
    require(value["type"] != "start" or value["sequence"] == 0, "invalid_sequence", "A start frame must have sequence zero.")
    require(value["type"] == "start" or value["sequence"] > 0, "invalid_sequence", "Subsequent frames need a positive sequence.")
    if value["type"] == "ack":
        require(value["payload"]["effective_event_time_s"] <= value["event_time_s"], "invalid_clock", "Acknowledgement cannot precede its effective event time.")
        command = context.get("command")
        if command:
            validate_document(command, "stream_command")
            require(all(value[key] == command[key] for key in ("stream_id", "epoch_id")) and
                    value["payload"]["request_id"] == command["request_id"] and
                    value["payload"]["operation"] == command["operation"], "request_mismatch", "Acknowledgement does not match the command.")
    if value["type"] != "readings":
        return
    readings = value["payload"]["readings"]
    _unique(readings, "sensor_id", "A batch contains at most one reading per sensor.")
    sensors = _sensor_maps(context["configuration"]) if "configuration" in context else None
    for reading in readings:
        require(reading["acquisition_start_s"] <= reading["acquisition_end_s"] <= reading["event_time_s"] <= value["event_time_s"],
                "invalid_clock", "Acquisition, event and frame times are out of order.")
        if sensors is not None:
            sensor = sensors.get(reading["sensor_id"])
            require(sensor is not None and sensor["enabled"], "invalid_id", "Reading references an unknown or disabled sensor.")
            _canonical(sensor, reading["value"], reading["unit"])


def _mapping_source_available(sensor, sample):
    if not (sensor["enabled"] and sensor["physical_support"]["kind"] != "point_unavailable" and
            sensor["quantity"] not in ("pressure", "temperature", "custom_numeric") and
            sample["receiver_status"] == "good" and sample["reported_quality"] == "good"):
        return False
    if sensor["quantity"] == "level":
        height = _canonical(sensor, sample["original_value"], sample["original_unit"])
        knots = sensor["calibration"]["knots"]
        return knots[0]["level_m"] <= height <= knots[-1]["level_m"]
    return True


def _validate_snapshot(value, context):
    require(all(field in context for field in ("configuration", "network", "physical_hash")),
            "missing_context", "Snapshot semantic validation requires registered configuration, network and physical hash.")
    _bound_identity(value, context)
    require(value["captured_event_time_s"] <= value["expires_event_time_s"], "invalid_clock", "Snapshot expiry precedes capture.")
    _unique(value["samples"], "sensor_id", "Snapshot samples must reference each sensor once.")
    _unique(value["commands"], "control_id", "Command references must be unique.")
    sensors = _sensor_maps(context["configuration"]) if "configuration" in context else None
    samples = {sample["sensor_id"]: sample for sample in value["samples"]}
    coverage = value["coverage"]
    required, missing, conflicts = (set(coverage[key]) for key in ("required_sensor_ids", "missing_sensor_ids", "conflicting_sensor_ids"))
    require(missing <= required and not (missing & set(samples)), "invalid_coverage", "Missing coverage must name required absent sensors.")
    require(required <= set(samples) | missing, "invalid_coverage", "Required sensors need a sample or explicit missing status.")
    if sensors is not None:
        require((required | missing | conflicts | set(samples)) <= set(sensors), "invalid_id", "Snapshot references an unknown sensor.")
    times = []
    for sample in value["samples"]:
        try:
            datetime.fromisoformat(sample["received_utc"].replace("Z", "+00:00"))
        except ValueError as error:
            raise ContractError("invalid_clock", "Receive time is not a valid UTC timestamp.") from error
        require(sample["acquisition_start_s"] <= sample["acquisition_end_s"] <= sample["event_time_s"] <= value["captured_event_time_s"],
                "invalid_clock", "Snapshot acquisition, event and capture times are out of order.")
        if sensors is not None:
            sensor = sensors[sample["sensor_id"]]
            canonical = _canonical(sensor, sample["original_value"], sample["original_unit"])
            require(sample["canonical_unit"] == sensor["unit"], "invalid_unit", "Canonical unit differs from sensor configuration.")
            require(sample["canonical_value"] == canonical if isinstance(canonical, str) else
                    isinstance(sample["canonical_value"], (int, float)) and not isinstance(sample["canonical_value"], bool) and
                    math.isclose(float(sample["canonical_value"]), canonical, rel_tol=1e-12, abs_tol=1e-12),
                    "conversion_mismatch", "Original and canonical values disagree with the declared conversion.")
            if sample["receiver_status"] == "good":
                require(value["captured_event_time_s"] - sample["event_time_s"] <= sensor["max_age_s"], "invalid_freshness", "A stale sample cannot be labeled good.")
        if sample["sensor_id"] in required:
            times.append(sample["event_time_s"])
    if value["availability"] == "available":
        require(not missing and not conflicts, "invalid_coverage", "Available snapshots cannot contain missing/conflicting required coverage.")
        require(all(samples[identity]["receiver_status"] == "good" and samples[identity]["reported_quality"] == "good" for identity in required),
                "invalid_coverage", "Available snapshots need good required observations.")
        if sensors is not None:
            require(all(_mapping_source_available(sensors[identity], samples[identity]) for identity in required),
                    "invalid_coverage", "Unavailable or unsupported required observations cannot yield available coverage.")
        if "configuration" in context and times:
            require(max(times) - min(times) <= context["configuration"]["policy"]["max_skew_s"], "invalid_clock", "Required observations are incoherent in time.")
    bindings = set()
    network = _context_network(context)
    assets = {asset["id"]: asset for asset in network["assets"]} if network is not None else {}
    for state in value["derived_state"]:
        key = state["quantity"], state["attachment_id"]
        require(key not in bindings, "duplicate_id", "Derived state has competing assignments.")
        bindings.add(key)
        require(_utf16_reference(state["attachment_id"]), "invalid_id", "Derived attachment exceeds the native reference bound.")
        require(not (set(state["source_sensor_ids"]) & conflicts), "unsupported_mapping", "Mapped state cannot consume observations with a declared conflict.")
        if sensors is not None:
            require(set(state["source_sensor_ids"]) <= set(samples), "invalid_id", "Derived state references absent samples.")
            for identity in state["source_sensor_ids"]:
                sensor, sample = sensors[identity], samples[identity]
                require(_mapping_source_available(sensor, sample), "unsupported_mapping", "Mapped state needs accepted supported source observations within their declared domain.")
        if state["origin"] in ("measured", "converted"):
            require(len(state["source_sensor_ids"]) == 1, "invalid_origin", "Direct measured/unit-converted state needs exactly one source observation.")
            sensor_id = state["source_sensor_ids"][0]
            sensor, sample = sensors[sensor_id], samples[sensor_id]
            require(sensor["quantity"] == state["quantity"] and sensor["attachment"]["id"] == state["attachment_id"] and
                    sample["canonical_unit"] == state["unit"], "invalid_origin", "Direct state must retain its source quantity, attachment and canonical unit.")
            actual, expected = state["value"], sample["canonical_value"]
            require(actual == expected if isinstance(expected, str) else isinstance(actual, (int, float)) and not isinstance(actual, bool) and
                    math.isclose(float(actual), float(expected), rel_tol=1e-12, abs_tol=1e-12),
                    "invalid_origin", "Direct state must retain its source canonical value.")
        require(state["origin"] != "assumed" or value["assumptions"], "invalid_origin", "Assumed state needs named assumptions.")
        require(state["origin"] != "inferred" or (state["source_sensor_ids"] and value["assumptions"]), "invalid_origin", "Inferred state needs source observations and named assumptions.")
        require(state["quantity"] not in ("pressure", "temperature", "custom_numeric"), "unsupported_mapping", "These channels are observation/alarm-only in G1.")
        require(state["unit"] == UNITS.get(state["quantity"]), "invalid_unit", "Derived state uses a noncanonical unit.")
        if state["quantity"] == "pump_state":
            require(state["value"] in PUMP_STATES, "invalid_value", "Derived pump state is not typed feedback.")
        else:
            require(isinstance(state["value"], (int, float)) and not isinstance(state["value"], bool), "invalid_value", "Derived numeric state is not numeric.")
        if state["quantity"] == "inventory" and value["availability"] == "available" and network is not None:
            target = assets.get(state["attachment_id"])
            require(target is not None and target["kind"] == "tank" and 0 <= state["value"] <= target["capacity_m3"],
                    "unrepresentable_state", "Out-of-model inventory evidence cannot initialize an available native state.")


def _validate_event(value, context):
    _bound_identity(value, context)
    require(value["onset_event_time_s"] <= value["updated_event_time_s"], "invalid_clock", "Event update precedes onset.")
    cleared = value["cleared_event_time_s"]
    require((value["status"] == "active" and cleared is None) or
            (value["status"] == "cleared" and cleared is not None and value["onset_event_time_s"] <= cleared <= value["updated_event_time_s"]),
            "invalid_event", "Physical clearing state/timestamps disagree.")
    if value["acknowledgement"] is not None:
        try:
            datetime.fromisoformat(value["acknowledgement"]["acknowledged_utc"].replace("Z", "+00:00"))
        except ValueError as error:
            raise ContractError("invalid_clock", "Acknowledgement timestamp is invalid.") from error
    if "configuration" in context:
        require(set(value["sensor_ids"]) <= set(_sensor_maps(context["configuration"])), "invalid_id", "Event references an unknown sensor.")
    require(all(_utf16_reference(item) for item in value["asset_ids"]), "invalid_id", "Event asset reference exceeds the native bound.")


def _validate_project(value, context):
    project = value["project"]
    _legacy_project(project)
    require("network" in context and "physical_hash" in context, "missing_context", "Project-v2 validation requires a registered network and its opaque physical hash.")
    require(_json_equal(project["network"], context["network"]), "physical_mismatch", "Embedded network differs from the registered project network.")
    require(value["sensor_configuration"]["physical_hash"] == context["physical_hash"], "physical_mismatch", "Configuration is detached from the registered project identity.")
    embedded_pipes = project.get("pipe_transport", {}).get("routes", {})
    for sensor in value["sensor_configuration"]["sensors"]:
        if sensor["quantity"] == "concentration":
            route_id = sensor["attachment"]["id"]
            require(route_id in embedded_pipes and _json_equal(embedded_pipes[route_id], context.get("pipe_parameters", {}).get(route_id)),
                    "parameter_mismatch", "Embedded concentration pipe differs from the registered parameters.")
    validate_document(value["sensor_configuration"], "sensor_configuration", {**context, "project": project})
    require(set(value["sensor_layout"]) <= set(_sensor_maps(value["sensor_configuration"])), "invalid_id", "Marker layout references an unknown sensor.")


def validate_document(value, kind=None, context=None):
    """Validate structure + semantic rules; context enables network/config binding.

    Without context, attachment existence and supplied-network identity cannot be
    established. Successful validation does not authenticate measurements or workers.
    """
    require(isinstance(value, dict), "invalid_shape", "A contract document must be an object.")
    _finite(value)
    kind = kind or ("project_v2" if value.get("version") == 2 else value.get("kind"))
    _shape(value, kind)
    context = {} if context is None else context
    _validate_context(context, kind)
    {"sensor_configuration": _validate_configuration, "stream_frame": _validate_frame,
     "stream_command": lambda value, context: None, "observed_snapshot": _validate_snapshot,
     "detection_event": _validate_event, "project_v2": _validate_project}[kind](value, context)
    return value


def sensor_semantic_bytes(configuration):
    validate_document(configuration, "sensor_configuration")
    sensors = sorted((copy.deepcopy(item) for item in configuration["sensors"]), key=lambda item: item["sensor_id"])
    for sensor in sensors:
        sensor.pop("label")
    projection = {"telemetry_version": 1, "physical_hash": configuration["physical_hash"],
                  "policy": configuration["policy"], "sensors": sensors}

    def normalize(value):
        if isinstance(value, bool):
            return value
        if isinstance(value, (int, float)):
            number = float(value)
            return {"$f64": struct.pack(">d", 0.0 if number == 0 else number).hex()}
        if isinstance(value, list):
            return [normalize(item) for item in value]
        if isinstance(value, dict):
            return {key: normalize(item) for key, item in value.items()}
        return value
    return json.dumps(normalize(projection), sort_keys=True, ensure_ascii=False,
                      separators=(",", ":"), allow_nan=False).encode("utf-8")


def sensor_semantic_hash(configuration):
    return hashlib.sha256(sensor_semantic_bytes(configuration)).hexdigest()


def frame_disposition(frame, context):
    validate_document(frame, "stream_frame", context)
    clock = context.get("clock")
    if not clock:
        return "accepted"
    if frame["stream_id"] != clock["stream_id"] or frame["epoch_id"] != clock["epoch_id"]:
        return "old_epoch"
    previous = context.get("previous_frame")
    if previous is not None and all(frame[key] == previous.get(key) for key in ("stream_id", "epoch_id", "sequence")):
        return "duplicate" if _json_equal(frame, previous) else "conflict"
    policy = context.get("configuration", {}).get("policy", DEFAULT_POLICY)
    if frame["event_time_s"] > clock["now_event_time_s"] + policy["future_allowance_s"]:
        return "future"
    if frame["event_time_s"] < clock["watermark_event_time_s"] - policy["reorder_window_s"]:
        return "history_only"
    if frame["event_time_s"] < clock["watermark_event_time_s"]:
        return "buffered"
    if clock.get("mode", "synthetic_live") != "synthetic_live":
        return "history_only"
    return "accepted"


def snapshot_disposition(snapshot, context=None):
    context = {} if context is None else context
    require(isinstance(snapshot, dict), "invalid_shape", "Snapshot must be an object.")
    _finite(snapshot)
    _shape(snapshot, "observed_snapshot")
    _validate_context(context, "observed_snapshot")
    if snapshot["mode"] != "synthetic_live":
        return "historical"
    if not all(field in context for field in ("configuration", "network", "physical_hash")):
        return "unavailable"
    validate_document(snapshot, "observed_snapshot", context)
    clock = context.get("clock")
    if not clock:
        return "unavailable"
    if clock and clock.get("mode", "synthetic_live") != "synthetic_live":
        return "historical"
    if clock and (snapshot["stream_id"] != clock["stream_id"] or snapshot["epoch_id"] != clock["epoch_id"]):
        return "historical"
    if snapshot["captured_event_time_s"] > clock["now_event_time_s"] + context["configuration"]["policy"]["future_allowance_s"]:
        return "unavailable"
    if clock and clock["now_event_time_s"] > snapshot["expires_event_time_s"]:
        return "expired"
    return snapshot["availability"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("document", type=Path)
    parser.add_argument("--kind", choices=tuple(SCHEMAS))
    parser.add_argument("--context", type=Path)
    args = parser.parse_args()
    try:
        value = decode_document(args.document.read_bytes())
        context = decode_document(args.context.read_bytes()) if args.context else {}
        validate_document(value, args.kind, context)
        result = {"valid": True, "scope": "Contract consistency only; no runtime execution or control authority."}
        if value.get("kind") == "sensor_configuration":
            result["configuration_hash"] = sensor_semantic_hash(value)
        print(json.dumps(result))
        return 0
    except (ContractError, OSError) as error:
        print(json.dumps({"valid": False, "code": getattr(error, "code", "read_error"), "message": str(error)[:300]}))
        return 1


if __name__ == "__main__":
    sys.exit(main())
