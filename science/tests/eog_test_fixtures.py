"""Small self-created EOG inputs; no official recordings or answer keys."""
from eog_model import compile_model
from eog_observations import Archive, Sample


def inputs(count=1, prefix="renamed/\u03b1"):
    sid = prefix + "/system"
    facilities, travel = [], {}
    for index in range(count):
        base = prefix + f"/facility:{index}"
        facilities.append({"id": base, "lat": 31.9, "lon": -102.3,
                           "wells": [{"id": base + f"/well:{j}", "baseline_water_bpd": water,
                                      "oil_bpd": oil, "gas_mcfd": gas}
                                     for j, (water, oil, gas) in enumerate(((450, 560, 2000), (700, 170, 600), (650, 390, 1400)))],
                           "tank": {"id": base + "/tank", "capacity_bbl": 800},
                           "pump": {"id": base + "/pump", "max_flow_bpd": 2700, "start_level_pct": 70, "stop_level_pct": 30},
                           "pipeline": {"id": base + "/pipe", "max_bpd": 3000, "max_pressure_psi": 1700}})
        travel[base] = {"response_min": 75, "cycle_min": 230}
    network = {"meta": {"units": {"volume": "bbl", "rate": "bpd", "gas_rate": "mcfd", "pressure": "psi"}, "timezone": "UTC"},
               "defaults": {"tank_limits_pct": {"warning": 85, "shutdown": 92}},
               "systems": [{"id": sid, "center": [31.9, -102.3], "truck_yard": [31.8, -102.2],
                            "disposal_site": {"id": prefix + "/disposal", "lat": 31.9, "lon": -102.3, "pressure_psi": 1050},
                            "facilities": facilities}]}
    ops = {"dataset_id": "self-created-analytical-fixture", "prices": {"oil_per_bbl": 60, "gas_per_mcf": 3},
           "expected_repair_hours": {"pump_failure": 6},
           "emergency_trucking": {"truck_capacity_bbl": 150, "cost_per_trip": 905},
           "systems": {sid: {"emergency_fleet_size": 2, "facilities": travel}}}
    return network, ops


def model(count=1, prefix="renamed/\u03b1"):
    return compile_model(*inputs(count, prefix))


def frame(m, t, *, index=0, status=1, flow=None, received=None, level=50, inflow=None,
          well_states=None, water=None):
    f = m.facilities[index]
    if flow is None:
        flow = f.pump_flow_bpd if status == 1 else 0
    if received is None:
        received = flow
    if well_states is None:
        well_states = (1,) * len(f.wells)
    if water is None:
        water = tuple(w.baseline_water_bpd if s == 1 else 0 for w, s in zip(f.wells, well_states))
    if inflow is None:
        inflow = sum(water)
    values = [(f.id, "inflow_bpd", inflow), (f.tank_id, "level_pct", level),
              (f.pump_id, "status", status), (f.pump_id, "flow_bpd", flow),
              (f.pump_id, "discharge_psi", 1200 if status == 1 else 1050),
              (f.pipeline_id, "receive_flow_bpd", received), (f.pipeline_id, "pressure_psi", 1100 if status == 1 else 1050)]
    for w, state, q in zip(f.wells, well_states, water):
        values.extend(((w.id, "status", state), (w.id, "water_bpd", q)))
    return [Sample(t, asset, metric, value) for asset, metric, value in values]


def view(m, rows, cutoff=None):
    rows = sorted(rows, key=lambda s: s.unix)
    cutoff = rows[-1].unix if cutoff is None else cutoff
    return Archive(m.channels(), rows, capture_cutoff_unix=cutoff).at(cutoff)
