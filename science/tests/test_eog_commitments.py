from dataclasses import FrozenInstanceError, asdict, replace
import json
import math
import os
from pathlib import Path
import random
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from eog_commitments import (CommitmentError, Dispatch, History, MAX_CONTROLS, ShutIn, reconstruct)
from eog_model import compile_model
from eog_observations import Archive
from eog_synthetic import Failure, SyntheticError, SyntheticStream, compile_experiment
from native_eog_plant import NativePlant
from eog_test_fixtures import frame, inputs, model, view
from test_eog_synthetic import experiment

EPOCH = 1791762900


def configured(count=2):
    n, o = inputs(count, prefix="commitments/\u03b2")
    sid = n["systems"][0]["id"]
    o["expected_repair_hours"]["pump_failure"] = 120 / 3600
    for f in n["systems"][0]["facilities"]:
        o["systems"][sid]["facilities"][f["id"]] = {"response_min": .25, "cycle_min": 3}
    return compile_model(n, o)


def observations(m, elapsed):
    end = EPOCH + math.floor(elapsed)
    rows = [s for i in range(len(m.facilities)) for s in frame(m, EPOCH, index=i)]
    if end != EPOCH:
        rows += [s for i in range(len(m.facilities)) for s in frame(m, end, index=i)]
    return view(m, rows, end)


def history(m, elapsed, commands=()):
    return History(m.fingerprint(), EPOCH, elapsed, True, tuple(commands))


class CommitmentTests(unittest.TestCase):
    def test_fractional_return_half_open_boundary_and_sequential_yard_job(self):
        m = configured(); f, g = m.facilities
        command = Dispatch(350.125, f.id, 0, 1)
        for now, busy in ((math.nextafter(545.125, 0), True), (545.125, False)):
            result = reconstruct(m, observations(m, now), history(m, now, (command,)))
            self.assertEqual((result.trucks[0].busy, result.trucks[0].available_again_elapsed_s), (busy, 545.125))
            self.assertEqual(result.scheduled_pickups[0].arrival_elapsed_s, 365.125)
        next_job = Dispatch(545.125, g.id, 0, 1)
        result = reconstruct(m, observations(m, 545.125), history(m, 545.125, (command, next_job)))
        self.assertEqual(result.trucks[0].available_again_elapsed_s, 740.125)
        with self.assertRaises(CommitmentError) as caught:
            reconstruct(m, observations(m, 545.125), history(m, 545.125, (command, replace(next_job, elapsed_s=math.nextafter(545.125, 0)))))
        self.assertEqual(caught.exception.code, "overbooked_history")

    def test_independent_system_fleets_and_well_facts_do_not_release_trucks(self):
        a, b = inputs(prefix="a"), inputs(prefix="b")
        a[0]["systems"].extend(b[0]["systems"]); a[1]["systems"].update(b[1]["systems"])
        m = compile_model(*a); f, g = m.facilities
        commands = (Dispatch(10.25, f.id, 0, 1), Dispatch(10.25, g.id, 0, 1), ShutIn(11, f.wells[0].id))
        result = reconstruct(m, observations(m, 100), history(m, 100, commands))
        self.assertEqual(sum(r.busy for r in result.trucks), 2)
        self.assertEqual({p.system_id for p in result.scheduled_pickups}, {f.system_id, g.system_id})

    def test_history_and_result_are_immutable_and_do_not_authorize_actions(self):
        m = configured(); h = history(m, 1)
        result = reconstruct(m, observations(m, 1), h)
        with self.assertRaises(FrozenInstanceError): h.cutoff_elapsed_s = 2
        with self.assertRaises(FrozenInstanceError): result.trucks[0].busy = True
        self.assertFalse(any(hasattr(result, key) for key in ("dispatch", "approve", "seed", "failures", "native")))
        json.dumps(asdict(result), allow_nan=False)

    def test_bad_unknown_incomplete_and_future_inputs_are_explicit(self):
        m = configured(); f = m.facilities[0]
        for build in (lambda: replace(history(m, 10), complete_from_free_start=False),
                      lambda: history(m, 10, (Dispatch(11, f.id, 0, 1),)),
                      lambda: history(m, 10, (ShutIn(9, f.wells[0].id), ShutIn(8, f.wells[0].id))),
                      lambda: Dispatch(math.nan, f.id, 0, 1), lambda: Dispatch(0, f.id, True, 1),
                      lambda: Dispatch(0, f.id, 0, 4096), lambda: ShutIn(0, "\ud800"),
                      lambda: replace(history(m, 0), commands=[ShutIn(0, f.wells[0].id)])):
            with self.subTest(build=build), self.assertRaises(CommitmentError): build()
        for command in (Dispatch(1, "foreign", 0, 1), Dispatch(1, f.id, 100, 1), ShutIn(1, "foreign")):
            with self.subTest(command=command), self.assertRaises(CommitmentError):
                reconstruct(m, observations(m, 1), history(m, 1, (command,)))

    def test_identity_cutoff_catalog_and_work_bounds(self):
        m = configured(); f = m.facilities[0]
        with self.assertRaises(CommitmentError): reconstruct(model(), observations(m, 1), history(m, 1))
        with self.assertRaises(CommitmentError): reconstruct(m, observations(m, 2), history(m, 1))
        with self.assertRaises(CommitmentError): history(m, 0, (ShutIn(0, f.wells[0].id),) * (MAX_CONTROLS + 1))
        # Five disjoint trucks, 4095 trips each, exceed the total retained bound.
        m = replace(m, systems=(replace(m.systems[0], fleet_size=5),))
        commands = tuple(Dispatch(0, f.id, i, 4095) for i in range(5))
        with self.assertRaises(CommitmentError) as caught:
            reconstruct(m, observations(m, 0), history(m, 0, commands))
        self.assertEqual(caught.exception.code, "trip_limit")

    def test_unrepresentable_calendar_never_becomes_free_or_zero_cost(self):
        m = configured(); f = replace(m.facilities[0], cycle_min=1e308)
        m = replace(m, facilities=(f, *m.facilities[1:]))
        with self.assertRaises(CommitmentError) as caught:
            reconstruct(m, observations(m, 1), history(m, 1, (Dispatch(1, f.id, 0, 1),)))
        self.assertEqual(caught.exception.code, "timing_unavailable")

    def test_catalog_mismatch_and_explicit_zero_fleet(self):
        m = configured(); catalog = list(m.channels())
        catalog[0] = replace(catalog[0], unit="m3/h")
        mismatched = Archive(catalog, frame(m, EPOCH)).at(EPOCH)
        with self.assertRaises(CommitmentError) as caught:
            reconstruct(m, mismatched, history(m, 0))
        self.assertEqual(caught.exception.code, "catalog_mismatch")
        m = replace(m, systems=(replace(m.systems[0], fleet_size=0),))
        self.assertEqual(reconstruct(m, observations(m, 0), history(m, 0)).trucks, ())

    def test_reconstruction_needs_no_native_files_or_future_observations(self):
        m = configured(); command = Dispatch(350.125, m.facilities[0].id, 0, 1)
        rows = frame(m, EPOCH) + frame(m, EPOCH + 350)
        a = Archive(m.channels(), rows + frame(m, EPOCH + 600)).at(EPOCH + 350)
        b = Archive(m.channels(), rows + frame(m, EPOCH + 600, status=2)).at(EPOCH + 350)
        h = history(m, 350.125, (command,))
        with patch("builtins.open", side_effect=AssertionError("no files")), patch.object(Path, "open", side_effect=AssertionError("no files")):
            self.assertEqual(reconstruct(m, a, h), reconstruct(m, b, h))


class NativeCommitmentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = os.environ.get("BREAKWATER_EOG_PLANT")
        if not cls.library: raise unittest.SkipTest("Set BREAKWATER_EOG_PLANT for actual causal commitment reconstruction.")

    def check(self, m, stream):
        h = stream.control_history()
        causal = stream.view()
        with patch.object(NativePlant, "snapshot", side_effect=AssertionError("No native truth in reconstruction")):
            result = reconstruct(m, causal, h)
        truth = stream.truth()
        for actual, expected in zip(truth["trucks"], result.trucks, strict=True):
            self.assertEqual(m.systems[actual["system"]].id, expected.system_id)
            self.assertEqual(actual["truck"], expected.truck)
            self.assertEqual(actual["busy"], expected.busy)
            self.assertEqual(actual["available_again_s"], expected.available_again_elapsed_s)
        return result

    def test_native_repair_does_not_release_truck_and_exact_return_allows_next_job(self):
        m = configured(); f, g = m.facilities
        cfg = experiment(m, duration=900, failures=(Failure(f.id, 300.125), Failure(g.id, 500.125)))
        with SyntheticStream(self.library, compile_experiment(m, cfg)) as stream:
            stream.advance(350.125); self.assertTrue(stream.dispatch(f.id, 0, 1)["accepted"])
            frozen = self.check(m, stream)
            stream.advance(375.25); self.assertTrue(stream.shut_in(f.wells[0].id)["accepted"])
            stream.advance(450); self.assertNotEqual(stream.truth()["facilities"][0]["pump_status"], 2)
            self.assertTrue(self.check(m, stream).trucks[0].busy)
            stream.advance(math.nextafter(545.125, 0)); self.assertFalse(stream.dispatch(g.id, 0, 1)["accepted"])
            self.assertEqual(len(stream.control_history().commands), 2)
            self.check(m, stream)
            stream.advance(545.125); self.assertTrue(stream.dispatch(g.id, 0, 1)["accepted"])
            self.check(m, stream)
            self.assertEqual(stream.control_history(350.125).commands, (Dispatch(350.125, f.id, 0, 1),))
            self.assertTrue(frozen.trucks[0].busy)
            stream.advance(750); self.assertFalse(self.check(m, stream).trucks[0].busy)
            stream.reset(); self.assertEqual(stream.control_history().commands, ())
            self.assertTrue(all(not t.busy for t in self.check(m, stream).trucks))

    def test_future_truth_rejected_attempts_and_fractional_controls_do_not_leak(self):
        m = configured(); f, g = m.facilities
        a = experiment(m, duration=900, failures=(Failure(f.id, 300.125), Failure(g.id, 700.125)))
        b = replace(a, failures=(Failure(f.id, 300.125), Failure(g.id, 800.125)))
        with SyntheticStream(self.library, compile_experiment(m, a)) as left, SyntheticStream(self.library, compile_experiment(m, b)) as right:
            for stream in (left, right):
                stream.advance(350.125); self.assertTrue(stream.dispatch(f.id, 0, 1)["accepted"])
            left.dispatch(f.id, 0, 1)  # rejected, no physical action
            self.assertEqual(left.control_history(), right.control_history())
            self.assertEqual(self.check(m, left), self.check(m, right))
            self.assertEqual(left.control_history(350).commands, ())
            payload = json.dumps(asdict(left.control_history()), ensure_ascii=False)
            self.assertTrue(all(key not in payload for key in ('"seed"', '"failures"', '"diagnostic"', '"experiment"')))
            with self.assertRaises(SyntheticError): left.control_history(351)

    def test_seeded_accepted_dispatches_match_actual_native_calendars(self):
        rng = random.Random(2026110810)
        for i in range(32):
            m = configured(); f, g = m.facilities
            response, cycle = rng.uniform(.1, .3), rng.uniform(1, 2)
            m = replace(m, facilities=tuple(replace(x, response_min=response, cycle_min=cycle) for x in m.facilities))
            cfg = experiment(m, duration=900, failures=(Failure(f.id, 300.125), Failure(g.id, 300.125)))
            with SyntheticStream(self.library, compile_experiment(m, cfg)) as stream:
                stream.advance(330.25)
                self.assertTrue(stream.dispatch(f.id, 0, 1)["accepted"])
                self.assertTrue(stream.dispatch(g.id, 1, 1)["accepted"])
                for t in (330.25, 360.5, 420.5, 600):
                    stream.advance(t); self.check(m, stream)

    def test_cancelled_native_run_cannot_publish_healthy_history(self):
        m = configured()
        with SyntheticStream(self.library, compile_experiment(m, experiment(m))) as stream:
            with self.assertRaises(SyntheticError): stream.advance(300, cancelled=lambda: True)
            with self.assertRaises(SyntheticError): stream.control_history()

    def test_equal_truck_ordinals_in_two_real_systems_are_independent(self):
        n, o = inputs(prefix="first-system")
        other_n, other_o = inputs(prefix="other-system")
        n["systems"].extend(other_n["systems"]); o["systems"].update(other_o["systems"])
        m = compile_model(n, o); f, g = m.facilities
        cfg = experiment(m, duration=6000, failures=(Failure(f.id, 300.125), Failure(g.id, 300.125)))
        with SyntheticStream(self.library, compile_experiment(m, cfg)) as stream:
            stream.advance(330.25)
            self.assertTrue(stream.dispatch(f.id, 0, 1)["accepted"])
            self.assertTrue(stream.dispatch(g.id, 0, 1)["accepted"])
            self.assertEqual(sum(t.busy for t in self.check(m, stream).trucks), 2)

    def test_later_cancel_cannot_invalidate_a_verified_historical_control_prefix(self):
        m = configured(); f = m.facilities[0]
        cfg = experiment(m, duration=900, failures=(Failure(f.id, 300.125),))
        with SyntheticStream(self.library, compile_experiment(m, cfg)) as stream:
            stream.advance(350.125); self.assertTrue(stream.dispatch(f.id, 0, 1)["accepted"])
            frozen = stream.control_history()
            stream.advance(600)
            with self.assertRaises(SyntheticError): stream.advance(900, cancelled=lambda: True)
            self.assertEqual(stream.control_history(350.125), frozen)
            with self.assertRaises(SyntheticError): stream.control_history()


if __name__ == "__main__": unittest.main()
