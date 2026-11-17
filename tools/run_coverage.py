#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build and measure complete native LLVM source line and branch coverage.

Usage: source the installed native LLVM toolchain's env.sh, set
QTSCROBBLER_QT_PREFIX to the Qt 6.12 SDK, then run:
  python3 tools/run_coverage.py --build-dir BUILD --report-dir NEW_REPORT_DIR

The default gate requires 95 percent lines AND branches from integer counts. Use
--baseline to record incomplete coverage explicitly without claiming that gate passed.
LLVM branch regions measure explicit source conditions; implicit C++ exception edges
are not branch regions. No production-file or logging exclusions are applied. Generated
Qt code, tests and SDK/system headers are outside the first-party source denominator.
Windows-only parse-mtp-win32.cpp and vendored xgetopt are listed as unmeasured separately.
"""

from __future__ import annotations

import argparse
from decimal import Decimal
import json
import hashlib
import re
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


def run(command: list[str], environment: dict[str, str], log: Path, *, json_output: bool = False) -> str:
    """Retain combined output and fail if any configure/build/test/report stage fails."""
    print("Running:", " ".join(command), flush=True)
    result = subprocess.run(command, cwd=ROOT, env=environment, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    log.write_text(result.stdout if json_output else result.stdout + result.stderr)
    if json_output and result.stderr:
        log.with_suffix(".stderr.log").write_text(result.stderr)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}); inspect {log}")
    if json_output:
        # LLVM 20 prints mapping diagnostics on stdout before its JSON payload.
        # Preserve every byte and accept only the documented warning prefix.
        start = result.stdout.find("{")
        prefix = result.stdout[:start] if start >= 0 else result.stdout
        if any(not re.fullmatch(r"warning: [0-9]+ functions have mismatched data", line)
               for line in prefix.splitlines()):
            raise ValueError(f"unexpected non-JSON tool output; inspect {log}")
        payload = result.stdout[start:] if start >= 0 else ""
        json.loads(payload)
        log.with_suffix(".stdout.log").write_text(result.stdout)
        log.write_text(payload)
        return payload
    return result.stdout


def passes(covered: int, total: int, minimum: Decimal) -> bool:
    """Compare raw counts, never display-rounded percentages; zero denominators fail."""
    return total > 0 and Decimal(covered) * 100 >= minimum * total


def fingerprints() -> dict[str, str]:
    paths = {path for folder in ("src", "tests", "cmake", "tools")
             for path in (ROOT / folder).rglob("*") if path.is_file()
             and "__pycache__" not in path.parts and path.suffix != ".pyc"}
    paths.update(path for name in ("CMakeLists.txt", "localPipeline.sh", "mull.yml", "Doxyfile",
                                  "requirements-quality.txt", ".clang-format")
                 if (path := ROOT / name).is_file())
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(paths)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--report-dir", type=Path, required=True)
    parser.add_argument("--minimum", type=Decimal, default=Decimal("95"))
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    try:
        if not args.minimum.is_finite() or not 0 <= args.minimum <= 100 or args.jobs < 1:
            raise ValueError("minimum must be 0..100 and jobs positive")
        initial_sources = fingerprints()
        build, reports = args.build_dir.resolve(), args.report_dir.resolve()
        if reports.exists() and any(reports.iterdir()):
            raise ValueError("report directory must be new or empty to prevent stale profiles")
        if build == ROOT or ROOT.is_relative_to(build):
            raise ValueError("build directory cannot contain the source tree")
        reports.mkdir(parents=True, exist_ok=True)
        profiles = reports / "raw"
        profiles.mkdir()
        env = os.environ.copy()
        env.update(QT_QPA_PLATFORM="offscreen", TZ="UTC", LLVM_PROFILE_FILE=str(profiles / "%m-%p.profraw"))
        tools = {name: shutil.which(name) for name in ("cmake", "ctest", "llvm-cov-20", "llvm-profdata-20")}
        if not all(tools.values()):
            raise ValueError("qualified CMake/CTest and native llvm-cov-20/llvm-profdata-20 required")
        if not env.get("QTSCROBBLER_QT_PREFIX"):
            raise ValueError("set QTSCROBBLER_QT_PREFIX explicitly")
        run([tools["cmake"], "-S", str(ROOT), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug",
             f"-DCMAKE_PREFIX_PATH={env['QTSCROBBLER_QT_PREFIX']}", "-DBUILD_TESTING=ON",
             "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DQTSCROBBLER_COVERAGE=ON", "-DQTSCROBBLER_MTP=ON",
             "-DQTSCROBBLER_MULL_PLUGIN:FILEPATH=", "-DQTSCROBBLER_SANITIZERS=OFF"],
            env, reports / "configure.log")
        run([tools["cmake"], "--build", str(build), "--parallel", str(args.jobs)], env, reports / "build.log")
        run([tools["ctest"], "--test-dir", str(build), "--output-on-failure", "--no-tests=error",
             "--output-junit", str(reports / "junit.xml"), "-j", str(args.jobs)], env, reports / "tests.log")
        raw = sorted(profiles.glob("*.profraw"))
        if not raw:
            raise ValueError("tests generated zero coverage profiles")
        merged = reports / "coverage.profdata"
        run([tools["llvm-profdata-20"], "merge", "-sparse", *map(str, raw), "-o", str(merged)],
            env, reports / "merge.log")
        ctest_json = run([tools["ctest"], "--test-dir", str(build), "--show-only=json-v1"],
                         env, reports / "ctest.json", json_output=True)
        binaries = {build / "qtscrob", build / "scrobbler"}
        for test in json.loads(ctest_json)["tests"]:
            path = Path(test["command"][0])
            if path.is_relative_to(build):
                with path.open("rb") as stream:
                    if stream.read(4) == b"\x7fELF":
                        binaries.add(path)
        ordered = sorted(binaries)
        objects = [f"-object={path}" for path in ordered[1:]]
        # Restrict export to maintained source directories; report missing mappings explicitly below.
        source_paths = [str(ROOT / "src/lib"), str(ROOT / "src/cli"), str(ROOT / "src/qt/src")]
        data = run([tools["llvm-cov-20"], "export", str(ordered[0]), *objects,
                    f"-instr-profile={merged}", *source_paths], env, reports / "coverage.json", json_output=True)
        files = json.loads(data)["data"][0]["files"]
        totals = {"lines": {"count": 0, "covered": 0}, "branches": {"count": 0, "covered": 0}}
        inventory = []
        for file in files:
            filename = Path(file["filename"]).resolve()
            if not filename.is_relative_to(ROOT / "src"):
                continue
            inventory.append(str(filename.relative_to(ROOT)))
            for metric in totals:
                for key in totals[metric]:
                    totals[metric][key] += file["summary"][metric][key]
        expected = {str(path.relative_to(ROOT)) for directory in source_paths
                    for path in Path(directory).glob("*.cpp") if path.name != "parse-mtp-win32.cpp"}
        missing = sorted(expected - set(inventory))
        diagnostics = {}
        for name in ("coverage.stdout.log", "coverage.stderr.log"):
            path = reports / name
            if path.exists():
                content = path.read_text()
                diagnostics[name] = content[:content.find("{")] if name.endswith("stdout.log") else content
        if fingerprints() != initial_sources:
            raise ValueError("source/test/build definitions changed during coverage run")
        summary = {"source_sha256": initial_sources, "mapping_diagnostics": diagnostics,
                   "exports_sha256": {name: hashlib.sha256((reports / name).read_bytes()).hexdigest()
                                      for name in ("coverage.json", "coverage.profdata")},
                   "binaries_sha256": {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in ordered},"mode": "baseline" if args.baseline else "gate", "target_percent": str(args.minimum),
                   "branch_semantics": "LLVM explicit source branch regions; implicit exception edges absent",
                   "totals": totals, "production_file_inventory": sorted(inventory),
                   "missing_production_files": missing,
                   "unmeasured_platform_or_vendored": ["src/lib/parse-mtp-win32.cpp", "src/common/xgetopt.c"]}
        summary["target_passed"] = not missing and all(
            passes(values["covered"], values["count"], args.minimum) for values in totals.values())
        (reports / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        run([tools["llvm-cov-20"], "show", str(ordered[0]), *objects,
             f"-instr-profile={merged}", "-format=html", f"-output-dir={reports / 'html'}",
             "-show-branches=count", *source_paths], env, reports / "html.log")
        print(json.dumps(summary, indent=2))
        if missing:
            raise ValueError("coverage omitted maintained production files")
        return 0 if args.baseline or summary["target_passed"] else 1
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"Coverage failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
