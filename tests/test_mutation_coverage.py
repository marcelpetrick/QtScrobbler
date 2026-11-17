"""Trusted LLVM source-point classification never changes mutation outcomes or score."""
from contextlib import closing
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    "mutation_coverage", Path(__file__).resolve().parents[1] / "tools/mutation_coverage.py")
coverage = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(coverage)


class MutationCoverageTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = str(self.root / "src/lib/example.cpp")
        self.sources = {"src/lib/example.cpp": "qualified source", "tests/test_example.cpp": "qualified tests"}
        self.export = {"type": "llvm.coverage.json.export", "version": "2.0.1", "data": [{"files": [{
            "filename": self.source,
            "segments": [[10, 1, 7, True, True, False], [10, 5, 0, True, True, False],
                         [10, 8, 7, True, False, False], [11, 1, 0, True, False, True],
                         [12, 1, 3, True, True, False], [13, 1, 0, False, False, False]],
            "expansions": [{"source_region": [12, 3, 12, 9, 3, 0, 1, 1]}],
        }]}]}
        self.write_evidence()

    def write_evidence(self):
        (self.root / "coverage.json").write_text(json.dumps(self.export))
        (self.root / "coverage.profdata").write_bytes(b"qualified indexed LLVM profile")
        self.summary = {"source_sha256": self.sources, "missing_production_files": [],
                        "production_file_inventory": ["src/lib/example.cpp"],
                        "exports_sha256": {name: coverage.digest(self.root / name) for name in coverage.EXPORTS}}
        self.path = self.root / "summary.json"
        self.path.write_text(json.dumps(self.summary))

    def evidence(self):
        return coverage.CoverageEvidence(self.path, self.sources)

    def test_exact_columns_region_transitions_gaps_macros_and_missing_mapping(self):
        evidence = self.evidence()
        for line, column, status in [(9, 99, "unknown"), (10, 1, "covered"), (10, 4, "covered"),
                                     (10, 5, "uncovered"), (10, 7, "uncovered"), (10, 8, "covered"),
                                     (11, 1, "unknown"), (11, 99, "unknown"), (12, 1, "covered"),
                                     (12, 3, "unknown"), (12, 8, "unknown"), (12, 9, "covered"),
                                     (13, 1, "unknown"), (14, 1, "unknown")]:
            with self.subTest(line=line, column=column):
                self.assertEqual(evidence.classify(self.source, line, column)["status"], status)
        self.assertEqual(evidence.classify("/unmapped/source.cpp", 10, 1)["status"], "unknown")
        self.assertEqual(evidence.classify(self.source, 0, 0)["status"], "unknown")
        self.assertEqual(evidence.classify(self.source, 10, 1)["execution_count"], 7)
        self.assertEqual(evidence.classify(self.source, 10, 5)["execution_count"], 0)

    def test_equal_position_segments_use_final_effective_region(self):
        self.export["data"][0]["files"][0]["segments"] = [
            [10, 1, 0, False, True, False], [10, 1, 7, True, False, False],
            [11, 1, 0, False, True, False], [11, 1, 3, True, False, True],
            [12, 1, 0, False, False, False],
        ]
        self.write_evidence()
        evidence = self.evidence()
        self.assertEqual(evidence.classify(self.source, 10, 1), {"status": "covered", "execution_count": 7})
        self.assertEqual(evidence.classify(self.source, 10, 50)["status"], "covered")
        self.assertEqual(evidence.classify(self.source, 11, 1)["status"], "unknown")

    def test_source_and_test_fingerprint_mismatch_is_fatal(self):
        for changed in ("src/lib/example.cpp", "tests/test_example.cpp"):
            expected = dict(self.sources, **{changed: "different content"})
            with self.subTest(changed=changed), self.assertRaisesRegex(ValueError, "fingerprints differ"):
                coverage.CoverageEvidence(self.path, expected)

    def test_export_or_profile_tampering_is_fatal(self):
        for name in coverage.EXPORTS:
            self.write_evidence()
            (self.root / name).write_bytes(b"changed artifact")
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "artifact fingerprint mismatch"):
                self.evidence()

    def test_incomplete_inventory_or_missing_export_hash_is_fatal(self):
        for key, value in [("missing_production_files", ["src/lib/unexecuted.cpp"]),
                           ("production_file_inventory", []), ("exports_sha256", {})]:
            self.write_evidence()
            self.summary[key] = value
            self.path.write_text(json.dumps(self.summary))
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "omits|fingerprints are missing"):
                self.evidence()

    def test_changed_evidence_after_loading_is_fatal(self):
        for filename in ("summary.json", "coverage.json", "coverage.profdata"):
            self.write_evidence()
            evidence = self.evidence()
            (self.root / filename).write_text("changed evidence")
            with self.subTest(filename=filename), self.assertRaisesRegex(ValueError, "changed|mismatch"):
                evidence.verify_unchanged()

    def test_unordered_or_invalid_segments_and_duplicate_files_are_fatal(self):
        for invalid in ([[10, 8, 7, True, False, False], [10, 1, 7, True, True, False]],
                        [[10, 1, -1, True, True, False]], [[10, 1, 1, "true", True, False]]):
            self.export["data"][0]["files"][0]["segments"] = invalid
            self.write_evidence()
            with self.assertRaisesRegex(ValueError, "coverage segment"):
                self.evidence()
        self.export["data"][0]["files"][0]["segments"] = []
        self.export["data"][0]["files"].append(dict(self.export["data"][0]["files"][0]))
        self.write_evidence()
        with self.assertRaisesRegex(ValueError, "duplicate file"):
            self.evidence()

    def sqlite_report(self):
        report = self.root / "mutants.sqlite"
        with closing(sqlite3.connect(report)) as database, database:
            database.execute("CREATE TABLE mutant (mutant_id TEXT, filename TEXT, line_number INT, column_number INT)")
            database.executemany("INSERT INTO mutant VALUES (?, ?, ?, ?)",
                                 [("covered", self.source, 10, 3), ("uncovered", self.source, 10, 6),
                                  ("gap", self.source, 11, 8), ("macro", self.source, 12, 5)])
        return report

    def score_summary(self):
        return {"unique_mutants": 10, "assertion_killed": 6, "score_percent": "60",
                "execution_outcomes": {"survived": 2, "timeout": 1, "crashed": 1},
                "unresolved": [{"id": "covered", "outcomes": ["survived"]},
                               {"id": "uncovered", "outcomes": ["survived"]},
                               {"id": "gap", "outcomes": ["timeout"]},
                               {"id": "macro", "outcomes": ["crashed"]}]}

    def test_classification_preserves_score_denominator_and_raw_outcomes(self):
        summary = self.score_summary()
        before = json.loads(json.dumps(summary))
        report = self.sqlite_report()
        coverage.classify_unresolved(summary, [report, report], self.evidence())
        self.assertEqual(summary["unresolved_source_coverage"], {"covered": 1, "uncovered": 1, "unknown": 2})
        for key in ("unique_mutants", "assertion_killed", "score_percent", "execution_outcomes"):
            self.assertEqual(summary[key], before[key])
        self.assertEqual([row["outcomes"] for row in summary["unresolved"]],
                         [row["outcomes"] for row in before["unresolved"]])

    def test_absent_evidence_is_explicitly_unknown(self):
        summary = self.score_summary()
        coverage.classify_unresolved(summary, [self.sqlite_report()], None)
        self.assertEqual(summary["unresolved_source_coverage"], {"covered": 0, "uncovered": 0, "unknown": 4})
        self.assertIsNone(summary["coverage_evidence"])
        self.assertTrue(all("no verified" in row["source_coverage"]["reason"] for row in summary["unresolved"]))

    def test_missing_or_conflicting_mutant_positions_are_fatal(self):
        summary = self.score_summary()
        report = self.sqlite_report()
        with closing(sqlite3.connect(report)) as database, database:
            database.execute("DELETE FROM mutant WHERE mutant_id='covered'")
        with self.assertRaisesRegex(ValueError, "no recorded source position"):
            coverage.classify_unresolved(summary, [report], self.evidence())
        with closing(sqlite3.connect(report)) as database, database:
            database.executemany("INSERT INTO mutant VALUES (?, ?, ?, ?)",
                                 [("covered", self.source, 10, 3), ("covered", self.source, 10, 6)])
        with self.assertRaisesRegex(ValueError, "conflicting mutant source positions"):
            coverage.classify_unresolved(self.score_summary(), [report], self.evidence())


if __name__ == "__main__":
    unittest.main()
