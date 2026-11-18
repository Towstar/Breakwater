"""Offline qualification only; no production worker imports this test driver."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import platform
import sys

from native_eog_committed import NativeCommittedEvaluator
from eog_committed_validation import validate

sys.path.insert(0, str(Path(__file__).resolve().parent / "tests"))
from test_eog_committed_response import case, generated, job


def encoded(value):
    return json.dumps(value, sort_keys=True, allow_nan=False, ensure_ascii=False,
                      separators=(",", ":")).encode("utf-8")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-library", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=False)
    evaluator = NativeCommittedEvaluator(args.native_library)
    counts = Counter()
    rows = []
    for index, (snapshot, plan, commitments) in enumerate(generated()):
        actual = evaluator.evaluate(snapshot, plan, commitments)
        checked = validate(snapshot, plan, commitments, actual)
        counts[actual["status"]] += 1
        rows.append(encoded({"case": index, "bindings": actual["bindings"],
            "status": actual["status"], "cost": actual["cost"],
            "native_sha256": sha(encoded(actual)),
            "reference_sha256": sha(encoded({"record": checked.record, "audit": checked.audit}))}))
    known = []
    for name in ("pending-only", "pending-plus-new", "prior-well-stop"):
        snapshot, plan, commitments = case()
        if name == "pending-plus-new": job(snapshot, plan)
        if name == "prior-well-stop": commitments["stopped_wells"] = [0]
        actual = evaluator.evaluate(snapshot, plan, commitments)
        checked = validate(snapshot, plan, commitments, actual)
        known.append({"name": name, "snapshot": snapshot, "plan": plan, "commitments": commitments,
                      "native": actual, "reference": {"record": checked.record, "audit": checked.audit}})
    (args.output_dir / "generated.ndjson").write_bytes(b"\n".join(rows) + b"\n")
    (args.output_dir / "known.json").write_bytes(encoded(known) + b"\n")
    summary = {"generated_cases": len(rows), "seed": 2026110811, "statuses": dict(counts),
               "known_cases": len(known), "method": evaluator.method,
               "validator": checked.record["method"], "all_comparisons_passed": True}
    (args.output_dir / "summary.json").write_bytes(encoded(summary) + b"\n")
    execution = {"python": platform.python_version(),
                 "library_sha256": sha(args.native_library.read_bytes()), "payloads": {}}
    for name in ("generated.ndjson", "known.json", "summary.json"):
        execution["payloads"][name] = sha((args.output_dir / name).read_bytes())
    (args.output_dir / "execution.json").write_bytes(encoded(execution) + b"\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__": main()
