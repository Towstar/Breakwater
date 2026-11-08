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


if __name__ == "__main__":
    unittest.main()
