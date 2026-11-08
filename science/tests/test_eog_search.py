import copy
from dataclasses import replace
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eog_model import compile_model
from eog_observations import Archive
from eog_plan_validator import ValidationUnavailable, prepare, validate, validate_prepared
from eog_response import ResponseUnavailable
import eog_search
from eog_search import Rules, search
from eog_test_fixtures import frame, inputs, model, view
from native_eog import NativeEvaluator


def identity(plan):
    # Private test identity; actual worker tests use the adopted semantic hash.
    return hashlib.sha256(json.dumps(plan, sort_keys=True).encode()).hexdigest()


def fixture(water=(1,), revenue=(1,), levels=(80,), *, horizon=20, response=5, cycle=50, capacity=10, trip_cost=3, fleet=1, shutdown=90):
    network, ops = inputs(len(levels), prefix="unfamiliar/search/\u03b2")
    sid = network["systems"][0]["id"]
    network["defaults"]["tank_limits_pct"] = {"warning": shutdown * 0.9, "shutdown": shutdown}
    for fi, f in enumerate(network["systems"][0]["facilities"]):
        f["tank"]["capacity_bbl"] = 100
        if shutdown < 70:
            f["pump"]["start_level_pct"] = shutdown * 0.7
            f["pump"]["stop_level_pct"] = shutdown * 0.35
        f["wells"] = [{"id": f["id"] + "/new-well:" + str(i), "baseline_water_bpd": q * 86400,
                       "oil_bpd": r * 86400, "gas_mcfd": 0} for i, (q, r) in enumerate(zip(water, revenue))]
        f["pump"]["max_flow_bpd"] = max(1, sum(water) * 86400 * 2)
        f["pipeline"]["max_bpd"] = f["pump"]["max_flow_bpd"] * 2
        ops["systems"][sid]["facilities"][f["id"]] = {"response_min": response / 60, "cycle_min": cycle / 60}
    ops["prices"] = {"oil_per_bbl": 1, "gas_per_mcf": 0}
    ops["expected_repair_hours"]["pump_failure"] = horizon / 3600
    ops["emergency_trucking"] = {"truck_capacity_bbl": capacity, "cost_per_trip": trip_cost}
    ops["systems"][sid]["emergency_fleet_size"] = fleet
    m = compile_model(network, ops)
    rows = []
    for i, level in enumerate(levels):
        rows += frame(m, 0, index=i, status=1 if level >= 70 else 0, level=level,
                      flow=sum(water) * 86400 if level >= 70 else 0)
        rows += frame(m, 300, index=i, status=2, level=level)
    return m, view(m, rows)


class SearchPolicyTests(unittest.TestCase):
    def test_rules_and_input_bounds_are_explicit(self):
        for kwargs in ({"queue_limit": 0}, {"maximum_jobs": True}, {"retained_evaluations": 1}):
            with self.assertRaises(ValueError): Rules(**kwargs)

    def test_prepared_validator_matches_direct_and_honors_expired_deadline(self):
        m = model(); v = view(m, frame(m, 0, status=0) + frame(m, 300, status=2))
        from test_eog_plan_validator import baseline
        _, p = baseline(m)
        prepared = prepare(m, v)
        self.assertEqual(validate_prepared(prepared, p), validate(m, v, p))
        with self.assertRaises(ValidationUnavailable) as caught:
            validate_prepared(prepared, p, deadline=1, clock=lambda: 1)
        self.assertEqual(caught.exception.code, "validation_time_limit")


class SearchNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("BREAKWATER_EOG_NATIVE")
        if not path: raise unittest.SkipTest("Set BREAKWATER_EOG_NATIVE for actual joint-search numerical qualification.")
        cls.native = NativeEvaluator(path)

    def run_search(self, m, v, **kwargs):
        return search(m, v, self.native, identity=identity, clock_context="snapshot_now",
                      candidate_limit=kwargs.pop("candidate_limit", 1500), time_limit_s=kwargs.pop("time_limit_s", 5), **kwargs)

    def selected(self, result):
        return next(e for e in result.result["evaluations"] if e["plan"]["plan_id"] == result.result["selected_plan_id"])

    def test_single_truck_known_global_minimum(self):
        # No truck costs at least 10 (one bbl/s, 10 bbl headroom, 20 s).
        # Any truck costs >=3; one early full load gives zero loss, hence min=3.
        m, v = fixture()
        result = self.run_search(m, v); chosen = self.selected(result)
        self.assertEqual(chosen["cost"]["total"], 3)
        self.assertEqual(result.validation.record["cost"], chosen["cost"])
        self.assertTrue(result.validation.record["independent"])
        self.assertIn(10, [e["cost"]["total"] for e in result.result["evaluations"]])

    def test_shared_fleet_known_minimum_prefers_high_exposure(self):
        # One truck cannot return within either 20 s repair. Untouched field A
        # loses 10, field B loses 20. Serve B for 3 => global minimum 13.
        m, v = fixture(levels=(80, 80))
        f = m.facilities[1]
        costly = replace(f.wells[0], oil_bpd=172800, revenue_per_hour=7200)
        m = replace(m, facilities=(m.facilities[0], replace(f, wells=(costly,))))
        result = self.run_search(m, v); chosen = self.selected(result)
        self.assertEqual(chosen["cost"]["total"], 13)
        self.assertEqual(chosen["plan"]["truck_jobs"][0]["facility_id"], f.id)
        self.assertEqual(len({j["truck_id"] for j in chosen["plan"]["truck_jobs"]}), 1)

    def test_sequential_yard_jobs_known_global_minimum(self):
        # Two pickups are necessary for cost below 6; two jobs on the same
        # truck at calls 0/7 arrive 2/9, both before their shutdowns. Min=2.
        m, v = fixture(levels=(80, 75), response=2, cycle=5, capacity=20, trip_cost=1)
        result = self.run_search(m, v, candidate_limit=3000)
        chosen = self.selected(result)
        self.assertEqual(chosen["cost"]["total"], 2)
        jobs = chosen["plan"]["truck_jobs"]
        self.assertEqual(len(jobs), 2); self.assertEqual(len({j["truck_id"] for j in jobs}), 1)
        ordered = sorted(jobs, key=lambda j: j["called_elapsed_s"])
        self.assertGreaterEqual(ordered[1]["called_elapsed_s"], ordered[0]["available_again_elapsed_s"])

    def test_mixed_truck_and_delayed_cheap_well_known_minimum(self):
        # With one load, 10 bbl still need curtailing. The cheap well costs
        # 0.1/bbl, so loss>=1 plus trip>=1. The produced candidate attains 2.
        m, v = fixture(water=(1, 1), revenue=(0.1, 10), response=2, capacity=20, trip_cost=1)
        chosen = self.selected(self.run_search(m, v))
        self.assertEqual(chosen["cost"]["total"], 2)
        self.assertTrue(chosen["plan"]["truck_jobs"]); self.assertTrue(chosen["plan"]["well_shut_ins"])
        self.assertGreater(chosen["plan"]["well_shut_ins"][0]["start_elapsed_s"], 0)

    def test_repeat_trip_economic_breakpoint_known_minimum(self):
        # 190 bbl need removing/avoiding. The cheap well can avoid 100 for 10;
        # nine 10-bbl trips cost45. Fewer need expensive curtailment; more cost
        # more than the cheap water saved. Prefix inventory permits min=55.
        m, v = fixture(water=(1, 1), revenue=(0.1, 10), horizon=100, response=2, cycle=5,
                       capacity=10, trip_cost=5)
        chosen = self.selected(self.run_search(m, v, candidate_limit=250))
        self.assertEqual(chosen["cost"]["total"], 55)
        self.assertEqual(sum(len(j["trips"]) for j in chosen["plan"]["truck_jobs"]), 9)

    def test_default_retention_preserves_available_response_family_comparisons(self):
        m, v = fixture(water=(1, 1), revenue=(0.1, 10), response=2, capacity=20, trip_cost=1)
        found = self.run_search(m, v)
        families = {eog_search._family(e) for e in found.result["evaluations"]}
        self.assertEqual(families, {(False, False), (False, True), (True, False), (True, True)})
        self.assertEqual(self.selected(found)["cost"]["total"], 2)
        limited = self.run_search(m, v, rules=Rules(retained_evaluations=2))
        self.assertLessEqual(len(limited.result["evaluations"]), 2)
        self.assertTrue(any(not e["plan"]["truck_jobs"] and not e["plan"]["well_shut_ins"] for e in limited.result["evaluations"]))
        self.assertEqual(self.selected(limited)["cost"]["total"], 2)

    def test_late_first_pickup_requires_real_prearrival_curtailment(self):
        # Without curtailment the tank trips at 5 s, before the first load at8.
        # Avoiding that needs the cheap well stopped by2, costing at least1.8;
        # one trip costs1. Representable controls approach the 2.8 infimum.
        m, v = fixture(water=(1, 1), revenue=(0.1, 10), response=8, capacity=20, trip_cost=1)
        chosen = self.selected(self.run_search(m, v))
        self.assertAlmostEqual(chosen["cost"]["total"], 2.8, places=10)
        self.assertEqual(chosen["outcome"], "avoids_shutdown")
        self.assertEqual(chosen["trajectories"][0]["shutdown_volume_bbl"], 90)
        action, = chosen["plan"]["well_shut_ins"]
        self.assertLess(action["start_elapsed_s"], 2)

    def test_well_only_fractional_knapsack_bound_and_zero_water_economics(self):
        # Exact relaxed minimum: 4*1 + 4*3 + (2/3)*12 =24. It is a
        # lower bound independent of native flags and the generated menus.
        m, v = fixture(water=(1, 2, 3), revenue=(1, 3, 12), levels=(0,), horizon=4, fleet=0, shutdown=10)
        result = self.run_search(m, v)
        lower = Fraction(4) + Fraction(12) + Fraction(2, 3) * 12
        self.assertAlmostEqual(self.selected(result)["cost"]["total"], float(lower), places=10)
        self.assertFalse(self.selected(result)["plan"]["truck_jobs"])
        # Stopping just before the water limit avoids tripping a zero-water,
        # positive-oil well. It must never be silently assigned zero exposure.
        m, v = fixture(water=(1, 0), revenue=(1, 100), fleet=0)
        chosen = self.selected(self.run_search(m, v))
        self.assertAlmostEqual(chosen["cost"]["total"], 10, places=10)
        self.assertEqual(chosen["outcome"], "avoids_shutdown")

    def test_no_action_zero_cost_and_small_budget_are_valid_not_infeasibility(self):
        m, v = fixture(levels=(10,), capacity=50)
        result = self.run_search(m, v)
        self.assertEqual(self.selected(result)["cost"]["total"], 0)
        self.assertFalse(self.selected(result)["plan"]["truck_jobs"])
        m, v = fixture()
        limited = self.run_search(m, v, candidate_limit=1)
        self.assertEqual(limited.result["search"]["evaluated_candidates"], 1)
        self.assertEqual(self.selected(limited)["cost"]["total"], 10)
        self.assertEqual(limited.result["search"]["optimality"], "best_found")
        self.assertFalse(limited.result["search"]["complete"])

    def test_finite_completed_space_and_unknown_arithmetic_do_not_share_claim(self):
        m, _ = fixture(fleet=0)
        rows = frame(m, 0, status=1, flow=86400, level=90) + frame(m, 300, status=2, level=90, well_states=(0,))
        result = self.run_search(m, view(m, rows))
        self.assertTrue(result.result["search"]["complete"])
        self.assertEqual(result.result["search"]["optimality"], "proven_within_space")
        m, v = fixture(fleet=0)
        uncertain = self.run_search(m, v)
        self.assertGreater(uncertain.audit["counts"]["numerical_unavailable"], 0)
        self.assertFalse(uncertain.result["search"]["complete"])

    def test_frontier_output_and_time_bounds_are_explicit(self):
        m, v = fixture()
        result = self.run_search(m, v, rules=Rules(queue_limit=1, state_key_limit=1))
        self.assertGreater(result.audit["counts"]["dropped_states"], 0)
        self.assertFalse(result.result["search"]["complete"])
        ticks = iter(range(1000))
        result = self.run_search(m, v, clock=lambda: next(ticks), time_limit_s=0.1)
        self.assertIsNone(result.result["selected_plan_id"])
        self.assertEqual(result.result["search"]["evaluated_candidates"], 0)
        self.assertEqual(result.result["search"]["optimality"], "unavailable")
        m = model(10); rows = [s for i in range(10) for s in frame(m, 0, index=i, status=0) + frame(m, 300, index=i, status=2)]
        with self.assertRaises(ResponseUnavailable) as caught:
            self.run_search(m, view(m, rows), rules=Rules(retained_bytes=4096))
        self.assertEqual(caught.exception.code, "search_output_limit")

    def test_independent_disagreement_is_not_hidden_by_native_flags(self):
        m, v = fixture()
        actual = eog_search.validate_prepared
        def forged(*args, **kwargs):
            checked = actual(*args, **kwargs)
            checked.record["plan_hash"] = "f" * 64
            return checked
        with patch.object(eog_search, "validate_prepared", forged), self.assertRaises(ResponseUnavailable) as caught:
            self.run_search(m, v)
        self.assertEqual(caught.exception.code, "independent_disagreement")

    def test_candidate_counter_bounds_actual_native_calls(self):
        m, v = fixture()
        with patch.object(self.native, "evaluate", wraps=self.native.evaluate) as invoked:
            result = self.run_search(m, v, candidate_limit=17)
        self.assertEqual(invoked.call_count, result.result["search"]["evaluated_candidates"])
        self.assertLessEqual(invoked.call_count, 17)

    def test_validation_deadline_never_turns_unchecked_candidate_into_selection(self):
        m, v = fixture()
        with patch.object(eog_search, "validate_prepared", side_effect=ValidationUnavailable("validation_time_limit", "Test planning deadline.")):
            result = self.run_search(m, v, candidate_limit=1)
        self.assertIsNone(result.result["selected_plan_id"])
        self.assertTrue(result.result["evaluations"][0]["feasible"])
        self.assertEqual(result.result["search"]["optimality"], "unavailable")
        self.assertFalse(result.result["search"]["complete"])

    def test_causal_reproducibility_and_no_file_access(self):
        m, v = fixture()
        original = self.run_search(m, v)
        # Newer observations exist in the archive but never enter this cutoff.
        rows = frame(m, 0, status=1, flow=86400, level=80) + frame(m, 300, status=2, level=80) + frame(m, 600, status=1, level=0)
        causal = Archive(m.channels(), rows).at(300)
        with patch("builtins.open", side_effect=AssertionError("file access")), patch.object(Path, "open", side_effect=AssertionError("file access")):
            repeated = self.run_search(m, causal)
        self.assertEqual(original.result["selected_plan_id"], repeated.result["selected_plan_id"])
        self.assertEqual(self.selected(original)["cost"], self.selected(repeated)["cost"])
        self.assertLessEqual(len(repeated.result["evaluations"]), Rules().retained_evaluations)


if __name__ == "__main__":
    unittest.main()
