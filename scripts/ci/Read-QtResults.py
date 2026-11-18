"""Retain Qt case counts without hiding unexpected dependency skips or failures."""
import argparse
import json
from pathlib import Path
import re

TOTALS = re.compile(r"^Totals:\s*(\d+) passed,\s*(\d+) failed,\s*(\d+) skipped,\s*(\d+) blacklisted", re.MULTILINE)
SKIP = re.compile(r"^SKIP\s*:\s*(.*?)\s*$", re.MULTILINE)
OPTIONAL_CALIBRATION = (
    "pipe_transport-results",
    "PipeTransportTests::actualPhysicsNeMoEstimateRequiresExplicitApply() "
    "Explicit real PhysicsNeMo runtime test not requested.",
)


def summarize(directory, allow_optional_calibration=False):
    suites = []
    problems = []
    optional_count = 0
    for path in sorted(directory.glob("*-results.txt")):
        text = path.read_text(encoding="utf-8-sig")
        totals = TOTALS.findall(text)
        if len(totals) != 1:
            problems.append(f"{path.name}: expected exactly one Qt case summary")
            continue
        passed, failed, skipped, blacklisted = map(int, totals[0])
        reasons = SKIP.findall(text)
        unexpected = []
        for reason in reasons:
            if allow_optional_calibration and (path.stem, reason) == OPTIONAL_CALIBRATION:
                optional_count += 1
            else:
                unexpected.append(reason)
        suites.append(dict(suite=path.stem, passed=passed, failed=failed,
                           skipped=skipped, blacklisted=blacklisted,
                           skip_reasons=reasons, unexpected_skips=unexpected))
        if not passed or failed or blacklisted or unexpected or len(reasons) != skipped:
            problems.append(f"{path.name}: unsuccessful, unaccounted or unexpected Qt cases")
    if len(suites) < 12:
        problems.append("Fewer than 12 required Qt suite reports were retained")
    if optional_count > 1:
        problems.append("The optional calibration case was skipped more than once")
    return dict(passed=not problems, qt_suites=suites, problems=problems,
                optional_calibration_skipped=optional_count)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--allow-optional-calibration", action="store_true")
    args = parser.parse_args()
    report = summarize(args.directory, args.allow_optional_calibration)
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if report["passed"] else 1)
