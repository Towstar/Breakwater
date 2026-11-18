"""Explicit real-runtime qualification; missing PhysicsNeMo is a failure.

Kept separate from stdlib/CPU discovery so the established optional-library skip
gate is unchanged. Run this deliberately with the pinned PhysicsNeMo interpreter.
"""
import math
import unittest
from transport_calibration import fit


def sample(decay=0.2, initial=1.0):
    return {"initial_mg_l": initial,
            "observations": [[t, initial * math.exp(-decay * t)] for t in (0.25, 0.5, 0.75, 1.0)]}


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
