import copy
from dataclasses import FrozenInstanceError
import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eog_model import ModelError, compile_model
from eog_test_fixtures import inputs


class EogModelTests(unittest.TestCase):
    def reject(self, network, ops, code=None):
        with self.assertRaises(ModelError) as caught:
            compile_model(network, ops)
        if code:
            self.assertEqual(caught.exception.code, code)

    def test_public_illustration_units_and_economics_without_labels(self):
        m = compile_model(*inputs())
        f = m.facilities[0]
        self.assertEqual((f.capacity_bbl, f.shutdown_pct, f.pump_flow_bpd), (800, 92, 2700))
        self.assertEqual(sum(w.revenue_per_hour for w in f.wells), 3300)
        self.assertEqual(sum(w.baseline_water_bpd for w in f.wells) / 24, 75)
        self.assertEqual((m.repair_hours, m.truck_capacity_bbl, m.cost_per_trip), (6, 150, 905))
        self.assertEqual(len(m.channels()), 13)

    def test_full_unicode_id_catalog_is_derived_not_hardcoded(self):
        m = compile_model(*inputs(2, "unfamiliar \U0001f30a / root"))
        self.assertEqual(len(m.facilities), 2)
        self.assertEqual(len(m.channels()), 26)
        self.assertTrue(all(c.asset_id.startswith("unfamiliar \U0001f30a / root") for c in m.channels()))

    def test_duplicate_cross_kind_asset_identity_is_rejected(self):
        n, o = inputs()
        f = n["systems"][0]["facilities"][0]
        f["pump"]["id"] = f["tank"]["id"]
        self.reject(n, o, "duplicate_identity")

    def test_unknown_or_missing_fields_are_explicit_errors(self):
        for key, value in (("new_hydraulics", {}), ("answer_key", {})):
            n, o = inputs()
            n[key] = value
            self.reject(n, o, "invalid_shape")
        n, o = inputs()
        del n["meta"]
        self.reject(n, o, "invalid_shape")

    def test_units_and_timezone_cannot_silently_change(self):
        for key, value in (("volume", "m3"), ("rate", "bbl/hour"), ("gas_rate", "mcf")):
            n, o = inputs()
            n["meta"]["units"][key] = value
            self.reject(n, o, "unsupported_units")
        n, o = inputs()
        n["meta"]["timezone"] = "local"
        self.reject(n, o, "unsupported_units")

    def test_nonfinite_bool_and_string_economics_rejected(self):
        for value in (math.nan, math.inf, -1, True, "60", 10 ** 1000):
            n, o = inputs()
            o["prices"]["oil_per_bbl"] = value
            self.reject(n, o, "invalid_value")

    def test_zero_prices_and_zero_fleet_are_valid_not_missing(self):
        n, o = inputs()
        o["prices"] = {"oil_per_bbl": 0, "gas_per_mcf": 0}
        next(iter(o["systems"].values()))["emergency_fleet_size"] = 0
        m = compile_model(n, o)
        self.assertEqual(m.systems[0].fleet_size, 0)
        self.assertEqual(sum(w.revenue_per_hour for w in m.facilities[0].wells), 0)

    def test_threshold_order_and_configuration_bounds(self):
        for stop, start, shutdown in ((70, 30, 92), (30, 30, 92), (30, 95, 92)):
            n, o = inputs()
            pump = n["systems"][0]["facilities"][0]["pump"]
            pump.update(stop_level_pct=stop, start_level_pct=start)
            n["defaults"]["tank_limits_pct"]["shutdown"] = shutdown
            self.reject(n, o, "invalid_thresholds")
        for value in (0, -1, 169):
            n, o = inputs()
            o["expected_repair_hours"]["pump_failure"] = value
            self.reject(n, o, "invalid_value")

    def test_ops_identity_sets_must_match_network(self):
        n, o = inputs()
        sid = n["systems"][0]["id"]
        o["systems"][sid]["facilities"]["unknown"] = {"response_min": 1, "cycle_min": 1}
        self.reject(n, o, "unknown_identity")
        n, o = inputs()
        o["systems"]["unknown"] = {"emergency_fleet_size": 1, "facilities": {}}
        self.reject(n, o, "unknown_identity")
        n, o = inputs()
        o["systems"].clear()
        self.reject(n, o, "invalid_shape")

    def test_geometry_is_checked_and_does_not_determine_physics(self):
        n, o = inputs()
        before = compile_model(n, o).fingerprint()
        n["systems"][0]["facilities"][0]["lat"] = -23
        n["systems"][0]["truck_yard"] = [70, -50]
        self.assertEqual(compile_model(n, o).fingerprint(), before)
        n["systems"][0]["center"] = [91, 0]
        self.reject(n, o, "invalid_value")

    def test_fingerprint_changes_for_economics_capacity_and_ids(self):
        n, o = inputs()
        original = compile_model(n, o).fingerprint()
        for change in (lambda n, o: o["prices"].update(oil_per_bbl=61),
                       lambda n, o: n["systems"][0]["facilities"][0]["tank"].update(capacity_bbl=900)):
            nn, oo = copy.deepcopy(n), copy.deepcopy(o)
            change(nn, oo)
            self.assertNotEqual(compile_model(nn, oo).fingerprint(), original)
        self.assertNotEqual(compile_model(*inputs(prefix="other")).fingerprint(), original)

    def test_compiled_model_is_immutable_and_input_changes_do_not_leak(self):
        n, o = inputs()
        m = compile_model(n, o)
        n["systems"][0]["facilities"].clear()
        self.assertEqual(len(m.facilities), 1)
        with self.assertRaises(FrozenInstanceError):
            m.repair_hours = 100

    def test_unrepresentable_well_revenue_is_not_infinity(self):
        n, o = inputs()
        o["prices"]["oil_per_bbl"] = 1e308
        self.reject(n, o, "unrepresentable_arithmetic")

    def test_empty_wells_and_invalid_fleet_or_dataset_are_explicit(self):
        n, o = inputs()
        n["systems"][0]["facilities"][0]["wells"] = []
        self.reject(n, o, "limit_exceeded")
        for fleet in (True, 1.5, -1, 257):
            n, o = inputs()
            next(iter(o["systems"].values()))["emergency_fleet_size"] = fleet
            self.reject(n, o, "invalid_value")
        n, o = inputs()
        o["dataset_id"] = ""
        self.reject(n, o, "invalid_identity")


if __name__ == "__main__":
    unittest.main()
