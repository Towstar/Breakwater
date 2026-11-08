import copy
from dataclasses import replace
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eog_detection import detect
from eog_response import ResponseUnavailable, compile_context, evaluate, native_plan
from eog_test_fixtures import frame, model, view
from native_eog import NativeEvaluator


def context(count=1, cutoff=300):
    m = model(count)
    rows = [s for i in range(count) for s in (frame(m, 0, index=i, status=0) + frame(m, 300, index=i, status=2))]
    return compile_context(m, view(m, rows, cutoff))


def proposal(c):
    return {"plan_id": "opaque/plan:alpha", "context": "snapshot_now", "start_unix": c["detection"].cutoff_unix,
            "repair_deadlines": [{"facility_id": f.id, "repair_elapsed_s": n["repair_s"]}
                                 for f, n in zip(c["facilities"], c["snapshot"]["facilities"])],
            "truck_jobs": [], "well_shut_ins": [], "inspection_assets": [], "plan_hash": "0" * 64}


class ResponseCompilationTests(unittest.TestCase):
    def test_complete_current_failure_set_and_pretrip_economics(self):
        c = context(2)
        self.assertEqual(len(c["facilities"]), 2)
        self.assertEqual(len(c["snapshot"]["fleets"]), 1)
        self.assertEqual([f["repair_s"] for f in c["snapshot"]["facilities"]], [21600, 21600])
        self.assertEqual([w["productive"] for w in c["snapshot"]["wells"]], [1] * 6)
        self.assertEqual(c["detection"].events[0].productive_well_ids, tuple(w.id for w in c["facilities"][0].wells))

    def test_seconds_are_derived_without_hours_roundtrip(self):
        m = replace(model(), repair_hours=0.10000000000000002)
        v = view(m, frame(m, 0, status=0) + frame(m, 300, status=2), 301)
        c = compile_context(m, v)
        self.assertEqual(c["snapshot"]["facilities"][0]["repair_s"], m.repair_hours * 3600 - 1)

    def test_expired_unknown_and_incomplete_context_are_unavailable(self):
        m = model()
        for rows, cutoff in ((frame(m, 300, status=2), 300),
                             (frame(m, 0, status=0) + frame(m, 300, status=2), 601)):
            with self.subTest(cutoff=cutoff), self.assertRaises(ResponseUnavailable):
                compile_context(m, view(m, rows, cutoff))
        m = replace(m, repair_hours=0.05)
        with self.assertRaises(ResponseUnavailable):
            compile_context(m, view(m, frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 600, status=2)))

    def test_unknown_ids_remain_invalid_without_repairing_candidate(self):
        c = context(); p = proposal(c); p["repair_deadlines"][0]["facility_id"] = "unknown"
        before = copy.deepcopy(p); indexed, violations = native_plan(c, p)
        self.assertEqual(indexed["horizons"][0]["facility"], 0xffffffff)
        self.assertEqual(violations[0]["code"], "unknown_facility"); self.assertEqual(p, before)

    def test_known_inactive_has_zero_cost_eligibility_and_unknown_is_not_zero(self):
        m = model(); rows = frame(m, 0, status=0, well_states=(1, 0, 1)) + frame(m, 300, status=2, well_states=(1, 0, 1))
        c = compile_context(m, view(m, rows))
        self.assertEqual([w["productive"] for w in c["snapshot"]["wells"]], [1, 0, 1])
        self.assertEqual(c["snapshot"]["wells"][1]["water_bpd"], 0)

    def test_already_shut_preserves_raw_inventory_and_pretrip_wells(self):
        m = model(); rows = frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 600, status=2, level=91.8, well_states=(0, 0, 0))
        c = compile_context(m, view(m, rows))
        f = c["snapshot"]["facilities"][0]
        self.assertEqual(f["already_shut_in"], 1); self.assertEqual(f["initial_bbl"], (91.8 / 100) * 800)
        self.assertEqual([w["productive"] for w in c["snapshot"]["wells"]], [1, 1, 1])

    def test_explicit_meter_reconciliation_retains_raw_evidence(self):
        m = model(); rows = frame(m, 0, status=0) + frame(m, 300, status=2, water=(440, 700, 650), inflow=1800)
        c = compile_context(m, view(m, rows))
        allocated = [w["water_bpd"] for w in c["snapshot"]["wells"]]
        self.assertEqual(math.fsum(allocated), 1800)
        self.assertAlmostEqual(allocated[0], 1800 * 440 / 1790)
        event = c["detection"].events[0]
        self.assertEqual(next(s.value for s in event.evidence if s.asset_id == m.facilities[0].wells[0].id and s.metric == "water_bpd" and s.unix == 300), 440)
        self.assertTrue(any("allocated in proportion" in item for item in c["assumptions"]))


class ResponseNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("BREAKWATER_EOG_NATIVE")
        if not path:
            raise unittest.SkipTest("Set BREAKWATER_EOG_NATIVE to the compiled EOG library.")
        cls.native = NativeEvaluator(path)

    def test_real_native_known_economics_and_exact_plan_retention(self):
        c = context(); p = proposal(c); before = copy.deepcopy(p)
        result = evaluate(c, p, self.native)
        self.assertTrue(result["feasible"]); self.assertEqual(result["outcome"], "automatic_shutdown")
        self.assertAlmostEqual(result["shutdowns"][0]["elapsed_s"], 4.48 * 3600)
        self.assertAlmostEqual(result["cost"]["total"], (6 - 4.48) * 3300)
        self.assertEqual(p, before); self.assertIs(result["plan"], p)

    def test_forged_repair_claim_and_missing_concurrent_facility_are_rejected(self):
        c = context(2); p = proposal(c); p["repair_deadlines"].pop()
        result = evaluate(c, p, self.native)
        self.assertFalse(result["feasible"]); self.assertIsNone(result["cost"])
        self.assertEqual(result["violations"][0]["code"], "repair_horizons")
        c = context(); p = proposal(c); p["repair_deadlines"][0]["repair_elapsed_s"] = math.nextafter(21600, math.inf)
        self.assertFalse(evaluate(c, p, self.native)["feasible"])

    def test_shared_labels_cannot_create_an_extra_truck(self):
        c = context(); p = proposal(c); f = c["facilities"][0]
        p["truck_jobs"] = [{"job_id": "job" + str(i), "truck_id": "opaque" + str(i), "system_id": f.system_id,
                            "facility_id": f.id, "called_elapsed_s": 0, "response_min": 75, "cycle_min": 230,
                            "trips": [{"arrival_elapsed_s": 4500, "volume_bbl": 150, "cost": 905}],
                            "available_again_elapsed_s": 18300} for i in range(3)]
        result = evaluate(c, p, self.native)
        self.assertFalse(result["feasible"]); self.assertIn("truck_job", [v["code"] for v in result["violations"]])


if __name__ == "__main__":
    unittest.main()
