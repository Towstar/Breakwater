from dataclasses import replace
import math
from pathlib import Path
import random
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eog_detection import Rules, detect, ongoing_alerts
from eog_model import ModelError, compile_model
from eog_observations import Archive, Channel, Policy, Sample
from eog_test_fixtures import frame, inputs, model, view


class EogDetectionTests(unittest.TestCase):
    def active(self, result, category="pump_failure"):
        return [e for e in result.events if e.category == category and e.state == "ongoing"]

    def test_known_failure_countdown_and_revenue(self):
        m = model()
        result = detect(m, view(m, frame(m, 0, status=0) + frame(m, 300, status=2)))
        event, = self.active(result)
        self.assertEqual((event.onset_unix, event.onset_bounds), (300, (0, 300)))
        self.assertAlmostEqual(event.countdown_hours, 4.48)
        self.assertEqual((event.repair_remaining_hours, event.hourly_revenue_exposure), (6, 3300))
        self.assertEqual(event.observed_volume_bbl, 400)
        self.assertEqual(event.coverage, "available")

    def test_historical_gap_forecast_uses_real_sample_age(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2)
        event, = self.active(detect(m, view(m, rows, 450)))
        self.assertAlmostEqual(event.countdown_hours, 4.48 - 150 / 3600)
        self.assertAlmostEqual(event.forecast_volume_at_cutoff_bbl, 400 + 75 * 150 / 3600)
        self.assertEqual(event.observed_volume_bbl, 400)
        self.assertAlmostEqual(event.repair_remaining_hours, 6 - 150 / 3600)
        self.assertTrue(any("actual acquisition" in a for a in event.assumptions))

    def test_predicted_crossing_between_samples_is_not_observed_trip(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2, level=91.9)
        event, = self.active(detect(m, view(m, rows, 450)))
        self.assertEqual((event.countdown_hours, event.countdown_state), (0, "predicted_reached_since_sample"))
        self.assertEqual(event.forecast_volume_at_cutoff_bbl, 736)
        self.assertLess(event.observed_volume_bbl, 736)

    def test_already_shut_in_does_not_erase_revenue(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 900, status=2, level=92, well_states=(0, 0, 0))
        event, = self.active(detect(m, view(m, rows)))
        self.assertEqual((event.countdown_state, event.countdown_hours), ("already_shutdown", 0))
        self.assertEqual(event.hourly_revenue_exposure, 3300)
        self.assertEqual(event.assumed_inflow_bpd, 0)
        self.assertTrue(any(s.unix == 0 and s.metric == "status" for s in event.evidence))

    def test_left_censored_fault_keeps_horizon_unknown(self):
        m = model()
        event, = self.active(detect(m, view(m, frame(m, 0, status=2))))
        self.assertIsNone(event.onset_unix)
        self.assertIsNone(event.repair_remaining_hours)
        self.assertAlmostEqual(event.countdown_hours, 4.48)
        self.assertEqual(event.recommended_action, "unavailable")

    def test_left_censored_already_shut_in_never_invents_zero_exposure(self):
        m = model()
        event, = self.active(detect(m, view(m, frame(m, 0, status=2, level=92, well_states=(0, 0, 0)))))
        self.assertIsNone(event.hourly_revenue_exposure)
        self.assertIsNone(event.repair_remaining_hours)
        self.assertEqual(event.countdown_state, "already_shutdown")

    def test_preexisting_inactive_well_has_no_opportunity_cost(self):
        m = model()
        rows = frame(m, 0, status=0, well_states=(1, 0, 1)) + frame(m, 300, status=2, well_states=(1, 0, 1))
        event, = self.active(detect(m, view(m, rows)))
        self.assertEqual(event.hourly_revenue_exposure, 2800)
        self.assertEqual((event.onset_unix, event.repair_remaining_hours), (300, 6))
        self.assertAlmostEqual(event.countdown_hours, 336 / ((450 + 650) / 24))

    def test_expired_configured_repair_does_not_resolve_persistent_fault(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 21900, status=2, level=92, well_states=(0, 0, 0))
        event, = self.active(detect(m, view(m, rows)))
        self.assertIsNone(event.repair_remaining_hours)
        self.assertEqual(event.state, "ongoing")
        self.assertEqual(event.hourly_revenue_exposure, 3300)

    def test_resolution_requires_observed_recovery_not_future_label(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 900, status=1)
        prefix_result = detect(m, Archive(m.channels(), rows).at(600))
        self.assertEqual(len(self.active(prefix_result)), 1)
        result = detect(m, view(m, rows))
        event, = [e for e in result.events if e.category == "pump_failure"]
        self.assertEqual((event.state, event.end_unix), ("resolved", 900))
        self.assertIsNone(event.countdown_hours)

    def test_repeated_failures_get_distinct_stable_episodes(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 600, status=1) + frame(m, 900, status=2)
        result = detect(m, view(m, rows))
        events = [e for e in result.events if e.category == "pump_failure"]
        self.assertEqual([e.state for e in events], ["resolved", "ongoing"])
        self.assertNotEqual(events[0].id, events[1].id)

    def test_fault_feedback_with_positive_delivery_is_ambiguous(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2, flow=1000)
        event, = self.active(detect(m, view(m, rows)))
        self.assertEqual(event.coverage, "ambiguous")
        self.assertIsNone(event.countdown_hours)
        self.assertTrue(any(s.metric == "flow_bpd" and s.value == 1000 for s in event.evidence))

    def test_finite_invalid_volume_is_retained_not_clipped(self):
        m = model()
        for level in (-2, 101):
            with self.subTest(level=level):
                rows = frame(m, 0, status=0) + frame(m, 300, status=2, level=level)
                event, = self.active(detect(m, view(m, rows)))
                self.assertEqual(event.coverage, "ambiguous")
                self.assertIsNone(event.countdown_hours)
                self.assertTrue(any(s.metric == "level_pct" and s.value == level for s in event.evidence))

    def test_extreme_finite_raw_level_does_not_create_infinite_derived_inventory(self):
        n, o = inputs()
        n["systems"][0]["facilities"][0]["tank"]["capacity_bbl"] = 1e308
        m = compile_model(n, o)
        event, = self.active(detect(m, view(m, frame(m, 0, status=0) + frame(m, 300, status=2, level=1e308))))
        self.assertEqual(event.coverage, "ambiguous")
        self.assertIsNone(event.observed_volume_bbl)
        self.assertTrue(any(s.metric == "level_pct" and s.value == 1e308 for s in event.evidence))

    def test_known_preexisting_all_off_wells_keep_real_zero_exposure(self):
        m = model()
        rows = frame(m, 0, status=0, well_states=(0, 0, 0)) + frame(m, 300, status=2, well_states=(0, 0, 0))
        event, = self.active(detect(m, view(m, rows)))
        self.assertEqual(event.hourly_revenue_exposure, 0)
        self.assertEqual(event.countdown_state, "no_future_crossing")
        self.assertEqual(event.repair_remaining_hours, 6)

    def test_stale_or_missing_evidence_does_not_clear_fault(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2)
        event, = self.active(detect(m, view(m, rows, 601)))
        self.assertEqual(event.coverage, "unavailable")
        self.assertIsNone(event.countdown_hours)
        f = m.facilities[0]
        rows = [s for s in rows if not (s.unix == 300 and s.key == (f.id, "inflow_bpd"))]
        event, = self.active(detect(m, view(m, rows)))
        self.assertEqual(event.coverage, "unavailable")

    def test_unbalanced_well_inflow_abstains(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2, inflow=500)
        event, = self.active(detect(m, view(m, rows)))
        self.assertEqual(event.coverage, "ambiguous")
        self.assertIsNone(event.countdown_hours)

    def test_zero_inflow_producing_state_has_no_future_crossing(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2, water=(0, 0, 0))
        event, = self.active(detect(m, view(m, rows)))
        self.assertEqual(event.countdown_state, "no_future_crossing")
        self.assertIsNone(event.countdown_hours)

    def test_normal_sawtooth_and_off_states_do_not_create_events(self):
        m = model()
        level, running, rows = 50.0, False, []
        for t in range(0, 90000, 300):
            if level >= 70:
                running = True
            elif level <= 30:
                running = False
            rows += frame(m, t, status=int(running), level=level)
            level += ((1800 - (2700 if running else 0)) / 24) * (300 / 3600) / 800 * 100
        self.assertEqual(detect(m, view(m, rows)).events, ())

    def test_sustained_leak_has_inspection_and_no_countdown(self):
        m = model()
        rows = sum((frame(m, t, received=2700 if t < 1200 else 1800) for t in range(0, 2400, 300)), [])
        event, = self.active(detect(m, view(m, rows)), "pipeline_leak")
        self.assertEqual((event.onset_unix, event.onset_bounds), (1200, (900, 1200)))
        self.assertEqual(event.detected_unix, 2100)
        self.assertIsNone(event.countdown_hours)
        self.assertEqual(event.recommended_action, "field_inspection")

    def test_one_bad_frame_does_not_create_inspection_event(self):
        m = model()
        rows = sum((frame(m, t, received=1800 if t == 1200 else 2700) for t in range(0, 3000, 300)), [])
        self.assertEqual(detect(m, view(m, rows)).events, ())

    def test_leak_and_degradation_can_coexist(self):
        m = model()
        rows = sum((frame(m, t, flow=2700 if t < 1200 else 2000, received=2700 if t < 1200 else 1000)
                    for t in range(0, 2400, 300)), [])
        result = detect(m, view(m, rows))
        self.assertEqual({e.category for e in result.events}, {"pipeline_leak", "pump_degradation"})
        self.assertTrue(all(e.countdown_hours is None for e in result.events))

    def test_degradation_followed_by_failure_stays_separate_and_masked(self):
        m = model()
        rows = sum((frame(m, t, flow=2700 if t < 1200 else 2000) for t in range(0, 2400, 300)), [])
        rows += frame(m, 2400, status=2)
        result = detect(m, view(m, rows))
        degradation, = self.active(result, "pump_degradation")
        failure, = self.active(result)
        self.assertEqual(degradation.recommended_action, "unavailable")
        self.assertEqual(degradation.coverage, "ambiguous")
        self.assertEqual(failure.onset_unix, 2400)
        self.assertAlmostEqual(failure.countdown_hours, 4.48)

    def test_off_period_does_not_resolve_leak(self):
        m = model()
        rows = sum((frame(m, t, received=1800 if t >= 1200 else 2700) for t in range(0, 2400, 300)), [])
        rows += frame(m, 2400, status=0)
        event, = self.active(detect(m, view(m, rows)), "pipeline_leak")
        self.assertEqual(event.coverage, "ambiguous")
        self.assertIsNone(event.end_unix)

    def test_sustained_observed_recovery_closes_inspection_episode(self):
        m = model()
        rows = sum((frame(m, t, received=1800 if 1200 <= t < 2400 else 2700) for t in range(0, 3600, 300)), [])
        event, = [e for e in detect(m, view(m, rows)).events if e.category == "pipeline_leak"]
        self.assertEqual((event.state, event.end_unix, event.updated_unix), ("resolved", 2400, 3300))

    def test_sampling_gap_breaks_confirmation_not_lifecycle(self):
        m = model()
        rows = sum((frame(m, t, received=1800) for t in (0, 300, 900, 1200)), [])
        self.assertEqual(detect(m, view(m, rows)).events, ())

    def test_rapid_samples_cannot_fake_sustained_five_minute_evidence(self):
        m = model()
        rows = sum((frame(m, t, received=1800) for t in (0, 1, 2, 3)), [])
        result = detect(m, view(m, rows))
        self.assertEqual(result.events, ())
        self.assertNotEqual(next(c for c in result.coverage if c.category == "pipeline_leak").state, "available")

    def test_excessive_running_flow_cannot_resolve_a_fault(self):
        m = model()
        rows = frame(m, 0, status=0) + frame(m, 300, status=2) + frame(m, 600, status=1, flow=10000)
        event, = self.active(detect(m, view(m, rows)))
        self.assertEqual(event.coverage, "ambiguous")

    def test_onset_bound_retains_the_actual_preceding_normal_pump_evidence(self):
        m = model()
        event, = self.active(detect(m, view(m, frame(m, 0, status=0) + frame(m, 300, status=2))))
        pump = m.facilities[0].pump_id
        self.assertTrue(any(s.unix == 0 and s.asset_id == pump and s.metric == "status" and s.value == 0
                            for s in event.evidence))

    def test_exact_leak_threshold_and_one_ulp_neighbors(self):
        m = model()
        for received, expected in ((2430, False), (math.nextafter(2430, -math.inf), True),
                                   (math.nextafter(2430, math.inf), False)):
            with self.subTest(received=received):
                rows = frame(m, 0)
                rows += sum((frame(m, t, received=received) for t in range(300, 1500, 300)), [])
                self.assertEqual(bool(self.active(detect(m, view(m, rows)), "pipeline_leak")), expected)

    def test_pressure_is_optional_evidence_not_invented_hydraulics(self):
        m = model()
        rows = sum((frame(m, t, received=1800) for t in range(0, 1500, 300)), [])
        rows = [s for s in rows if s.metric not in ("pressure_psi", "discharge_psi")]
        event, = self.active(detect(m, view(m, rows)), "pipeline_leak")
        self.assertEqual(event.recommended_action, "field_inspection")

    def test_wrong_units_or_status_domains_are_rejected(self):
        m = model()
        catalog = list(m.channels())
        catalog[0] = replace(catalog[0], unit="m3/h")
        with self.assertRaises(ModelError) as caught:
            detect(m, Archive(catalog, frame(m, 0)).at(0))
        self.assertEqual(caught.exception.code, "unit_mismatch")

    def test_unknown_status_never_means_healthy(self):
        m = model()
        result = detect(m, view(m, frame(m, 0, status=7)))
        self.assertEqual(result.events, ())
        self.assertTrue(all(c.state == "unavailable" for c in result.coverage))

    def test_future_values_cannot_change_any_current_event(self):
        m = model()
        prefix = frame(m, 0, status=0) + frame(m, 300, status=2)
        a = detect(m, Archive(m.channels(), prefix + frame(m, 600, status=1)).at(450))
        b = detect(m, Archive(m.channels(), prefix + frame(m, 600, status=2, level=92, well_states=(0, 0, 0))).at(450))
        c = detect(m, view(m, prefix, 450))
        self.assertEqual(a, b)
        self.assertEqual(a, c)

    def test_unfamiliar_changed_network_uses_actual_capacity_economics_and_ids(self):
        n, o = inputs(prefix="new\U0001f30a-network")
        n["systems"][0]["facilities"][0]["tank"]["capacity_bbl"] = 1200
        o["prices"] = {"oil_per_bbl": 30, "gas_per_mcf": 6}
        m = compile_model(n, o)
        event, = self.active(detect(m, view(m, frame(m, 0, status=0) + frame(m, 300, status=2))))
        self.assertAlmostEqual(event.countdown_hours, (1104 - 600) / 75)
        self.assertEqual(event.hourly_revenue_exposure, (1120 * 30 + 4000 * 6) / 24)
        self.assertTrue(event.asset_id.startswith("new\U0001f30a-network"))

    def test_all_system_urgency_uses_countdown_then_exposure(self):
        m = model(3)
        rows = []
        for index, level in enumerate((50, 75, 75)):
            rows += frame(m, 0, index=index, status=0, level=40)
            rows += frame(m, 300, index=index, status=2, level=level)
        result = detect(m, view(m, rows))
        active = list(ongoing_alerts(result))
        self.assertGreater(active[-1].countdown_hours, active[0].countdown_hours)
        higher = replace(active[1], hourly_revenue_exposure=10000)
        altered = replace(result, events=(active[0], higher, active[-1]))
        self.assertEqual(ongoing_alerts(altered)[0].hourly_revenue_exposure, 10000)

    def test_rules_and_event_budget_are_explicit(self):
        for args in ({"inspection_samples": 1}, {"leak_fraction": 0}, {"zero_flow_bpd": math.nan}, {"max_events": 0}):
            with self.assertRaises(ModelError):
                Rules(**args)
        m = model(2)
        rows = frame(m, 0, index=0, status=2) + frame(m, 0, index=1, status=2)
        with self.assertRaises(ModelError) as caught:
            detect(m, view(m, rows), Rules(max_events=1))
        self.assertEqual(caught.exception.code, "limit_exceeded")

    def test_retained_inspection_evidence_is_bounded_and_current(self):
        m = model()
        rows = sum((frame(m, t, received=1800 if t >= 1200 else 2700) for t in range(0, 60000, 300)), [])
        event, = self.active(detect(m, view(m, rows)), "pipeline_leak")
        self.assertLessEqual(len(event.evidence), 15)
        self.assertEqual(max(s.unix for s in event.evidence), rows[-1].unix)

    def test_detector_does_not_open_files_or_read_labels(self):
        m = model()
        snapshot = view(m, frame(m, 0, status=0) + frame(m, 300, status=2))
        with patch("builtins.open", side_effect=AssertionError("No files allowed")):
            self.assertEqual(len(self.active(detect(m, snapshot))), 1)

    def test_generated_countdown_oracle_and_prefix_invariance(self):
        rng = random.Random(2026110802)
        for _ in range(60):
            n, o = inputs(prefix=f"network-{rng.randrange(1000000)}")
            capacity = rng.uniform(500, 2000)
            n["systems"][0]["facilities"][0]["tank"]["capacity_bbl"] = capacity
            m = compile_model(n, o)
            level = rng.uniform(30, 80)
            age = rng.randrange(0, 301)
            prefix = frame(m, 0, status=0, level=30) + frame(m, 300, status=2, level=level)
            full = Archive(m.channels(), prefix + frame(m, 900, status=1)).at(300 + age)
            result = detect(m, full)
            event, = self.active(result)
            expected = ((92 - level) * capacity / 100) / 75 - age / 3600
            self.assertAlmostEqual(event.countdown_hours, expected, places=11)
            self.assertEqual(result, detect(m, view(m, prefix, 300 + age)))


if __name__ == "__main__":
    unittest.main()
