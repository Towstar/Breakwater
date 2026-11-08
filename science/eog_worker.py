"""One-shot protected-capture EOG worker, separate from storage worker v1.

Startup paths are trusted application configuration. Requests cannot select code
or schemas. Only the three fixed captured input names are opened. The accepted
contract is pinned by normalized file hashes, permitting LF/CRLF checkouts only.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

from eog_detection import detect
from eog_inputs import InputError, load_inputs
from eog_model import ModelError
from eog_observations import ObservationError
from eog_response import ResponseUnavailable, compile_context, evaluate
from native_eog import NativeError, NativeEvaluator

METHOD = "eog-worker-0.1.1"
ADOPTED_CONTRACT = "72cadd1de7eed6666a60faff1d296e29c8f026ee"
CONTRACT_HASHES = {
    "validate.py": "626deb9b5de0a722f3583adf8f1a28753880bb9d70cf75b78771dda1c7b2faec",
    "common-v1.schema.json": "e4bc08c5ce5e6b608a64d94417740a74dc5e343df6e4349478697eb4f21cd6e7",
    "analysis-request-v1.schema.json": "c9cf0408d5e6540900f55be2f27c91a90c48aff66316d8e76d89332b0b630eb2",
    "analysis-response-v1.schema.json": "44c27baf998e8aced87befe3e80613fc0b9bb44c61abf1c075cb41b65ac76d20",
    "capture-manifest-v1.schema.json": "77889d1e39ae260e8e58b7e116f21bc19d2f3ca4c281c433c8f97550d240aeae",
}
BINDINGS = ("request_id", "operation", "method_version", "dataset_identity", "capture_id", "cutoff_unix")
MAX_REQUEST = 1024 * 1024
MAX_RESPONSE = 4 * 1024 * 1024


def contract_module(root):
    folder = Path(root)
    if not folder.is_absolute():
        raise RuntimeError("Contract startup path must be absolute.")
    for name, digest in CONTRACT_HASHES.items():
        if hashlib.sha256((folder / name).read_bytes().replace(b"\r\n", b"\n")).hexdigest() != digest:
            raise RuntimeError("Contract files differ from the exact adopted revision.")
    spec = importlib.util.spec_from_file_location("_breakwater_adopted_eog_contract", folder / "validate.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _identity(value):
    return hashlib.sha256(json.dumps(value, ensure_ascii=False, sort_keys=True, allow_nan=False,
                                    separators=(",", ":")).encode("utf-8")).hexdigest()


def _envelope(request, status="completed", result=None, code=None, message=None):
    return {"eog_version": 1, "kind": "analysis_response", **{key: request[key] for key in BINDINGS},
            "status": status, "result": result,
            "diagnostics": [] if code is None else [{"code": code, "message": message}]}


def _detection_result(model, view, selected):
    known = {system.id for system in model.systems}
    if set(selected) - known:
        raise InputError("unknown_system", "Requested system is not present in captured inputs.")
    chosen = set(selected) if selected else known
    detection = detect(model, view)
    events = [e for e in detection.events if e.system_id in chosen]
    channels = {channel.key: channel for channel in model.channels()}
    samples = {}
    def evidence(sample):
        eid = "sample-" + _identity((sample.unix, sample.asset_id, sample.metric, sample.value))
        if eid not in samples:
            channel = channels[sample.key]
            quality = "stale" if view.cutoff_unix - sample.unix > 300 else "good"
            if channel.status_codes is not None and sample.value not in channel.status_codes:
                quality = "unsupported"
            samples[eid] = {"evidence_id": eid, "asset_id": sample.asset_id, "metric": sample.metric,
                            "timestamp_unix": sample.unix, "value": sample.value, "unit": channel.unit, "quality": quality}
        return eid
    # Healthy detection also retains the samples used for its current coverage.
    for f in model.facilities:
        if f.system_id not in chosen:
            continue
        assets = {f.id, f.tank_id, f.pump_id, f.pipeline_id, *(w.id for w in f.wells)}
        for key in channels:
            if key[0] in assets:
                reading = view.latest(*key)
                if reading.sample is not None:
                    evidence(reading.sample)
    output_events, assumptions = [], []
    limitations = ["Method " + detection.method + "; rules " + detection.rules_fingerprint + ".",
                   "Inspection timing/recovery is experimental and remains unqualified on retained official causal cuts.",
                   "No calibrated confidence intervals or fault probabilities are supplied."]
    for event in events:
        assumptions.extend(event.assumptions)
        event_limitations = list(event.limitations)
        if event.countdown_state == "predicted_reached_since_sample":
            event_limitations.append("Zero countdown is a held-rate predicted crossing since the last sample, not an observed automatic trip.")
        output_events.append({"event_id": event.id, "system_id": event.system_id, "facility_id": event.facility_id,
            "asset_id": event.asset_id, "category": event.category, "state": event.state, "onset_unix": event.onset_unix,
            "onset_bounds": None if event.onset_bounds is None else
                {"earliest_unix": event.onset_bounds[0], "latest_unix": event.onset_bounds[1]},
            "updated_unix": event.updated_unix, "end_unix": event.end_unix,
            "evidence_ids": list(dict.fromkeys(evidence(sample) for sample in event.evidence)),
            "countdown_hours": event.countdown_hours if event.coverage == "available" and event.state == "ongoing" else None,
            "lost_revenue_per_hour": event.hourly_revenue_exposure, "coverage": event.coverage,
            "threshold_version": detection.method + ":" + detection.rules_fingerprint[:16],
            "recommended_action": "plan_required" if event.recommended_action == "compare_responses" else event.recommended_action,
            "limitations": event_limitations})
    selected_facilities = {f.id for f in model.facilities if f.system_id in chosen}
    coverage = [c.state for c in detection.coverage if c.facility_id in selected_facilities]
    overall = "available" if all(c == "available" for c in coverage) else "partial" if any(c == "available" for c in coverage) else "unavailable"
    return {"events": output_events, "evidence": list(samples.values()), "coverage": overall,
            "assumptions": list(dict.fromkeys(assumptions)), "limitations": limitations}


def handle(request, contract, native_library=None):
    """Returns a fully request-bound response. Request validation precedes file I/O."""
    contract.validate_document(request, "analysis_request")
    try:
        manifest = request["capture_manifest"]
        contract.verify_capture(request["capture_directory"], manifest)
        loaded = load_inputs(request["capture_directory"], capture_cutoff_unix=request["cutoff_unix"])
        expected = {name: item["sha256"] for name, item in manifest["captured_files"].items()}
        if dict(loaded.file_hashes) != expected:
            raise InputError("capture_changed", "Loaded bytes differ from the verified capture manifest.")
        view = loaded.observations.at(request["cutoff_unix"])
        if request["operation"] == "detect":
            result = _detection_result(loaded.model, view, request["arguments"]["system_ids"])
        elif request["operation"] == "evaluate_plan":
            if native_library is None:
                raise ResponseUnavailable("native_unavailable", "The native EOG response library was not configured.")
            context = compile_context(loaded.model, view)
            result = {"evaluation": evaluate(context, request["arguments"]["plan"], NativeEvaluator(native_library))}
        else:
            raise ResponseUnavailable("operation_unavailable", "Joint search and independent validation are not implemented by this worker revision.")
        result["analysis_id"] = "analysis-" + _identity(({key: request[key] for key in BINDINGS}, METHOD, loaded.model.fingerprint()))
        response = _envelope(request, result=result)
    except (ResponseUnavailable, NativeError) as error:
        response = _envelope(request, "unavailable", code=error.code, message=str(error))
    except (InputError, ModelError, ObservationError, contract.ContractError) as error:
        response = _envelope(request, "invalid_input", code=error.code, message="Captured input or requested asset failed verification.")
    except Exception:
        # No raw request, path, telemetry or exception details in the process log.
        response = _envelope(request, "failed", code="provider_failed", message="The numerical provider could not complete the request.")
    try:
        contract.validate_document(response, "analysis_response", {"request": request})
        if len(encode_response(response)) > MAX_RESPONSE:
            raise ValueError("response size")
    except Exception:
        response = _envelope(request, "unavailable", code="complete_result_unavailable",
                             message="A complete contract-valid result could not be represented within the response boundary.")
        contract.validate_document(response, "analysis_response", {"request": request})
    return response


def encode_response(response):
    return json.dumps(response, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8") + b"\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contracts-root", type=Path, default=Path(__file__).resolve().parents[1] / "contracts" / "eog")
    parser.add_argument("--native-library", type=Path)
    args = parser.parse_args(argv)
    try:
        contract = contract_module(args.contracts_root)
        line = sys.stdin.buffer.readline(MAX_REQUEST + 1)
        if len(line) > MAX_REQUEST or not line.endswith(b"\n"):
            raise ValueError("invalid request framing")
        request = contract.decode_document(line)
        contract.validate_document(request, "analysis_request")
    except Exception:
        sys.stderr.write("EOG startup, framing or request validation failed.\n")
        return 2
    response = handle(request, contract, args.native_library)
    sys.stdout.buffer.write(encode_response(response))
    sys.stdout.buffer.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
