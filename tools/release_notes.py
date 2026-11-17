#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Write GitHub release notes from real evidence only.

Sections: the CHANGELOG entry for the version, the checks that passed in the CI run
that qualified the commit (read from its quality reports), the release build's own
unit tests, and the SHA-256 of every published file. Any missing or failed mandatory
CI stage is an error, so a release can never claim checks that did not pass.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

# Stages the CI full gate must report as PASS before a commit may be released.
REQUIRED_STAGES = ("Unit Tests", "Format", "static", "docs", "Sanitizers", "Coverage",
                   "MutationToolchain", "Mutation", "Package")
LABELS = {
    "Unit Tests": "Unit, integration and end-to-end tests (Qt Test / CTest)",
    "Format": "clang-format formatting",
    "static": "Static analysis (clang-tidy, Clazy, Cppcheck)",
    "docs": "Documentation and Doxygen checks",
    "Sanitizers": "AddressSanitizer / UndefinedBehaviorSanitizer / leak checks",
    "Coverage": "Line and branch coverage gate (95%)",
    "MutationToolchain": "Mutation tool semantic preflight",
    "Mutation": "Native mutation testing gate (95% assertion-killed)",
    "Package": "Extracted package checks",
}
SUMMARY_LINE = re.compile(r"^(?P<stage>[A-Za-z][A-Za-z :]*?)\s+: (?P<status>PASS|FAIL|SKIP|WARN)\b")


def changelog_entry(changelog: Path, version: str) -> list[str]:
    lines, inside = [], False
    for line in changelog.read_text().splitlines():
        if line.startswith("* version"):
            if inside:
                break
            inside = line.split()[2] == version
            continue
        if inside:
            lines.append(line)
    while lines and not lines[-1].strip():
        lines.pop()
    if not lines:
        raise ValueError(f"documents/CHANGELOG has no entry for version {version}")
    return lines


def stage_results(reports: Path) -> dict[str, str]:
    results = {}
    for line in (reports / "summary.txt").read_text().splitlines():
        if match := SUMMARY_LINE.match(line):
            results[match["stage"].strip()] = match["status"]
    return results


def percent(covered: int, count: int) -> str:
    # Truncate, never round up, so a displayed value is never above the measurement.
    return f"{int(covered * 10000 // count) / 100:.2f}%"


def verification(reports: Path) -> list[str]:
    results = stage_results(reports)
    failed = [stage for stage in REQUIRED_STAGES if results.get(stage) != "PASS"]
    if failed:
        raise ValueError("CI did not pass every mandatory stage: " + ", ".join(
            f"{stage}={results.get(stage, 'missing')}" for stage in failed))
    details = {}
    junit = reports / "junit.xml"
    if junit.is_file():
        text = junit.read_text()
        if match := re.search(r'<testsuite[^>]*\btests="(\d+)"[^>]*\bfailures="(\d+)"', text):
            details["Unit Tests"] = f"{match[1]} CTest suites, {match[2]} failures"
    totals = json.loads((reports / "coverage/summary.json").read_text())["totals"]
    lines, branches = totals["lines"], totals["branches"]
    details["Coverage"] = (f"lines {lines['covered']}/{lines['count']} ({percent(lines['covered'], lines['count'])}), "
                           f"branches {branches['covered']}/{branches['count']} "
                           f"({percent(branches['covered'], branches['count'])})")
    mutation = json.loads((reports / "mutation/summary.json").read_text())
    details["Mutation"] = (f"{mutation['assertion_killed']}/{mutation['unique_mutants']} mutants killed "
                           f"({mutation['score_percent'][:5]}%)")
    return [f"- ✅ {LABELS[stage]}" + (f": {details[stage]}" if stage in details else "")
            for stage in REQUIRED_STAGES]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--ci-run", required=True, help="URL of the qualifying CI run")
    parser.add_argument("--ci-reports", type=Path, required=True, help="the CI run's build-reports directory")
    parser.add_argument("--release-tests", required=True, help="release build unit-test result line")
    parser.add_argument("--changelog", type=Path, default=Path("documents/CHANGELOG"))
    parser.add_argument("--asset", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    notes = [f"# QtScrobbler {args.version}", "", "## Changes", "", *changelog_entry(args.changelog, args.version), "",
             "## Downloads (Linux x86-64)", "",
             "- **AppImage**: self-contained, bundles Qt 6.12. `chmod +x` and run it; "
             "the CLI is inside as `usr/bin/scrobbler`.",
             "- **tar.gz**: stripped `qtscrob` (GUI) and `scrobbler` (CLI); needs Qt >= 6.12 and libmtp installed.",
             "", "## Checks passed before this release", "",
             f"Full CI gate on commit `{args.commit}`: {args.ci_run}", "",
             *verification(args.ci_reports),
             f"- ✅ Release build: {args.release_tests}", "",
             "## SHA-256", "", "```text"]
    for asset in args.asset:
        notes.append(f"{hashlib.sha256(asset.read_bytes()).hexdigest()}  {asset.name}")
    notes += ["```", ""]
    args.output.write_text("\n".join(notes))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
