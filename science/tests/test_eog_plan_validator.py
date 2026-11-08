import ast
import copy
from dataclasses import replace
from fractions import Fraction
import math
import os
from pathlib import Path
import random
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eog_plan_validator import Arithmetic, METHOD, ValidationUnavailable, reconstruct, validate
from eog_observations import Archive, Policy
from eog_test_fixtures import frame, model, view
from eog_response import compile_context, evaluate
from native_eog import NativeEvaluator


def baseline(m, cutoff=300, rows=None):
    if rows is None:
        rows = [s for i in range(len(m.facilities)) for s in frame(m, 0, index=i, status=0) + frame(m, 300, index=i, status=2)]
    v = view(m, rows, cutoff)
    p = {"plan_id": "independent-fixture", "plan_hash": "0" * 64, "context": "snapshot_now", "start_unix": cutoff,
         "repair_deadlines": [{"facility_id": f.id, "repair_elapsed_s": m.repair_hours * 3600 - (cutoff - 300)} for f in m.facilities],
         "truck_jobs": [], "well_shut_ins": [], "inspection_assets": []}
    return v, p


def job(m, f, name, truck, call=0, count=1):
    first = call + f.response_min * 60; cycle = f.cycle_min * 60
    return {"job_id": name, "truck_id": truck, "system_id": f.system_id, "facility_id": f.id,
            "called_elapsed_s": call, "response_min": f.response_min, "cycle_min": f.cycle_min,
            "trips": [{"arrival_elapsed_s": first + i * cycle, "volume_bbl": m.truck_capacity_bbl, "cost": m.cost_per_trip} for i in range(count)],
            "available_again_elapsed_s": first + (count - 1) * cycle + cycle}


class IndependentValidatorTests(unittest.TestCase):
    def test_no_action_known_inventory_cost_and_raw_evidence(self):
        m = model(); v, p = baseline(m); before = copy.deepcopy(p)
        result = validate(m, v, p)
        r = result.record
        self.assertTrue(r["independent"]); self.assertTrue(r["feasible"])
        self.assertEqual(r["validator_method"], METHOD)
        self.assertEqual(r["cost"], {"truck_cost": 0, "lost_revenue": 5016, "total": 5016})
        self.assertEqual(r["shutdowns"][0]["elapsed_s"], 16128)
        self.assertEqual(r["minimum_shutdown_margin_bbl"], 0)
        self.assertEqual(result.audit["facilities"][0]["final_bbl"], 736)
        self.assertEqual(p, before)
        self.assertEqual(len(r["evidence_ids"]), len(set(r["evidence_ids"])))
        self.assertTrue(all(s["timestamp_unix"] <= 300 for s in r["evidence"]))

    def test_truck_and_delayed_well_economics(self):
        m = model(); v, p = baseline(m); f = m.facilities[0]
        p["truck_jobs"] = [job(m, f, "job-alpha", "opaque-truck")]
        hauled = validate(m, v, p)
        self.assertTrue(hauled.record["feasible"]); self.assertEqual(hauled.record["cost"]["total"], 905)
        self.assertEqual(hauled.record["outcome"], "avoids_shutdown")
        self.assertEqual(hauled.audit["facilities"][0]["final_bbl"], 700)
        p["truck_jobs"] = []; p["well_shut_ins"] = [{"facility_id": f.id, "well_id": f.wells[1].id,
                                                     "start_elapsed_s": 7200, "end_elapsed_s": 21600}]
        well = validate(m, v, p)
        self.assertEqual(well.record["cost"]["total"], 2000)
        self.assertAlmostEqual(well.audit["facilities"][0]["final_bbl"], 733.3333333333333)

    def test_no_calls_to_detector_compiler_native_or_files(self):
        m = model(); v, p = baseline(m)
        with patch("eog_response.compile_context", side_effect=AssertionError("compiler called")), \
             patch("eog_response.evaluate", side_effect=AssertionError("evaluator called")), \
             patch("eog_detection.detect", side_effect=AssertionError("detector called")), \
             patch("native_eog.NativeEvaluator.evaluate", side_effect=AssertionError("native called")), \
             patch("builtins.open", side_effect=AssertionError("file read")), \
             patch.object(Path, "open", side_effect=AssertionError("path read")):
            self.assertTrue(validate(m, v, p).record["feasible"])
        tree = ast.parse((Path(__file__).resolve().parents[1] / "eog_plan_validator.py").read_text(encoding="utf-8"))
        imports = [n.module for n in ast.walk(tree) if isinstance(n, ast.ImportFrom)]
        imports += [a.name for n in ast.walk(tree) if isinstance(n, ast.Import) for a in n.names]
        self.assertFalse({"eog_response", "native_eog", "eog_detection", "ctypes", "torch"} & set(imports))

    def test_left_censored_expired_stale_and_unknown_context_are_unavailable(self):
        m = model(); _, p = baseline(m)
        for rows, cutoff in ((frame(m, 300, status=2), 300),
                             (frame(m, 0, status=0) + frame(m, 300, status=2), 601)):
            with self.subTest(cutoff=cutoff), self.assertRaises(ValidationUnavailable):
                validate(m, view(m, rows, cutoff), p)
        short = replace(m, repair_hours=0.05)
        v = view(short, frame(short, 0, status=0) + frame(short, 300, status=2) + frame(short, 600, status=2))
        with self.assertRaises(ValidationUnavailable): validate(short, v, p)
        with self.assertRaises(ValidationUnavailable): validate(m, baseline(m)[0], p, METHOD)

    def test_caller_freshness_policy_cannot_relax_public_300_seconds(self):
        m = model(); v, p = baseline(m)
        raw = Archive(m.channels(), frame(m, 0, status=0) + frame(m, 300, status=2), capture_cutoff_unix=1000)
        v = raw.at(1000, policy=Policy(max_age_s=3600))
        with self.assertRaises(ValidationUnavailable): validate(m, v, p)

    def test_deadline_claim_exactly_recomputed_and_full_set_required(self):
        m = model(2); v, p = baseline(m)
        for changed in (p["repair_deadlines"][:1], [dict(x, repair_elapsed_s=math.nextafter(x["repair_elapsed_s"], math.inf)) for x in p["repair_deadlines"]]):
            bad = {**p, "repair_deadlines": changed}; r = validate(m, v, bad).record
            self.assertFalse(r["feasible"]); self.assertIsNone(r["cost"]); self.assertEqual(r["validated_horizons"], [])

    def test_unknown_actions_wrong_ownership_and_partial_loads_are_invalid(self):
        m = model(); v, p = baseline(m); f = m.facilities[0]
        for field, value in (("volume_bbl", 149), ("cost", 904), ("arrival_elapsed_s", 4501)):
            bad = copy.deepcopy(p); j = job(m, f, "job", "truck"); j["trips"][0][field] = value; bad["truck_jobs"] = [j]
            self.assertFalse(validate(m, v, bad).record["feasible"])
        p["well_shut_ins"] = [{"facility_id": f.id, "well_id": "unknown", "start_elapsed_s": 0, "end_elapsed_s": 21600}]
        r = validate(m, v, p).record; self.assertFalse(r["feasible"]); self.assertIsNone(r["cost"])

    def test_shared_fleet_pairwise_calendar_and_final_return(self):
        m = model(2); v, p = baseline(m)
        a, b = m.facilities
        p["truck_jobs"] = [job(m, a, "one", "opaque", 0), job(m, b, "two", "opaque", 100)]
        r = validate(m, v, p).record
        self.assertFalse(r["feasible"]); self.assertIn("fleet_overlap", [v["code"] for v in r["violations"]])
        p["truck_jobs"][1] = job(m, b, "two", "opaque", p["truck_jobs"][0]["available_again_elapsed_s"])
        # It is no longer overbooked, but the actual second arrival exceeds repair.
        r = validate(m, v, p).record
        self.assertNotIn("fleet_overlap", [v["code"] for v in r["violations"]])
        self.assertIn("truck_trip", [v["code"] for v in r["violations"]])

    def test_same_time_full_inventory_is_combined(self):
        m = model(); v, p = baseline(m, rows=frame(m, 0, status=0) + frame(m, 300, status=2, level=20))
        p["truck_jobs"] = [job(m, m.facilities[0], str(i), str(i)) for i in range(2)]
        r = validate(m, v, p).record
        self.assertFalse(r["feasible"]); self.assertIn("insufficient_inventory", [v["code"] for v in r["violations"]])

    def test_threshold_first_tie_one_ulp_and_late_pickup(self):
        m = model(); f = m.facilities[0]
        w = replace(f.wells[0], baseline_water_bpd=86400, oil_bpd=86400, gas_mcfd=0, revenue_per_hour=3600)
        f = replace(f, capacity_bbl=1000, shutdown_pct=10, wells=(w,), response_min=1, cycle_min=1)
        m = replace(m, facilities=(f,), oil_per_bbl=1, gas_per_mcf=0, repair_hours=200 / 3600, truck_capacity_bbl=50, cost_per_trip=100)
        v, p = baseline(m, rows=frame(m, 0, status=0, level=0) + frame(m, 300, status=2, level=0))
        p["well_shut_ins"] = [{"facility_id": f.id, "well_id": w.id, "start_elapsed_s": 100, "end_elapsed_s": 200}]
        tied = validate(m, v, p).record
        self.assertEqual(tied["shutdowns"], [{"facility_id": f.id, "elapsed_s": 100}])
        p["well_shut_ins"][0]["start_elapsed_s"] = math.nextafter(100, 0)
        self.assertEqual(validate(m, v, p).record["outcome"], "avoids_shutdown")
        p["well_shut_ins"] = []; p["truck_jobs"] = [job(m, f, "late", "truck", 40)]
        late = validate(m, v, p)
        self.assertEqual(late.record["outcome"], "automatic_shutdown"); self.assertEqual(late.record["cost"]["total"], 200)
        self.assertEqual(late.audit["facilities"][0]["final_bbl"], 50)

    def test_future_prefix_invariance_and_preexisting_inactive(self):
        m = model(); prefix = frame(m, 0, status=0, well_states=(1, 0, 1)) + frame(m, 300, status=2, well_states=(1, 0, 1))
        v, p = baseline(m, rows=prefix)
        a = validate(m, v, p)
        future = frame(m, 600, status=1, level=0)
        b = validate(m, Archive(m.channels(), prefix + future, capture_cutoff_unix=600).at(300), p)
        self.assertEqual(a, b)
        self.assertNotIn(m.facilities[0].wells[1].id, [x["well_id"] for x in a.audit["facilities"][0]["well_losses"]])

    def test_repeated_failure_uses_current_onset_and_never_expected_repair_as_recovery(self):
        m = model()
        rows = (frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 600, status=1) +
                frame(m, 900, status=2))
        v = view(m, rows)
        _, p = baseline(m, cutoff=900)
        p["repair_deadlines"][0]["repair_elapsed_s"] = 21600
        r = validate(m, v, p)
        self.assertEqual(r.audit["facilities"][0]["onset_unix"], 900)
        self.assertTrue(r.record["feasible"])

    def test_already_shut_preserves_recorded_inventory_and_pretrip_cost(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 600, status=2, level=91.8, well_states=(0, 0, 0))
        v, p = baseline(m, cutoff=600, rows=rows)
        r = validate(m, v, p)
        self.assertEqual(r.audit["facilities"][0]["final_bbl"], (91.8 / 100) * 800)
        self.assertEqual(r.record["shutdowns"][0]["elapsed_s"], 0)
        self.assertAlmostEqual(r.record["cost"]["total"], 19525)

    def test_native_ambiguous_volume_rounding_is_not_certified(self):
        m = model(); f = m.facilities[0]
        w = replace(f.wells[0], baseline_water_bpd=86400)
        f = replace(f, capacity_bbl=1000, shutdown_pct=50, wells=(w,))
        m = replace(m, facilities=(f,), repair_hours=200 / 3600)
        v, p = baseline(m, rows=frame(m, 0, status=0, level=40) + frame(m, 300, status=2, level=40))
        p["well_shut_ins"] = [{"facility_id": f.id, "well_id": w.id, "start_elapsed_s": math.nextafter(100, 0), "end_elapsed_s": 200}]
        with self.assertRaises(ValidationUnavailable) as caught:
            validate(m, v, p)
        self.assertEqual(caught.exception.code, "numerical_unavailable")

    def test_rational_primitive_rounds_one_ulp_and_bounds_work(self):
        a = Arithmetic(maximum=3)
        self.assertEqual(a.add(1, 2**-53), 1)
        self.assertEqual(a.round(Fraction(1) + Fraction(3, 2**53)), math.nextafter(math.nextafter(1, 2), 2))
        self.assertEqual(a.mul(2, 3), 6)
        with self.assertRaises(ValidationUnavailable): a.add(1, 1)

    def test_validation_identity_includes_economics_and_exact_candidate(self):
        m = model(); v, p = baseline(m)
        original = validate(m, v, p)
        changed = validate(replace(m, oil_per_bbl=61), v, p)
        self.assertNotEqual(original.record["validation_id"], changed.record["validation_id"])
        self.assertGreater(changed.record["cost"]["total"], original.record["cost"]["total"])

    def test_nonadvancing_yard_return_and_extreme_late_call_are_invalid(self):
        m = model(); f = replace(m.facilities[0], cycle_min=math.ulp(0.0))
        m = replace(m, facilities=(f,))
        v, p = baseline(m); p["truck_jobs"] = [job(m, f, "one", "truck")]
        r = validate(m, v, p).record
        self.assertFalse(r["feasible"]); self.assertIn("yard_return", [x["code"] for x in r["violations"]])
        p["truck_jobs"] = [job(m, f, "one", "truck", call=1e308)]
        r = validate(m, v, p).record
        self.assertFalse(r["feasible"]); self.assertIsNone(r["cost"])


class IndependentNativeCrossChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("BREAKWATER_EOG_NATIVE")
        if not path: raise unittest.SkipTest("Set BREAKWATER_EOG_NATIVE for real native/independent cross-checks.")
        cls.native = NativeEvaluator(path)

    def test_randomized_candidate_recomputation_from_raw_view(self):
        rng = random.Random(2026110806)
        for case in range(360):
            m = model(); f = m.facilities[0]
            wells = tuple(replace(w, baseline_water_bpd=rng.uniform(100, 12000), oil_bpd=rng.uniform(10, 800),
                                  gas_mcfd=rng.uniform(0, 2500)) for w in f.wells)
            f = replace(f, wells=wells, response_min=5, cycle_min=30)
            m = replace(m, facilities=(f,), repair_hours=rng.uniform(0.5, 10))
            rows = frame(m, 0, status=0) + frame(m, 300, status=2, level=rng.uniform(5, 80))
            v, p = baseline(m, rows=rows)
            horizon = p["repair_deadlines"][0]["repair_elapsed_s"]
            if case % 3:
                for w in wells:
                    if rng.random() < 0.5:
                        p["well_shut_ins"].append({"facility_id": f.id, "well_id": w.id,
                                                  "start_elapsed_s": rng.random() * horizon, "end_elapsed_s": horizon})
            if case % 4:
                for i in range(rng.randrange(3)):
                    p["truck_jobs"].append(job(m, f, str(i), str(i), rng.random() * 600, rng.randrange(1, 4)))
            actual = evaluate(compile_context(m, v), p, self.native)
            checked = validate(m, v, p)
            with self.subTest(case=case):
                self.assertEqual(checked.record["feasible"], actual["feasible"])
                self.assertEqual(checked.record["outcome"], actual["outcome"])
                if actual["feasible"]:
                    self.assertEqual(checked.record["shutdowns"], actual["shutdowns"])
                    self.assertEqual(checked.record["cost"], actual["cost"])
                    for trace, audit in zip(actual["trajectories"], checked.audit["facilities"]):
                        self.assertEqual(trace["volumes_bbl"][-1], audit["final_bbl"])

    def test_joint_facilities_and_between_frame_snapshot_recompute(self):
        m = model(3)
        for cutoff in (300, 450):
            v, p = baseline(m, cutoff=cutoff)
            p["truck_jobs"] = [job(m, f, "job" + str(i), "truck" + str(i)) for i, f in enumerate(m.facilities[:2])]
            a = evaluate(compile_context(m, v), p, self.native); b = validate(m, v, p).record
            self.assertEqual((a["feasible"], a["cost"], a["shutdowns"]), (b["feasible"], b["cost"], b["shutdowns"]))
            self.assertEqual(a["outcome"], "automatic_shutdown")
            p["truck_jobs"][1]["truck_id"] = "truck0"
            self.assertFalse(validate(m, v, p).record["feasible"])
            self.assertFalse(evaluate(compile_context(m, v), p, self.native)["feasible"])


if __name__ == "__main__":
    unittest.main()
