"""Causal evidence adversaries; generated inputs only, no labels or runtime ML."""
from dataclasses import FrozenInstanceError
import math
from pathlib import Path
import random
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from eog_observations import Archive, Channel, ObservationError, Policy, Sample, View


FLOW = Channel("pump/renamed-\u03b1", "flow_bpd", "bpd")
STATUS = Channel(FLOW.asset_id, "status", "state", (0, 1, 2))
TANK = Channel("tank:custom", "level_pct", "%")
CATALOG = (FLOW, STATUS, TANK)


def row(t, channel=FLOW, value=0):
    return Sample(t, channel.asset_id, channel.metric, value)


class CausalObservationTests(unittest.TestCase):
    def reject(self, code, call):
        with self.assertRaises(ObservationError) as caught:
            call()
        self.assertEqual(caught.exception.code, code)

    def test_dataset_clock_is_actual_sample_extrema(self):
        archive = Archive(CATALOG, [row(100), row(400)])
        self.assertEqual((archive.first_unix, archive.now_unix, archive.sample_count), (100, 400, 2))
        for t in (99, 401):
            self.reject("invalid_cutoff", lambda: archive.at(t))

    def test_query_at_cutoff_never_uses_next_acquisition(self):
        archive = Archive(CATALOG, [row(0, value=10), row(300, value=20), row(600, value=999)])
        view = archive.at(450)
        self.assertEqual(view.latest(*FLOW.key).sample, row(300, value=20))
        self.assertEqual(view.series(*FLOW.key), (row(0, value=10), row(300, value=20)))

    def test_missing_is_not_zero_or_healthy(self):
        reading = Archive(CATALOG, [row(0)]).at(0).latest(*STATUS.key)
        self.assertEqual((reading.sample, reading.age_s, reading.state), (None, None, "missing"))

    def test_future_only_channel_is_missing_in_every_causal_query(self):
        view = Archive(CATALOG, [row(0), row(600, TANK, 90)]).at(300)
        self.assertEqual(view.latest(*TANK.key).state, "missing")
        self.assertEqual(view.series(*TANK.key), ())
        self.assertEqual(view.window(*TANK.key, 0).state, "missing")

    def test_freshness_boundary_is_inclusive_without_epsilon(self):
        archive = Archive(CATALOG, [row(0), row(601, TANK, 40)])
        for cutoff, state in ((300, "available"), (301, "stale")):
            reading = archive.at(cutoff).latest(*FLOW.key)
            self.assertEqual((reading.state, reading.age_s), (state, cutoff))
            self.assertEqual(reading.sample, row(0))

    def test_stale_raw_evidence_remains_visible(self):
        archive = Archive(CATALOG, [row(0, value=-7), row(600, TANK, 104)])
        reading = archive.at(600).latest(*FLOW.key)
        self.assertEqual((reading.state, reading.sample.value), ("stale", -7))
        self.assertEqual(archive.at(600).latest(*TANK.key).sample.value, 104)

    def test_unusual_status_is_evidence_with_unavailable_interpretation(self):
        for value in (-1, 0.5, 3):
            with self.subTest(value=value):
                view = Archive(CATALOG, [row(0, STATUS, value)]).at(0)
                reading = view.latest(*STATUS.key)
                self.assertEqual((reading.sample.value, reading.state), (value, "unsupported_status"))
        for value in (0, 1, 2):
            self.assertEqual(Archive(CATALOG, [row(0, STATUS, value)]).at(0).latest(*STATUS.key).state, "available")

    def test_no_numeric_string_bool_nonfinite_or_overflow_coercion(self):
        for value in ("0", True, None, math.nan, math.inf, -math.inf, 10 ** 1000):
            with self.subTest(type=type(value).__name__):
                self.reject("invalid_value", lambda: row(0, value=value))

    def test_timestamp_is_exact_integer_and_bounded(self):
        for value in (True, 0.0, 0.5, -1, 253402300800):
            self.reject("invalid_parameter", lambda: row(value))
        self.assertEqual(row(253402300799).unix, 253402300799)

    def test_full_id_and_unicode_are_retained(self):
        channel = Channel("  \U0001f30a custom / tank:1  ", "level_pct", "%")
        sample = row(0, channel, -0.0)
        result = Archive([channel], [sample]).at(0).latest(*channel.key)
        self.assertEqual(result.channel.asset_id, channel.asset_id)
        self.assertEqual(math.copysign(1, result.sample.value), -1)

    def test_id_length_uses_utf16_units_and_rejects_surrogates(self):
        Channel("\U0001f30a" * 128, "metric", "bpd")
        self.reject("limit_exceeded", lambda: Channel("\U0001f30a" * 129, "metric", "bpd"))
        for value in ("", "a\x00b", "\ud800", 5):
            self.reject("invalid_identity", lambda: Channel(value, "metric", "bpd"))

    def test_duplicate_acquisition_and_channel_catalog_are_rejected(self):
        self.reject("duplicate_sample", lambda: Archive(CATALOG, [row(0), row(0, TANK), row(0)]))
        self.reject("duplicate_channel", lambda: Archive([FLOW, FLOW], [row(0)]))

    def test_unordered_and_unknown_rows_are_explicit_errors(self):
        self.reject("unordered_samples", lambda: Archive(CATALOG, [row(300), row(0)]))
        alien = Channel("another", "flow_bpd", "bpd")
        self.reject("unknown_channel", lambda: Archive(CATALOG, [row(0, alien)]))
        self.reject("unknown_channel", lambda: Archive(CATALOG, [row(0)]).at(0).latest(*alien.key))

    def test_archive_and_returned_evidence_are_immutable(self):
        rows, channels = [row(0)], list(CATALOG)
        archive = Archive(channels, rows)
        rows.append(row(300))
        channels.clear()
        self.assertEqual(archive.sample_count, 1)
        with self.assertRaises(FrozenInstanceError):
            archive.now_unix = 5
        with self.assertRaises(TypeError):
            archive._series[FLOW.key] = ()
        with self.assertRaises(FrozenInstanceError):
            archive.at(0).latest(*FLOW.key).sample.value = 4

    def test_empty_input_and_catalog_do_not_create_fake_window(self):
        self.reject("empty_input", lambda: Archive(CATALOG, []))
        self.reject("invalid_parameter", lambda: Archive([], [row(0)]))

    def test_work_limits_consume_only_one_excess_input(self):
        consumed = []
        def source():
            for t in range(100):
                consumed.append(t)
                yield row(t)
        self.reject("limit_exceeded", lambda: Archive(CATALOG, source(), max_samples=2))
        self.assertEqual(consumed, [0, 1, 2])
        self.reject("limit_exceeded", lambda: Archive(CATALOG, [row(0)], max_channels=2))

    def test_queries_do_not_silently_decimate_evidence(self):
        view = Archive(CATALOG, [row(t) for t in range(4)]).at(3)
        self.reject("limit_exceeded", lambda: view.series(*FLOW.key, max_points=3))
        self.assertEqual([r.unix for r in view.series(*FLOW.key, start_unix=2, max_points=2)], [2, 3])

    def test_cohort_requires_matching_acquisition_by_default(self):
        view = Archive(CATALOG, [row(0), row(300, TANK, 40)]).at(300)
        cohort = view.cohort([FLOW.key, TANK.key])
        self.assertEqual(cohort.state, "incoherent")
        self.assertEqual([r.state for r in cohort.readings], ["available", "available"])

    def test_explicit_skew_policy_can_allow_prior_frame(self):
        archive = Archive(CATALOG, [row(0), row(300, TANK, 40)])
        cohort = archive.at(300, Policy(max_skew_s=300)).cohort([FLOW.key, TANK.key])
        self.assertEqual(cohort.state, "available")
        cohort = archive.at(300, Policy(max_skew_s=299)).cohort([FLOW.key, TANK.key])
        self.assertEqual(cohort.state, "incoherent")

    def test_cohort_retains_every_unavailable_reason(self):
        archive = Archive(CATALOG, [row(0), row(600, TANK, 40)])
        cohort = archive.at(600).cohort([FLOW.key, STATUS.key, TANK.key])
        self.assertEqual(cohort.state, "unavailable")
        self.assertEqual([r.state for r in cohort.readings], ["stale", "missing", "available"])

    def test_invalid_cohort_keys_and_policy_do_not_get_defaults(self):
        view = Archive(CATALOG, [row(0)]).at(0)
        self.reject("invalid_parameter", lambda: view.cohort([]))
        self.reject("duplicate_channel", lambda: view.cohort([FLOW.key, FLOW.key]))
        self.reject("invalid_parameter", lambda: view.cohort(["pump"]))
        for options in ({"max_age_s": -1}, {"max_gap_s": 0}, {"max_skew_s": True}):
            self.reject("invalid_parameter", lambda: Policy(**options))

    def test_window_exposes_internal_gap_despite_fresh_latest(self):
        view = Archive(CATALOG, [row(0), row(600), row(900)]).at(900)
        self.assertEqual(view.latest(*FLOW.key).state, "available")
        self.assertEqual(view.window(*FLOW.key, 0).state, "gap")
        self.assertEqual(view.window(*FLOW.key, 600).state, "available")

    def test_window_keeps_predecessor_and_checks_boundary_gap(self):
        view = Archive(CATALOG, [row(0), row(600)]).at(600)
        window = view.window(*FLOW.key, 599)
        self.assertEqual((window.state, [s.unix for s in window.samples]), ("gap", [0, 600]))

    def test_window_tail_respects_gap_policy_even_when_fresh(self):
        view = Archive(CATALOG, [row(0), row(200, TANK)]).at(200, Policy(max_gap_s=100))
        self.assertEqual(view.latest(*FLOW.key).state, "available")
        self.assertEqual(view.window(*FLOW.key, 0).state, "gap")

    def test_status_catalog_is_bounded_and_cannot_contain_arbitrary_numbers(self):
        for codes in ((-1,), (256,), (True,), (1, 1), tuple(range(17)), (), [0, 1]):
            self.reject("invalid_parameter", lambda: Channel("pump", "status", "state", codes))

    def test_window_distinguishes_missing_history_and_stale(self):
        view = Archive(CATALOG, [row(0, TANK), row(300), row(900, TANK)]).at(900)
        self.assertEqual(view.window(*FLOW.key, 0).state, "insufficient_history")
        self.assertEqual(view.window(*FLOW.key, 300).state, "stale")
        self.assertEqual(view.window(*STATUS.key, 0).state, "missing")

    def test_window_preserves_unsupported_earlier_status(self):
        view = Archive(CATALOG, [row(0, STATUS, 9), row(300, STATUS, 1)]).at(300)
        self.assertEqual(view.latest(*STATUS.key).state, "available")
        self.assertEqual(view.window(*STATUS.key, 0).state, "unsupported_status")

    def test_window_and_series_validate_bounds(self):
        view = Archive(CATALOG, [row(0), row(300)]).at(300)
        for start in (-1, 301, True):
            self.reject("invalid_parameter", lambda: view.window(*FLOW.key, start))
            self.reject("invalid_parameter", lambda: view.series(*FLOW.key, start_unix=start))
        self.reject("limit_exceeded", lambda: view.window(*FLOW.key, 0, max_points=1))

    def test_evidence_hash_excludes_future_and_is_order_independent_at_same_time(self):
        prefix = [row(0), row(0, TANK, 10), row(300)]
        a = Archive(CATALOG, prefix + [row(600, value=100)]).at(300)
        b = Archive(CATALOG, prefix + [row(600, value=-100), row(900)]).at(300)
        c = Archive(reversed(CATALOG), [prefix[1], prefix[0], prefix[2]]).at(300)
        self.assertEqual(a.evidence_fingerprint(), b.evidence_fingerprint())
        self.assertEqual(a.evidence_fingerprint(), c.evidence_fingerprint())

    def test_evidence_hash_binds_cutoff_values_units_status_domain_and_policy(self):
        base = Archive([FLOW], [row(0), row(300)])
        identity = base.at(300).evidence_fingerprint()
        alternatives = [
            base.at(299).evidence_fingerprint(),
            base.at(300, Policy(max_skew_s=1)).evidence_fingerprint(),
            Archive([FLOW], [row(0, value=1), row(300)]).at(300).evidence_fingerprint(),
            Archive([Channel(FLOW.asset_id, FLOW.metric, "bbl/h")], [row(0), row(300)]).at(300).evidence_fingerprint(),
            Archive([Channel(FLOW.asset_id, FLOW.metric, "bpd", (0, 1))], [row(0), row(300)]).at(300).evidence_fingerprint(),
        ]
        self.assertTrue(all(other != identity for other in alternatives))

    def test_randomized_views_match_independent_prefix_filter(self):
        rng = random.Random(20261108)
        rows = [row(t, channel, rng.uniform(-100, 100))
                for t in range(0, 6001, 300) for channel in (FLOW, TANK)]
        archive = Archive(CATALOG, rows)
        for cutoff in [0, 6000] + [rng.randrange(6001) for _ in range(100)]:
            view = archive.at(cutoff)
            prefix = [s for s in rows if s.unix <= cutoff]
            for channel in (FLOW, TANK):
                expected = tuple(s for s in prefix if s.key == channel.key)
                self.assertEqual(view.series(*channel.key), expected)
                self.assertEqual(view.latest(*channel.key).sample, expected[-1])
                self.assertTrue(all(s.unix <= cutoff for s in view.window(*channel.key, 0).samples))

    def test_all_queries_need_no_files_or_model_packages(self):
        with patch("builtins.open", side_effect=AssertionError("No input file is permitted")):
            view = Archive(CATALOG, [row(0), row(300)]).at(300)
            view.latest(*FLOW.key)
            view.series(*FLOW.key)
            view.window(*FLOW.key, 0)
            view.cohort([FLOW.key, TANK.key])
            view.evidence_fingerprint()

    def test_direct_view_construction_cannot_bypass_cutoff(self):
        archive = Archive(CATALOG, [row(0), row(300)])
        self.reject("invalid_cutoff", lambda: View(archive, 301, Policy()))
        self.reject("invalid_parameter", lambda: View(archive, 0, None))


if __name__ == "__main__":
    unittest.main()
