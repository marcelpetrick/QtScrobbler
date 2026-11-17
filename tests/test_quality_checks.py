# SPDX-License-Identifier: GPL-2.0-only
"""Negative native quality gate contracts; failures must retain truthful evidence."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location(
    "quality_checks", Path(__file__).resolve().parents[1] / "tools/run_quality_checks.py")
QUALITY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(QUALITY)


class QualityChecksTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.checks = QUALITY.Checks(self.root, 2)

    def test_nonzero_exit_is_fatal_with_actual_exit_and_output(self):
        with self.assertRaisesRegex(ValueError, "exited 17"):
            self.checks.command("failing-tool", [sys.executable, "-c", "print('observed failure'); exit(17)"])
        record = self.checks.commands[0]
        self.assertEqual(record["status"], "FAIL")
        self.assertEqual(record["exit_code"], 17)
        self.assertIn("observed failure", (self.root / record["log"]).read_text())

    def test_timeout_is_fatal_and_preserves_partial_output(self):
        self.checks.timeout = 0.1
        with self.assertRaisesRegex(ValueError, "exceeded"):
            self.checks.command("timeout", [sys.executable, "-u", "-c",
                                            "import time; print('started'); time.sleep(5)"])
        record = self.checks.commands[0]
        self.assertTrue(record["timed_out"])
        self.assertEqual(record["status"], "FAIL")
        self.assertIn("started", (self.root / record["log"]).read_text())

    def test_missing_required_analyzer_is_fatal(self):
        with mock.patch.object(QUALITY.shutil, "which", return_value=None):
            with self.assertRaisesRegex(ValueError, "mandatory native tool is missing"):
                self.checks.tool("clang-tidy")
        self.assertEqual(self.checks.versions, {})

    def test_complete_production_scope_includes_unexecuted_mtp(self):
        source, build = self.root / "source", self.root / "build"
        build.mkdir()
        files = ["src/lib/core.cpp", "src/lib/parse-mtp-libmtp.cpp", "src/cli/main.cpp", "src/qt/src/ui.cpp"]
        rows = []
        for name in files:
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("int unexecuted() { return 0; }")
            rows.append({"file": str(path), "directory": str(build), "command": "c++ -c " + str(path)})
        inventory = build / "compile_commands.json"
        inventory.write_text(json.dumps(rows[:-1]))
        with self.assertRaisesRegex(ValueError, "omits maintained production"):
            QUALITY.production_commands(build, source)
        inventory.write_text(json.dumps(rows + [{"file": "/generated/moc_core.cpp", "command": "c++"}]))
        selected = QUALITY.production_commands(build, source)
        self.assertEqual(len(selected), 4)
        self.assertTrue(any("parse-mtp-libmtp.cpp" in row["file"] for row in selected))

    def test_broken_local_document_link_fails(self):
        (self.root / "README.md").write_text("# Readme\n\n[Missing](missing.md)\n")
        with mock.patch.object(QUALITY, "REQUIRED_DOCS", ("README.md",)):
            with self.assertRaisesRegex(ValueError, "broken local document link"):
                QUALITY.validate_docs(self.root)

    def test_unclosed_fence_fails(self):
        (self.root / "README.md").write_text("# Readme\n\n```sh\nunclosed\n")
        with mock.patch.object(QUALITY, "REQUIRED_DOCS", ("README.md",)):
            with self.assertRaisesRegex(ValueError, "unclosed Markdown"):
                QUALITY.validate_docs(self.root)

    def test_runtime_version_must_match_exactly_for_both_entry_points(self):
        for program in ("scrobbler", "QTScrob"):
            QUALITY.require_runtime_version(f"{program} (v0.13.36): help", "0.13.36", program)
            for output in (f"{program} (v0.13.360): help", "unrelated text 0.13.36"):
                with self.subTest(program=program, output=output), self.assertRaises(ValueError):
                    QUALITY.require_runtime_version(output, "0.13.36", program)

    def test_missing_package_binary_fails_before_execution(self):
        with self.assertRaisesRegex(ValueError, "package binaries"):
            self.checks.package(self.root / "empty-build")
        self.assertEqual(self.checks.commands, [])


if __name__ == "__main__":
    unittest.main()
