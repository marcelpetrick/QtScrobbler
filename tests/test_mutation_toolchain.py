#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Offline accounting contracts for the native signal-removal semantic preflight."""
import os
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from contextlib import closing

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_mutation_toolchain import Commands, SIGNAL_LINE, check_report, executable, check_compile_inventory


class SemanticReportTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "probe.cpp"
        self.source.write_text("// generated probe source\n")
        self.report = self.root / "signal-removal.sqlite"
        self.row = ["signal-mutant", "cxx_remove_void_call", str(self.source), SIGNAL_LINE, 10, 1, 1,
                    "FAIL!  : ProbeTest::signalPayload() Compared values are not the same\n"
                    "   Actual   (emitted.count()): 0\n   Expected (1): 1\n"
                    "Totals: 2 passed, 1 failed, 0 skipped\n", "", ""]

    def write_report(self, rows, version="0.39.1"):
        with closing(sqlite3.connect(self.report)) as database:
            database.execute("CREATE TABLE information (key TEXT, value TEXT)")
            database.execute("INSERT INTO information VALUES ('Mull Version', ?)", (version,))
            database.execute("CREATE TABLE mutant (mutant_id TEXT, mutator TEXT, filename TEXT, "
                             "line_number INTEGER, column_number INTEGER, execution_status INTEGER, "
                             "exit_status INTEGER, stdout TEXT, stderr TEXT, mutation_replacement TEXT)")
            database.executemany("INSERT INTO mutant VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", rows)
            database.commit()

    def test_one_signal_removal_assertion_kill_passes(self):
        self.write_report([self.row])
        proof = check_report(self.report, self.source)
        self.assertEqual(proof["mutant_id"], "signal-mutant")
        self.assertEqual(proof["summary"]["assertion_killed"], 1)
        self.assertEqual(proof["summary"]["unique_mutants"], 1)

    def test_duplicate_public_identity_cannot_hide_cleanup_mutant(self):
        self.write_report([self.row, self.row])
        with self.assertRaisesRegex(ValueError, "exactly one signal-call mutant"):
            check_report(self.report, self.source)

    def test_survivor_crash_timeout_and_uncovered_never_pass(self):
        for status, exit_code in [(2, 0), (3, 0), (4, -11), (8, 0)]:
            with self.subTest(status=status):
                self.report.unlink(missing_ok=True)
                row = list(self.row)
                row[5:7] = [status, exit_code]
                self.write_report([row])
                with self.assertRaisesRegex(ValueError, "emitted.count"):
                    check_report(self.report, self.source)

    def test_unrelated_assertion_is_not_signal_removal_proof(self):
        row = list(self.row)
        row[7] = row[7].replace("emitted.count()", "payload.length()")
        self.write_report([row])
        with self.assertRaisesRegex(ValueError, "emitted.count"):
            check_report(self.report, self.source)

    def test_wrong_scope_or_operator_is_rejected(self):
        for index, value in [(1, "cxx_add_to_sub"), (2, str(self.root / "test_probe.cpp")),
                             (3, SIGNAL_LINE + 1)]:
            with self.subTest(field=index):
                self.report.unlink(missing_ok=True)
                row = list(self.row)
                row[index] = value
                self.write_report([row])
                with self.assertRaisesRegex(ValueError, "expected probe.cpp signal-call"):
                    check_report(self.report, self.source)

    def test_missing_or_unknown_version_report_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "missing report"):
            check_report(self.report, self.source)
        self.write_report([self.row], version="unknown")
        with self.assertRaisesRegex(ValueError, "Mull version"):
            check_report(self.report, self.source)

    def test_compiler_driver_symlink_name_is_preserved(self):
        driver = self.root / "clang++"
        driver.symlink_to(sys.executable)
        self.assertEqual(executable(str(driver), "compiler"), driver)

    def test_probe_only_inventory_rejects_mapping_and_test_instrumentation(self):
        plugin = self.root / "mull.so"
        inventory = self.root / "compile_commands.json"
        original = [
            {"file": str(self.source), "arguments": ["clang++", "-c", str(self.source),
             f"-fpass-plugin={plugin}", "-fprofile-instr-generate", "-grecord-command-line"]},
            {"file": str(self.root / "test_probe.cpp"), "arguments": ["clang++", "-c", "test_probe.cpp"]},
        ]
        inventory.write_text(json.dumps(original))
        self.assertEqual(check_compile_inventory(inventory, self.source, plugin), original)
        for mode in ("mapping", "test-instrumentation", "missing-probe"):
            with self.subTest(mode=mode):
                rows = json.loads(json.dumps(original))
                if mode == "mapping":
                    rows[1]["arguments"].append("-fcoverage-mapping")
                elif mode == "test-instrumentation":
                    rows[1]["arguments"].append(f"-fpass-plugin={plugin}")
                else:
                    rows = rows[1:]
                inventory.write_text(json.dumps(rows))
                with self.assertRaises(ValueError):
                    check_compile_inventory(inventory, self.source, plugin)

    def test_timed_out_native_command_is_reaped_and_retained(self):
        commands = Commands(self.root, os.environ.copy(), 1)
        with self.assertRaisesRegex(ValueError, "exceeded 1s"):
            commands.run("timeout", [sys.executable, "-c",
                                    "import time; print('started', flush=True); time.sleep(30)"])
        self.assertTrue(commands.records[0]["timed_out"])
        self.assertEqual(commands.records[0]["status"], "FAIL")
        self.assertIn("started", (self.root / commands.records[0]["log"]).read_text())

    def test_failed_native_command_retains_diagnostic_and_exit(self):
        commands = Commands(self.root, os.environ.copy(), 5)
        with self.assertRaisesRegex(ValueError, "exited 7"):
            commands.run("failure", [sys.executable, "-c", "print('native diagnostic'); raise SystemExit(7)"])
        self.assertEqual(commands.records[0]["status"], "FAIL")
        self.assertEqual(commands.records[0]["exit_code"], 7)
        self.assertIn("native diagnostic", (self.root / commands.records[0]["log"]).read_text())


if __name__ == "__main__":
    unittest.main()
