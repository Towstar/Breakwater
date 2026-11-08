"""Explicit OFFLINE label evaluator: never imported by detector/provider modules.

Consumes an already-produced detector report and a separately supplied answer key.
It does not invoke detection, alter rules, emit plans, or expose reference actions.
"""
from __future__ import annotations

import argparse
from collections import Counter, deque
import hashlib
import json
from pathlib import Path

CATEGORIES = ("pipeline_leak", "pump_degradation", "pump_failure")


def _integer(value):
    if type(value) is not int or not 0 <= value <= 253402300799:
        raise ValueError("Evaluation timestamps must be exact nonnegative Unix integers.")
    return value


def evaluate(report, labels):
    """Maximum-cardinality exact-ID/category temporal matching, deterministic ties.

    An event must actually be emitted by the truth endpoint to count as detected.
    Neighbors are ordered by onset distance then reference ID. The algorithm does
    not claim to minimize a global onset-error objective among all maximum matches.
    """
    if type(report) is not dict or report.get("status") != "completed" or type(labels) is not list:
        raise ValueError("Need a completed offline detection report and an explicit label array.")
    cutoff = _integer(report["actual_cutoff_unix"])
    if report["detection"]["cutoff_unix"] != cutoff:
        raise ValueError("Detector and report cutoffs differ.")
    truth = []
    ids = set()
    for label in labels:
        identifier = label["id"]
        if type(identifier) is not str or identifier in ids or label["category"] not in CATEGORIES:
            raise ValueError("Invalid/duplicate reference identity or category.")
        ids.add(identifier)
        onset = _integer(label["onset_unix"])
        end = label["end_unix"]
        if end is not None and _integer(end) < onset:
            raise ValueError("Reference endpoint precedes onset.")
        if onset <= cutoff:
            truth.append({"id": identifier, "system": label["system"], "facility": label["facility"],
                          "asset": label["asset"], "category": label["category"], "onset": onset,
                          "end": end if end is not None and end <= cutoff else None})
    predictions = report["detection"]["events"]
    if len(predictions) > 4096 or len(truth) > 4096:
        raise ValueError("Offline matching exceeds its declared 4096-event bound.")
    predicted_ids = set()
    for event in predictions:
        if event["id"] in predicted_ids or event["category"] not in CATEGORIES:
            raise ValueError("Invalid/duplicate prediction identity/category.")
        predicted_ids.add(event["id"])
        first, emitted = _integer(event["first_evidence_unix"]), _integer(event["detected_unix"])
        if not first <= emitted <= cutoff:
            raise ValueError("Prediction evidence/emission exceeds cutoff or is reversed.")
        if event["onset_unix"] is not None and _integer(event["onset_unix"]) > emitted:
            raise ValueError("Estimated onset is after emission.")
        end = event["end_unix"]
        if event["state"] not in ("ongoing", "resolved") or ((end is None) != (event["state"] == "ongoing")):
            raise ValueError("Prediction lifecycle is inconsistent.")
        if end is not None and not first <= _integer(end) <= cutoff:
            raise ValueError("Prediction endpoint lies outside the causal interval.")
    neighbors = []
    for event in predictions:
        start = event["onset_unix"] if event["onset_unix"] is not None else event["first_evidence_unix"]
        candidates = [index for index, label in enumerate(truth)
                      if (label["system"], label["facility"], label["asset"], label["category"]) ==
                         (event["system_id"], event["facility_id"], event["asset_id"], event["category"])
                      and event["detected_unix"] <= (label["end"] if label["end"] is not None else cutoff)
                      and start <= (label["end"] if label["end"] is not None else cutoff)
                      and (event["end_unix"] if event["end_unix"] is not None else cutoff) >= label["onset"]]
        neighbors.append(sorted(candidates, key=lambda i: (abs(start - truth[i]["onset"]), truth[i]["id"])))
    assigned, assigned_prediction = {}, {}
    for root in range(len(predictions)):
        queue, seen_prediction, parents, terminal = deque([root]), {root}, {}, None
        while queue and terminal is None:
            predicted = queue.popleft()
            for reference in neighbors[predicted]:
                if reference in parents:
                    continue
                parents[reference] = predicted
                if reference not in assigned:
                    terminal = reference
                    break
                successor = assigned[reference]
                if successor not in seen_prediction:
                    seen_prediction.add(successor)
                    queue.append(successor)
        while terminal is not None:
            predicted = parents[terminal]
            old_reference = assigned_prediction.get(predicted)
            assigned[terminal] = predicted
            assigned_prediction[predicted] = terminal
            terminal = old_reference
    matched_predictions = set(assigned.values())
    pairs, confusion = [], Counter()
    for reference, predicted in sorted(assigned.items()):
        label, event = truth[reference], predictions[predicted]
        actual_state = "ongoing" if label["end"] is None else "resolved"
        confusion[(actual_state, event["state"])] += 1
        pairs.append({"reference_id": label["id"], "event_id": event["id"], "category": label["category"],
                      "onset_error_seconds": None if event["onset_unix"] is None else event["onset_unix"] - label["onset"],
                      "detection_delay_seconds": event["detected_unix"] - label["onset"],
                      "resolution_error_seconds": None if label["end"] is None or event["end_unix"] is None else event["end_unix"] - label["end"]})
    by_category = {}
    for category in CATEGORIES:
        tp = sum(truth[index]["category"] == category for index in assigned)
        count_truth = sum(label["category"] == category for label in truth)
        count_predicted = sum(event["category"] == category for event in predictions)
        by_category[category] = {"truth": count_truth, "predicted": count_predicted, "matched": tp,
                                 "false_positive": count_predicted - tp, "missed": count_truth - tp,
                                 "precision": None if not count_predicted else tp / count_predicted,
                                 "recall": None if not count_truth else tp / count_truth}
    false_positive = [event["id"] for index, event in enumerate(predictions) if index not in matched_predictions]
    missed = [label["id"] for index, label in enumerate(truth) if index not in assigned]
    # Predeclared presence gate: complete exact-ID/category coverage, no unmatched
    # alarms, and correct observed lifecycle. Timing is separately measured.
    early = [pair["event_id"] for pair in pairs if pair["detection_delay_seconds"] < 0]
    passed = not false_positive and not missed and not early and all(a == b for a, b in confusion)
    return {"evaluation_version": 1, "mode": "offline_label_evaluation_only", "cutoff_unix": cutoff,
            "gate": "all labeled events matched, no unmatched/early alarms, matched lifecycle correct",
            "gate_passed": passed, "by_category": by_category, "pairs": pairs,
            "false_positive_event_ids": false_positive, "missed_reference_ids": missed,
            "early_matched_alarm_ids": early,
            "lifecycle_confusion": [{"truth": a, "predicted": b, "count": n} for (a, b), n in sorted(confusion.items())],
            "limitations": ["Practice labels only; correlated cuts are not independent held-out qualification.",
                            "Matching is exact ID/category plus causal interval overlap, not physical onset certification.",
                            "No countdown/cost/feasibility/optimality or installed application gate is established.",
                            "This evaluator never invokes production detection or passes labels into it."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--detection-report", required=True, type=Path)
    parser.add_argument("--offline-labels", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    report_raw, labels_raw = args.detection_report.read_bytes(), args.offline_labels.read_bytes()
    if len(report_raw) > 4 * 1024 * 1024 or len(labels_raw) > 4 * 1024 * 1024:
        raise ValueError("Offline evaluator input exceeds 4 MiB.")
    result = evaluate(json.loads(report_raw), json.loads(labels_raw))
    result["input_sha256"] = {"detection_report": hashlib.sha256(report_raw).hexdigest(),
                              "offline_labels": hashlib.sha256(labels_raw).hexdigest()}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"gate_passed": result["gate_passed"], "by_category": result["by_category"]}))
    return 0 if result["gate_passed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
