#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Classify unresolved mutants from independently verified LLVM source coverage.

Coverage describes source execution, not whether a mutant was detected. No result
from this module changes assertion kills, raw mutation outcomes or the denominator.
Gap regions, macro expansions and missing mappings remain explicitly unknown.
"""
from bisect import bisect_right
from collections import Counter
import hashlib
import json
from pathlib import Path
import sqlite3
from contextlib import closing


EXPORTS = ("coverage.json", "coverage.profdata")


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def point_in_region(point: tuple[int, int], region: list) -> bool:
    return (region[0], region[1]) <= point < (region[2], region[3])


class CoverageEvidence:
    def __init__(self, summary_path: Path, expected_sources: dict[str, str]):
        self.summary_path = summary_path.resolve()
        self.summary_sha256 = digest(self.summary_path)
        self.summary = json.loads(self.summary_path.read_text())
        if not expected_sources or self.summary.get("source_sha256") != expected_sources:
            raise ValueError("coverage source/test/build fingerprints differ from mutation campaign")
        if (self.summary.get("missing_production_files") != []
                or not self.summary.get("production_file_inventory")):
            raise ValueError("coverage evidence omits maintained production files")
        self.exports = self.summary.get("exports_sha256", {})
        if not isinstance(self.exports, dict) or not set(EXPORTS) <= self.exports.keys():
            raise ValueError("coverage export/profile fingerprints are missing")
        self.verify_unchanged()
        payload = json.loads((self.summary_path.parent / "coverage.json").read_text())
        if payload.get("type") != "llvm.coverage.json.export" or not payload.get("data"):
            raise ValueError("unsupported or empty LLVM coverage export")
        self.files = {}
        for data in payload["data"]:
            for record in data["files"]:
                name = str(Path(record["filename"]).resolve())
                if name in self.files:
                    raise ValueError("LLVM coverage contains duplicate file mappings")
                segments = record["segments"]
                previous = (0, 0)
                for segment in segments:
                    if (len(segment) != 6 or any(type(value) is not int or value < 0 for value in segment[:3])
                            or not all(type(value) is bool for value in segment[3:])
                            or segment[0] < 1 or segment[1] < 1
                            or (segment[0], segment[1]) < previous):
                        raise ValueError("invalid or unordered LLVM coverage segment")
                    previous = (segment[0], segment[1])
                expansions = [entry["source_region"] for entry in record.get("expansions", [])]
                for region in expansions:
                    if (len(region) < 4 or any(type(value) is not int or value < 1 for value in region[:4])
                            or (region[0], region[1]) >= (region[2], region[3])):
                        raise ValueError("invalid LLVM macro expansion region")
                self.files[name] = {"segments": segments, "expansions": expansions,
                                    "positions": [(row[0], row[1]) for row in segments]}

    def verify_unchanged(self):
        if digest(self.summary_path) != self.summary_sha256:
            raise ValueError("coverage summary changed during mutation campaign")
        for filename in EXPORTS:
            if digest(self.summary_path.parent / filename) != self.exports[filename]:
                raise ValueError(f"coverage artifact fingerprint mismatch: {filename}")

    def provenance(self) -> dict:
        return {"summary": str(self.summary_path), "summary_sha256": self.summary_sha256,
                "exports_sha256": self.exports,
                "semantics": "exact LLVM source point; gaps, macros and missing mappings are unknown"}

    def classify(self, filename: str, line: int, column: int) -> dict:
        point = (line, column)
        record = self.files.get(str(Path(filename).resolve()))
        if line < 1 or column < 1 or not record:
            return {"status": "unknown", "reason": "source position has no coverage mapping"}
        if any(point_in_region(point, region) for region in record["expansions"]):
            return {"status": "unknown", "reason": "source position is a macro expansion"}
        index = bisect_right(record["positions"], point) - 1
        if index < 0:
            return {"status": "unknown", "reason": "source position precedes coverage mapping"}
        segment = record["segments"][index]
        if not segment[3] or segment[5]:
            return {"status": "unknown", "reason": "source position is outside a counted region or in a gap"}
        return {"status": "covered" if segment[2] > 0 else "uncovered", "execution_count": segment[2]}


def classify_unresolved(summary: dict, reports: list[Path], evidence: CoverageEvidence | None) -> None:
    """Attach independent source execution classifications while retaining every outcome."""
    locations = {}
    for report in reports:
        with closing(sqlite3.connect(report.resolve().as_uri() + "?mode=ro", uri=True)) as database:
            for identity, filename, line, column in database.execute(
                    "SELECT mutant_id, filename, line_number, column_number FROM mutant"):
                location = (filename, line, column)
                if identity in locations and locations[identity] != location:
                    raise ValueError("conflicting mutant source positions during coverage classification")
                locations[identity] = location
    counts = Counter({"covered": 0, "uncovered": 0, "unknown": 0})
    for mutant in summary["unresolved"]:
        if mutant["id"] not in locations:
            raise ValueError("unresolved mutant has no recorded source position")
        classification = (evidence.classify(*locations[mutant["id"]]) if evidence is not None else
                          {"status": "unknown", "reason": "no verified independent LLVM coverage evidence supplied"})
        mutant["source_coverage"] = classification
        counts[classification["status"]] += 1
    summary["unresolved_source_coverage"] = dict(counts)
    summary["coverage_evidence"] = evidence.provenance() if evidence is not None else None
    if evidence is not None:
        evidence.verify_unchanged()
