#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Marcel Petrick
"""Run a fresh native Mull campaign over CTest executables and both application binaries.

Build first with Clang and QTSCROBBLER_MULL_PLUGIN, then run:
  python3 tools/run_mutation_tests.py --build-dir BUILD --report-dir NEW_REPORT_DIRECTORY

Use --baseline to measure an incomplete suite without claiming the 95% gate passes.
No dependencies are installed and no container is used. Each report retains raw outcomes,
commands, binary/source/config hashes and the exact Mull version. Existing nonempty report
directories are rejected so a stale result cannot pass a fresh campaign.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
from decimal import Decimal
import hashlib
import json
import os
from pathlib import Path
import shutil
import shlex
import sqlite3
import subprocess
import sys
import tempfile

from mutation_score import passes, summarize


ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_hashes(root: Path = ROOT) -> dict[str, str]:
    """Fingerprint source, resources, test contracts, tooling and build definitions."""
    paths = {path for folder in ("src", "tests", "cmake", "tools")
             for path in (root / folder).rglob("*") if path.is_file()
             and "__pycache__" not in path.parts and path.suffix != ".pyc"}
    paths.update(path for name in ("CMakeLists.txt", "localPipeline.sh", "mull.yml", "Doxyfile",
                                  "requirements-quality.txt", ".clang-format")
                 if (path := root / name).is_file())
    return {str(path.relative_to(root)): sha256(path) for path in sorted(paths)}


def validate_build(build: Path, root: Path, *, require_objects: bool = False) -> dict:
    """Require the complete maintained source scope from the selected source checkout."""
    cache_path = build / "CMakeCache.txt"
    cache = {}
    for line in cache_path.read_text().splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line or ":" not in line:
            continue
        key, value = line.split("=", 1)
        cache[key.split(":", 1)[0]] = value
    if Path(cache.get("CMAKE_HOME_DIRECTORY", "")).resolve() != root:
        raise ValueError("CMake source checkout does not match --source-root")
    if cache.get("QTSCROBBLER_MTP") != "ON":
        raise ValueError("full mutation campaign requires QTSCROBBLER_MTP=ON")
    plugin = Path(cache.get("QTSCROBBLER_MULL_PLUGIN", ""))
    if not plugin.is_file():
        raise ValueError("build must select an existing QTSCROBBLER_MULL_PLUGIN")
    commands_path = build / "compile_commands.json"
    if not commands_path.is_file():
        raise ValueError("configure with CMAKE_EXPORT_COMPILE_COMMANDS=ON")
    commands = json.loads(commands_path.read_text())
    expected = {path.resolve() for folder in ("src/lib", "src/cli", "src/qt/src")
                for path in (root / folder).glob("*.cpp") if path.name != "parse-mtp-win32.cpp"}
    if not expected:
        raise ValueError("source checkout has no maintained production files")
    inventory = {}
    for entry in commands:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        path = path.resolve()
        if path not in expected:
            continue
        command = entry.get("command", " ".join(entry.get("arguments", [])))
        if f"-fpass-plugin={plugin}" not in command:
            raise ValueError(f"production file lacks mutation instrumentation: {path}")
        if any(flag not in command for flag in ("-fprofile-instr-generate", "-fcoverage-mapping")):
            raise ValueError(f"production file lacks mutation coverage profiling: {path}")
        record = dict(entry)
        if require_objects:
            tokens = entry.get("arguments") or shlex.split(command)
            if "-o" not in tokens or tokens.index("-o") + 1 >= len(tokens):
                raise ValueError(f"production compile command has no object output: {path}")
            output = Path(tokens[tokens.index("-o") + 1])
            if not output.is_absolute():
                output = Path(entry["directory"]) / output
            if not output.is_file():
                raise ValueError(f"production object output is missing: {output}")
            record["object_sha256"] = sha256(output)
            record["object_path"] = str(output.resolve())
        inventory[str(path.relative_to(root))] = record
    missing = expected - {root / name for name in inventory}
    if missing:
        raise ValueError("mutation build omits production files: " + ", ".join(map(str, sorted(missing))))
    return {"production_compile_inventory": inventory,
            "compile_commands_sha256": sha256(commands_path),
            "plugin_sha256": sha256(plugin), "plugin_path": str(plugin.resolve()),
            "cmake_cache_sha256": sha256(cache_path)}


def run(arguments: list[str], *, environment: dict, timeout: int, log: Path) -> str:
    try:
        result = subprocess.run(arguments, env=environment, cwd=ROOT, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=timeout, check=False)
    except subprocess.TimeoutExpired as error:
        output = error.stdout or b""
        log.write_text(output.decode(errors="replace") if isinstance(output, bytes) else output)
        raise ValueError(f"campaign command timed out; inspect {log}") from error
    log.write_text(result.stdout)
    if result.returncode:
        raise ValueError(f"command exited {result.returncode}; inspect {log}")
    return result.stdout


def native_tests(ctest: str, build: Path, environment: dict) -> tuple[list[dict], list[str]]:
    result = subprocess.run([ctest, "--test-dir", str(build), "--show-only=json-v1"],
                            env=environment, text=True, capture_output=True, check=True)
    tests, skipped = [], []
    for test in json.loads(result.stdout)["tests"]:
        command = test.get("command", [])
        if not command:
            raise ValueError(f"CTest test has no command: {test['name']}")
        executable = Path(command[0])
        if not executable.is_file():
            raise ValueError(f"CTest executable is missing: {executable}")
        with executable.open("rb") as stream:
            # Python itself is ELF too; mutate only executables built by this project.
            native = executable.resolve().is_relative_to(build) and stream.read(4) == b"\x7fELF"
        if native:
            tests.append({"name": test["name"], "command": command})
        else:
            skipped.append(test["name"])
    if not tests:
        raise ValueError("CTest contains zero native executables")
    return tests, skipped


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=ROOT,
                        help="explicit source checkout; defaults to this script checkout")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--report-dir", type=Path, required=True)
    parser.add_argument("--runner", default="mull-runner")
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--minimum", type=Decimal, default=Decimal("95"))
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--mutant-timeout-ms", type=int, default=15000,
                        help="minimum mutant timeout; Mull uses max(this, 10 times baseline duration)")
    parser.add_argument("--command-timeout", type=int, default=3600)
    parser.add_argument("--baseline", action="store_true")
    arguments = parser.parse_args()
    try:
        # Validate the threshold even for a baseline invocation.
        passes({"infrastructure_errors": [], "assertion_killed": 0, "unique_mutants": 1},
               arguments.minimum)
        if min(arguments.jobs, arguments.workers, arguments.mutant_timeout_ms, arguments.command_timeout) <= 0:
            raise ValueError("workers and timeouts must be positive")
        build, reports = arguments.build_dir.resolve(), arguments.report_dir.resolve()
        if reports.exists() and any(reports.iterdir()):
            raise ValueError("report directory must be new or empty")
        reports.mkdir(parents=True, exist_ok=True)
        root = arguments.source_root.resolve()
        provenance = validate_build(build, root)
        cmake = shutil.which(arguments.cmake)
        if not cmake:
            raise ValueError("qualified CMake executable is required to rebuild current sources")
        runner = shutil.which(arguments.runner)
        ctest = shutil.which(arguments.ctest)
        if not runner or not ctest:
            raise ValueError("Mull runner and CTest must be executable")
        environment = os.environ.copy()
        environment["MULL_CONFIG"] = str(root / "mull.yml")
        environment["QT_QPA_PLATFORM"] = "offscreen"
        environment["TZ"] = "UTC"
        raw_profiles = reports / "raw"
        raw_profiles.mkdir()
        environment["LLVM_PROFILE_FILE"] = str(raw_profiles / "%m-%p.profraw")
        sources = source_hashes(root)
        run([cmake, "--build", str(build), "--clean-first", "--parallel", str(arguments.jobs)],
            environment=environment, timeout=arguments.command_timeout, log=reports / "rebuild.log")
        provenance = validate_build(build, root, require_objects=True)
        if source_hashes(root) != sources:
            raise ValueError("source/test files changed during mutation rebuild")
        version = run([runner, "--version"], environment=environment, timeout=30,
                      log=reports / "mull-version.log")
        if not version.startswith("mull-runner 0.38.0\n"):
            raise ValueError("this score reader is qualified for Mull 0.38.0 only")
        tests, non_native = native_tests(ctest, build, environment)
        groups = [
            {"name": test["name"], "executable": test["command"][0],
             "test_program": None, "arguments": test["command"][1:]}
            for test in tests
        ]
        by_name = {Path(test["command"][0]).name: test for test in tests}
        for executable, tester in [("scrobbler", "test_cli"), ("qtscrob", "test_gui")]:
            if tester not in by_name or not (build / executable).is_file():
                raise ValueError(f"application mutation requires {executable} and {tester}")
            groups.append({"name": f"application-{executable}",
                           "executable": str(build / executable),
                           "test_program": by_name[tester]["command"][0], "arguments": []})

        manifest = {
            "schema_version": 1,
            "started_utc": datetime.now(timezone.utc).isoformat(),
            "mode": "baseline" if arguments.baseline else "gate",
            "target_minimum": str(arguments.minimum),
            "build_directory": str(build),
            "configuration_sha256": sha256(root / "mull.yml"),
            "source_root": str(root),
            "build_provenance": provenance,
            "runner_sha256": sha256(Path(runner)),
            "source_sha256": sources,
            "compiler_cache_sha256": sha256(build / "CMakeCache.txt"),
            "non_native_ctest_tests": non_native,
            "profile_directory": str(raw_profiles),
            "profile_file_pattern": environment["LLVM_PROFILE_FILE"],
            "timeout_policy": {"minimum_mutant_ms": arguments.mutant_timeout_ms,
                               "original_test_limit_ms": arguments.mutant_timeout_ms,
                               "effective_mutant_limit": "max(minimum_mutant_ms, 10 * Mull baseline duration)",
                               "command_limit_seconds": arguments.command_timeout},
            "groups": groups,
            "completed": False,
        }
        (reports / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        sqlite_reports = []
        with tempfile.TemporaryDirectory(prefix="qtscrobbler-mutation-home-") as home:
            environment.update(HOME=home, XDG_CONFIG_HOME=home, XDG_CACHE_HOME=home)
            for index, group in enumerate(groups):
                name = f"{index:02d}-{group['name']}"
                group["binary_sha256"] = sha256(Path(group["executable"]))
                command = [runner, "--include-not-covered", "--allow-surviving",
                           "--workers", str(arguments.workers),
                           "--timeout", str(arguments.mutant_timeout_ms),
                           "--minimum-timeout", str(arguments.mutant_timeout_ms),
                           "--reporters", "SQLite", "--reporters", "IDE",
                           "--report-dir", str(reports), "--report-name", name]
                if group["test_program"]:
                    command.extend(["--test-program", group["test_program"]])
                command.extend([group["executable"], *group["arguments"]])
                group["command"] = command
                print(f"Mutation campaign: {group['name']}", flush=True)
                output = run(command, environment=environment, timeout=arguments.command_timeout,
                             log=reports / f"{name}.log")
                report = reports / f"{name}.sqlite"
                if report.is_file():
                    sqlite_reports.append(report)
                    group["sqlite_report"] = report.name
                elif "No mutants found." in output and not group["test_program"]:
                    group["sqlite_report"] = None
                    group["note"] = "No production mutants linked into this test executable"
                else:
                    raise ValueError(f"missing fresh mutation report for {group['name']}")
                (reports / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        if source_hashes(root) != sources or sha256(root / "mull.yml") != manifest["configuration_sha256"]:
            raise ValueError("production sources or mutation configuration changed during campaign")
        if validate_build(build, root, require_objects=True) != provenance:
            raise ValueError("mutation compile configuration or plugin changed during campaign")
        if sha256(Path(runner)) != manifest["runner_sha256"]:
            raise ValueError("Mull runner changed during campaign")
        for group in groups:
            if sha256(Path(group["executable"])) != group["binary_sha256"]:
                raise ValueError("a test/application binary changed during the campaign")
        summary = summarize(sqlite_reports)
        summary["target_minimum"] = str(arguments.minimum)
        summary["satisfies_target"] = passes(summary, arguments.minimum)
        summary["mode"] = manifest["mode"]
        (reports / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        profiles = list(raw_profiles.glob("*.profraw"))
        manifest["raw_profile_count"] = len(profiles)
        manifest["raw_profile_bytes"] = sum(path.stat().st_size for path in profiles)
        if not profiles:
            raise ValueError("mutation test runs emitted no source coverage profiles")
        manifest["completed"] = True
        manifest["finished_utc"] = datetime.now(timezone.utc).isoformat()
        (reports / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"Assertion-killed {summary['assertion_killed']}/{summary['unique_mutants']}; "
              f"score {summary['score_percent']}%; target {arguments.minimum}% "
              f"{'PASS' if summary['satisfies_target'] else 'FAIL'}")
        if summary["infrastructure_errors"]:
            return 2
        return 0 if arguments.baseline or summary["satisfies_target"] else 1
    except (ValueError, OSError, KeyError, json.JSONDecodeError,
            sqlite3.Error, subprocess.CalledProcessError) as error:
        print(f"Mutation campaign error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
