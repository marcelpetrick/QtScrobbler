"""Black-box checks of mandatory pipeline failure propagation using disposable tools."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PipelineTests(unittest.TestCase):
    def run_pipeline(self, *, cli_status=0, ctest_status=0, metadata=True, hang=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tools = root / "bin"
            tools.mkdir()
            build = root / "build"
            reports = root / "reports"

            def executable(name, body):
                path = tools / name
                path.write_text(f"#!{sys.executable}\n" + body)
                path.chmod(0o755)

            executable("qmake6", 'print("QMake version 3.1\\nUsing Qt version 6.11.2 in /fake")\n')
            executable("g++", 'print("fake compiler")\n')
            executable("pkg-config", 'print("1.1.23")\n')
            executable(
                "cmake",
                "from pathlib import Path\nimport sys\n"
                'if "--version" in sys.argv: print("cmake version test"); sys.exit(0)\n'
                f"build=Path({str(build)!r})\nbuild.mkdir(exist_ok=True)\n"
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
            environment["PATH"] = str(tools) + os.pathsep + environment["PATH"]
            environment["QTSCROBBLER_BUILD_DIR"] = str(build)
            environment["QTSCROBBLER_JOBS"] = "1"
            result = subprocess.run(
                ["bash", str(ROOT / "localPipeline.sh"), "--noRun", "--noOpen",
                 "--report-dir", str(reports)],
                env=environment, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=15, check=False,
            )
            self.assertTrue((reports / "cmake_build.log").exists(), result.stdout)
            return result

    def test_green_baseline(self):
        result = self.run_pipeline()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout, r"Unit Tests\s+: PASS")

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
