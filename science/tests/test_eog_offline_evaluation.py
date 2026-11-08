import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from evaluate_eog_detection import evaluate


def reference(identifier="truth", onset=100, end=500, category="pump_failure"):
    return {"id": identifier, "onset_unix": onset, "end_unix": end, "system": "sys", "facility": "fac",
            "asset": "pump", "category": category, "reference_action": {"SECRET_OFFLINE_ONLY": "never copied"}}


def event(identifier="pred", onset=100, detected=100, end=500, category="pump_failure"):
    return {"id": identifier, "first_evidence_unix": onset, "onset_unix": onset, "detected_unix": detected,
            "end_unix": end, "state": "ongoing" if end is None else "resolved",
            "system_id": "sys", "facility_id": "fac", "asset_id": "pump", "category": category}


def report(events, cutoff=1000):
    return {"status": "completed", "actual_cutoff_unix": cutoff, "detection": {"cutoff_unix": cutoff, "events": events}}


class OfflineEvaluationTests(unittest.TestCase):
    def test_exact_match_passes_without_copying_reference_actions(self):
        result = evaluate(report([event()]), [reference()])
        self.assertTrue(result["gate_passed"])
        self.assertEqual(result["by_category"]["pump_failure"]["matched"], 1)
        self.assertNotIn("SECRET_OFFLINE_ONLY", repr(result))

    def test_missed_category_does_not_become_a_passing_empty_result(self):
        result = evaluate(report([]), [reference()])
        self.assertFalse(result["gate_passed"])
        self.assertEqual(result["missed_reference_ids"], ["truth"])
        self.assertIsNone(result["by_category"]["pump_failure"]["precision"])
        self.assertEqual(result["by_category"]["pump_failure"]["recall"], 0)

    def test_wrong_id_or_category_is_both_false_positive_and_miss(self):
        for changed in (dict(event(), asset_id="unknown"), dict(event(), category="pipeline_leak")):
            result = evaluate(report([changed]), [reference()])
            self.assertEqual((len(result["false_positive_event_ids"]), len(result["missed_reference_ids"])), (1, 1))

    def test_alarm_emitted_after_true_endpoint_cannot_match(self):
        result = evaluate(report([event(detected=600, end=800)]), [reference()])
        self.assertFalse(result["gate_passed"])
        self.assertEqual(result["pairs"], [])

    def test_resolution_confusion_and_detection_delay_are_separate(self):
        result = evaluate(report([event(detected=200, end=None)]), [reference()])
        self.assertFalse(result["gate_passed"])
        self.assertEqual(result["pairs"][0]["detection_delay_seconds"], 100)
        self.assertEqual(result["lifecycle_confusion"], [{"truth": "resolved", "predicted": "ongoing", "count": 1}])

    def test_future_reference_end_is_censored_at_historical_cutoff(self):
        result = evaluate(report([event(detected=200, end=None)], cutoff=300), [reference()])
        self.assertTrue(result["gate_passed"])
        self.assertIsNone(result["pairs"][0]["resolution_error_seconds"])

    def test_future_reference_onset_is_not_an_current_miss(self):
        result = evaluate(report([], cutoff=300), [reference(onset=400, end=500)])
        self.assertTrue(result["gate_passed"])
        self.assertEqual(result["missed_reference_ids"], [])

    def test_maximum_matching_does_not_spend_two_predictions_on_one_reference(self):
        # First prediction overlaps both references; second overlaps only the first.
        labels = [reference("one", 100, 300), reference("two", 400, 600)]
        predictions = [event("broad", 100, 100, 600), event("narrow", 100, 100, 300)]
        result = evaluate(report(predictions), labels)
        self.assertEqual(len(result["pairs"]), 2)
        self.assertEqual(result["false_positive_event_ids"], [])
        self.assertFalse(result["gate_passed"])  # Broad early alarm for the later event remains a failed gate.

    def test_fragmented_extra_alarm_is_not_hidden(self):
        result = evaluate(report([event("one"), event("two", 200, 200, 400)]), [reference()])
        self.assertEqual(len(result["pairs"]), 1)
        self.assertEqual(len(result["false_positive_event_ids"]), 1)
        self.assertFalse(result["gate_passed"])

    def test_duplicate_and_future_prediction_rows_are_rejected(self):
        for predicted in ([event(), event()], [event(detected=1001)], [dict(event(), state="healthy")]):
            with self.assertRaises(ValueError):
                evaluate(report(predicted), [reference()])

    def test_evaluation_does_not_mutate_predictions_or_labels(self):
        labels, predicted = [reference()], report([event()])
        before = copy.deepcopy((labels, predicted))
        evaluate(predicted, labels)
        self.assertEqual((labels, predicted), before)


if __name__ == "__main__":
    unittest.main()
