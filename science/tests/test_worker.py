import json
from pathlib import Path
import subprocess
import sys
import unittest

WORKER = Path(__file__).resolve().parents[1] / "worker.py"


class WorkerTests(unittest.TestCase):
    def run_worker(self, lines):
        completed = subprocess.run([sys.executable, str(WORKER)], input="\n".join(lines) + "\n",
                                   text=True, capture_output=True, timeout=10, check=True)
        self.assertEqual(completed.stderr, "")
        return [json.loads(line) for line in completed.stdout.splitlines()]

    def test_health_and_explicit_unimplemented_method(self):
        results = self.run_worker([json.dumps({"protocol_version": 1, "request_id": "check", "method": "health"}),
                                   json.dumps({"protocol_version": 1, "request_id": "train", "method": "train"})])
        self.assertEqual(results[0]["request_id"], "check")
        self.assertEqual(results[0]["result"]["methods"], ["health"])
        self.assertEqual(results[0]["result"]["scientific_execution"], "not_implemented")
        self.assertEqual(results[1]["error"]["code"], "unsupported_method")

    def test_malformed_requests_do_not_break_next_request(self):
        results = self.run_worker(["bad-json", "[]", '{"protocol_version":2,"request_id":"old","method":"health"}',
                                  '{"protocol_version":1,"request_id":"new","method":"health"}'])
        self.assertEqual([r["status"] for r in results], ["error", "error", "error", "completed"])


if __name__ == "__main__":
    unittest.main()
