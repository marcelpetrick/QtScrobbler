# SPDX-License-Identifier: GPL-2.0-only
"""Coverage thresholds and LLVM diagnostic parsing must fail closed."""
from decimal import Decimal
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))  # the runners import their shared sibling modules
import source_fingerprint  # noqa: E402

SPEC = importlib.util.spec_from_file_location('coverage_runner', Path(__file__).resolve().parents[1] / 'tools/run_coverage.py')
COVERAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COVERAGE)


class CoverageToolsTest(unittest.TestCase):
    def test_coverage_and_mutation_share_one_fingerprint_scope(self):
        # Mutation rejects coverage evidence whose map differs, so both runners must
        # use the same function; a private list once omitted requirements-bootstrap.txt.
        mutation_spec = importlib.util.spec_from_file_location('mutation_runner_scope', TOOLS / 'run_mutation_tests.py')
        mutation = importlib.util.module_from_spec(mutation_spec)
        mutation_spec.loader.exec_module(mutation)
        self.assertIs(COVERAGE.source_hashes, source_fingerprint.source_hashes)
        self.assertIs(mutation.source_hashes, source_fingerprint.source_hashes)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name in source_fingerprint.TOP_LEVEL_FILES + ('src/a.cpp', 'tools/b.py', 'README.md',
                                                              'tools/__pycache__/c.pyc', 'tests/d.pyc'):
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_text(name)
            hashes = source_fingerprint.source_hashes(root)
        self.assertEqual(set(hashes), set(source_fingerprint.TOP_LEVEL_FILES) | {'src/a.cpp', 'tools/b.py'})
        self.assertIn('requirements-bootstrap.txt', hashes)

    def test_threshold_uses_counts(self):
        self.assertFalse(COVERAGE.passes(94995, 100000, Decimal('95')))
        self.assertTrue(COVERAGE.passes(95, 100, Decimal('95')))
        self.assertFalse(COVERAGE.passes(0, 0, Decimal('0')))

    def test_llvm_diagnostics_preserved(self):
        output = 'warning: 2 functions have mismatched data\n{"data": []}'
        with tempfile.TemporaryDirectory() as temp, patch.object(COVERAGE.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, output, 'mapping detail')):
            log = Path(temp) / 'coverage.json'
            self.assertEqual(COVERAGE.run(['cov'], {}, log, json_output=True), '{"data": []}')
            self.assertEqual(log.with_suffix('.stdout.log').read_text(), output)
            self.assertEqual(log.with_suffix('.stderr.log').read_text(), 'mapping detail')

    def test_unexpected_output_rejected(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(COVERAGE.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'fatal mapping problem\n{}', '')):
            with self.assertRaises(ValueError):
                COVERAGE.run(['cov'], {}, Path(temp) / 'coverage.json', json_output=True)

    def test_failed_tool_cannot_produce_success(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(COVERAGE.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, '{}', 'failure')):
            with self.assertRaises(RuntimeError):
                COVERAGE.run(['cov'], {}, Path(temp) / 'coverage.json', json_output=True)


if __name__ == '__main__':
    unittest.main()
