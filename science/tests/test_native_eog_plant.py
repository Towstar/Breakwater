import copy
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from native_eog import NativeError
from native_eog_plant import NativePlant


def scenario():
    return {"duration_s": 180, "oil_per_bbl": 1, "gas_per_mcf": 0, "truck_capacity_bbl": 20, "cost_per_trip": 3,
        "fleet_sizes": [1], "facilities": [{"system": 0, "first_well": 0, "well_count": 1, "initially_running": 0,
            "capacity_bbl": 100, "initial_bbl": 50, "start_bbl": 70, "stop_bbl": 30, "shutdown_bbl": 90,
            "pump_bpd": 259200, "pipeline_capacity_bpd": 300000, "response_min": 2 / 60, "cycle_min": 20 / 60}],
        "wells": [{"water_bpd": 86400, "oil_bpd": 172800, "gas_mcfd": 0, "productive": 1}],
        "failures": [], "rate_changes": [], "point_limit": 262144, "event_limit": 1000000, "trip_limit": 16384}


class NativePlantTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = os.environ.get("BREAKWATER_EOG_PLANT")
        if not cls.library:
            raise unittest.SkipTest("Set BREAKWATER_EOG_PLANT for actual persistent native execution.")

    def test_real_native_clock_copied_snapshot_trace_and_reset(self):
        source = scenario()
        with NativePlant(self.library, source) as plant:
            initial = plant.snapshot()
            source["wells"][0]["water_bpd"] = 1e100
            final = plant.advance(180)
            self.assertEqual((final["status"], final["facilities"][0]["volume_bbl"], final["cost"]["total"]), ("completed", 50, 0))
            self.assertEqual(initial["elapsed_s"], 0)
            self.assertEqual(plant.trace(0, 1)[0]["elapsed_s"], 0)
            self.assertEqual(len(plant.trace()), final["trace_count"])
            self.assertEqual(plant.trace(final["trace_count"]), [])
            self.assertEqual(plant.reset(), initial)
            self.assertEqual(plant.advance(180)["facilities"], final["facilities"])

    def test_strict_shapes_types_flags_counts_and_native_values(self):
        bad = []
        for key, value in (("duration_s", True), ("duration_s", float("inf")), ("point_limit", -1), ("point_limit", 262145)):
            item = scenario(); item[key] = value; bad.append(item)
        item = scenario(); item["wells"][0]["productive"] = 2; bad.append(item)
        item = scenario(); item["facilities"][0]["initial_bbl"] = -1; bad.append(item)
        item = scenario(); item["unexpected"] = 1; bad.append(item)
        item = scenario(); item["fleet_sizes"] = [1] * 65; bad.append(item)
        for item in bad:
            with self.subTest(item=item), self.assertRaises(NativeError): NativePlant(self.library, item)

    def test_invalid_calls_preserve_clock_and_close_is_idempotent(self):
        plant = NativePlant(self.library, scenario())
        before = plant.snapshot()
        for call in (lambda: plant.advance(-1), lambda: plant.trace(100000), lambda: plant.trace(limit=True),
                     lambda: plant.dispatch(True, 0, 1), lambda: plant.advance(1, cancelled=0)):
            with self.assertRaises(NativeError): call()
            self.assertEqual(plant.snapshot(), before)
        self.assertFalse(plant.dispatch(0, 0, 1)["accepted"])
        plant.close(); plant.close()
        with self.assertRaises(NativeError) as caught: plant.snapshot()
        self.assertEqual(caught.exception.code, "native_closed")

    def test_real_actions_final_return_and_whole_load_failure(self):
        source = scenario(); source["failures"] = [{"facility": 0, "start_s": 0, "repair_s": 20}]
        with NativePlant(self.library, source) as plant:
            self.assertTrue(plant.dispatch(0, 0, 1)["accepted"])
            self.assertFalse(plant.dispatch(0, 0, 1)["accepted"])
            after = plant.advance(20)
            self.assertEqual(after["completed_full_loads"], 1)
            self.assertTrue(after["trucks"][0]["busy"])
            self.assertEqual(after["trucks"][0]["available_again_s"], 22)
            self.assertFalse(plant.advance(22)["trucks"][0]["busy"])
        source["truck_capacity_bbl"] = 100
        with NativePlant(self.library, source) as plant:
            self.assertTrue(plant.dispatch(0, 0, 1)["accepted"])
            after = plant.advance(3)
            self.assertEqual(after["status"], "invalid_action")
            self.assertIsNone(after["cost"])
            self.assertEqual(after["completed_full_loads"], 0)
            self.assertEqual(after["facilities"][0]["volume_bbl"], 52)

    def test_native_well_control_and_cancel_never_claim_completed_cost(self):
        source = scenario(); source["failures"] = [{"facility": 0, "start_s": 0, "repair_s": 20}]
        with NativePlant(self.library, source) as plant:
            self.assertTrue(plant.shut_in(0)["accepted"])
            self.assertEqual(plant.advance(20)["cost"]["lost_revenue"], 40)
            plant.reset()
            cancelled = plant.advance(0, cancelled=lambda: True)
            self.assertEqual(cancelled["status"], "cancelled")
            self.assertIsNone(cancelled["cost"])

    def test_cancellation_callback_cannot_reenter_or_destroy_live_native_state(self):
        with NativePlant(self.library, scenario()) as plant:
            for action in (plant.close, plant.reset, plant.snapshot):
                with self.assertRaises(NativeError) as caught:
                    plant.advance(10, cancelled=action)
                self.assertEqual(caught.exception.code, "callback_failed")
                self.assertEqual(plant.snapshot()["status"], "cancelled")
                plant.reset()
            self.assertEqual(plant.advance(180)["status"], "completed")

    def test_failure_rate_schedules_are_copied_and_never_returned(self):
        source = scenario(); source["failures"] = [{"facility": 0, "start_s": 10, "repair_s": 20}]
        with NativePlant(self.library, source) as plant:
            source["failures"][0]["start_s"] = 0
            before = plant.advance(5)
            self.assertEqual(before["facilities"][0]["pump_status"], 0)
            self.assertNotIn("failures", before); self.assertNotIn("rate_changes", before)
            self.assertEqual(plant.advance(10)["facilities"][0]["pump_status"], 2)

    def test_explicit_absolute_library_has_no_fallback(self):
        for path in ("relative.dll", str(Path(self.library).with_name("nonexistent-plant.dll"))):
            with self.assertRaises(NativeError) as caught: NativePlant(path, scenario())
            self.assertEqual(caught.exception.code, "native_unavailable")


if __name__ == "__main__": unittest.main()
