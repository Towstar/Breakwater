"""Independent rejection probes for the source-allocation verification harness."""
import copy
import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import verify_source_feeds as v


def native_case():
    network = v.expected_network("balanced")
    # Match the exporter representation, not an array supplied by its executable.
    for a in network["assets"]:
        for key in ("capacity_m3", "initial_m3", "inflow_m3h", "curtailment_m3h", "curtailment_max_m3h", "cost_per_m3"):
            a.setdefault(key, 0.)
    fields = [[80.+8.*t for t in v.TIMES], [30.+8.*t for t in v.TIMES]]
    result = dict(tank_ids=["tank_0", "tank_1"], time_h=list(v.TIMES), volumes_m3=fields, cost=72., feasible=True, mass_residual_m3=0.)
    return dict(id="balanced", horizon_h=3., step_h=.025, network=network,
                solvers={method: copy.deepcopy(result) for method in ("dense", "sparse", "affine")})


class VerifierTests(unittest.TestCase):
    def test_independent_fixture_is_accepted(self):
        v.validate_case(native_case())

    def test_corrupted_field_cost_and_ledger_are_rejected(self):
        for field, value in (("cost", 73.), ("mass_residual_m3", float("nan")), ("mass_residual_m3", -1.), ("mass_residual_m3", 1.)):
            with self.subTest(field=field, value=value):
                c = native_case(); c["solvers"]["dense"][field] = value
                with self.assertRaises(AssertionError): v.validate_case(c)
        c = native_case(); c["solvers"]["sparse"]["volumes_m3"][1][-1] += 1.
        with self.assertRaises(AssertionError): v.validate_case(c)

    def test_shifted_missing_times_order_and_identity_are_rejected(self):
        for change in ("shift", "missing", "order", "identity", "inputs"):
            with self.subTest(change=change):
                c = native_case(); r = c["solvers"]["affine"]
                if change == "shift": r["time_h"][-2] += 1e-15
                elif change == "missing": r["time_h"].pop()
                elif change == "order": r["tank_ids"].reverse()
                elif change == "identity": c["id"] = "forged"
                else: c["network"]["routes"][0]["source_share_bps"] = 4999
                with self.assertRaises(AssertionError): v.validate_case(c)

    def test_wire_identity_indices_provenance_and_fields_are_independently_checked(self):
        payload = dict(identity={"snapshot_id": "fixed"}, network=v.expected_network("balanced"))
        wire = dict(method="storage_rollout", method_version=2, identity=payload["identity"], time_h=list(v.TIMES), time_indices=list(range(121)),
                    tank_ids=["tank_0", "tank_1"], volumes_m3=[[0.]*121, [0.]*121], cost=72.,
                    provenance=dict(implementation="torch_storage_float64", implementation_version="torch-constant-flow-0.2.0+worker-v1", dtype="float64", device="cpu", execution_status="completed"))
        v.validate_wire(payload, wire)
        for field, value in (("identity", {"snapshot_id": "forged"}), ("method_version", 1), ("tank_ids", ["tank_1", "tank_0"]),
                             ("time_indices", [0, 120]), ("time_h", v.TIMES[:-1]), ("cost", float("inf")), ("provenance", {}), ("volumes_m3", [[float("nan")]*121]*2)):
            with self.subTest(field=field):
                r = copy.deepcopy(wire); r[field] = value
                with self.assertRaises(AssertionError): v.validate_wire(payload, r)


if __name__ == "__main__": unittest.main()
