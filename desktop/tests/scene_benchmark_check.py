"""Exercise actual D3D11/WARP body picking without a software FPS target."""
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid


def main():
    executable, artifact_root = map(Path, sys.argv[1:3])
    output = artifact_root.resolve() / ("scene-picking-warp-" + uuid.uuid4().hex)
    output.mkdir(parents=True)
    report_path = output / "benchmark.json"
    environment = dict(os.environ, QSG_RHI_BACKEND="d3d11",
                       QSG_RHI_PREFER_SOFTWARE_RENDERER="1", NVIDIA_API_KEY="",
                       BREAKWATER_BENCHMARK_COMPACT="1")
    command = [str(executable.resolve()), "--scene-benchmark", str(report_path)]
    print("Retained scene-picking evidence:", output, flush=True)
    with (output / "process.log").open("wb") as log:
        result = subprocess.run(command, env=environment, stdout=log,
                                stderr=subprocess.STDOUT, timeout=90)
    report = json.loads(report_path.read_text(encoding="utf-8"))
    failures = []

    def require(condition, message):
        if not condition:
            failures.append(message)

    require(result.returncode == 0, f"benchmark exited {result.returncode}")
    require(report["benchmark_version"] == 2, "unexpected benchmark protocol")
    require(report["status"] == "measured", "renderer did not complete settled measurement")
    require(report["renderer"]["status"] == "identified", "renderer was not identified")
    require(report["renderer"].get("software") is True, "actual renderer is not WARP")
    require(report["software_renderer_requested"] is True, "software rendering was not requested")
    require(report["compact_fixture_requested"] is True and
            report["client_width"] == 1008 and report["client_height"] == 672,
            "hosted-runner compact client area was not reproduced")
    require(report["fixture_fitted"] is True, "benchmark did not fit the fixture to the viewport")
    require(report["assets"] == 6, "canonical fixture no longer contains six assets")
    require(report["numerical_identity_unchanged"] is True, "view-only sweep changed numerical identity")
    require(report["post_stop_rendered_frames"] >= 2, "picking ran before transforms settled")
    require(report["measurement_window_ms"] >= 10000, "sample window was shorter than ten seconds")
    require(report["frames"] > 0, "no rendered frames were measured")
    require(report["screenshot_written"] is True and
            report_path.with_suffix(".json.png").stat().st_size > 0, "screenshot was not retained")
    picks = report["actual_body_picks"]
    require(picks["projected"] == report["assets"], "some assets were outside the viewport")
    require(picks["pickable"] == report["assets"], "some visible bodies could not be picked")
    require(picks["failed_asset_ids"] == [], "failed assets: " + str(picks["failed_asset_ids"]))
    require(picks["labels"] == report["assets"] and picks["label_overlaps"] == 0,
            "labels are missing or overlap")
    require(picks["culled_label_ids"] == [], "culled labels: " + str(picks["culled_label_ids"]))
    probes = {probe["asset_id"]: probe for probe in picks["body_probes"]}
    require(len(probes) == report["assets"], "missing per-asset probe diagnostics")
    require(probes.get("treatment", {}).get("body_models", 0) >= 2,
            "treatment's separated bodies were not discovered")
    for asset_id, probe in probes.items():
        require(probe["pickable"] and any(attempt["hit"] == "asset:" + asset_id
                                          for attempt in probe["attempts"]),
                "no actual nearest-object hit for " + asset_id)
    # Software-renderer FPS remains diagnostic. Functional picking, retained
    # evidence and numerical identity must pass even when hardware targets do not.
    if failures:
        raise AssertionError("\n".join(failures) + "\nEvidence: " + str(output))
    print(f"PASS: {picks['pickable']}/{report['assets']} actual body picks; "
          f"WARP {report['fps']:.2f} FPS (diagnostic only)")


if __name__ == "__main__":
    main()
