"""Black-box checks of mandatory pipeline failure propagation using disposable tools."""

import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PipelineTests(unittest.TestCase):
    def run_pipeline(self, *, cli_status=0, ctest_status=0, metadata=True, hang=False, full=False, fail_stage=None, unfinished=False, catalogs=True):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "source"
            for directory in ("src/lib", "src/cli", "src/qt/src", "src/language", "tests"):
                (fixture / directory).mkdir(parents=True, exist_ok=True)
            (fixture / "src/lib/common.h").write_text('#define CLIENT_VERSION "0.0.1"\n')
            if catalogs:
                translation = '<translation type="unfinished">' if unfinished else '<translation>'
                (fixture / "src/language/de.ts").write_text(
                    '<TS><context><source>Fixture</source>' + translation + 'Fixture</translation></context></TS>\n')
            shutil.copy2(ROOT / "localPipeline.sh", fixture / "localPipeline.sh")
            tools = root / "bin"
            tools.mkdir()
            build = root / "build"
            reports = root / "reports"

            def executable(name, body):
                path = tools / name
                path.write_text(f"#!{sys.executable}\n" + body)
                path.chmod(0o755)

            executable("qmake6", 'print("QMake version 3.1\\nUsing Qt version 6.12.0 in /fake")\n')
            executable("clang-format", f"import sys\nsys.exit({1 if fail_stage == 'Format' else 0})\n")
            executable("python3",
                       "import sys\nargs=sys.argv\n"
                       "stage='static' if '--stage' not in args else args[args.index('--stage')+1]\n"
                       "if any('run_coverage.py' in a for a in args): stage='Coverage'\n"
                       "if any('run_mutation_tests.py' in a for a in args): stage='Mutation'\n"
                       f"sys.exit(1 if stage == {fail_stage!r} else 0)\n")
            executable("g++", 'print("fake compiler")\n')
            executable("pkg-config", 'print("1.1.23")\n')
            executable(
                "cmake",
                "from pathlib import Path\nimport sys\n"
                'if "--version" in sys.argv: print("cmake version test"); sys.exit(0)\n'
                + ("if any('sanitizers' in a for a in sys.argv): sys.exit(1)\n" if fail_stage == "Sanitizers" else "")
                + f"build=Path({str(build)!r})\nbuild.mkdir(exist_ok=True)\n"
                + ('(build/"CTestTestfile.cmake").touch()\n' if metadata else "")
                + f"cli={('sleep 30' if hang else 'echo usage; exit ' + str(cli_status))!r}\n"
                'for name, body in [("scrobbler", cli), ("qtscrob", "exit 0")]:\n'
                ' p=build/name; p.write_text("#!/bin/sh\\n" + body + "\\n"); p.chmod(0o755)\n'
                '(build/"libscrobble.a").touch()\n',
            )
            executable(
                "ctest",
                "import sys\n"
                'assert "--no-tests=error" in sys.argv\n'
                'assert "--output-junit" in sys.argv\n'
                f"sys.exit({ctest_status})\n",
            )
            environment = os.environ.copy()
            environment.pop("QTSCROBBLER_QT_PREFIX", None)
            environment["PATH"] = str(tools) + os.pathsep + environment["PATH"]
            environment["QTSCROBBLER_BUILD_DIR"] = str(build)
            environment["QTSCROBBLER_JOBS"] = "1"
            plugin = root / "plugin"
            plugin.touch()
            environment["QTSCROBBLER_MULL_PLUGIN"] = str(plugin)
            arguments = ["bash", str(fixture / "localPipeline.sh"), "--noRun", "--noOpen",
                         "--report-dir", str(reports)]
            if not full:
                arguments.append("--quick")
            result = subprocess.run(
                arguments,
                env=environment, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=15, check=False,
            )
            self.assertTrue((reports / "cmake_build.log").exists(), result.stdout)
            return result

    def test_conflicting_modes_fail_before_build_or_reports(self):
        for flags in (("--quick", "--baseline"), ("--baseline", "--quick"),
                      ("--quick", "--mutation"), ("--mutation", "--quick"),
                      ("--baseline", "--mutation"), ("--mutation", "--baseline")):
            with self.subTest(flags=flags), tempfile.TemporaryDirectory() as temporary:
                reports = Path(temporary) / "reports"
                result = subprocess.run(
                    ["bash", str(ROOT / "localPipeline.sh"), *flags, "--report-dir", str(reports)],
                    text=True, capture_output=True, timeout=5, check=False)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("Choose only one mode", result.stderr)
                self.assertFalse(reports.exists())

    def test_green_baseline(self):
        result = self.run_pipeline()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout, r"Unit Tests\s+: PASS")

    def test_full_gate_runs_each_stage_and_propagates_failures(self):
        result = self.run_pipeline(full=True)
        self.assertEqual(result.returncode, 0, result.stdout)
        for stage in ("Format", "static", "docs", "Sanitizers", "Coverage", "Mutation", "Package"):
            with self.subTest(stage=stage):
                result = self.run_pipeline(full=True, fail_stage=stage if stage != "Package" else "package")
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertRegex(result.stdout, rf"{stage}\s+: FAIL")

    def test_unfinished_translations_are_fatal(self):
        result = self.run_pipeline(unfinished=True)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout, r"Translations\s+: FAIL")

    def test_missing_catalogs_are_fatal(self):
        result = self.run_pipeline(catalogs=False)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout, r"Translations\s+: FAIL")

    def test_failed_tests_are_fatal(self):
        result = self.run_pipeline(ctest_status=8)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout, r"Unit Tests\s+: FAIL")
        self.assertNotRegex(result.stdout, r"Unit Tests\s+: SKIP")

    def test_missing_suite_is_fatal(self):
        result = self.run_pipeline(metadata=False)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout, r"Unit Tests\s+: FAIL")

    def test_help_text_does_not_hide_nonzero_exit(self):
        result = self.run_pipeline(cli_status=7)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("--help exited 7", result.stdout)
        self.assertRegex(result.stdout, r"Smoke: CLI\s+: FAIL")

    def test_help_timeout_is_fatal(self):
        result = self.run_pipeline(hang=True)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("timed out after 5 s", result.stdout)
        self.assertRegex(result.stdout, r"Smoke: CLI\s+: FAIL")


if __name__ == "__main__":
    unittest.main()
