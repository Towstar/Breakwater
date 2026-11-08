"""Offline actual-native commitment comparison, with no labels or model API.

Small self-created scenarios are test infrastructure, not runtime imports. Outputs
separate accepted-control projections from the future-bearing host receipt.
"""
import argparse
from dataclasses import asdict
import hashlib
import json
import math
from pathlib import Path
import sys

from eog_commitments import reconstruct
from eog_synthetic import Failure, SyntheticStream, compile_experiment


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, allow_nan=False, indent=2) + "\n", encoding="utf-8")


def qualify(library, output):
    sys.path.insert(0, str(Path(__file__).with_name("tests")))
    from test_eog_commitments import configured
    from test_eog_synthetic import experiment
    m = configured(); f, g = m.facilities
    config = experiment(m, duration=900, failures=(Failure(f.id, 300.125), Failure(g.id, 500.125)))
    compiled = compile_experiment(m, config)
    output.mkdir(parents=True, exist_ok=False)
    write(output / "model.json", asdict(m))
    checks = []
    with SyntheticStream(library, compiled) as stream:
        def capture(label):
            h = stream.control_history(); causal = stream.view()
            ledger = reconstruct(m, causal, h)
            actual = stream.truth()["trucks"]
            for native, known in zip(actual, ledger.trucks, strict=True):
                assert (m.systems[native["system"]].id, native["truck"], native["busy"], native["available_again_s"]) == (
                    known.system_id, known.truck, known.busy, known.available_again_elapsed_s)
            payload = {"history": asdict(h), "ledger": asdict(ledger), "actual_native_trucks": actual,
                       "causal_observations": [asdict(s) for channel in m.channels() for s in causal.series(*channel.key)]}
            write(output / (label + ".json"), payload)
            checks.append({"case": label, "elapsed_s": h.cutoff_elapsed_s, "controls": len(h.commands),
                           "scheduled_pickups": len(ledger.scheduled_pickups), "busy_trucks": sum(t.busy for t in ledger.trucks),
                           "passed": True})
        capture("initial")
        stream.advance(350.125); assert stream.dispatch(f.id, 0, 1)["accepted"]; capture("first-dispatch")
        old = stream.control_history()
        stream.advance(375.25); assert stream.shut_in(f.wells[0].id)["accepted"]; capture("well-command")
        stream.advance(450); capture("after-first-repair")
        stream.advance(math.nextafter(545.125, 0)); assert not stream.dispatch(g.id, 0, 1)["accepted"]; capture("before-return")
        stream.advance(545.125); assert stream.dispatch(g.id, 0, 1)["accepted"]; capture("sequential-job")
        assert stream.control_history(350.125) == old
        stream.advance(650); capture("after-second-repair")
        stream.advance(740.125); capture("final-return")
        stream.advance(900); capture("completed")
        write(output / "host-only-receipt.json", stream.provenance())
        stream.reset(); capture("reset")
    summary = {"qualification": "offline_actual_native_commitment_comparison", "checks": checks, "passed": all(c["passed"] for c in checks),
               "native_library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
               "limitations": ["Synthetic known controls only; history caller authenticity is not established by these types.",
                   "Ledger output is scheduled resource commitment, not independent inventory/cost validation or operating approval.",
                   "No stateful optimization, public simulation wire, installed/model/UI acceptance or physical qualification."]}
    write(output / "summary.json", summary)
    print(json.dumps(summary, ensure_ascii=True))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-library", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    qualify(args.native_library.resolve(), args.output_dir.resolve())
