#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Marcel Petrick
"""Score native Mull 0.38 SQLite reports using proven Qt Test assertion failures.

Usage: python3 tools/mutation_score.py REPORT.sqlite [...] --minimum 95 --output summary.json

Reports may contain the same source mutant from several executables. It is counted once,
and is killed if any execution demonstrates a normal nonzero Qt Test assertion failure.
Timeouts, signals, unchecked exits and missing coverage never count as kills. Malformed,
missing, unknown-version and infrastructure-error reports fail closed.
"""

from __future__ import annotations

import argparse
from collections import Counter
from contextlib import closing
from decimal import Decimal
import json
from pathlib import Path
import re
import sqlite3
import sys


STATUSES = {
    0: "invalid", 1: "failed", 2: "survived", 3: "timeout", 4: "crashed",
    5: "abnormal_exit", 6: "dry_run", 7: "fail_fast", 8: "uncovered",
}
REQUIRED_COLUMNS = {
    "mutant_id", "mutator", "filename", "line_number", "column_number",
    "execution_status", "exit_status", "stdout", "stderr", "mutation_replacement",
}


def assertion_killed(row: sqlite3.Row) -> bool:
    """Require captured QTest evidence as well as Mull's generic Failed result."""
    output = (row["stdout"] or "") + "\n" + (row["stderr"] or "")
    totals = re.search(r"Totals:\s+\d+ passed,\s+(\d+) failed,", output)
    return (
        row["execution_status"] == 1
        and 0 < row["exit_status"] < 126
        and re.search(r"(?m)^FAIL!\s+: ", output) is not None
        and totals is not None
        and int(totals.group(1)) > 0
        and "QFATAL" not in output
    )


def summarize(reports: list[Path]) -> dict:
    """Validate reports, aggregate by stable mutant ID, and retain all outcome counts."""
    if not reports:
        raise ValueError("at least one fresh Mull SQLite report is required")
    mutants: dict[str, dict] = {}
    executions: Counter = Counter()
    errors: list[str] = []
    for report in reports:
        if not report.is_file():
            raise ValueError(f"missing report: {report}")
        with closing(sqlite3.connect(report.resolve().as_uri() + "?mode=ro", uri=True)) as database:
            database.row_factory = sqlite3.Row
            columns = {row[1] for row in database.execute("PRAGMA table_info(mutant)")}
            if not REQUIRED_COLUMNS <= columns:
                raise ValueError(f"unsupported Mull SQLite schema: {report}")
            versions = {
                row[0] for row in database.execute(
                    "SELECT value FROM information WHERE key = 'Mull Version'"
                )
            }
            if versions != {"0.38.0"}:
                raise ValueError(f"unsupported or missing Mull version in {report}: {versions}")
            rows = database.execute("SELECT * FROM mutant").fetchall()
            if not rows:
                raise ValueError(f"empty mutation report: {report}")
            for row in rows:
                identity = row["mutant_id"]
                if not identity or not row["filename"] or row["line_number"] < 1:
                    raise ValueError(f"invalid mutant identity/location in {report}")
                if "_autogen/" in row["filename"] or "/CMakeFiles/" in row["filename"]:
                    raise ValueError(f"generated code leaked into production mutation scope: {identity}")
                status = row["execution_status"]
                if status not in STATUSES:
                    raise ValueError(f"unknown execution status {status} in {report}")
                executions[STATUSES[status]] += 1
                signature = (
                    row["mutator"], row["filename"], row["line_number"],
                    row["column_number"], row["mutation_replacement"],
                )
                mutant = mutants.setdefault(identity, {
                    "signature": signature, "killed": False, "outcomes": set(),
                })
                if mutant["signature"] != signature:
                    raise ValueError(f"conflicting source/replacement for mutant {identity}")
                mutant["killed"] |= assertion_killed(row)
                mutant["outcomes"].add(STATUSES[status])
                output = (row["stdout"] or "") + "\n" + (row["stderr"] or "")
                if status in {0, 6} or "Failed to spawn:" in output or "Wait error:" in output:
                    errors.append(f"{identity}: {STATUSES[status]} or execution infrastructure error")

    if not mutants:
        raise ValueError("campaign generated zero mutants")
    killed = sum(mutant["killed"] for mutant in mutants.values())
    unresolved = [
        {"id": identity, "outcomes": sorted(mutant["outcomes"])}
        for identity, mutant in sorted(mutants.items()) if not mutant["killed"]
    ]
    # FailFast is only trustworthy if another execution proves this exact mutant was killed.
    errors.extend(
        f"{item['id']}: fail-fast result has no proven assertion kill"
        for item in unresolved if "fail_fast" in item["outcomes"]
    )
    return {
        "schema_version": 1,
        "mull_version": "0.38.0",
        "reports": [str(report.resolve()) for report in reports],
        "unique_mutants": len(mutants),
        "assertion_killed": killed,
        "not_assertion_killed": len(mutants) - killed,
        "score_percent": str(Decimal(killed) * 100 / Decimal(len(mutants))),
        "execution_outcomes": dict(sorted(executions.items())),
        "equivalent_exclusions": 0,
        "infrastructure_errors": sorted(set(errors)),
        "unresolved": unresolved,
    }


def passes(summary: dict, minimum: Decimal) -> bool:
    """Compare integer totals without using a displayed or rounded percentage."""
    if not minimum.is_finite() or not 0 <= minimum <= 100:
        raise ValueError("minimum mutation score must be between 0 and 100")
    return (
        not summary["infrastructure_errors"]
        and Decimal(summary["assertion_killed"]) * 100
        >= minimum * summary["unique_mutants"]
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reports", type=Path, nargs="+")
    parser.add_argument("--minimum", type=Decimal, default=Decimal("95"))
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        summary = summarize(arguments.reports)
        successful = passes(summary, arguments.minimum)
        summary["minimum_percent"] = str(arguments.minimum)
        summary["passed"] = successful
        rendered = json.dumps(summary, indent=2) + "\n"
        if arguments.output:
            arguments.output.write_text(rendered)
        print(rendered, end="")
        return 0 if successful else 1
    except (ValueError, OSError, sqlite3.Error) as error:
        print(f"Mutation report error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
