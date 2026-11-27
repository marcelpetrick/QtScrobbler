"""Black-box checks of mandatory pipeline failure propagation using disposable tools."""

import os
import json
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PipelineTests(unittest.TestCase):
    def run_pipeline(self, *, cli_status=0, ctest_status=0, metadata=True, hang=False, full=False, fail_stage=None, unfinished=False, catalogs=True, native_environment=True, support_artifacts=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "source"
            for directory in ("src/lib", "src/cli", "src/qt/src", "src/language", "tests", "tools/native"):
                (fixture / directory).mkdir(parents=True, exist_ok=True)
            (fixture / "src/lib/common.h").write_text('#define CLIENT_VERSION "0.0.1"\n')
            (fixture / "tools/native/format_fixture.cpp").write_text("int native_format_fixture;\n")
            if catalogs:
                translation = '<translation type="unfinished">' if unfinished else '<translation>'
                (fixture / "src/language/de.ts").write_text(
                    '<TS><context><source>Fixture</source>' + translation + 'Fixture</translation></context></TS>\n')
            shutil.copy2(ROOT / "localPipeline.sh", fixture / "localPipeline.sh")
            tools = root / "bin"
            tools.mkdir()
            build = root / "build"
            reports = root / "reports"
            calls = root / "tool-calls.jsonl"

            def executable(name, body):
                path = tools / name
                path.write_text(f"#!{sys.executable}\n" + body)
                path.chmod(0o755)

            executable("qmake6", 'print("QMake version 3.1\\nUsing Qt version 6.12.0 in /fake")\n')
            executable("clang-format",
                       "import sys, json\nfrom pathlib import Path\n"
                       f"with Path({str(calls)!r}).open('a') as stream: stream.write(json.dumps(sys.argv) + '\\n')\n"
                       f"sys.exit({1 if fail_stage == 'Format' else 0})\n")
            executable("python3",
                       "import sys, json, subprocess\nfrom pathlib import Path\nargs=sys.argv\n"
                       f"with Path({str(calls)!r}).open('a') as stream: stream.write(json.dumps(args) + '\\n')\n"
                       "if any('run_offline.py' in a for a in args): sys.exit(subprocess.run(args[args.index('--')+1:]).returncode)\n"
                       "stage='static' if '--stage' not in args else args[args.index('--stage')+1]\n"
                       "if any('run_coverage.py' in a for a in args): stage='Coverage'\n"
                       "if any('run_mutation_tests.py' in a for a in args): stage='Mutation'\n"
                       "if any('check_mutation_toolchain.py' in a for a in args): stage='MutationToolchain'\n"
                       f"sys.exit(1 if stage == {fail_stage!r} else 0)\n")
            executable("g++", 'print("fake compiler")\n')
            executable("native-cc", 'print("fake native C compiler")\n')
            executable("native-cxx", 'print("fake native C++ compiler")\n')
            executable("pkg-config", 'print("1.1.23")\n')
            executable(
                "cmake",
                "from pathlib import Path\nimport sys, json\n"
                f"with Path({str(calls)!r}).open('a') as stream: stream.write(json.dumps(sys.argv) + '\\n')\n"
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
            for name in ("CC", "CXX", "QTSCROBBLER_MULL_CALLFIX_MANIFEST", "QTSCROBBLER_MULL_CALLFIX_SHIM",
                         "QTSCROBBLER_MULL_CALLFIX_SOURCE"):
                environment.pop(name, None)
            if native_environment:
                environment["CC"] = str(tools / "native-cc")
                environment["CXX"] = str(tools / "native-cxx")
            if support_artifacts:
                for suffix in ("MANIFEST", "SHIM", "SOURCE"):
                    artifact = root / f"native support {suffix}"
                    artifact.touch()
                    environment[f"QTSCROBBLER_MULL_CALLFIX_{suffix}"] = str(artifact)
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
            result.tool_calls = [json.loads(line) for line in calls.read_text().splitlines()]
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

    def test_existing_reports_are_preserved_before_any_stage(self):
        for mode in ("--quick", "--baseline", "--mutation"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                reports = Path(temporary) / "reports"
                reports.mkdir()
                previous = reports / ".historical-result"
                previous.write_bytes(b"original evidence\n")
                result = subprocess.run(
                    ["bash", str(ROOT / "localPipeline.sh"), mode, "--report-dir", str(reports)],
                    text=True, capture_output=True, timeout=5, check=False)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("Report directory must be new or empty", result.stderr)
                self.assertEqual(previous.read_bytes(), b"original evidence\n")
                self.assertEqual(list(reports.iterdir()), [previous])

    def test_green_baseline(self):
        result = self.run_pipeline()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout, r"Unit Tests\s+: PASS")

    def test_full_gate_runs_each_stage_and_propagates_failures(self):
        result = self.run_pipeline(full=True)
        self.assertEqual(result.returncode, 0, result.stdout)
        stages = ("Format", "static", "docs", "Sanitizers", "Coverage", "MutationToolchain", "Mutation", "Package")
        for stage in stages:
            self.assertRegex(result.stdout, rf"{stage}\s+: PASS")
        for stage in stages:
            with self.subTest(stage=stage):
                result = self.run_pipeline(full=True, fail_stage=stage if stage != "Package" else "package")
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertRegex(result.stdout, rf"{stage}\s+: FAIL")

    def test_failed_semantic_preflight_blocks_untrusted_campaign(self):
        result = self.run_pipeline(full=True, fail_stage="MutationToolchain")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout, r"MutationToolchain\s+: FAIL")
        self.assertRegex(result.stdout, r"Mutation\s+: FAIL")
        self.assertRegex(result.stdout, r"Package\s+: PASS")
        self.assertFalse(any("run_mutation_tests.py" in argument
                             for call in result.tool_calls for argument in call))

    def test_missing_native_compilers_fail_preflight(self):
        result = self.run_pipeline(full=True, native_environment=False)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("executable CC and CXX", result.stdout)
        self.assertRegex(result.stdout, r"MutationToolchain\s+: FAIL")
        self.assertFalse(any("run_mutation_tests.py" in argument
                             for call in result.tool_calls for argument in call))

    def test_mutation_uses_explicit_compilers_workers_and_support_artifacts(self):
        result = self.run_pipeline(full=True, support_artifacts=True)
        self.assertEqual(result.returncode, 0, result.stdout)
        configure = next(call for call in result.tool_calls
                         if any(argument.startswith("-DCMAKE_CXX_COMPILER=") for argument in call))
        self.assertTrue(any(argument.endswith("/native-cc") and argument.startswith("-DCMAKE_C_COMPILER=")
                            for argument in configure))
        self.assertTrue(any(argument.endswith("/native-cxx") and argument.startswith("-DCMAKE_CXX_COMPILER=")
                            for argument in configure))
        campaign = next(call for call in result.tool_calls
                        if any("run_mutation_tests.py" in argument for argument in call))
        self.assertEqual(campaign[campaign.index("--workers") + 1], "1")
        self.assertEqual(campaign[campaign.index("--command-timeout") + 1], "7200")
        # Ratchet: the pipeline passes its enforced minimum explicitly.
        self.assertEqual(campaign[campaign.index("--minimum") + 1], "80")
        self.assertTrue(campaign[campaign.index("--toolchain-summary") + 1].endswith(
            "/mutation-toolchain/summary.json"))
        preflight = next(call for call in result.tool_calls
                         if any("check_mutation_toolchain.py" in argument for argument in call))
        artifacts = [preflight[index + 1] for index, argument in enumerate(preflight) if argument == "--artifact"]
        self.assertEqual(len(artifacts), 3)
        self.assertTrue(all("native support " in path for path in artifacts))
        self.assertTrue(preflight[preflight.index("--report-dir") + 1].endswith("/mutation-toolchain"))
        formatting = next(call for call in result.tool_calls if Path(call[0]).name == "clang-format")
        self.assertTrue(any(argument.endswith("/tools/native/format_fixture.cpp") for argument in formatting))

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
