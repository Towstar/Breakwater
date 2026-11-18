"""Create a visual editor-limit fixture; not a numerical accuracy benchmark."""
import copy
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
document = json.loads((root / "examples/portfolio/sandbox/healthy.json").read_text(encoding="utf-8"))
assets, routes, positions = [], [], {}
source = next(row for row in document["network"]["assets"] if row["kind"] == "source")
tank = next(row for row in document["network"]["assets"] if row["kind"] == "tank")
route = next(row for row in document["network"]["routes"] if not row["has_pump"])
for index in range(64):
    for offset, template, kind in ((0, tank, "tank"), (1, source, "source")):
        row = copy.deepcopy(template)
        row.update(id=f"{kind}_{index}", label=f"{kind.title()} {index + 1}")
        if kind == "source":
            row.update(inflow_m3h=5, curtailment_m3h=0, curtailment_max_m3h=0)
        else:
            row.update(initial_m3=30, capacity_m3=120)
        assets.append(row)
        point = 2 * index + offset
        positions[row["id"]] = {"x": (point % 16 - 7.5) * 100, "z": (point // 16 - 3.5) * 100}
    for edge in range(4):
        row = copy.deepcopy(route)
        row.update(id=f"route_{index}_{edge}", source_id=f"source_{index}" if edge == 0 else f"tank_{index}",
                   target_id=f"tank_{index}" if edge == 0 else f"tank_{(index + edge) % 64}",
                   capacity_m3h=5 if edge == 0 else 25, setpoint_m3h=5 if edge == 0 else 0,
                   has_pump=False, available=True)
        routes.append(row)
document["network"] = {"assets": assets, "routes": routes}
document["layout"]["positions"] = positions
with Path(sys.argv[1]).open("x", encoding="utf-8", newline="\n") as output:
    output.write(json.dumps(document, indent=2) + "\n")
print("Created the 128-asset/256-route visual fixture. No numerical accuracy claim.")
