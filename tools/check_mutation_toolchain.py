#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Marcel Petrick
"""Prove native Mull removes a Qt signal call and reports its assertion failure.

Source the qualified native LLVM environment and explicitly select the Qt SDK:
  python3 tools/check_mutation_toolchain.py --report-dir NEW_DIRECTORY

The private generated project instruments only probe.cpp with cxx_remove_void_call.
Exactly one signal-removal mutant must fail the independent QSignalSpy assertion;
phantom cleanup mutants, duplicate identities, survivors and abnormal exits fail.
This semantic preflight does not establish application mutation coverage or score.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import signal
from contextlib import closing
import sqlite3
import subprocess
import sys

from mutation_score import assertion_killed, summarize


PROBE_HEADER = '''#pragma once
#include <QObject>
#include <QString>
class Probe : public QObject {
    Q_OBJECT
public:
    void send();
signals:
    void value(QString payload);
};
'''
PROBE_SOURCE = '''#include "probe.h"

void Probe::send() {
    emit value(QStringLiteral("payload"));
}
'''
PROBE_TEST = '''#include "probe.h"
#include <QSignalSpy>
#include <QtTest>

class ProbeTest : public QObject {
    Q_OBJECT
private slots:
    void signalPayload() {
        QTest::failOnWarning(QRegularExpression(".*"));
        Probe probe;
        QSignalSpy emitted(&probe, &Probe::value);
        QVERIFY(emitted.isValid());
        probe.send();
        QCOMPARE(emitted.count(), 1);
        QCOMPARE(emitted.first().first().toString(), QStringLiteral("payload"));
    }
};
QTEST_GUILESS_MAIN(ProbeTest)
#include "test_probe.moc"
'''
PROBE_CMAKE = '''cmake_minimum_required(VERSION 3.28)
project(QtScrobblerMutationSentinel LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Test)
add_executable(test_probe probe.cpp probe.h test_probe.cpp)
target_link_libraries(test_probe PRIVATE Qt6::Core Qt6::Test)
set_source_files_properties(probe.cpp PROPERTIES
    COMPILE_OPTIONS "-fpass-plugin=${MULL_PLUGIN};-fprofile-instr-generate;-grecord-command-line")
target_link_options(test_probe PRIVATE -fprofile-instr-generate)
'''
PROBE_CONFIG = '''mutators:
  - cxx_remove_void_call
includePaths:
  - /probe\\.cpp$
parallelization:
  workers: 1
  executionWorkers: 1
'''
SIGNAL_LINE = next(index for index, line in enumerate(PROBE_SOURCE.splitlines(), 1) if "emit value(" in line)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_report(report: Path, source: Path) -> dict:
    """Reject reports unless the sole real signal mutant proves its missing emission."""
    summary = summarize([report])
    with closing(sqlite3.connect(report.resolve().as_uri() + "?mode=ro", uri=True)) as database:
        database.row_factory = sqlite3.Row
        rows = database.execute("SELECT * FROM mutant").fetchall()
    if len(rows) != 1:
        raise ValueError(f"expected exactly one signal-call mutant, found {len(rows)}; "
                         "duplicate or phantom compiler-cleanup mutants invalidate this toolchain")
    row = rows[0]
    if (row["mutator"] != "cxx_remove_void_call" or Path(row["filename"]).resolve() != source.resolve()
            or row["line_number"] != SIGNAL_LINE):
        raise ValueError("native report does not identify the expected probe.cpp signal-call mutation")
    output = (row["stdout"] or "") + "\n" + (row["stderr"] or "")
    if (not assertion_killed(row) or "ProbeTest::signalPayload" not in output
            or not re.search(r"Actual\s+\(emitted\.count\(\)\)\s*:\s*0(?:\s|$)", output)):
        raise ValueError("signal-removal mutant did not produce a normal Qt Test assertion "
                         "showing emitted.count() == 0; native mutation semantics are untrusted")
    if summary["assertion_killed"] != 1 or summary["unique_mutants"] != 1 or summary["infrastructure_errors"]:
        raise ValueError("native signal-removal report failed strict assertion-kill accounting")
    return {"mutant_id": row["mutant_id"], "mutator": row["mutator"],
            "filename": row["filename"], "line_number": row["line_number"],
            "column_number": row["column_number"], "summary": summary}


def check_compile_inventory(path: Path, source: Path, plugin: Path) -> list[dict]:
    """Prove instrumentation is confined to the signal implementation translation unit."""
    rows = json.loads(path.read_text())
    instrumented = []
    for row in rows:
        tokens = row.get("arguments") or shlex.split(row["command"])
        if "-fcoverage-mapping" in tokens:
            raise ValueError("semantic probe must omit LLVM source coverage mapping")
        plugins = [token for token in tokens if token.startswith("-fpass-plugin=")]
        if plugins:
            if Path(row["file"]).resolve() != source.resolve() or plugins != [f"-fpass-plugin={plugin}"]:
                raise ValueError("only probe.cpp may load the selected mutation plugin")
            if "-fprofile-instr-generate" not in tokens or "-grecord-command-line" not in tokens:
                raise ValueError("probe.cpp lacks mutation profiling or recorded compiler arguments")
            instrumented.append(row)
    if len(instrumented) != 1:
        raise ValueError("expected exactly one mutation-instrumented probe.cpp compilation")
    return rows


class Commands:
    def __init__(self, reports: Path, environment: dict, timeout: int):
        self.reports = reports
        self.environment = environment
        self.timeout = timeout
        self.records = []

    def run(self, name: str, arguments: list[str]) -> str:
        log = self.reports / f"{len(self.records):02d}-{name}.log"
        record = {"name": name, "command": arguments, "timeout_seconds": self.timeout,
                  "log": log.name, "status": "FAIL"}
        self.records.append(record)
        try:
            process = subprocess.Popen(arguments, cwd=self.reports, env=self.environment,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
        except OSError as error:
            log.write_text(str(error) + "\n")
            record["spawn_error"] = str(error)
            raise
        try:
            output, _ = process.communicate(timeout=self.timeout)
        except subprocess.TimeoutExpired as error:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            output, _ = process.communicate()
            log.write_bytes(output)
            record["timed_out"] = True
            raise ValueError(f"{name} exceeded {self.timeout}s; inspect {log}") from error
        log.write_bytes(output)
        record["exit_code"] = process.returncode
        if process.returncode:
            raise ValueError(f"{name} exited {process.returncode}; inspect {log}")
        record["status"] = "PASS"
        return output.decode(errors="replace")


def executable(value: str | None, name: str) -> Path:
    if not value:
        raise ValueError(f"explicit {name} is required")
    selected = shutil.which(value)
    if not selected:
        raise ValueError(f"{name} is not executable: {value}")
    return Path(selected).absolute()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report-dir", type=Path, required=True)
    parser.add_argument("--qt-prefix", type=Path, default=os.environ.get("QTSCROBBLER_QT_PREFIX"))
    parser.add_argument("--compiler", default=os.environ.get("CXX"))
    parser.add_argument("--plugin", type=Path, default=os.environ.get("QTSCROBBLER_MULL_PLUGIN"))
    parser.add_argument("--runner", default="mull-runner")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--artifact", type=Path, action="append", default=[],
                        help="additional native compiler support artifact to fingerprint (repeatable)")
    parser.add_argument("--timeout", type=int, default=120, help="positive per-command bound in seconds")
    args = parser.parse_args()
    reports = args.report_dir.resolve()
    if args.timeout < 1 or (reports.exists() and any(reports.iterdir())):
        parser.error("timeout must be positive and report directory must be new or empty")
    reports.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    commands = Commands(reports, environment, args.timeout)
    manifest = {"schema_version": 1, "status": "FAIL", "started_utc": datetime.now(timezone.utc).isoformat(),
                "commands": commands.records, "scope": "Qt signal-removal semantic preflight only"}
    try:
        if args.qt_prefix is None or not args.qt_prefix.is_dir():
            raise ValueError("explicit existing --qt-prefix or QTSCROBBLER_QT_PREFIX is required")
        if args.plugin is None or not args.plugin.is_file():
            raise ValueError("explicit existing --plugin or QTSCROBBLER_MULL_PLUGIN is required")
        selected = {"compiler": executable(args.compiler, "Clang C++ compiler"),
                    "runner": executable(args.runner, "Mull runner"),
                    "cmake": executable(args.cmake, "CMake"), "plugin": args.plugin.resolve()}
        manifest["toolchain"] = {name: {"path": str(path), "sha256": sha256(path)}
                                 for name, path in selected.items()}
        manifest["support_artifacts"] = [{"path": str(path.resolve()), "sha256": sha256(path)}
                                         for path in args.artifact]
        manifest["native_environment"] = {name: environment.get(name, "")
                                          for name in ("LD_LIBRARY_PATH", "LD_PRELOAD")}
        manifest["qt_prefix"] = str(args.qt_prefix.resolve())
        source, build, raw = (reports / name for name in ("source", "build", "raw"))
        source.mkdir()
        raw.mkdir()
        for name, content in {"probe.h": PROBE_HEADER, "probe.cpp": PROBE_SOURCE,
                              "test_probe.cpp": PROBE_TEST, "CMakeLists.txt": PROBE_CMAKE,
                              "mull.yml": PROBE_CONFIG}.items():
            (source / name).write_text(content)
        manifest["source_sha256"] = {path.name: sha256(path) for path in sorted(source.iterdir())}
        environment.update(MULL_CONFIG=str(source / "mull.yml"), QT_QPA_PLATFORM="offscreen", TZ="UTC",
                           LLVM_PROFILE_FILE=str(raw / "%m-%p.profraw"))
        home = reports / "home"
        home.mkdir(mode=0o700)
        environment.update(HOME=str(home), XDG_CONFIG_HOME=str(home), XDG_CACHE_HOME=str(home),
                           XDG_RUNTIME_DIR=str(home))
        commands.run("compiler-version", [str(selected["compiler"]), "--version"])
        runner_version = commands.run("runner-version", [str(selected["runner"]), "--version"])
        if not runner_version.startswith("mull-runner 0.39.1\n"):
            raise ValueError("sentinel score accounting requires Mull 0.39.1")
        commands.run("cmake-version", [str(selected["cmake"]), "--version"])
        commands.run("configure", [str(selected["cmake"]), "-S", str(source), "-B", str(build),
                                   "-DCMAKE_BUILD_TYPE=Debug", f"-DCMAKE_CXX_COMPILER={selected['compiler']}",
                                   f"-DCMAKE_PREFIX_PATH={args.qt_prefix.resolve()}",
                                   f"-DMULL_PLUGIN={selected['plugin']}"])
        commands.run("build", [str(selected["cmake"]), "--build", str(build), "--parallel", "1"])
        manifest["compile_inventory"] = check_compile_inventory(
            build / "compile_commands.json", source / "probe.cpp", selected["plugin"])
        manifest["compile_commands_sha256"] = sha256(build / "compile_commands.json")
        binary = build / "test_probe"
        manifest["binary_sha256"] = sha256(binary)
        output = commands.run("original", [str(binary)])
        if "PASS   : ProbeTest::signalPayload()" not in output or "0 failed" not in output:
            raise ValueError("original QSignalSpy count/payload contract did not pass")
        commands.run("mutation", [str(selected["runner"]), "--no-cache", "--include-not-covered", "--allow-surviving",
                                  "--workers", "1", "--timeout", "3000", "--minimum-timeout", "3000",
                                  "--reporters", "SQLite", "--reporters", "IDE", "--report-dir", str(reports),
                                  "--report-name", "signal-removal", str(binary)])
        manifest["semantic_proof"] = check_report(reports / "signal-removal.sqlite", source / "probe.cpp")
        if manifest["binary_sha256"] != sha256(binary):
            raise ValueError("sentinel binary changed during mutation execution")
        if manifest["source_sha256"] != {path.name: sha256(path) for path in sorted(source.iterdir())}:
            raise ValueError("sentinel sources/configuration changed during execution")
        if any(record["sha256"] != sha256(Path(record["path"]))
               for record in manifest["toolchain"].values()):
            raise ValueError("native toolchain changed during semantic preflight")
        if any(record["sha256"] != sha256(Path(record["path"]))
               for record in manifest["support_artifacts"]):
            raise ValueError("native compiler support artifact changed during semantic preflight")
        manifest["status"] = "PASS"
    except (ValueError, OSError, sqlite3.Error, subprocess.SubprocessError, KeyError,
            json.JSONDecodeError) as error:
        manifest["error"] = str(error)
        print(f"Mutation semantic preflight: FAIL: {error}", file=sys.stderr)
    finally:
        manifest["finished_utc"] = datetime.now(timezone.utc).isoformat()
        (reports / "summary.json").write_text(json.dumps(manifest, indent=2) + "\n")
    if manifest["status"] == "PASS":
        print("Mutation semantic preflight: PASS: signal removal proved by QSignalSpy assertion")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
