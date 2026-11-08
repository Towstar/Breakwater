"""Run the actual science discovery and retain explicit optional-skip evidence."""
import argparse
import json
from pathlib import Path
import sys
import unittest


OPTIONAL_REASON = "PhysicsNeMo is optional in the A3-only environment"


def summarize(result):
    skipped = [{"test": test.id(), "reason": reason} for test, reason in result.skipped]
    unexpected = [item for item in skipped if
                  ".ActualPhysicsNeMoTests." not in item["test"] or item["reason"] != OPTIONAL_REASON]
    passed_count = result.testsRun - len(result.failures) - len(result.errors) - len(result.skipped) \
                   - len(result.expectedFailures) - len(result.unexpectedSuccesses)
    return {
        "passed": result.wasSuccessful() and not unexpected and passed_count > 0,
        "tests_run": result.testsRun,
        "tests_passed": passed_count,
        "failures": len(result.failures), "errors": len(result.errors),
        "expected_failures": len(result.expectedFailures),
        "unexpected_successes": len(result.unexpectedSuccesses),
        "skipped": skipped, "unexpected_skips": unexpected,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    suite = unittest.defaultTestLoader.discover(str(root / "science" / "tests"))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = summarize(result)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
