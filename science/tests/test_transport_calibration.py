import importlib.util
import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from transport_calibration import fit, reference, validate


def sample(decay=0.2, initial=1.0):
    return {"initial_mg_l": initial,
            "observations": [[t, initial * math.exp(-decay * t)] for t in (0.25, 0.5, 0.75, 1.0)]}


class CalibrationInputTests(unittest.TestCase):
    def test_independent_reference(self):
        initial, rows = validate(sample(0.7, 10.0))
        self.assertAlmostEqual(reference(initial, rows), 0.7, places=13)

    def test_malformed_inputs(self):
        cases = [None, {}, {**sample(), "extra": 1}, {**sample(), "initial_mg_l": True},
                 {**sample(), "initial_mg_l": 0}, {**sample(), "initial_mg_l": float("nan")},
                 {**sample(), "observations": [[0, 1], [0.5, 0.9], [1, 0.8]]},
                 {**sample(), "observations": [[1, 0.9], [0.5, 0.8], [2, 0.7]]},
                 {**sample(), "observations": [[0.1, -1], [0.2, 0.9], [0.3, 0.8]]},
                 {**sample(), "observations": [[0.1, 2], [0.2, 0.9], [0.3, 0.8]]},
                 {**sample(), "observations": [[0.1, 1], [0.2, 0.9]]},
                 {**sample(), "observations": [[0.1, 1]] * 65}]
        for payload in cases:
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                validate(payload)

    def test_unsupported_rate_rejected(self):
        with self.assertRaises(ValueError):
            initial, rows = validate(sample(11.0))
            reference(initial, rows)


@unittest.skipUnless(importlib.util.find_spec("physicsnemo"), "PhysicsNeMo not installed")
class RealPhysicsNeMoCalibrationTests(unittest.TestCase):
    def test_known_decay_recovery(self):
        for decay, initial in ((0.2, 1.0), (0.7, 10.0), (2.0, 0.1)):
            with self.subTest(decay=decay):
                outcome = fit(sample(decay, initial))
                self.assertEqual(outcome["status"], "ok")
                self.assertLess(abs(outcome["decay_per_h"] - decay), 1e-6)
                self.assertEqual(outcome["observations"], 4)
                self.assertGreater(outcome["optimizer_evaluations"], 0)
                self.assertEqual(outcome["physicsnemo_version"], "2.2.2")

    def test_mismatched_observations_rejected(self):
        payload = sample()
        payload["observations"][0][1] = 0.4
        self.assertEqual(fit(payload)["status"], "rejected")

    def test_zero_decay_and_reproducible_binding(self):
        first = fit(sample(0.0))
        second = fit(sample(0.0))
        self.assertEqual(first["status"], "ok")
        self.assertLess(first["decay_per_h"], 1e-6)
        self.assertEqual(first["input_sha256"], second["input_sha256"])
        self.assertEqual(first["decay_per_h"], second["decay_per_h"])


if __name__ == "__main__":
    unittest.main()
