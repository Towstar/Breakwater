"""Synthetic summary checks only; never hosted or clean-Windows evidence."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("bundle_check", ROOT / "scripts/verify_portfolio_bundle.py")
bundle_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle_check)


class BundleReadinessTests(unittest.TestCase):
    def summary(self, configured=True):
        return dict(code="live_not_enabled", exit_code=3, provider_configured=configured,
                    live_enabled=False, archive_written=False, model_requests=0,
                    numeric_exchanges=0, executed_tools=0)

    def test_bundled_runtime_can_be_ready_without_any_operation(self):
        bundle_check.validate_live_cli_readiness(self.summary(), True)

    def test_explicit_invalid_override_must_remain_unavailable(self):
        bundle_check.validate_live_cli_readiness(self.summary(False), False)
        with self.assertRaises(RuntimeError):
            bundle_check.validate_live_cli_readiness(self.summary(True), False)

    def test_missing_bundled_runtime_is_rejected(self):
        with self.assertRaises(RuntimeError):
            bundle_check.validate_live_cli_readiness(self.summary(False), True)

    def test_model_or_provider_work_is_not_a_readiness_pass(self):
        for field in ("model_requests", "numeric_exchanges", "executed_tools"):
            with self.subTest(field=field):
                summary = self.summary()
                summary[field] = 1
                with self.assertRaises(RuntimeError):
                    bundle_check.validate_live_cli_readiness(summary, True)

    def test_live_or_archived_output_is_rejected(self):
        for field in ("live_enabled", "archive_written"):
            with self.subTest(field=field):
                summary = self.summary()
                summary[field] = True
                with self.assertRaises(RuntimeError):
                    bundle_check.validate_live_cli_readiness(summary, True)

    def test_missing_or_foreign_status_is_rejected(self):
        for summary in ({}, dict(self.summary(), code="provider_unavailable"),
                        dict(self.summary(), exit_code=0)):
            with self.subTest(summary=summary):
                with self.assertRaises(RuntimeError):
                    bundle_check.validate_live_cli_readiness(summary, True)


if __name__ == "__main__":
    unittest.main()
