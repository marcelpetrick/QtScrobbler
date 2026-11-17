#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Offline fixtures reject stale native compiler, shim and semantic-proof evidence."""
from contextlib import closing
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import sqlite3
import sys
import subprocess
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_mutation_toolchain import SIGNAL_LINE, check_report
from mutation_toolchain_evidence import digest, validate
import install_mull_callfix as callfix


class ToolchainEvidenceTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.project = self.root / "project"
        self.build = self.root / "build"
        self.build.mkdir()
        self.reports = self.root / "sentinel"
        (self.reports / "source").mkdir(parents=True)
        (self.reports / "build").mkdir()
        self.official = self.root / "official"
        self.compiler = self.artifact(self.official / "usr/bin/clang++-20", b"\x7fELFcompiler")
        self.plugin = self.artifact(self.official / "usr/lib/mull-ir-frontend-20", b"plugin")
        self.runner = self.artifact(self.official / "usr/bin/mull-runner", b"\x7fELFrunner")
        self.artifact(self.official / "usr/lib/x86_64-linux-gnu/libLLVM.so.20.1", b"LLVM library")
        self.artifact(self.official / "env.sh", b"qualified environment")
        self.summary = self.reports / "summary.json"
        for name in ("CMakeLists.txt", "mull.yml", "probe.h", "probe.cpp", "test_probe.cpp"):
            (self.reports / "source" / name).write_text("fixture " + name)
        self.artifact(self.reports / "build/test_probe", b"\x7fELFprobe")
        self.probe_commands = [{"file": str(self.reports / "source/probe.cpp"), "arguments": [
            str(self.compiler), "-c", "probe.cpp", f"-fpass-plugin={self.plugin}",
            "-fprofile-instr-generate", "-grecord-command-line"]}]
        (self.reports / "build/compile_commands.json").write_text(json.dumps(self.probe_commands))
        report = self.reports / "signal-removal.sqlite"
        with closing(sqlite3.connect(report)) as database, database:
            database.execute("CREATE TABLE information (key TEXT, value TEXT)")
            database.execute("INSERT INTO information VALUES ('Mull Version', '0.38.0')")
            database.execute("CREATE TABLE mutant (mutant_id TEXT, mutator TEXT, filename TEXT, "
                             "line_number INTEGER, column_number INTEGER, execution_status INTEGER, "
                             "exit_status INTEGER, stdout TEXT, stderr TEXT, mutation_replacement TEXT)")
            output = ("FAIL!  : ProbeTest::signalPayload() Compared values differ\n"
                      "Actual   (emitted.count()): 0\nTotals: 2 passed, 1 failed, 0 skipped\n")
            database.execute("INSERT INTO mutant VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                             ("signal", "cxx_remove_void_call", str(self.reports / "source/probe.cpp"),
                              SIGNAL_LINE, 10, 1, 1, output, "", ""))
        stamp = datetime.now(timezone.utc).isoformat()
        self.data = {"schema_version": 1, "status": "PASS", "started_utc": stamp, "finished_utc": stamp,
                     "toolchain": {name: self.record(path) for name, path in (
                         ("compiler", self.compiler), ("plugin", self.plugin), ("runner", self.runner))},
                     "source_sha256": {path.name: digest(path) for path in (self.reports / "source").iterdir()},
                     "binary_sha256": digest(self.reports / "build/test_probe"),
                     "compile_commands_sha256": digest(self.reports / "build/compile_commands.json"),
                     "compile_inventory": self.probe_commands,
                     "semantic_proof": check_report(report, self.reports / "source/probe.cpp"),
                     "support_artifacts": [],
                     "native_environment": {name: os.environ.get(name, "")
                                            for name in ("LD_LIBRARY_PATH", "LD_PRELOAD")}}
        self.set_compiler(self.compiler)
        self.write_summary()

    @staticmethod
    def artifact(path, content):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        path.chmod(0o755)
        return path

    @staticmethod
    def record(path):
        return {"path": str(path), "sha256": digest(path)}

    def set_compiler(self, compiler):
        self.compiler = compiler
        (self.build / "CMakeCache.txt").write_text(
            f"CMAKE_CXX_COMPILER:FILEPATH={compiler}\nQTSCROBBLER_MULL_PLUGIN:FILEPATH={self.plugin}\n")
        self.production = {"src/lib/example.cpp": {"arguments": [str(compiler), "-c", "example.cpp"]}}

    def write_summary(self):
        self.summary.write_text(json.dumps(self.data))

    def check(self):
        return validate(self.summary, self.build, self.runner, self.production, self.project)

    def add_callfix(self):
        for name in ("tools/install_mull_callfix.py", "tools/native/void_call_fix.cpp",
                     "tools/native/callfix_cfg_test.cpp"):
            source = Path(__file__).resolve().parents[1] / name
            self.artifact(self.project / name, source.read_bytes())
        self.prefix = self.root / "callfix"
        wrapper = self.artifact(self.prefix / "bin/clang++-20-callfix", b"#!/usr/bin/python3\n# fixture wrapper\n")
        self.shim = self.artifact(self.prefix / "lib/libmull-callfix.so", b"qualified shim")
        self.artifact(self.prefix / "native/void_call_fix.cpp",
                      (self.project / "tools/native/void_call_fix.cpp").read_bytes())
        self.artifact(self.prefix / "native/callfix_cfg_test.cpp",
                      (self.project / "tools/native/callfix_cfg_test.cpp").read_bytes())
        self.artifact(self.prefix / "native/callfix-cfg-test", b"\x7fELFCFG test")
        self.artifact(self.prefix / "env.sh", b"native wrapper environment")
        official = {}
        for relative in ("usr/bin/clang++-20", "usr/lib/mull-ir-frontend-20",
                         "usr/lib/x86_64-linux-gnu/libLLVM.so.20.1", "env.sh"):
            path = self.official / relative
            official[relative] = dict(self.record(path), resolved=str(path.resolve()))
        self.callfix_manifest = self.prefix / callfix.MANIFEST
        self.callfix_manifest.write_text(json.dumps({
            "schema_version": 1,
            "build_inputs": {"patch_id": callfix.PATCH_ID, "artifacts": callfix.artifact_resolution(),
                             "native_source_sha256": digest(self.project / "tools/native/void_call_fix.cpp"),
                             "cfg_test_sha256": digest(self.project / "tools/native/callfix_cfg_test.cpp"),
                             "installer_sha256": digest(self.project / "tools/install_mull_callfix.py"),
                             "official_toolchain": official},
            "installed_files": callfix.inventory(self.prefix)}))
        self.set_compiler(wrapper)
        self.data["toolchain"]["compiler"] = self.record(wrapper)
        self.data["support_artifacts"] = [self.record(self.callfix_manifest), self.record(self.shim)]
        self.write_summary()

    def test_standalone_campaign_requires_semantic_summary_before_creating_reports(self):
        runner = Path(__file__).resolve().parents[1] / "tools/run_mutation_tests.py"
        reports = self.root / "campaign-reports"
        result = subprocess.run([sys.executable, str(runner), "--build-dir", str(self.build),
                                 "--report-dir", str(reports)], capture_output=True, text=True,
                                timeout=5, check=False)
        self.assertEqual(result.returncode, 2)
        self.assertIn("--toolchain-summary", result.stderr)
        self.assertFalse(reports.exists())

    def test_exact_unchanged_native_proof_passes(self):
        evidence = self.check()
        self.assertEqual(evidence["summary_sha256"], digest(self.summary))
        self.assertEqual(evidence["semantic_proof"]["summary"]["assertion_killed"], 1)
        self.assertIsNone(evidence["callfix"])

    def test_failed_or_fabricated_kill_summary_is_rejected(self):
        self.data["status"] = "FAIL"
        self.write_summary()
        with self.assertRaisesRegex(ValueError, "passing native semantic"):
            self.check()
        self.data["status"] = "PASS"
        self.data["semantic_proof"]["summary"]["assertion_killed"] = 0
        self.write_summary()
        with self.assertRaisesRegex(ValueError, "differs from its native"):
            self.check()

    def test_changed_compiler_plugin_and_runner_are_rejected(self):
        for path in (self.compiler, self.plugin, self.runner):
            with self.subTest(path=path):
                content = path.read_bytes()
                path.write_bytes(content + b"changed")
                with self.assertRaisesRegex(ValueError, "changed since semantic"):
                    self.check()
                path.write_bytes(content)

    def test_changed_loader_search_or_preload_environment_is_rejected(self):
        for name in ("LD_LIBRARY_PATH", "LD_PRELOAD"):
            with self.subTest(variable=name), mock.patch.dict(os.environ, {name: "changed-native-loader-input"}):
                with self.assertRaisesRegex(ValueError, "loader environment differs"):
                    self.check()

    def test_missing_loader_environment_cannot_reuse_semantic_proof(self):
        del self.data["native_environment"]
        self.write_summary()
        with self.assertRaisesRegex(ValueError, "loader environment is missing"):
            self.check()

    def test_compiler_cache_path_must_match_proved_driver_name(self):
        replacement = self.artifact(self.root / "clang", self.compiler.read_bytes())
        self.set_compiler(replacement)
        with self.assertRaisesRegex(ValueError, "selected compiler differs"):
            self.check()

    def test_each_production_command_must_use_proved_compiler_without_launcher(self):
        self.production["src/lib/example.cpp"]["arguments"] = [str(self.runner), str(self.compiler), "-c", "source.cpp"]
        with self.assertRaisesRegex(ValueError, "bypasses the proved compiler"):
            self.check()

    def test_changed_probe_source_binary_inventory_and_raw_report_are_rejected(self):
        for relative in ("source/probe.cpp", "build/test_probe", "build/compile_commands.json"):
            with self.subTest(path=relative):
                path = self.reports / relative
                content = path.read_bytes()
                path.write_bytes(content + b"changed")
                with self.assertRaises(ValueError):
                    self.check()
                path.write_bytes(content)
        with closing(sqlite3.connect(self.reports / "signal-removal.sqlite")) as database, database:
            database.execute("UPDATE mutant SET execution_status=2, exit_status=0")
        with self.assertRaisesRegex(ValueError, "emitted.count"):
            self.check()

    def test_missing_summary_and_fixture_fingerprints_fail_closed(self):
        self.summary.unlink()
        with self.assertRaises(OSError):
            self.check()
        self.data["source_sha256"] = {}
        self.write_summary()
        with self.assertRaisesRegex(ValueError, "fixture fingerprint inventory"):
            self.check()

    def test_wrapper_requires_qualified_support_manifest(self):
        self.compiler.write_bytes(b"#!/bin/sh\n# changed compiler kind\n")
        self.data["toolchain"]["compiler"] = self.record(self.compiler)
        self.write_summary()
        with self.assertRaisesRegex(ValueError, "qualified callfix manifest"):
            self.check()

    def test_recognized_unchanged_callfix_closure_passes(self):
        self.add_callfix()
        result = self.check()
        self.assertEqual(result["callfix"]["sha256"], digest(self.callfix_manifest))
        self.assertIn("usr/lib/x86_64-linux-gnu/libLLVM.so.20.1", result["callfix"]["build_inputs"]["official_toolchain"])

    def test_tampered_shim_cfg_or_official_llvm_fail_with_unchanged_manifest(self):
        self.add_callfix()
        for path in (self.shim, self.prefix / "native/callfix_cfg_test.cpp",
                     self.official / "usr/lib/x86_64-linux-gnu/libLLVM.so.20.1"):
            with self.subTest(path=path):
                content = path.read_bytes()
                path.write_bytes(content + b"tampered")
                with self.assertRaises(ValueError):
                    self.check()
                path.write_bytes(content)

    def test_changed_project_native_source_invalidates_installer_proof(self):
        self.add_callfix()
        (self.project / "tools/native/void_call_fix.cpp").write_text("different reviewed correction")
        with self.assertRaisesRegex(ValueError, "source/build input changed"):
            self.check()


if __name__ == "__main__":
    unittest.main()
