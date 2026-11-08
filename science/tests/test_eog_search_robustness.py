"""Finite-extreme regression cases for seeds, real evaluation and worker binding."""
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import eog_search
import eog_worker
from native_eog import NativeEvaluator
from test_eog_search import fixture, identity
from test_eog_worker import capture


class SearchSeedRobustnessTests(unittest.TestCase):
    def snapshot(self):
        m, v = fixture(capacity=1e308, response=2, cycle=5)
        return eog_search.compile_context(m, v)["snapshot"]

    def test_trip_priorities_do_not_overflow_before_native_inventory_checks(self):
        snapshot = self.snapshot()
        # Four first/return arrivals fit before 20s. Every full load is physically
        # impossible in this 100-bbl fixture, but priority generation must finish.
        counts = eog_search._trip_counts(snapshot, 0, 0, eog_search.Rules(), lambda: None)
        self.assertEqual(counts, (1, 2, 3, 4))

    def test_prefix_seed_generation_keeps_finite_control_times(self):
        snapshot = self.snapshot()
        schedule = (eog_search.Job(0, 0, 0, 2),)
        menu, preferred = eog_search._local_policies(snapshot, 0, schedule, eog_search.Rules(), lambda: None)
        self.assertEqual(preferred, ())
        self.assertTrue(all(math.isfinite(t) and 0 <= t <= 20 for policy in menu for _, t in policy))

    def test_schedule_estimate_does_not_convert_an_overflowed_product_to_fraction(self):
        snapshot = self.snapshot()
        schedule = (eog_search.Job(0, 0, 0, 2),)
        # Optimistic seed estimate only: two 3-cost loads, no terminal loss in
        # the relaxed inventory equation. Actual inventory validity is separate.
        self.assertEqual(eog_search._estimate(snapshot, schedule, lambda: None), 6)


class SearchExtremeNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = os.environ.get("BREAKWATER_EOG_NATIVE")
        if not cls.library:
            raise unittest.SkipTest("Set BREAKWATER_EOG_NATIVE for actual extreme-parameter search qualification.")
        cls.native = NativeEvaluator(cls.library)

    def run_search(self, **parameters):
        m, v = fixture(response=2, cycle=5, **parameters)
        result = eog_search.search(m, v, self.native, identity=identity,
                                  clock_context="snapshot_now", candidate_limit=150, time_limit_s=5)
        selected = next(e for e in result.result["evaluations"]
                        if e["plan"]["plan_id"] == result.result["selected_plan_id"])
        self.assertEqual(result.validation.record["cost"], selected["cost"])
        self.assertTrue(result.validation.record["independent"])
        self.assertEqual(selected["cost"]["total"], 10)
        self.assertFalse(selected["plan"]["truck_jobs"])
        json.dumps(result.result, allow_nan=False)
        return result

    def test_oversized_finite_loads_do_not_discard_a_valid_independently_checked_baseline(self):
        found = self.run_search(capacity=1e308)
        self.assertGreater(found.audit["counts"]["invalid"], 0)

    def test_subnormal_load_size_keeps_a_real_finite_selected_plan(self):
        self.run_search(capacity=math.ulp(0.0))

    def test_genuinely_unrepresentable_candidate_cost_stays_unavailable(self):
        found = self.run_search(capacity=5, trip_cost=1e308)
        self.assertGreater(found.audit["counts"]["numerical_unavailable"], 0)
        self.assertFalse(found.result["search"]["complete"])

    def test_real_worker_search_and_no_dll_validation_accept_finite_extreme_truck_setting(self):
        contract_root = os.environ.get("BREAKWATER_EOG_CONTRACTS")
        if not contract_root:
            self.skipTest("Set BREAKWATER_EOG_CONTRACTS for actual bound worker qualification.")
        contract = eog_worker.contract_module(contract_root)
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            request, _, _ = capture(folder, contract)
            operations = json.loads((folder / "ops.json").read_text(encoding="utf-8"))
            operations["emergency_trucking"]["truck_capacity_bbl"] = 1e308
            raw = json.dumps(operations).encode()
            (folder / "ops.json").write_bytes(raw)
            manifest = request["capture_manifest"]
            digest = hashlib.sha256(raw).hexdigest()
            manifest["source_hashes"]["ops.json"] = digest
            manifest["captured_files"]["ops.json"] = {"sha256": digest, "size_bytes": len(raw)}
            manifest["dataset_identity"] = contract.dataset_identity(manifest["source_hashes"])
            manifest["capture_id"] = contract.capture_identity(manifest)
            for key in ("dataset_identity", "capture_id"):
                request[key] = manifest[key]
            request["operation"] = "search_plans"
            request["arguments"] = {"system_ids": [], "candidate_limit": 100, "time_limit_s": 5}

            def execute(query, native):
                command = [sys.executable, str(Path(eog_worker.__file__).resolve()), "--contracts-root", contract_root]
                if native:
                    command += ["--native-library", self.library]
                run = subprocess.run(command, input=eog_worker.encode_response(query), capture_output=True, timeout=30)
                self.assertEqual(run.returncode, 0, run.stderr.decode(errors="replace"))
                self.assertEqual(run.stderr, b"")
                self.assertEqual(run.stdout.count(b"\n"), 1)
                response = contract.decode_document(run.stdout)
                contract.validate_document(response, context={"request": query})
                self.assertEqual(response["status"], "completed", response["diagnostics"])
                return response

            response = execute(request, True)
            selected = next(e for e in response["result"]["evaluations"]
                            if e["plan"]["plan_id"] == response["result"]["selected_plan_id"])
            self.assertFalse(selected["plan"]["truck_jobs"])
            validation_request = copy.deepcopy(request)
            validation_request["operation"] = "validate_plan"
            validation_request["request_id"] = "extreme-independent-check"
            validation_request["arguments"] = {"plan": selected["plan"], "evaluator_method": self.native.method}
            independent = execute(validation_request, False)["result"]["validation"]
            self.assertTrue(independent["independent"])
            self.assertEqual(independent["cost"], selected["cost"])
            self.assertEqual(independent["plan_hash"], selected["plan"]["plan_hash"])


if __name__ == "__main__":
    unittest.main()
