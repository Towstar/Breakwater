"""Offline imported-model/causal synthetic qualification; never loads answer keys.

Generated dataset files are separate, explicitly synthetic outputs. The complete
scenario receipt remains outside their three-file folder. Only causal View enters
detection. Full CSVs stay in the requested new output folder; compact hashes,
selected original samples and actual results can be retained for review.
"""
from __future__ import annotations

import argparse
import csv
from dataclasses import asdict, replace
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import sys
import time

from eog_detection import detect
from eog_inputs import load_inputs
from eog_model import compile_model
from eog_synthetic import METHOD, SyntheticStream, compile_experiment, seeded_experiment


def write(path, value):
    path.write_bytes(json.dumps(value, ensure_ascii=False, allow_nan=False, indent=2).encode() + b"\n")


def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024): sha.update(block)
    return sha.hexdigest()


def case(model, network, ops, root, library, start_unix, duration_s, seed):
    begin = time.perf_counter()
    root.mkdir()
    config = seeded_experiment(model, start_unix=start_unix, duration_s=duration_s, seed=seed)
    compiled = compile_experiment(model, config)
    with SyntheticStream(library, compiled) as stream:
        stream.advance(duration_s)
        observed = stream.view(); truth = stream.truth(); detection = detect(model, observed)
        no_future = replace(config, failures=(), rate_changes=())
        with SyntheticStream(library, compile_experiment(model, no_future)) as alternative:
            alternative.advance(300)
            prefix = stream.view(start_unix + 300); alternate_prefix = alternative.view()
            equal_prefix = prefix.evidence_fingerprint() == alternate_prefix.evidence_fingerprint() and detect(model, prefix) == detect(model, alternate_prefix)
        # Serialize only the observation API's causal rows, never simulator truth.
        rows = sorted((sample for channel in model.channels() for sample in observed.series(*channel.key)),
                      key=lambda s: (s.unix, s.asset_id, s.metric))
        dataset = root / "synthetic-dataset"; dataset.mkdir()
        write(dataset / "network.json", network)
        synthetic_ops = dict(ops); synthetic_ops["dataset_id"] = "synthetic-" + compiled.fingerprint[:20]
        write(dataset / "ops.json", synthetic_ops)
        with (dataset / "telemetry.csv").open("w", encoding="utf-8", newline="") as handle:
            writer = csv.writer(handle, lineterminator="\n")
            writer.writerow(("timestamp", "ts_unix", "asset_id", "metric", "value"))
            for sample in rows:
                stamp = datetime.fromtimestamp(sample.unix, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
                writer.writerow((stamp, sample.unix, sample.asset_id, sample.metric, sample.value))
        # Deliberate forbidden-file sentinel: load_inputs consumes only fixed names.
        (dataset / "answer_key.json").write_text("FORBIDDEN: never read this sentinel", encoding="utf-8")
        roundtrip = load_inputs(dataset)
        same_model = model.fingerprint() == roundtrip.model.fingerprint()
        same_observations = observed.evidence_fingerprint() == roundtrip.observations.at(observed.cutoff_unix).evidence_fingerprint()
        same_detection = detection == detect(roundtrip.model, roundtrip.observations.at(observed.cutoff_unix))
        write(root / "host-only-provenance.json", stream.provenance())
        write(root / "truth-final.json", truth)
        write(root / "detection.json", asdict(detection))
        focus = {start_unix, observed.cutoff_unix}
        if config.failures:
            first = min(f.start_s for f in config.failures)
            focus.update((start_unix + math.floor(first / 300) * 300, start_unix + math.ceil(first / 300) * 300))
        write(root / "selected-original-observations.json", [asdict(r) for r in rows if r.unix in focus])
        events = [e for e in detection.events if e.category == "pump_failure"]
        matched, used = [], set()
        for incident in sorted(config.failures, key=lambda f: (f.facility_id, f.start_s)):
            candidates = [(i, e) for i, e in enumerate(events) if i not in used and e.facility_id == incident.facility_id and
                          start_unix + incident.start_s <= e.onset_unix < start_unix + incident.start_s + model.repair_hours * 3600]
            if not candidates:
                matched.append({"facility_id": incident.facility_id, "truth_onset_elapsed_s": incident.start_s, "matched": False}); continue
            index, event = min(candidates, key=lambda pair: pair[1].onset_unix); used.add(index)
            delay = event.detected_unix - (start_unix + incident.start_s)
            matched.append({"facility_id": incident.facility_id, "truth_onset_elapsed_s": incident.start_s, "matched": True,
                            "observed_onset_unix": event.onset_unix, "detected_unix": event.detected_unix, "delay_s": delay})
        mass = max(abs(f["balance_residual_bbl"]) for f in truth["facilities"])
        false_inspections = sum(e.category != "pump_failure" for e in detection.events)
        report = {"scenario_fingerprint": compiled.fingerprint, "model_fingerprint": model.fingerprint(),
            "seed": seed, "duration_s": duration_s, "synthetic_start_unix": start_unix,
            "actual_first_sample_unix": roundtrip.observations.first_unix, "actual_last_sample_unix": roundtrip.observations.last_unix,
            "systems": len(model.systems), "facilities": len(model.facilities), "channels": len(model.channels()), "samples": len(rows),
            "native_status": truth["status"], "native_total_cost": truth["cost"], "maximum_abs_mass_residual_bbl": mass,
            "model_parameters_roundtrip_exactly": same_model, "all_observations_roundtrip_exactly": same_observations,
            "detection_roundtrip_exactly": same_detection, "future_truth_change_preserves_prefix_and_detection": equal_prefix,
            "pump_failure_checks": matched, "unmatched_pump_events": len(events) - len(used), "false_inspection_events": false_inspections,
            "generated_file_hashes": dict(roundtrip.file_hashes), "wall_s": time.perf_counter() - begin,
            "limitations": ["Synthetic initialization, forcing and bounded noise; not a recreation of official observations or a real future.",
                "No pipeline-leak/pump-degradation forcing is generated; this does not qualify their timeliness.",
                "Detector sees integer acquisition times, not hidden fractional true onsets; onset/repair forecasts retain that uncertainty.",
                "No evolving planner integration or application controls/installation are executed."]}
        report["passed"] = bool(truth["status"] == "completed" and mass <= 1e-6 and same_model and same_observations and same_detection and equal_prefix and
            all(c["matched"] and 0 <= c["delay_s"] <= 300 for c in matched) and len(events) == len(matched) and false_inspections == 0)
        write(root / "summary.json", report)
        return report


def qualify(dataset, output, library, duration_s, seed):
    source = load_inputs(dataset)
    network = json.loads((dataset / "network.json").read_bytes().decode("utf-8-sig"))
    ops = json.loads((dataset / "ops.json").read_bytes().decode("utf-8-sig"))
    output.mkdir(parents=True, exist_ok=False)
    official = case(source.model, network, ops, output / "imported-official-model", library, source.observations.last_unix, duration_s, seed)
    # Offline fixture construction only; never imported by production code.
    sys.path.insert(0, str(Path(__file__).with_name("tests")))
    from eog_test_fixtures import inputs
    unfamiliar_network, unfamiliar_ops = inputs(3, prefix="unfamiliar/synthetic/\u03b2/71")
    unfamiliar_network["defaults"]["tank_limits_pct"] = {"warning": 83, "shutdown": 91.25}
    for f in unfamiliar_network["systems"][0]["facilities"]:
        f["tank"]["capacity_bbl"] *= 1.37
        f["pump"]["start_level_pct"] = 65; f["pump"]["stop_level_pct"] = 25
        for w in f["wells"]:
            w["baseline_water_bpd"] *= 0.73; w["oil_bpd"] *= 1.13; w["gas_mcfd"] *= 0.84
    unfamiliar_ops["prices"] = {"oil_per_bbl": 71.25, "gas_per_mcf": 2.45}
    unfamiliar_ops["systems"][unfamiliar_network["systems"][0]["id"]]["emergency_fleet_size"] = 1
    unfamiliar_ops["expected_repair_hours"]["pump_failure"] = 4.25
    unfamiliar = compile_model(unfamiliar_network, unfamiliar_ops)
    generated = case(unfamiliar, unfamiliar_network, unfamiliar_ops, output / "unfamiliar-altered-model", library,
                     source.observations.last_unix, duration_s, seed + 1)
    unchanged = all(digest(dataset / name) == sha for name, sha in source.file_hashes)
    report = {"qualification": METHOD, "input_raw_set_identity": source.raw_input_set_identity, "input_file_hashes": dict(source.file_hashes),
        "source_inputs_unchanged": unchanged, "native_library_sha256": digest(library), "cases": [official, generated],
        "limitations": ["Known generated forcing is used only by this offline comparison; detector receives no labels/seed/future receipt.",
            "The original official dataset is used for model import only; it is not relabeled as simulated reproduction.",
            "The unrelated altered network is synthetic, not the unpublished judging dataset."]}
    report["passed"] = bool(unchanged and all(c["passed"] for c in report["cases"]))
    write(output / "summary.json", report)
    print(json.dumps(report, ensure_ascii=True, indent=2))
    return report["passed"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset-folder", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--native-library", required=True, type=Path)
    parser.add_argument("--duration-s", type=int, default=86400)
    parser.add_argument("--seed", type=int, default=941)
    args = parser.parse_args()
    return 0 if qualify(args.dataset_folder.resolve(), args.output_dir.resolve(), args.native_library.resolve(), args.duration_s, args.seed) else 1


if __name__ == "__main__": raise SystemExit(main())
