"""Frozen inspection-change families; no official recordings or labels."""
from dataclasses import replace
from fractions import Fraction
import math
from pathlib import Path
import random
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eog_detection import Rules, detect
from eog_model import ModelError, compile_model
from eog_observations import Archive, Policy
from eog_test_fixtures import frame, inputs, model, view


class CumulativeInspectionTests(unittest.TestCase):
    def episodes(self, m, rows, **kwargs):
        return [e for e in detect(m, view(m, rows), **kwargs).events if e.category != "pump_failure"]

    def test_three_percent_step_matches_independent_fraction_score(self):
        m = model()
        for category in ("pipeline_leak", "pump_degradation"):
            with self.subTest(category=category):
                rows = frame(m, 0)
                score = 0.0
                first = None
                for t in range(300, 9000, 300):
                    flow = 2619 if category == "pump_degradation" else 2700
                    received = flow if category == "pump_degradation" else 2619
                    rows += frame(m, t, flow=flow, received=received)
                    increment = float(Fraction.from_float(.03) - Fraction.from_float(.015))
                    score = max(0.0, float(Fraction.from_float(score) + Fraction.from_float(increment)))
                    if first is None and score > .15:
                        first = t
                event, = self.episodes(m, rows)
                self.assertEqual((event.category, event.first_evidence_unix, event.onset_unix, event.detected_unix),
                                 (category, 300, 300, first))
                self.assertIsNone(event.onset_bounds)
                self.assertIsNone(event.countdown_hours)
                self.assertTrue(any("meter bias" in s for s in event.limitations))
                self.assertEqual(self.episodes(m, rows, rules=Rules(inspection_cumulative=False)), [])

    def test_one_or_two_large_outliers_do_not_create_an_episode(self):
        m = model()
        for count in (1, 2):
            rows = sum((frame(m, t, received=0 if 1200 <= t < 1200 + count * 300 else 2700)
                        for t in range(0, 12000, 300)), [])
            self.assertEqual(self.episodes(m, rows), [])

    def test_short_cycles_retain_only_observed_continuity(self):
        m = model()
        rows = sum((frame(m, t, status=1 if t % 1800 < 600 else 0,
                          received=2619 if t % 1800 < 600 else 0) for t in range(0, 24600, 300)), [])
        event, = self.episodes(m, rows)
        self.assertEqual(event.category, "pipeline_leak")
        self.assertTrue(any(s.value == 0 and s.metric == "status" for s in event.evidence))
        self.assertEqual(event.coverage, "ambiguous")  # final sample is stopped

    def test_gap_invalid_stop_and_fault_break_pending_score(self):
        m = model()
        for mode in ("missing", "invalid_stop", "fault", "missing_receive", "unsupported"):
            with self.subTest(mode=mode):
                rows = frame(m, 0)
                for t in range(300, 9300, 300):
                    # Break every sixth frame, before this small residual can confirm.
                    if t % 1800:
                        rows += frame(m, t, received=2619)
                    elif mode == "invalid_stop":
                        rows += frame(m, t, status=0, flow=100)
                    elif mode in ("fault", "unsupported"):
                        rows += frame(m, t, status=2 if mode == "fault" else 7)
                    elif mode == "missing_receive":
                        rows += [s for s in frame(m, t) if s.metric != "receive_flow_bpd"]
                self.assertEqual(self.episodes(m, rows), [])

    def test_fast_acquisitions_cannot_accumulate_score(self):
        m = model()
        rows = sum((frame(m, t, received=2619) for t in range(500)), [])
        self.assertEqual(self.episodes(m, rows), [])

    def test_six_hour_expiry_prevents_cross_cycle_old_score(self):
        m = model()
        rows = []
        # Eight deficient running samples on each side of more than six hours off;
        # individually insufficient, with every intervening acquisition present.
        for t in range(0, 30000, 300):
            running = t < 2400 or 25200 <= t < 27600
            rows += frame(m, t, status=int(running), received=2619 if running else 0)
        self.assertEqual(self.episodes(m, rows), [])

    def test_recovery_requires_running_agreement_and_next_episode_is_fresh(self):
        m = model()
        rows = frame(m, 0)
        rows += sum((frame(m, t, received=2619) for t in range(300, 9300, 300)), [])
        rows += sum((frame(m, t, status=0) for t in range(9300, 12000, 300)), [])
        event, = self.episodes(m, rows)
        self.assertEqual((event.state, event.end_unix), ("ongoing", None))
        rows += sum((frame(m, t) for t in range(12000, 13200, 300)), [])
        event, = self.episodes(m, rows)
        self.assertEqual((event.state, event.end_unix, event.updated_unix), ("resolved", 12000, 12900))
        rows += sum((frame(m, t, received=2619) for t in range(13200, 22200, 300)), [])
        events = self.episodes(m, rows)
        self.assertEqual([e.state for e in events], ["resolved", "ongoing"])
        self.assertGreaterEqual(events[1].first_evidence_unix, 13200)
        self.assertNotEqual(events[0].id, events[1].id)

    def test_future_changes_and_file_access_cannot_change_causal_score(self):
        m = model()
        rows = frame(m, 0) + sum((frame(m, t, received=2619) for t in range(300, 9300, 300)), [])
        a = Archive(m.channels(), rows + frame(m, 9600)).at(9300)
        b = Archive(m.channels(), rows + frame(m, 9600, status=2)).at(9300)
        with patch("builtins.open", side_effect=AssertionError("No files in detector")):
            self.assertEqual(detect(m, a), detect(m, b))
            self.assertEqual(detect(m, a), detect(m, view(m, rows, 9300)))

    def test_original_evidence_is_bounded_and_raw_values_remain_exact(self):
        m = model()
        rows = frame(m, 0) + sum((frame(m, t, received=2619) for t in range(300, 60000, 300)), [])
        event, = self.episodes(m, rows)
        self.assertLessEqual(len(event.evidence), (21600 // 300 + 2) * 3)
        originals = {(s.unix, s.key): s for s in rows}
        self.assertTrue(all(s == originals[s.unix, s.key] for s in event.evidence))
        self.assertEqual(max(s.unix for s in event.evidence), rows[-1].unix)

    def test_persistent_meter_bias_has_an_identical_observation_counterexample(self):
        m = model()
        # A 3% physical pipeline loss with accurate meters and healthy transport
        # with a -3% receiving-meter bias produce the exact same visible sequence.
        physical_loss, biased_meter = frame(m, 0), frame(m, 0)
        for t in range(300, 9300, 300):
            physical_loss += frame(m, t, received=2700 * (1 - .03))
            biased_meter += frame(m, t, received=2700 * .97)
        self.assertEqual(detect(m, view(m, physical_loss)), detect(m, view(m, biased_meter)))
        self.assertTrue(self.episodes(m, physical_loss))

    def test_cumulative_rule_values_and_stricter_gap_policy_are_checked(self):
        for changes in ({"inspection_cumulative": 1}, {"inspection_window_s": 899},
                        {"inspection_window_s": 21601}, {"inspection_drift_fraction": 0},
                        {"inspection_score_threshold": math.inf}, {"inspection_influence_fraction": .01}):
            with self.subTest(changes=changes), self.assertRaises(ModelError):
                Rules(**changes)
        m = model()
        rows = frame(m, 0) + sum((frame(m, t, received=2619) for t in range(300, 9300, 300)), [])
        causal = Archive(m.channels(), rows).at(rows[-1].unix, Policy(max_gap_s=299))
        self.assertEqual(detect(m, causal).events, ())

    def test_predeclared_small_loss_and_short_cycle_family(self):
        self.small_loss_family(2026110804, 32)

    def test_fresh_seed_small_loss_and_short_cycle_family(self):
        self.small_loss_family(2026110806, 64)

    def test_second_fresh_seed_small_loss_and_short_cycle_family(self):
        self.small_loss_family(2026110808, 128)

    def small_loss_family(self, seed, count):
        rng = random.Random(seed)
        for index in range(count):
            n, o = inputs(prefix=f"small-deficiency/{seed}/{index}/\u03b1")
            nominal = rng.uniform(1000, 8000)
            n["systems"][0]["facilities"][0]["pump"]["max_flow_bpd"] = nominal
            n["systems"][0]["facilities"][0]["pipeline"]["max_bpd"] = nominal * 1.2
            m = compile_model(n, o)
            for cycling in (False, True):
                for category in (None, "pipeline_leak", "pump_degradation"):
                    with self.subTest(index=index, cycling=cycling, category=category):
                        rows = []
                        onset = 7200
                        for t in range(0, 36000, 300):
                            running = not cycling or t % 1800 < 600
                            q = nominal * (.97 if t >= onset and category == "pump_degradation" else 1)
                            receipt = q * (.97 if t >= onset and category == "pipeline_leak" else 1)
                            rows += frame(m, t, status=int(running), flow=q * rng.uniform(.99, 1.01) if running else 0,
                                          received=receipt * rng.uniform(.99, 1.01) if running else 0)
                        events = self.episodes(m, rows)
                        if category is None:
                            self.assertEqual(events, [])
                        else:
                            self.assertEqual({e.category for e in events}, {category})
                            self.assertEqual(len(events), 1)
                            self.assertGreaterEqual(events[0].detected_unix, onset)
                            self.assertLessEqual(events[0].detected_unix - onset, 4 * 3600)

    def test_predeclared_healthy_noisy_day_family(self):
        self.healthy_family(2026110805, 32)

    def test_fresh_seed_healthy_noisy_day_family(self):
        self.healthy_family(2026110807, 64)

    def test_second_fresh_seed_healthy_noisy_day_family(self):
        self.healthy_family(2026110809, 128)

    def healthy_family(self, seed, count):
        rng = random.Random(seed)
        for index in range(count):
            with self.subTest(index=index):
                n, o = inputs(prefix=f"healthy-cycling/{seed}/{index}")
                nominal = rng.uniform(1000, 8000)
                n["systems"][0]["facilities"][0]["pump"]["max_flow_bpd"] = nominal
                n["systems"][0]["facilities"][0]["pipeline"]["max_bpd"] = nominal * 1.2
                m = compile_model(n, o)
                rows = []
                for t in range(0, 86400, 300):
                    running = t % 1800 < 600
                    rows += frame(m, t, status=int(running),
                                  flow=nominal * rng.uniform(.975, 1.025) if running else 0,
                                  received=nominal * rng.uniform(.975, 1.025) if running else 0)
                self.assertEqual(self.episodes(m, rows), [])


if __name__ == "__main__":
    unittest.main()
