# SPDX-License-Identifier: GPL-2.0-only
"""Coverage thresholds and LLVM diagnostic parsing must fail closed."""
from decimal import Decimal
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('coverage_runner', Path(__file__).resolve().parents[1] / 'tools/run_coverage.py')
COVERAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COVERAGE)


class CoverageToolsTest(unittest.TestCase):
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
