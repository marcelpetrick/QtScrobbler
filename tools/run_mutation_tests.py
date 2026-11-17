#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Marcel Petrick
"""Run a fresh native Mull campaign over CTest executables and both application binaries.

Build first with Clang and QTSCROBBLER_MULL_PLUGIN, then run:
  python3 tools/run_mutation_tests.py --build-dir BUILD --report-dir NEW_REPORT_DIRECTORY \
    --toolchain-summary FRESH_SEMANTIC_PREFLIGHT/summary.json

Use --baseline to measure an incomplete suite without claiming the 95% gate passes.
Mutation builds retain profiling but omit source coverage mapping, so Mull executes all
generated mutants rather than applying its unreliable LLVM 20 coverage filter. Separate
LLVM coverage reports remain required; survivors include unexercised mutations until
classified from trusted coverage evidence. Unfiltered native discovery inventories retain
the complete denominator. Later groups rerun only source lines containing candidates
without an earlier proven assertion kill; raw execution reports remain authoritative.

No dependencies are installed and no container is used. Each report retains raw outcomes,
commands, binary/source/config hashes and the exact Mull version. Existing nonempty report
directories are rejected so a stale result cannot pass a fresh campaign.
"""

from __future__ import annotations

import argparse
from contextlib import closing
from datetime import datetime, timezone
from decimal import Decimal
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import shlex
import sqlite3
import subprocess
import sys
import tempfile
import time

from source_fingerprint import source_hashes  # noqa: F401  (shared with run_coverage)
from mutation_score import REQUIRED_COLUMNS, STATUSES, assertion_killed, passes, summarize


ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
        command = entry.get("command", " ".join(entry.get("arguments", [])))
        tokens = entry.get("arguments") or shlex.split(command)
        if "-fcoverage-mapping" in tokens:
            raise ValueError(f"mutation build must omit coverage mapping to avoid false uncovered mutants: {path}")
        if path not in expected:
            continue
        if f"-fpass-plugin={plugin}" not in command:
            raise ValueError(f"production file lacks mutation instrumentation: {path}")
        if "-fprofile-instr-generate" not in tokens:
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


def read_candidates(report: Path, *, discovery: bool) -> tuple[dict[str, tuple], set[str]]:
    """Read native evidence without treating discovery rows as execution outcomes."""
    with closing(sqlite3.connect(report.resolve().as_uri() + "?mode=ro", uri=True)) as database:
        database.row_factory = sqlite3.Row
        if not REQUIRED_COLUMNS <= {row[1] for row in database.execute("PRAGMA table_info(mutant)")}:
            raise ValueError(f"unsupported inventory schema: {report}")
        versions = {row[0] for row in database.execute(
            "SELECT value FROM information WHERE key = 'Mull Version'")}
        if versions != {"0.38.0"}:
            raise ValueError(f"unsupported inventory Mull version: {report}")
        candidates, killed = {}, set()
        for row in database.execute("SELECT * FROM mutant"):
            identity = row["mutant_id"]
            status = row["execution_status"]
            if (not identity or not row["filename"] or row["line_number"] < 1
                    or not Path(row["filename"]).is_absolute()):
                raise ValueError(f"invalid candidate identity/location: {report}")
            if status not in STATUSES or (status != 6 if discovery else status == 6):
                raise ValueError(f"unexpected discovery/execution status: {report}")
            signature = (row["mutator"], row["filename"], row["line_number"],
                         row["column_number"], row["mutation_replacement"])
            if identity in candidates and candidates[identity] != signature:
                raise ValueError(f"conflicting inventory signature: {identity}")
            candidates[identity] = signature
            if not discovery and assertion_killed(row):
                killed.add(identity)
        if not candidates:
            raise ValueError(f"empty native candidate report: {report}")
        return candidates, killed


def merge_candidates(destination: dict[str, tuple], candidates: dict[str, tuple]) -> None:
    for identity, signature in candidates.items():
        if identity in destination and destination[identity] != signature:
            raise ValueError(f"conflicting candidate signature: {identity}")
        destination[identity] = signature


def selected_candidates(candidates: dict[str, tuple], killed: set[str]) -> dict[str, tuple]:
    """Mull selects whole lines; retain killed siblings of any unresolved mutant."""
    lines = {(signature[1], signature[2]) for identity, signature in candidates.items() if identity not in killed}
    return {identity: signature for identity, signature in candidates.items()
            if (signature[1], signature[2]) in lines}


def validate_base_config(base: Path) -> str:
    text = base.read_text()
    # An inherited range/diff/coverage setting could silently truncate discovery.
    # The maintained base config contains only the reviewed production path scope.
    if re.search(r"(?m)^\s*(mutationRanges|gitDiffRef|gitProjectRoot|gitDiffPaths)\s*:", text):
        raise ValueError("base mutation config cannot contain incremental selection")
    return text


def write_selection(base: Path, config: Path, ranges: Path, candidates: dict[str, tuple]) -> None:
    if not candidates:
        raise ValueError("refusing blank mutation ranges")
    text = validate_base_config(base)
    lines = sorted({(signature[1], signature[2]) for signature in candidates.values()})
    if any("\n" in path or "\r" in path for path, _line in lines):
        raise ValueError("source path cannot be represented in mutation ranges")
    ranges.write_text("".join(f"{path}:{line}\n" for path, line in lines))
    config.write_text(text.rstrip() + "\nmutationRanges: " + json.dumps(str(ranges.resolve())) + "\n")


def require_complete_inventory(discovered: dict[str, tuple], executed: dict[str, tuple]) -> None:
    if not discovered or discovered != executed:
        missing = sorted(discovered.keys() - executed.keys())
        extra = sorted(executed.keys() - discovered.keys())
        raise ValueError(f"candidate inventory differs from executions: missing={missing}, extra={extra}")


def verify_artifacts(artifacts: dict[str, str]) -> None:
    for filename, expected in artifacts.items():
        if not Path(filename).is_file() or sha256(Path(filename)) != expected:
            raise ValueError(f"campaign evidence changed: {filename}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=ROOT,
                        help="explicit source checkout; defaults to this script checkout")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--report-dir", type=Path, required=True)
    parser.add_argument("--runner", default="mull-runner")
    parser.add_argument("--toolchain-summary", type=Path, required=True,
                        help="passing native semantic preflight for this exact compiler/plugin/runner")
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--minimum", type=Decimal, default=Decimal("95"))
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--mutant-timeout-ms", type=int, default=15000,
                        help="minimum mutant timeout; Mull uses max(this, 10 times baseline duration)")
    parser.add_argument("--command-timeout", type=int, default=3600)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--coverage-summary", type=Path,
                        help="fresh independent LLVM coverage summary for unresolved source-position classification")
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
        validate_base_config(root / "mull.yml")
        provenance = validate_build(build, root)
        cmake = shutil.which(arguments.cmake)
        if not cmake:
            raise ValueError("qualified CMake executable is required to rebuild current sources")
        runner = shutil.which(arguments.runner)
        ctest = shutil.which(arguments.ctest)
        if not runner or not ctest:
            raise ValueError("Mull runner and CTest must be executable")
        from mutation_toolchain_evidence import validate as validate_toolchain_evidence
        semantic_proof = validate_toolchain_evidence(
            arguments.toolchain_summary, build, Path(runner), provenance["production_compile_inventory"], root)
        environment = os.environ.copy()
        environment["MULL_CONFIG"] = str(root / "mull.yml")
        environment["QT_QPA_PLATFORM"] = "offscreen"
        environment["TZ"] = "UTC"
        raw_profiles = reports / "raw"
        raw_profiles.mkdir()
        environment["LLVM_PROFILE_FILE"] = str(raw_profiles / "%m-%p.profraw")
        sources = source_hashes(root)
        from mutation_coverage import CoverageEvidence, classify_unresolved
        coverage_evidence = (CoverageEvidence(arguments.coverage_summary, sources)
                             if arguments.coverage_summary is not None else None)
        run([cmake, "--build", str(build), "--clean-first", "--parallel", str(arguments.jobs)],
            environment=environment, timeout=arguments.command_timeout, log=reports / "rebuild.log")
        provenance = validate_build(build, root, require_objects=True)
        if validate_toolchain_evidence(arguments.toolchain_summary, build, Path(runner),
                                       provenance["production_compile_inventory"], root) != semantic_proof:
            raise ValueError("native semantic toolchain proof changed during mutation rebuild")
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
            "toolchain_evidence": semantic_proof,
            "runner_sha256": sha256(Path(runner)),
            "source_sha256": sources,
            "coverage_evidence": coverage_evidence.provenance() if coverage_evidence is not None else None,
            "compiler_cache_sha256": sha256(build / "CMakeCache.txt"),
            "non_native_ctest_tests": non_native,
            "profile_directory": str(raw_profiles),
            "profile_file_pattern": environment["LLVM_PROFILE_FILE"],
            "coverage_filter": {
                "enabled": False,
                "policy": "every generated candidate retained; no coverage-based skipping; proven assertion kills reused",
                "reason": "Mull 0.38.0 misclassifies covered LLVM 20 source regions as uncovered",
                "survivor_classification": "survivors include unexercised mutations; separate LLVM evidence required",
            },
            "timeout_policy": {"minimum_mutant_ms": arguments.mutant_timeout_ms,
                               "original_test_limit_ms": arguments.mutant_timeout_ms,
                               "effective_mutant_limit": "max(minimum_mutant_ms, 10 * Mull baseline duration)",
                               "command_limit_seconds": arguments.command_timeout},
            "groups": groups,
            "completed": False,
        }
        (reports / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        sqlite_reports = []
        artifacts = {}
        discovered, executed, killed = {}, {}, set()
        inventories = []
        manifest["selection_policy"] = "skip only lines whose entire candidate set already has proven assertion kills"
        manifest["evidence_sha256"] = artifacts
        with tempfile.TemporaryDirectory(prefix="qtscrobbler-mutation-home-") as home:
            environment.update(HOME=home, XDG_CONFIG_HOME=home, XDG_CACHE_HOME=home)
            # Discover every binary unfiltered before selecting any execution subset.
            for index, group in enumerate(groups):
                name = f"{index:02d}-{group['name']}"
                group["binary_sha256"] = sha256(Path(group["executable"]))
                if group["test_program"]:
                    group["test_program_sha256"] = sha256(Path(group["test_program"]))
                command = [runner, "--include-not-covered", "--allow-surviving", "--strict",
                           "--workers", str(arguments.workers),
                           "--timeout", str(arguments.mutant_timeout_ms),
                           "--minimum-timeout", str(arguments.mutant_timeout_ms),
                           "--reporters", "SQLite", "--reporters", "IDE",
                           "--report-dir", str(reports)]
                if group["test_program"]:
                    command.extend(["--test-program", group["test_program"]])
                suffix = [group["executable"], *group["arguments"]]
                group["runner_command_prefix"] = command
                discovery_command = [*command, "--dry-run", "--report-name", name + "-inventory", *suffix]
                group["inventory_command"] = discovery_command
                print(f"Mutation inventory: {group['name']}", flush=True)
                log = reports / f"{name}-inventory.log"
                started = time.monotonic()
                output = run(discovery_command, environment=environment, timeout=arguments.command_timeout, log=log)
                group["inventory_seconds"] = time.monotonic() - started
                artifacts[str(log)] = sha256(log)
                inventory = reports / f"{name}-inventory.sqlite"
                if inventory.is_file():
                    candidates, _ = read_candidates(inventory, discovery=True)
                    artifacts[str(inventory)] = sha256(inventory)
                    group["inventory_report"] = inventory.name
                elif "No mutants found." in output and not group["test_program"]:
                    candidates = {}
                    group["inventory_report"] = None
                else:
                    raise ValueError(f"missing fresh candidate inventory for {group['name']}")
                inventories.append(candidates)
                merge_candidates(discovered, candidates)
                group["generated_candidates"] = len(candidates)
                (reports / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
            # Let inexpensive original suites establish kills before expensive
            # GUI/controller suites repeat them. Discovery includes the original
            # suite warmup, so its observed duration supplies a fresh ordering.
            ordered = sorted(enumerate(zip(groups, inventories)), key=lambda item: item[1][0]["inventory_seconds"])
            manifest["execution_order"] = [group["name"] for _index, (group, _candidates) in ordered]
            for index, (group, candidates) in ordered:
                verify_artifacts(artifacts)
                name = f"{index:02d}-{group['name']}"
                selected = selected_candidates(candidates, killed)
                group["selected_candidates"] = len(selected)
                group["already_proven_candidates"] = len(candidates) - len(selected)
                if not selected:
                    group["sqlite_report"] = None
                    group["note"] = "No candidates" if not candidates else "All candidates already assertion-killed in retained reports"
                else:
                    config, ranges = reports / f"{name}-runner.yml", reports / f"{name}-ranges.txt"
                    write_selection(root / "mull.yml", config, ranges, selected)
                    for path in (config, ranges):
                        artifacts[str(path)] = sha256(path)
                    selected_environment = dict(environment, MULL_CONFIG=str(config))
                    command = [*group["runner_command_prefix"], "--report-name", name,
                               group["executable"], *group["arguments"]]
                    group["command"] = command
                    group["runner_config"] = config.name
                    group["mutation_ranges"] = ranges.name
                    print(f"Mutation campaign: {group['name']} ({len(selected)}/{len(candidates)} candidates)", flush=True)
                    log = reports / f"{name}.log"
                    run(command, environment=selected_environment, timeout=arguments.command_timeout, log=log)
                    report = reports / f"{name}.sqlite"
                    actual, newly_killed = read_candidates(report, discovery=False)
                    if actual != selected:
                        raise ValueError(f"selected inventory differs from native execution report: {group['name']}")
                    merge_candidates(executed, actual)
                    killed.update(newly_killed)
                    for path in (report, log):
                        artifacts[str(path)] = sha256(path)
                    sqlite_reports.append(report)
                    group["sqlite_report"] = report.name
                (reports / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        verify_artifacts(artifacts)
        require_complete_inventory(discovered, executed)
        manifest["generated_unique_candidates"] = len(discovered)
        manifest["executed_unique_candidates"] = len(executed)
        if source_hashes(root) != sources or sha256(root / "mull.yml") != manifest["configuration_sha256"]:
            raise ValueError("production sources or mutation configuration changed during campaign")
        if validate_build(build, root, require_objects=True) != provenance:
            raise ValueError("mutation compile configuration or plugin changed during campaign")
        if sha256(Path(runner)) != manifest["runner_sha256"]:
            raise ValueError("Mull runner changed during campaign")
        if validate_toolchain_evidence(arguments.toolchain_summary, build, Path(runner),
                                       provenance["production_compile_inventory"], root) != semantic_proof:
            raise ValueError("native semantic toolchain proof changed during mutation campaign")
        for group in groups:
            if (sha256(Path(group["executable"])) != group["binary_sha256"]
                    or (group["test_program"] and sha256(Path(group["test_program"])) != group["test_program_sha256"])):
                raise ValueError("a test/application binary changed during the campaign")
        verify_artifacts(artifacts)
        summary = summarize(sqlite_reports)
        verify_artifacts(artifacts)
        classify_unresolved(summary, sqlite_reports, coverage_evidence)
        verify_artifacts(artifacts)
        summary["target_minimum"] = str(arguments.minimum)
        summary["satisfies_target"] = passes(summary, arguments.minimum)
        summary["mode"] = manifest["mode"]
        summary["coverage_filter"] = manifest["coverage_filter"]
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
