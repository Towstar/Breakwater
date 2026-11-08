import copy
import json
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from native_eog import NativeError, NativeEvaluator


def fixtures():
    snapshot = {"oil_per_bbl": 60, "gas_per_mcf": 4, "truck_capacity_bbl": 150, "cost_per_trip": 100,
                "fleets": [{"trucks": 2}], "facilities": [{"system": 0, "first_well": 0, "well_count": 1,
                "already_shut_in": 0, "capacity_bbl": 1000, "initial_bbl": 800, "shutdown_bbl": 920,
                "repair_s": 7200, "response_min": 30, "cycle_min": 60}],
                "wells": [{"water_bpd": 2400, "oil_bpd": 24, "gas_mcfd": 240, "productive": 1}]}
    plan = {"horizons": [{"facility": 0, "repair_s": 7200}], "stops": [], "jobs": [], "trips": []}
    return snapshot, plan


class NativeEogInputTests(unittest.TestCase):
    def test_explicit_library_required(self):
        for path in ("relative.dll", Path(__file__).parent / "nonexistent.dll"):
            with self.assertRaises(NativeError) as caught:
                NativeEvaluator(path)
            self.assertEqual(caught.exception.code, "native_unavailable")


class NativeEogExecutionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Explicit optional dependency, consistent with other native science checks.
        # Qualification commands always provide this variable; no skipped pass claim.
        path = os.environ.get("BREAKWATER_EOG_NATIVE")
        if not path:
            raise unittest.SkipTest("Set BREAKWATER_EOG_NATIVE to a built native response library.")
        cls.native = NativeEvaluator(path)

    def test_actual_native_no_action_and_copy_lifetime(self):
        snapshot, plan = fixtures()
        before = copy.deepcopy((snapshot, plan))
        result = self.native.evaluate(snapshot, plan)
        self.assertEqual(result["method"], "eog-native-seconds-0.1.1")
        self.assertEqual(result["status"], "available")
        self.assertEqual(result["cost"], {"truck_cost": 0, "lost_revenue": 80, "total": 80})
        self.assertEqual(result["facilities"][0]["shutdown_s"], 4320)
        saved = copy.deepcopy(result)
        for _ in range(30):
            self.native.evaluate(snapshot, plan)
        self.assertEqual(result, saved)
        self.assertEqual((snapshot, plan), before)
        json.dumps(result, allow_nan=False)

    def test_invalid_claim_is_structured_and_uncosted(self):
        snapshot, plan = fixtures()
        plan["horizons"][0]["repair_s"] = math.nextafter(7200, math.inf)
        result = self.native.evaluate(snapshot, plan)
        self.assertEqual(result["status"], "invalid")
        self.assertIsNone(result["cost"])
        self.assertEqual(result["violations"][0]["code"], "repair_horizons")
        json.dumps(result, allow_nan=False)

    def test_full_trips_and_fleet(self):
        snapshot, plan = fixtures()
        plan["jobs"] = [{"facility": 0, "system": 0, "truck": 0, "first_trip": 0, "trip_count": 2,
                          "called_s": 0, "response_min": 30, "cycle_min": 60, "available_again_s": 9000}]
        plan["trips"] = [{"arrival_s": t, "volume_bbl": 150, "cost": 100} for t in (1800, 5400)]
        result = self.native.evaluate(snapshot, plan)
        self.assertEqual(result["cost"]["total"], 200)
        self.assertFalse(result["automatic_shutdown"])
        self.assertEqual(result["facilities"][0]["final_bbl"], 700)
        self.assertEqual([(p["elapsed_s"], p["volume_bbl"]) for p in result["facilities"][0]["points"]],
                         [(0, 800), (1800, 850), (1800, 700), (5400, 800), (5400, 650), (7200, 700)])

    def test_strict_bridge_fields_indices_and_numeric_values(self):
        for field, value in (("well_count", -1), ("system", 2**32), ("first_well", True),
                             ("initial_bbl", math.nan), ("initial_bbl", 10**400), ("repair_s", "7200")):
            snapshot, plan = fixtures()
            snapshot["facilities"][0][field] = value
            with self.subTest(field=field, value=str(value)[:25]), self.assertRaises(NativeError):
                self.native.evaluate(snapshot, plan)
        snapshot, plan = fixtures(); plan["unknown"] = []
        with self.assertRaises(NativeError):
            self.native.evaluate(snapshot, plan)

    def test_native_snapshot_failure_is_not_zero_cost(self):
        snapshot, plan = fixtures(); snapshot["facilities"][0]["initial_bbl"] = -0.001
        with self.assertRaises(NativeError) as caught:
            self.native.evaluate(snapshot, plan)
        self.assertEqual(caught.exception.code, "native_input")

    def test_derived_overflow_and_underflow_are_unavailable(self):
        snapshot, plan = fixtures(); snapshot["oil_per_bbl"] = 1e308
        result = self.native.evaluate(snapshot, plan)
        self.assertEqual(result["status"], "unavailable"); self.assertIsNone(result["cost"])
        snapshot, plan = fixtures(); snapshot["wells"][0]["water_bpd"] = math.ulp(0.0)
        result = self.native.evaluate(snapshot, plan)
        self.assertEqual(result["status"], "unavailable"); self.assertIsNone(result["cost"])


if __name__ == "__main__":
    unittest.main()
