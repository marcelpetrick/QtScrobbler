# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Marcel Petrick
"""Accounting tests ensure crashes, timeouts and duplicate mutant runs cannot inflate score."""

from decimal import Decimal
from contextlib import closing
import importlib.util
import json
import sys
from pathlib import Path
import sqlite3
import tempfile
import unittest
from unittest import mock


SPEC = importlib.util.spec_from_file_location(
    "mutation_score", Path(__file__).resolve().parents[1] / "tools/mutation_score.py"
)
SCORE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCORE)
INSTALLER_SPEC = importlib.util.spec_from_file_location(
    "install_mutation_toolchain",
    Path(__file__).resolve().parents[1] / "tools/install_mutation_toolchain.py",
)
INSTALLER = importlib.util.module_from_spec(INSTALLER_SPEC)
INSTALLER_SPEC.loader.exec_module(INSTALLER)
RUNNER_SPEC = importlib.util.spec_from_file_location(
    "run_mutation_tests", Path(__file__).resolve().parents[1] / "tools/run_mutation_tests.py"
)
RUNNER = importlib.util.module_from_spec(RUNNER_SPEC)
with mock.patch.dict(sys.modules, {"mutation_score": SCORE}):
    RUNNER_SPEC.loader.exec_module(RUNNER)
ASSERTION = "FAIL!  : Test::boundary() assertion failed\nTotals: 0 passed, 1 failed, 0 skipped,\n"


class MutationScoreTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def report(self, name, rows, version="0.38.0"):
        path = self.root / name
        with closing(sqlite3.connect(path)) as database, database:
            database.execute("CREATE TABLE information (key TEXT, value TEXT)")
            database.execute("INSERT INTO information VALUES ('Mull Version', ?)", (version,))
            database.execute("""CREATE TABLE mutant (
                mutant_id TEXT, mutator TEXT, filename TEXT, line_number INT,
                column_number INT, execution_status INT, exit_status INT,
                stdout TEXT, stderr TEXT, mutation_replacement TEXT)""")
            for identity, status, exit_status, output in rows:
                database.execute("INSERT INTO mutant VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                                 (identity, "cxx_lt_to_le", "/source/src/lib/common.cpp", 23,
                                  22, status, exit_status, output, "", "<="))
        return path

    def build_fixture(self):
        source = self.root / "source"
        build = self.root / "build"
        build.mkdir()
        plugin = self.root / "mull-plugin.so"
        plugin.write_bytes(b"qualified test plugin")
        commands = []
        for folder in ("src/lib", "src/cli", "src/qt/src"):
            directory = source / folder
            directory.mkdir(parents=True)
            path = directory / "example.cpp"
            path.write_text("int example() { return 1; }\n")
            commands.append({"file": str(path), "directory": str(build),
                             "command": f"clang++ -fprofile-instr-generate -fcoverage-mapping -fpass-plugin={plugin} -c {path}"})
        (build / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={source}\nQTSCROBBLER_MTP:BOOL=ON\n"
            f"QTSCROBBLER_MULL_PLUGIN:FILEPATH={plugin}\n")
        (build / "compile_commands.json").write_text(json.dumps(commands))
        return source, build, commands

    def test_mutation_scope_requires_selected_checkout_and_mtp(self):
        source, build, _ = self.build_fixture()
        provenance = RUNNER.validate_build(build, source)
        self.assertEqual(len(provenance["production_compile_inventory"]), 3)
        with self.assertRaisesRegex(ValueError, "source checkout"):
            RUNNER.validate_build(build, self.root)
        cache = build / "CMakeCache.txt"
        cache.write_text(cache.read_text().replace("MTP:BOOL=ON", "MTP:BOOL=OFF"))
        with self.assertRaisesRegex(ValueError, "MTP=ON"):
            RUNNER.validate_build(build, source)

    def test_mutation_scope_cannot_omit_unexecuted_production_source(self):
        source, build, commands = self.build_fixture()
        new_source = source / "src/lib/never_executed.cpp"
        new_source.write_text("int unused() { return 10; }\n")
        with self.assertRaisesRegex(ValueError, "omits production files"):
            RUNNER.validate_build(build, source)
        commands.append({"file": str(new_source), "directory": str(build),
                         "command": f"clang++ -c {new_source}"})
        (build / "compile_commands.json").write_text(json.dumps(commands))
        with self.assertRaisesRegex(ValueError, "lacks mutation instrumentation"):
            RUNNER.validate_build(build, source)

    def test_mutation_scope_requires_coverage_mapping_for_uncovered_status(self):
        source, build, commands = self.build_fixture()
        for flag in ("-fprofile-instr-generate", "-fcoverage-mapping"):
            changed = [dict(entry) for entry in commands]
            changed[0]["command"] = changed[0]["command"].replace(flag, "")
            (build / "compile_commands.json").write_text(json.dumps(changed))
            with self.subTest(flag=flag), self.assertRaisesRegex(ValueError, "coverage profiling"):
                RUNNER.validate_build(build, source)

    def test_missing_compile_inventory_fails_closed(self):
        source, build, _ = self.build_fixture()
        (build / "compile_commands.json").unlink()
        with self.assertRaisesRegex(ValueError, "EXPORT_COMPILE_COMMANDS"):
            RUNNER.validate_build(build, source)

    def test_campaign_requires_rebuilt_object_outputs(self):
        source, build, commands = self.build_fixture()
        for index, entry in enumerate(commands):
            entry["command"] += f" -o source{index}.o"
        (build / "compile_commands.json").write_text(json.dumps(commands))
        with self.assertRaisesRegex(ValueError, "object output is missing"):
            RUNNER.validate_build(build, source, require_objects=True)
        for index in range(len(commands)):
            (build / f"source{index}.o").write_bytes(b"fresh compiled production object")
        result = RUNNER.validate_build(build, source, require_objects=True)
        self.assertTrue(all(row.get("object_sha256")
                            for row in result["production_compile_inventory"].values()))

    def test_source_fingerprint_includes_test_contracts(self):
        source, _, _ = self.build_fixture()
        (source / "tests").mkdir()
        test = source / "tests/test_contract.cpp"
        test.write_text("original assertions")
        before = RUNNER.source_hashes(source)
        test.write_text("changed assertions")
        self.assertNotEqual(before, RUNNER.source_hashes(source))

    def test_build_definitions_python_contracts_and_resources_are_fingerprinted(self):
        source, _, _ = self.build_fixture()
        for name in ("CMakeLists.txt", "cmake/Instrumentation.cmake", "tests/test_gate.py",
                     "tools/mutation_score.py", "src/qt/resources/catalog.qrc", "mull.yml"):
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("original definition")
            before = RUNNER.source_hashes(source)
            path.write_text("changed definition")
            with self.subTest(name=name):
                self.assertNotEqual(before, RUNNER.source_hashes(source))
        cache = source / "tests/__pycache__/test_gate.pyc"
        cache.parent.mkdir()
        before = RUNNER.source_hashes(source)
        cache.write_bytes(b"disposable bytecode")
        self.assertEqual(before, RUNNER.source_hashes(source))

    def test_duplicate_runs_count_once_and_any_assertion_kills(self):
        first = self.report("first.sqlite", [("same", 2, 0, ""), ("other", 8, 0, "")])
        second = self.report("second.sqlite", [("same", 1, 1, ASSERTION)])
        result = SCORE.summarize([first, second])
        self.assertEqual(result["unique_mutants"], 2)
        self.assertEqual(result["assertion_killed"], 1)
        self.assertEqual(result["score_percent"], "50")
        self.assertTrue(SCORE.passes(result, Decimal("50")))
        self.assertFalse(SCORE.passes(result, Decimal("50.0000001")))

    def test_timeout_crash_and_unexplained_failure_are_not_kills(self):
        report = self.report("outcomes.sqlite", [
            ("timeout", 3, -1, ASSERTION), ("crash", 4, -1, ASSERTION),
            ("signal", 1, -1, ASSERTION), ("unchecked-exit", 1, 1, ""),
            ("abnormal", 5, 1, ASSERTION), ("uncovered", 8, 0, ""),
            ("fatal", 1, 1, ASSERTION + "QFATAL: failure"),
        ])
        result = SCORE.summarize([report])
        self.assertEqual(result["unique_mutants"], 7)
        self.assertEqual(result["assertion_killed"], 0)
        self.assertFalse(SCORE.passes(result, Decimal("95")))

    def test_infrastructure_failure_blocks_even_zero_threshold(self):
        report = self.report("spawn.sqlite", [("spawn", 1, -1, "Failed to spawn: no file")])
        result = SCORE.summarize([report])
        self.assertTrue(result["infrastructure_errors"])
        self.assertFalse(SCORE.passes(result, Decimal("0")))

    def test_fail_fast_needs_another_assertion_kill(self):
        report = self.report("fast.sqlite", [("fast", 7, 0, "")])
        self.assertFalse(SCORE.passes(SCORE.summarize([report]), Decimal("0")))
        other = self.report("killed.sqlite", [("fast", 1, 1, ASSERTION)])
        self.assertTrue(SCORE.passes(SCORE.summarize([report, other]), Decimal("100")))

    def test_missing_empty_unknown_version_and_invalid_status_fail_closed(self):
        cases = [[], [self.root / "missing.sqlite"],
                 [self.report("empty.sqlite", [])],
                 [self.report("version.sqlite", [("one", 1, 1, ASSERTION)], "0.99.0")],
                 [self.report("status.sqlite", [("one", 99, 0, "")])]]
        for reports in cases:
            with self.subTest(reports=reports), self.assertRaises(ValueError):
                SCORE.summarize(reports)

    def test_rounding_cannot_raise_score_to_threshold(self):
        summary = {"infrastructure_errors": [], "assertion_killed": 18999, "unique_mutants": 20000}
        self.assertFalse(SCORE.passes(summary, Decimal("95")))
        for minimum in [Decimal("-1"), Decimal("101"), Decimal("NaN")]:
            with self.subTest(minimum=minimum), self.assertRaises(ValueError):
                SCORE.passes(summary, minimum)

    def test_generated_code_cannot_leak_into_denominator(self):
        report = self.report("generated.sqlite", [("generated", 1, 1, ASSERTION)])
        with closing(sqlite3.connect(report)) as database, database:
            database.execute("UPDATE mutant SET filename = '/build/src/lib/scrobble_autogen/moc_parse.cpp'")
        with self.assertRaisesRegex(ValueError, "generated code leaked"):
            SCORE.summarize([report])

    def test_corrupt_cached_tool_is_never_extracted_or_executed(self):
        prefix = self.root / "native-prefix"
        downloads = prefix / "downloads"
        downloads.mkdir(parents=True)
        (downloads / "bad.deb").write_bytes(b"corrupt tool archive")
        with mock.patch.object(INSTALLER, "ARTIFACTS", [("https://example.invalid/bad.deb", "0" * 64)]), \
                mock.patch.object(INSTALLER.platform, "system", return_value="Linux"), \
                mock.patch.object(INSTALLER.platform, "machine", return_value="x86_64"), \
                mock.patch.object(INSTALLER.shutil, "which", return_value="/usr/bin/bsdtar"), \
                mock.patch.object(INSTALLER.subprocess, "check_output") as execute:
            with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                INSTALLER.install(prefix)
            execute.assert_not_called()


if __name__ == "__main__":
    unittest.main()
