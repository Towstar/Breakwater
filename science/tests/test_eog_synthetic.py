from dataclasses import asdict, replace
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eog_detection import detect, ongoing_alerts
from eog_model import compile_model
from eog_observations import View
from eog_synthetic import (Experiment, Failure, Initial, Noise, RateChange, SyntheticError,
                           SyntheticStream, compile_experiment, seeded_experiment)
from eog_test_fixtures import inputs, model
from native_eog_plant import NativePlant


def experiment(m, *, duration=1800, failures=(), noise=Noise(), initial_volume=400):
    return Experiment(1791762900, duration, 431, tuple(Initial(f.id, initial_volume, False, tuple(w.id for w in f.wells)) for f in m.facilities), failures, noise=noise)


class SyntheticCompilationTests(unittest.TestCase):
    def test_imported_parameters_opaque_ids_and_explicit_initial_conditions(self):
        m = model(2, prefix="unknown/\u03b2")
        config = experiment(m)
        compiled = compile_experiment(m, config); raw = compiled.native_input()
        self.assertEqual(raw["fleet_sizes"], [m.systems[0].fleet_size])
        self.assertEqual(raw["cost_per_trip"], m.cost_per_trip)
        self.assertEqual(raw["oil_per_bbl"], m.oil_per_bbl)
        for f, n in zip(m.facilities, raw["facilities"]):
            self.assertEqual(n["shutdown_bbl"], f.shutdown_pct / 100 * f.capacity_bbl)
            self.assertEqual(n["pump_bpd"], f.pump_flow_bpd)
            self.assertEqual(n["response_min"], f.response_min)
            self.assertEqual(n["well_count"], len(f.wells))
        with self.assertRaises(SyntheticError): compile_experiment(m, replace(config, initial=config.initial[:1]))
        with self.assertRaises(SyntheticError): compile_experiment(m, replace(config, initial=(config.initial[0],) * 2))
        with self.assertRaises(TypeError): compiled.facility_indices["invented"] = 0
        raw["facilities"][0]["initial_bbl"] = -1
        self.assertEqual(compiled.native_input()["facilities"][0]["initial_bbl"], 400)

    def test_known_model_repair_duration_and_no_hidden_initial_productivity(self):
        m = model(); field = m.facilities[0]
        config = experiment(m, failures=(Failure(field.id, 600.125),))
        compiled = compile_experiment(m, config)
        self.assertEqual(compiled.native_input()["failures"][0]["repair_s"], m.repair_hours * 3600)
        for invalid in (replace(config.initial[0], productive_well_ids=("foreign",)),
                        replace(config.initial[0], volume_bbl=-1), replace(config.initial[0], pump_running=1)):
            with self.assertRaises(SyntheticError): compile_experiment(m, replace(config, initial=(invalid,)))

    def test_invalid_numeric_schedule_and_bounded_complete_output(self):
        m = model(); config = experiment(m)
        for action in (lambda: replace(config, seed=True), lambda: Noise(rate_relative_amplitude=float("nan")),
                       lambda: replace(config, initial=list(config.initial)), lambda: Noise(dropout_every=1),
                       lambda: compile_experiment(m, replace(config, sample_limit=13)),
                       lambda: compile_experiment(m, replace(config, failures=(Failure("foreign", 1),))),
                       lambda: compile_experiment(m, replace(config, failures=(Failure(m.facilities[0].id, 1), Failure(m.facilities[0].id, 2))))):
            with self.subTest(action=action), self.assertRaises(SyntheticError): action()

    def test_seeded_receipt_repeatability_and_nonhardcoded_networks(self):
        m = model(3, prefix="generated:42")
        kwargs = {"start_unix": 1791762900, "duration_s": 86400, "seed": 912, "failure_count": 4, "rate_change_count": 5}
        a, b = seeded_experiment(m, **kwargs), seeded_experiment(m, **kwargs)
        self.assertEqual(a, b)
        self.assertEqual(compile_experiment(m, a).fingerprint, compile_experiment(m, b).fingerprint)
        self.assertEqual(len(a.failures), 4)
        self.assertTrue(all(f.facility_id in {x.id for x in m.facilities} for f in a.failures))
        self.assertNotEqual(a, seeded_experiment(m, **{**kwargs, "seed": 913}))

    def test_unobservable_future_economics_are_not_silently_passed_to_analysis(self):
        m = model(); well = m.facilities[0].wells[0]
        altered = replace(experiment(m), rate_changes=(RateChange(well.id, 300.25, well.baseline_water_bpd, well.oil_bpd + 1, well.gas_mcfd),))
        with self.assertRaises(SyntheticError) as caught: compile_experiment(m, altered)
        self.assertEqual(caught.exception.code, "unobservable_economics")


class SyntheticExecutionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = os.environ.get("BREAKWATER_EOG_PLANT")
        if not cls.library: raise unittest.SkipTest("Set BREAKWATER_EOG_PLANT for actual imported-model synthetic observations.")

    def test_future_truth_cannot_change_an_earlier_observation_or_detection(self):
        m = model(); fid = m.facilities[0].id
        a = experiment(m, failures=(Failure(fid, 600.125),))
        b = replace(a, failures=(Failure(fid, 1200.125),))
        ca, cb = compile_experiment(m, a), compile_experiment(m, b)
        self.assertNotEqual(ca.fingerprint, cb.fingerprint)
        with SyntheticStream(self.library, ca) as left, SyntheticStream(self.library, cb) as right:
            left.advance(600); right.advance(600)
            va, vb = left.view(), right.view()
            self.assertIs(type(va), View)
            self.assertEqual(va.evidence_fingerprint(), vb.evidence_fingerprint())
            self.assertEqual(detect(m, va), detect(m, vb))
            self.assertFalse(any(hasattr(va, key) for key in ("seed", "failures", "rate_changes", "provenance")))
            frozen = va.evidence_fingerprint()
            left.advance(900); right.advance(900)
            self.assertEqual(frozen, va.evidence_fingerprint())
            self.assertNotEqual(left.view().evidence_fingerprint(), right.view().evidence_fingerprint())
            events = [e for e in detect(m, left.view()).events if e.category == "pump_failure"]
            self.assertEqual(len(events), 1)
            self.assertEqual(events[0].onset_unix, a.start_unix + 900)

    def test_advance_partition_reset_and_noise_are_exactly_repeatable(self):
        m = model(2); config = seeded_experiment(m, start_unix=1791762900, duration_s=3600, seed=83, failure_count=2, rate_change_count=2)
        compiled = compile_experiment(m, config)
        with SyntheticStream(self.library, compiled) as coarse, SyntheticStream(self.library, compiled) as fine:
            coarse.advance(3600)
            for t in (75.5, 300, 550.25, 900, 1000.125, 1800, 3600): fine.advance(t)
            self.assertEqual(coarse.view().evidence_fingerprint(), fine.view().evidence_fingerprint())
            self.assertEqual(coarse.truth()["facilities"], fine.truth()["facilities"])
            expected = coarse.view().evidence_fingerprint()
            coarse.reset(); coarse.advance(3600)
            self.assertEqual(coarse.view().evidence_fingerprint(), expected)

    def test_noise_keeps_finite_negative_raw_values_without_mutating_truth(self):
        m = model()
        negative = None
        for seed in range(32):
            config = replace(experiment(m, initial_volume=0), seed=seed, noise=Noise(level_pct_amplitude=10))
            with SyntheticStream(self.library, compile_experiment(m, config)) as stream:
                raw = stream.view().latest(m.facilities[0].tank_id, "level_pct").sample.value
                self.assertEqual(stream.truth()["facilities"][0]["volume_bbl"], 0)
                self.assertLessEqual(abs(raw), 10)
                if raw < 0: negative = raw; break
        self.assertIsNotNone(negative)

    def test_gap_keeps_actual_age_and_cannot_fabricate_current_samples(self):
        m = model(); config = experiment(m, noise=Noise(dropout_every=2))
        with SyntheticStream(self.library, compile_experiment(m, config)) as stream:
            stream.advance(450)
            view = stream.view(); latest = view.latest(m.facilities[0].tank_id, "level_pct")
            self.assertEqual((latest.sample.unix, latest.age_s, latest.state), (config.start_unix, 450, "stale"))
            with self.assertRaises(SyntheticError): stream.view(config.start_unix + 451)
            stream.advance(600)
            self.assertEqual(stream.view().window(m.facilities[0].tank_id, "level_pct", config.start_unix).state, "gap")

    def test_healthy_day_creates_no_fault_alerts_from_declared_small_noise(self):
        m = model()
        config = seeded_experiment(m, start_unix=1791762900, duration_s=86400, seed=2, failure_count=0, rate_change_count=0)
        with SyntheticStream(self.library, compile_experiment(m, config)) as stream:
            stream.advance(86400)
            self.assertEqual(ongoing_alerts(detect(m, stream.view())), ())
            self.assertGreater(stream.truth()["facilities"][0]["pumped_bbl"], 0)

    def test_fractional_commands_current_rates_and_persistent_real_commitment(self):
        network, ops = inputs(); field = network["systems"][0]["facilities"][0]; sid = network["systems"][0]["id"]
        ops["expected_repair_hours"]["pump_failure"] = 120 / 3600
        ops["systems"][sid]["facilities"][field["id"]] = {"response_min": 0.25, "cycle_min": 3}
        m = compile_model(network, ops)
        config = experiment(m, duration=600, failures=(Failure(m.facilities[0].id, 300.125),))
        with SyntheticStream(self.library, compile_experiment(m, config)) as stream:
            stream.advance(350)
            self.assertTrue(stream.dispatch(m.facilities[0].id, 0, 1)["accepted"])
            stream.advance(375.25)
            self.assertTrue(stream.shut_in(m.facilities[0].wells[0].id)["accepted"])
            old = stream.view()
            self.assertEqual(old.cutoff_unix, config.start_unix + 375)
            stream.advance(450)
            self.assertTrue(stream.truth()["trucks"][0]["busy"])
            self.assertEqual(stream.truth()["trucks"][0]["available_again_s"], 545)
            self.assertEqual(stream.truth()["wells"][0]["producing"], 1)
            stream.advance(545)
            self.assertFalse(stream.truth()["trucks"][0]["busy"])
            stream.advance(600)
            self.assertEqual(stream.truth()["completed_full_loads"], 1)
            self.assertEqual(old.cutoff_unix, config.start_unix + 375)

    def test_analysis_uses_only_the_causal_view_with_no_native_or_file_access(self):
        m = model(); config = experiment(m, failures=(Failure(m.facilities[0].id, 600.125),))
        with SyntheticStream(self.library, compile_experiment(m, config)) as stream:
            with patch("builtins.open", side_effect=AssertionError("file read")), patch.object(Path, "open", side_effect=AssertionError("file read")):
                stream.advance(900)
                view = stream.view()
                with patch.object(NativePlant, "snapshot", side_effect=AssertionError("truth read")):
                    result = detect(m, view)
            self.assertTrue(any(e.category == "pump_failure" for e in result.events))

    def test_cancellation_invalid_clock_and_reset_preserve_prior_evidence(self):
        m = model(); config = experiment(m)
        with SyntheticStream(self.library, compile_experiment(m, config)) as stream:
            initial = stream.view().evidence_fingerprint()
            with self.assertRaises(SyntheticError) as caught: stream.advance(1800, cancelled=lambda: True)
            self.assertEqual(caught.exception.code, "plant_cancelled")
            self.assertEqual(stream.view().evidence_fingerprint(), initial)
            stream.reset(); stream.advance(900)
            preserved = stream.view().evidence_fingerprint()
            with self.assertRaises(SyntheticError): stream.advance(100)
            self.assertEqual(stream.view().evidence_fingerprint(), preserved)

    def test_control_journal_replays_fractional_actions_and_bounds_before_mutation(self):
        network, ops = inputs(); field = network["systems"][0]["facilities"][0]; sid = network["systems"][0]["id"]
        ops["expected_repair_hours"]["pump_failure"] = 120 / 3600
        ops["systems"][sid]["facilities"][field["id"]] = {"response_min": 0.25, "cycle_min": 3}
        m = compile_model(network, ops)
        config = experiment(m, duration=600, failures=(Failure(m.facilities[0].id, 300.125),))
        compiled = compile_experiment(m, config)
        with SyntheticStream(self.library, compiled) as original:
            original.advance(350); original.dispatch(m.facilities[0].id, 0, 1)
            original.advance(375.25); original.shut_in(m.facilities[0].wells[0].id)
            original.advance(450); original.advance(545); original.advance(600); receipt = original.provenance()
            with SyntheticStream(self.library, compiled) as replay:
                for command in receipt["controls"]:
                    replay.advance(command["elapsed_s"])
                    self.assertTrue(command["accepted"])
                    if command["operation"] == "dispatch": replay.dispatch(command["facility_id"], command["truck"], command["trips"])
                    else: replay.shut_in(command["well_id"])
                replay.advance(600)
                self.assertEqual({k: v for k, v in replay.truth().items() if k != "trace_count"},
                                 {k: v for k, v in original.truth().items() if k != "trace_count"})
                self.assertNotEqual(replay.truth()["trace_count"], original.truth()["trace_count"])
                self.assertEqual(replay.view().evidence_fingerprint(), original.view().evidence_fingerprint())
                self.assertEqual(replay.provenance()["run_fingerprint"], receipt["run_fingerprint"])
            original.reset(); self.assertEqual(original.provenance()["controls"], [])
        with SyntheticStream(self.library, compile_experiment(m, replace(config, control_limit=1))) as bounded:
            bounded.advance(350); bounded.dispatch(m.facilities[0].id, 0, 1)
            prior = bounded.truth()
            with self.assertRaises(SyntheticError) as caught: bounded.shut_in(m.facilities[0].wells[0].id)
            self.assertEqual(caught.exception.code, "control_limit")
            self.assertEqual(bounded.truth(), prior)


if __name__ == "__main__": unittest.main()
