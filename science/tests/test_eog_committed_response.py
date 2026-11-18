import copy
import ctypes
import math
import os
from pathlib import Path
import random
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from native_eog import NativeError, NativeEvaluator
from native_eog_committed import NativeCommittedEvaluator
from eog_committed_validation import CommittedValidationError, validate
from native_eog_plant import NativePlant


def case():
    s = {"oil_per_bbl": 1, "gas_per_mcf": 0, "truck_capacity_bbl": 10, "cost_per_trip": 3,
         "fleets": [{"trucks": 2}], "wells": [{"water_bpd": 86400, "oil_bpd": 86400, "gas_mcfd": 0, "productive": 1}],
         "facilities": [{"system": 0, "first_well": 0, "well_count": 1, "already_shut_in": 0,
                         "capacity_bbl": 100, "initial_bbl": 80, "shutdown_bbl": 90, "repair_s": 20,
                         "response_min": 2/60, "cycle_min": 5/60}]}
    p = {"horizons": [{"facility": 0, "repair_s": 20}], "stops": [], "jobs": [], "trips": []}
    c = {"occupied": [{"system": 0, "truck": 0, "available_s": 10}],
         "pickups": [{"facility": 0, "system": 0, "truck": 0, "arrival_s": 5}], "stopped_wells": []}
    return s, p, c


def job(s, p, called=10, *, truck=0, trips=1):
    f = s["facilities"][0]; first = called + f["response_min"] * 60; cycle = f["cycle_min"] * 60
    start = len(p["trips"])
    p["trips"].extend({"arrival_s": first + k*cycle, "volume_bbl": s["truck_capacity_bbl"], "cost": s["cost_per_trip"]} for k in range(trips))
    p["jobs"].append({"facility": 0, "system": 0, "truck": truck, "first_trip": start, "trip_count": trips,
                      "called_s": called, "response_min": f["response_min"], "cycle_min": f["cycle_min"],
                      "available_again_s": p["trips"][-1]["arrival_s"] + cycle})


def generated(seed=2026110811, count=400):
    rng = random.Random(seed)
    for i in range(count):
        s, p, c = case(); f = s["facilities"][0]
        f["initial_bbl"] = rng.uniform(20, 88)
        s["wells"][0]["water_bpd"] = rng.uniform(0, 2) * 86400
        s["wells"][0]["oil_bpd"] = rng.uniform(0, 3) * 86400
        s["truck_capacity_bbl"] = rng.uniform(2, 25); s["cost_per_trip"] = rng.uniform(0, 10)
        free = rng.uniform(6, 14); c["occupied"][0]["available_s"] = free
        c["pickups"][0]["arrival_s"] = rng.uniform(.25, free - .25)
        if i % 5 == 0: c["stopped_wells"] = [0]
        if i % 3 == 0: job(s, p, free)
        elif i % 3 == 1: job(s, p, math.nextafter(free, 0))
        if i % 7 == 0 and not c["stopped_wells"]:
            p["stops"] = [{"facility": 0, "well": 0, "start_s": rng.uniform(0, 20), "end_s": 20}]
        yield s, p, c


class CommittedPureTests(unittest.TestCase):
    def test_known_future_cost_and_volume_without_native_or_files(self):
        s, p, c = case(); job(s, p)
        with patch("ctypes.CDLL", side_effect=AssertionError("no native")), patch("builtins.open", side_effect=AssertionError("no file")), \
                patch.object(Path, "open", side_effect=AssertionError("no file")):
            result = validate(s, p, c)
        self.assertEqual(result.record["status"], "available")
        self.assertEqual(result.record["cost"], {"truck_cost": 6, "lost_revenue": 0, "total": 6})
        self.assertEqual(result.audit["final_inventories"][0]["final_bbl"], 80)

    def test_prior_stop_preserves_future_opportunity_cost(self):
        s, p, c = case(); c["stopped_wells"] = [0]
        result = validate(s, p, c)
        self.assertEqual(result.record["cost"]["total"], 23)
        s["facilities"][0].update(initial_bbl=90, already_shut_in=1)
        self.assertEqual(validate(s, p, c).record["cost"]["lost_revenue"], 20)

    def test_exact_return_boundary_and_one_ulp_early(self):
        s, p, c = case(); job(s, p)
        self.assertEqual(validate(s, p, c).record["status"], "available")
        s, p, c = case(); job(s, p, math.nextafter(10, 0))
        result = validate(s, p, c)
        self.assertEqual(result.record["status"], "invalid")
        self.assertIsNone(result.record["cost"])
        self.assertTrue(any(x["code"] == "fleet_overlap" for x in result.audit["issues"]))


class CommittedNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("BREAKWATER_EOG_NATIVE")
        if not path: raise unittest.SkipTest("Set BREAKWATER_EOG_NATIVE to a DLL with the committed-state extension.")
        cls.native = NativeCommittedEvaluator(path); cls.legacy = NativeEvaluator(path)

    def checked(self, s, p, c):
        result = self.native.evaluate(s, p, c)
        checked = validate(s, p, c, result)
        self.assertTrue(checked.record["independent"])
        return result, checked

    def test_known_committed_and_new_loads_and_prior_loss(self):
        s, p, c = case(); result, checked = self.checked(s, p, c)
        self.assertEqual(result["cost"]["total"], 3)
        job(s, p); self.assertEqual(self.checked(s, p, c)[0]["cost"]["total"], 6)
        p["jobs"].clear(); p["trips"].clear(); c["stopped_wells"] = [0]
        self.assertEqual(self.checked(s, p, c)[0]["cost"]["lost_revenue"], 20)

    def test_busy_return_tie_and_duplicate_prior_stop_are_invalid(self):
        s, p, c = case(); job(s, p, math.nextafter(10, 0))
        self.assertEqual(self.checked(s, p, c)[0]["status"], "invalid")
        s, p, c = case(); c["stopped_wells"] = [0]
        p["stops"] = [{"facility": 0, "well": 0, "start_s": 0, "end_s": 20}]
        self.assertEqual(self.checked(s, p, c)[0]["status"], "invalid")

    def test_simultaneous_known_and_new_pickups_need_full_combined_inventory(self):
        s, p, c = case(); s["facilities"][0]["initial_bbl"] = 15; s["wells"][0]["water_bpd"] = 0
        job(s, p, 3, truck=1)
        result, reference = self.checked(s, p, c)
        self.assertEqual(result["status"], "invalid")
        self.assertTrue(any(v["code"] == "insufficient_inventory" for v in result["violations"]))
        self.assertEqual(reference.audit["issues"][0]["elapsed_s"], 5)

    def test_malformed_context_rejects_in_both_implementations(self):
        builders = [lambda c: c["occupied"].append(dict(c["occupied"][0])),
                    lambda c: c["occupied"].clear(), lambda c: c["pickups"][0].update(arrival_s=0),
                    lambda c: c["pickups"][0].update(arrival_s=20), lambda c: c.update(stopped_wells=[10]),
                    lambda c: c.update(stopped_wells=[0, 0]), lambda c: c["pickups"][0].update(system=5)]
        for build in builders:
            s, p, c = case(); build(c)
            with self.subTest(c=c):
                with self.assertRaises(NativeError): self.native.evaluate(s, p, c)
                with self.assertRaises(CommittedValidationError): validate(s, p, c)

    def test_independent_result_binding_rejects_forged_context_cost_and_method(self):
        s, p, c = case(); result = self.native.evaluate(s, p, c)
        for key, value in (("method", "eog-native-seconds-0.1.1"), ("cost", {"truck_cost": 0, "lost_revenue": 0, "total": 0})):
            forged = copy.deepcopy(result); forged[key] = value
            with self.assertRaises(CommittedValidationError): validate(s, p, c, forged)
        altered = copy.deepcopy(c); altered["occupied"][0]["available_s"] += 1
        with self.assertRaises(CommittedValidationError): validate(s, p, altered, result)
        result["facilities"][0]["final_bbl"] += 1
        with self.assertRaises(CommittedValidationError): validate(s, p, c, result)

    def test_numerical_cost_underflow_overflow_and_no_partial_outcome_claim(self):
        s, p, c = case(); s["cost_per_trip"] = 1e308; job(s, p)
        result, _ = self.checked(s, p, c)
        self.assertEqual(result["status"], "unavailable"); self.assertIsNone(result["automatic_shutdown"])
        s, p, c = case(); s["oil_per_bbl"] = 1e-300; s["wells"][0]["oil_bpd"] = 1e-300; c["stopped_wells"] = [0]
        self.assertEqual(self.checked(s, p, c)[0]["status"], "unavailable")

    def test_legacy_and_empty_committed_context_match_all_available_values(self):
        for s, p, _ in generated(count=60):
            c = {"occupied": [], "pickups": [], "stopped_wells": []}
            actual, _ = self.checked(s, p, c); legacy = self.legacy.evaluate(s, p)
            for key in ("status", "cost", "facilities", "violations"):
                self.assertEqual(actual[key], legacy[key])
            if actual["status"] == "available": self.assertEqual(actual["automatic_shutdown"], legacy["automatic_shutdown"])

    def test_generated_mixed_committed_cases_match_independent_arithmetic(self):
        counts = dict(available=0, invalid=0, unavailable=0)
        for i, (s, p, c) in enumerate(generated()):
            with self.subTest(case=i):
                actual, _ = self.checked(s, p, c); counts[actual["status"]] += 1
        self.assertGreater(counts["available"], 50); self.assertGreater(counts["invalid"], 50)

    def test_cross_facility_reservation_and_independent_system_slots(self):
        s, p, c = case()
        s["wells"].append(copy.deepcopy(s["wells"][0]))
        s["facilities"].append({**s["facilities"][0], "first_well": 1})
        p["horizons"].append({"facility": 1, "repair_s": 20})
        c["pickups"][0]["facility"] = 1
        job(s, p)
        self.assertEqual(self.checked(s, p, c)[0]["cost"]["total"], 16)
        s["fleets"].append({"trucks": 1}); s["facilities"][1]["system"] = 1
        c["occupied"][0]["system"] = 1; c["pickups"][0]["system"] = 1
        p["jobs"].clear(); p["trips"].clear(); job(s, p, 0)
        self.assertEqual(self.checked(s, p, c)[0]["cost"]["total"], 6)

    def test_complete_point_and_independent_work_bounds_are_unavailable(self):
        s, p, c = case(); s["truck_capacity_bbl"] = .001
        c["pickups"] = [{"facility": 0, "system": 0, "truck": 0, "arrival_s": .001 + k * .001} for k in range(4095)]
        c["occupied"][0]["available_s"] = c["pickups"][-1]["arrival_s"] + .001
        result, reference = self.checked(s, p, c)
        self.assertEqual(result["status"], "unavailable")
        self.assertIsNone(result["cost"])
        s, p, c = case()
        limited = validate(s, p, c, maximum_primitives=1)
        self.assertEqual(limited.record["status"], "unavailable")
        self.assertIsNone(limited.record["cost"])

    def test_committed_suffix_matches_actual_stateful_plant_and_excludes_sunk_cost(self):
        library = os.environ.get("BREAKWATER_EOG_PLANT")
        if not library: self.skipTest("Set BREAKWATER_EOG_PLANT for an actual stateful suffix comparison.")
        for prior_stop in (False, True):
            with self.subTest(prior_stop=prior_stop):
                s, p, c = case()
                raw = {"duration_s": 20, "oil_per_bbl": 1, "gas_per_mcf": 0, "truck_capacity_bbl": 10, "cost_per_trip": 3,
                    "fleet_sizes": [2], "wells": copy.deepcopy(s["wells"]),
                    "facilities": [{"system": 0, "first_well": 0, "well_count": 1, "initially_running": 1,
                        "capacity_bbl": 100, "initial_bbl": 80, "start_bbl": 70, "stop_bbl": 30, "shutdown_bbl": 90,
                        "pump_bpd": 259200, "pipeline_capacity_bpd": 300000, "response_min": 2/60, "cycle_min": 5/60}],
                    "failures": [{"facility": 0, "start_s": 0, "repair_s": 20}], "rate_changes": [],
                    "point_limit": 262144, "event_limit": 1000000, "trip_limit": 16384}
                with NativePlant(library, raw) as plant:
                    self.assertTrue(plant.dispatch(0, 0, 2)["accepted"])
                    if prior_stop:
                        plant.advance(1); self.assertTrue(plant.shut_in(0)["accepted"])
                    before = plant.advance(2)
                    s["facilities"][0].update(initial_bbl=before["facilities"][0]["volume_bbl"], repair_s=18)
                    p["horizons"][0]["repair_s"] = 18
                    if prior_stop: c["stopped_wells"] = [0]
                    job(s, p, 10)
                    result, _ = self.checked(s, p, c)
                    plant.advance(12); self.assertTrue(plant.dispatch(0, 0, 1)["accepted"])
                    after = plant.advance(20)
                    self.assertEqual(result["facilities"][0]["final_bbl"], after["facilities"][0]["volume_bbl"])
                    self.assertEqual(result["cost"]["total"], after["cost"]["total"] - before["cost"]["total"])
                    self.assertEqual(result["cost"]["truck_cost"], 6)


if __name__ == "__main__": unittest.main()
