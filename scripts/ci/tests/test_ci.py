"""Offline checks of the workflow trust boundary and test-result failure policy."""
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location('ci_science', ROOT / 'scripts/ci/Run-PythonTests.py')
SCIENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCIENCE)
QT_SPEC = importlib.util.spec_from_file_location('ci_qt', ROOT / 'scripts/ci/Read-QtResults.py')
QT = importlib.util.module_from_spec(QT_SPEC)
QT_SPEC.loader.exec_module(QT)


class WorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = yaml.safe_load((ROOT / '.github/workflows/windows-checks.yml').read_text(encoding='utf-8'))

    def test_fork_pull_requests_run_without_secrets_or_write_token(self):
        self.assertIn('pull_request', self.workflow['on'])
        self.assertNotIn('pull_request_target', self.workflow['on'])
        self.assertEqual(self.workflow['permissions'], {'contents': 'read'})
        self.assertEqual(self.workflow['env']['NVIDIA_API_KEY'], '')
        self.assertNotIn('secrets.', (ROOT / '.github/workflows/windows-checks.yml').read_text())

    def test_heavy_jobs_require_both_native_modes(self):
        jobs = self.workflow['jobs']
        self.assertEqual(set(jobs), {'native', 'desktop', 'science', 'lean'})
        self.assertEqual(set(jobs['native']['strategy']['matrix']['mode']), {'normal', 'asan'})
        self.assertEqual(set(jobs['desktop']['strategy']['matrix']['mode']), {'normal', 'asan'})
        for key in ('desktop', 'science', 'lean'):
            self.assertEqual(jobs[key]['needs'], 'native')

    def test_actions_are_exact_pins_and_checkout_cannot_leave_credentials(self):
        for job in self.workflow['jobs'].values():
            self.assertEqual(job['runs-on'], 'windows-2022')
            for step in job['steps']:
                if 'uses' in step:
                    self.assertRegex(step['uses'], r'^actions/[a-z-]+@[0-9a-f]{40}$')
                if step.get('uses', '').startswith('actions/checkout@'):
                    self.assertIs(step['with']['persist-credentials'], False)

    def test_job_budgets_keep_a_bounded_asan_only_exception(self):
        for name, job in self.workflow['jobs'].items():
            timeout = job['timeout-minutes']
            if name == 'desktop':
                self.assertEqual(timeout, "${{ matrix.mode == 'asan' && 90 || 60 }}")
            else:
                self.assertIs(type(timeout), int)
                self.assertGreater(timeout, 0)
                self.assertLessEqual(timeout, 60)

    def test_evidence_is_uploaded_even_after_failure_without_build_trees(self):
        for job in self.workflow['jobs'].values():
            uploads = [s for s in job['steps'] if s.get('uses', '').startswith('actions/upload-artifact@')]
            self.assertEqual(len(uploads), 1)
            self.assertEqual(uploads[0]['if'], 'always()')
            self.assertEqual(uploads[0]['with']['if-no-files-found'], 'error')
            self.assertNotIn('ci-build', uploads[0]['with']['path'])

    def test_caches_exclude_credentials_and_only_restore_declared_tool_paths(self):
        for job in self.workflow['jobs'].values():
            for step in job['steps']:
                if step.get('uses', '').startswith('actions/cache@'):
                    for path in step['with']['path'].splitlines():
                        self.assertRegex(path, r'^(\.tools/(Qt|qt-downloads|uv|uv-python|uv-python-cache|uv-cache|lean|mathlib-cache)|proofs/\.lake/packages)$')
                    self.assertIn('hashFiles(', step['with']['key'])

    def test_desktop_uses_warp_with_actual_rhi(self):
        env = self.workflow['jobs']['desktop']['env']
        self.assertEqual(env['QSG_RHI_BACKEND'], 'd3d11')
        self.assertEqual(env['QSG_RHI_PREFER_SOFTWARE_RENDERER'], '1')
        self.assertNotIn('QT_QUICK_BACKEND', env)

    def test_windows_bootstrap_has_an_available_pin_separate_from_science(self):
        for job in self.workflow['jobs'].values():
            for step in job['steps']:
                if step.get('uses', '').startswith('actions/setup-python@'):
                    self.assertEqual(step['with']['python-version'], '3.12.10')
        setup = (ROOT / 'scripts/Setup-ScientificWorker.ps1').read_text()
        self.assertIn('cpython-3.12.13-windows-x86_64-none', setup)

    def test_setup_evidence_precedes_dependencies_and_finishes_before_upload(self):
        for name, job in self.workflow['jobs'].items():
            steps = job['steps']
            start = next(i for i, s in enumerate(steps) if s.get('name') == 'Initialize setup evidence')
            finish = next(i for i, s in enumerate(steps) if s.get('name') == 'Record job outcome')
            upload = next(i for i, s in enumerate(steps) if s.get('uses', '').startswith('actions/upload-artifact@'))
            checkout = next(i for i, s in enumerate(steps) if s.get('uses', '').startswith('actions/checkout@'))
            self.assertEqual(start, checkout + 1)
            self.assertLess(finish, upload)
            self.assertEqual(steps[finish]['if'], 'always()')
            self.assertIn('-Outcome', steps[finish]['run'])
            self.assertIn(f'-Suite {name}', steps[start]['run'])

    def test_failed_bootstrap_retains_real_metadata_without_claiming_tests_or_copying_secrets(self):
        # The fixture copies only this helper into an ignored temporary root.
        # It simulates a job outcome; no package installation or product test runs.
        with tempfile.TemporaryDirectory(prefix='job-evidence-', dir=ROOT / '.tools') as temporary:
            root = Path(temporary)
            helper = root / 'scripts/ci/Write-JobEvidence.ps1'
            helper.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / 'scripts/ci/Write-JobEvidence.ps1', helper)
            env = os.environ.copy()
            env.update(NVIDIA_API_KEY='fixture-secret-that-must-not-be-exported', GITHUB_RUN_ID='fixture-run')
            for args in ([], ['-Phase', 'finish', '-Outcome', 'failure']):
                completed = subprocess.run([shutil.which('pwsh'), '-NoProfile', '-File', str(helper), '-Suite', 'native', *args],
                                           env=env, capture_output=True, text=True, timeout=30)
                self.assertEqual(completed.returncode, 0, completed.stderr)
            report_root = root / 'out/ci/native-normal'
            first = json.loads((report_root / 'job-start.json').read_text())
            last = json.loads((report_root / 'job-finish.json').read_text())
            self.assertEqual(first['job_outcome'], 'running')
            self.assertEqual(last['job_outcome'], 'failure')
            self.assertEqual(last['github']['run_id'], 'fixture-run')
            self.assertEqual(last['test_execution_status'], 'not_started')
            self.assertFalse(last['verification_available'])
            self.assertFalse(last['ctest_results_available'])
            self.assertNotIn('fixture-secret-that-must-not-be-exported', json.dumps(last))


def result_for(*cases):
    return unittest.TextTestRunner(stream=io.StringIO()).run(unittest.TestSuite(cases))


class ResultTests(unittest.TestCase):
    def test_success_is_reported(self):
        report = SCIENCE.summarize(result_for(unittest.FunctionTestCase(lambda: None)))
        self.assertTrue(report['passed'])
        self.assertEqual(report['tests_passed'], 1)

    def test_real_failures_cannot_become_a_successful_report(self):
        class Failure(unittest.TestCase):
            def runTest(self):
                self.fail('deliberate result-policy fixture')
        report = SCIENCE.summarize(result_for(Failure()))
        self.assertFalse(report['passed'])
        self.assertEqual(report['failures'], 1)

    def test_empty_discovery_is_not_a_pass(self):
        self.assertFalse(SCIENCE.summarize(unittest.TestResult())['passed'])

    def test_known_optional_physicsnemo_skip_is_explicit(self):
        class Optional(unittest.TestCase):
            def id(self):
                return 'test_physicsnemo.ActualPhysicsNeMoTests.test_fixture'
            def runTest(self):
                self.skipTest(SCIENCE.OPTIONAL_REASON)
        report = SCIENCE.summarize(result_for(Optional(), unittest.FunctionTestCase(lambda: None)))
        self.assertTrue(report['passed'])
        self.assertEqual(report['tests_passed'], 1)
        self.assertEqual(len(report['skipped']), 1)

    def test_all_optional_skips_are_not_executed_science(self):
        class Optional(unittest.TestCase):
            def id(self):
                return 'test_physicsnemo.ActualPhysicsNeMoTests.test_fixture'
            def runTest(self):
                self.skipTest(SCIENCE.OPTIONAL_REASON)
        report = SCIENCE.summarize(result_for(Optional()))
        self.assertFalse(report['passed'])
        self.assertEqual(report['tests_passed'], 0)

    def test_other_dependency_skip_fails_the_job(self):
        class MissingTorch(unittest.TestCase):
            def runTest(self):
                self.skipTest('PyTorch is required for actual worker science execution')
        report = SCIENCE.summarize(result_for(MissingTorch()))
        self.assertFalse(report['passed'])
        self.assertEqual(len(report['unexpected_skips']), 1)


class QtResultTests(unittest.TestCase):
    def summarize(self, *, skip=None, failed=0, blacklisted=0, allow=True, count=12):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for i in range(count):
                name = 'pipe_transport-results' if i == 0 else f'suite-{i}-results'
                reason = f'SKIP   : {skip}\n' if skip and i == 0 else ''
                (root / f'{name}.txt').write_text(
                    reason + f'Totals: 3 passed, {failed} failed, {bool(reason):d} skipped, '
                    f'{blacklisted} blacklisted, 1ms\n', encoding='utf-8')
            return QT.summarize(root, allow)

    def test_exact_optional_calibration_skip_is_retained(self):
        report = self.summarize(skip=QT.OPTIONAL_CALIBRATION[1])
        self.assertTrue(report['passed'])
        self.assertEqual(report['optional_calibration_skipped'], 1)
        self.assertEqual(report['qt_suites'][0]['skipped'], 1)

    def test_optional_qualification_cannot_skip_its_real_case(self):
        self.assertFalse(self.summarize(skip=QT.OPTIONAL_CALIBRATION[1], allow=False)['passed'])

    def test_other_missing_dependency_or_changed_reason_is_a_failure(self):
        for reason in ('StorageTests::actualTorch() Torch unavailable',
                       QT.OPTIONAL_CALIBRATION[1] + ' changed'):
            with self.subTest(reason=reason):
                self.assertFalse(self.summarize(skip=reason)['passed'])

    def test_failures_blacklists_and_missing_suites_remain_failures(self):
        self.assertTrue(self.summarize()['passed'])
        for kwargs in ({'failed': 1}, {'blacklisted': 1}, {'count': 11}):
            with self.subTest(kwargs=kwargs):
                self.assertFalse(self.summarize(**kwargs)['passed'])

    def test_missing_duplicate_and_unaccounted_totals_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for i in range(11):
                (root / f'other-{i}-results.txt').write_text(
                    'Totals: 3 passed, 0 failed, 0 skipped, 0 blacklisted\n', encoding='utf-8')
            path = root / 'suite-results.txt'
            for text in ('', 'Totals: 3 passed, 0 failed, 1 skipped, 0 blacklisted',
                         'Totals: 3 passed, 0 failed, 0 skipped, 0 blacklisted\n' * 2):
                path.write_text(text, encoding='utf-8')
                report = QT.summarize(root, True)
                self.assertFalse(report['passed'])
                self.assertTrue(any('suite-results.txt:' in item for item in report['problems']))


if __name__ == '__main__':
    unittest.main()
