#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Selection must save duplicate execution without changing the candidate denominator."""
from contextlib import closing
import importlib.util
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import run_mutation_tests as campaign
from mutation_score import summarize

ASSERTION = "FAIL!  : Test::boundary() comparison failed\nTotals: 0 passed, 1 failed, 0 skipped,\n"


class CampaignSelectionTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.base = self.root / "mull.yml"
        self.base.write_text("mutators:\n  - cxx_all\nparallelization:\n  workers: 4\n")

    def signature(self, line=10, column=2, replacement="<="):
        return ("cxx_lt_to_le", "/source/src/lib/common.cpp", line, column, replacement)

    def report(self, name, rows, version="0.38.0"):
        path = self.root / name
        with closing(sqlite3.connect(path)) as db, db:
            db.execute("CREATE TABLE information (key TEXT, value TEXT)")
            db.execute("INSERT INTO information VALUES ('Mull Version', ?)", (version,))
            db.execute("""CREATE TABLE mutant (mutant_id TEXT, mutator TEXT, filename TEXT,
                line_number INT, column_number INT, mutation_replacement TEXT,
                execution_status INT, exit_status INT, stdout TEXT, stderr TEXT)""")
            for identity, signature, status, code, output in rows:
                db.execute("INSERT INTO mutant VALUES (?,?,?,?,?,?,?,?,?,?)",
                           (identity, *signature, status, code, output, ""))
        return path

    def test_killed_siblings_are_retained_when_same_line_is_unresolved(self):
        candidates = {"a": self.signature(), "b": self.signature(column=7), "c": self.signature(11)}
        self.assertEqual(campaign.selected_candidates(candidates, {"a", "c"}),
                         {"a": candidates["a"], "b": candidates["b"]})
        self.assertEqual(campaign.selected_candidates(candidates, set(candidates)), {})

    def test_discovery_and_execution_evidence_remain_separate(self):
        inventory = self.report("inventory.sqlite", [("a", self.signature(), 6, 0, "")])
        self.assertEqual(campaign.read_candidates(inventory, discovery=True), ({"a": self.signature()}, set()))
        with self.assertRaisesRegex(ValueError, "discovery/execution"):
            campaign.read_candidates(inventory, discovery=False)
        self.assertTrue(summarize([inventory])["infrastructure_errors"])

    def test_only_proven_assertion_failures_allow_deduplication(self):
        rows = [("a", self.signature(), 1, 1, ASSERTION),
                ("timeout", self.signature(11), 3, 0, ""),
                ("crash", self.signature(12), 4, 11, ASSERTION),
                ("unchecked", self.signature(13), 1, 1, "failure"),
                ("fatal", self.signature(14), 1, 1, ASSERTION + "QFATAL")]
        candidates, killed = campaign.read_candidates(self.report("out.sqlite", rows), discovery=False)
        self.assertEqual(killed, {"a"})
        self.assertEqual(set(campaign.selected_candidates(candidates, killed)), set(candidates) - {"a"})

    def test_aggregate_denominator_and_native_outcomes_are_unchanged(self):
        signatures = {"a": self.signature(), "b": self.signature(11), "c": self.signature(12)}
        first = self.report("first.sqlite", [("a", signatures["a"], 1, 1, ASSERTION),
                                              ("b", signatures["b"], 2, 0, "")])
        second = self.report("second.sqlite", [("b", signatures["b"], 1, 1, ASSERTION),
                                                ("c", signatures["c"], 2, 0, "")])
        executed = {}
        killed = set()
        for path in [first, second]:
            found, detected = campaign.read_candidates(path, discovery=False)
            campaign.merge_candidates(executed, found)
            killed.update(detected)
        campaign.require_complete_inventory(signatures, executed)
        summary = summarize([first, second])
        self.assertEqual((summary["unique_mutants"], summary["assertion_killed"]), (3, 2))
        self.assertFalse(summary["infrastructure_errors"])
        self.assertEqual(killed, {"a", "b"})

    def test_missing_extra_and_changed_candidates_fail_integrity(self):
        candidates = {"a": self.signature()}
        for actual in [{}, {**candidates, "extra": self.signature()}, {"a": self.signature(replacement=">=")}]:
            with self.subTest(actual=actual), self.assertRaisesRegex(ValueError, "inventory differs"):
                campaign.require_complete_inventory(candidates, actual)
        with self.assertRaisesRegex(ValueError, "inventory differs"):
            campaign.require_complete_inventory({}, {})

    def test_conflicting_signatures_are_never_merged(self):
        with self.assertRaisesRegex(ValueError, "conflicting candidate signature"):
            campaign.merge_candidates({"a": self.signature()}, {"a": self.signature(column=8)})
        path = self.report("conflict.sqlite", [("a", self.signature(), 6, 0, ""),
                                               ("a", self.signature(11), 6, 0, "")])
        with self.assertRaisesRegex(ValueError, "conflicting inventory signature"):
            campaign.read_candidates(path, discovery=True)

    def test_generated_ranges_are_absolute_nonempty_unique_lines(self):
        config, ranges = self.root / "runner.yml", self.root / "ranges.txt"
        campaign.write_selection(self.base, config, ranges,
                                 {"a": self.signature(), "b": self.signature(column=8), "c": self.signature(11)})
        self.assertEqual(ranges.read_text(), "/source/src/lib/common.cpp:10\n/source/src/lib/common.cpp:11\n")
        self.assertIn(f'mutationRanges: "{ranges}"', config.read_text())
        self.assertTrue(config.read_text().startswith(self.base.read_text()))
        with self.assertRaisesRegex(ValueError, "blank mutation ranges"):
            campaign.write_selection(self.base, config, ranges, {})

    def test_incremental_base_configuration_cannot_shrink_discovery(self):
        for key in ["mutationRanges", "gitDiffRef", "gitProjectRoot", "gitDiffPaths"]:
            self.base.write_text(key + ": forbidden\n")
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "incremental selection"):
                campaign.validate_base_config(self.base)

    def test_mutated_deleted_evidence_fails_hash_binding(self):
        path = self.root / "evidence"
        path.write_text("original")
        evidence = {str(path): campaign.sha256(path)}
        campaign.verify_artifacts(evidence)
        path.write_text("changed")
        with self.assertRaisesRegex(ValueError, "evidence changed"):
            campaign.verify_artifacts(evidence)
        path.unlink()
        with self.assertRaisesRegex(ValueError, "evidence changed"):
            campaign.verify_artifacts(evidence)

    def test_empty_sqlite_report_cannot_hide_candidates(self):
        for discovery in (True, False):
            with self.subTest(discovery=discovery), self.assertRaisesRegex(ValueError, "empty native candidate"):
                campaign.read_candidates(self.report(f"empty-{discovery}.sqlite", []), discovery=discovery)

    def test_wrong_version_and_execution_in_inventory_rejected(self):
        wrong = self.report("wrong.sqlite", [("a", self.signature(), 6, 0, "")], "0.39.0")
        with self.assertRaisesRegex(ValueError, "Mull version"):
            campaign.read_candidates(wrong, discovery=True)
        execution = self.report("execution.sqlite", [("a", self.signature(), 2, 0, "")])
        with self.assertRaisesRegex(ValueError, "discovery/execution"):
            campaign.read_candidates(execution, discovery=True)


if __name__ == "__main__":
    unittest.main()
