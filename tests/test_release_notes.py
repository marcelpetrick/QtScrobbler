# SPDX-License-Identifier: GPL-2.0-only
"""Release notes may only list checks that the qualifying CI run actually passed."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    "release_notes", Path(__file__).resolve().parents[1] / "tools/release_notes.py")
NOTES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NOTES)

SUMMARY = """Qt6 Check              : PASS Qt 6.12.0
Unit Tests             : PASS QTest suite passed
Format                 : PASS see Format.log
static                 : PASS see static.log
docs                   : PASS see docs.log
Sanitizers             : PASS see Sanitizers.log
Coverage               : PASS see Coverage.log
MutationToolchain      : PASS see MutationToolchain.log
Mutation               : PASS see Mutation.log
Package                : PASS see Package.log
Launch App             : SKIP Suppressed by --noRun
"""


class ReleaseNotesTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.reports = self.root / "build-reports"
        (self.reports / "coverage").mkdir(parents=True)
        (self.reports / "mutation").mkdir()
        (self.reports / "summary.txt").write_text(SUMMARY)
        (self.reports / "junit.xml").write_text('<testsuite name="x"\n\ttests="26"\n\tfailures="0">\n</testsuite>')
        (self.reports / "coverage/summary.json").write_text(json.dumps(
            {"totals": {"lines": {"count": 2749, "covered": 2722}, "branches": {"count": 1063, "covered": 1027}}}))
        (self.reports / "mutation/summary.json").write_text(json.dumps(
            {"assertion_killed": 1900, "unique_mutants": 1990, "score_percent": "95.47738693467336",
             "target_minimum": "67"}))
        self.changelog = self.root / "CHANGELOG"
        self.changelog.write_text("* version 1.2.3 (2026-11-17)\n- Fix one\n- Fix two\n\n"
                                  "* version 1.2.2 (2026-11-01)\n- Older\n")

    def write(self, version="1.2.3", assets=()):
        output = self.root / "notes.md"
        arguments = ["--version", version, "--commit", "abc123", "--ci-run", "https://ci/run/1",
                     "--ci-reports", str(self.reports), "--release-tests", "26/26 CTest suites PASS",
                     "--changelog", str(self.changelog), "--output", str(output)]
        for asset in assets:
            arguments += ["--asset", str(asset)]
        self.assertEqual(NOTES.main(arguments), 0)
        return output.read_text()

    def test_notes_contain_changes_checks_and_exact_checksums(self):
        asset = self.root / "QtScrobbler-1.2.3-x86_64.AppImage"
        asset.write_bytes(b"appimage bytes")
        notes = self.write(assets=[asset])
        self.assertIn("- Fix one\n- Fix two", notes)
        self.assertNotIn("Older", notes)
        self.assertIn("Full CI gate on commit `abc123`: https://ci/run/1", notes)
        self.assertIn("26 CTest suites, 0 failures", notes)
        self.assertIn("lines 2722/2749 (99.01%), branches 1027/1063 (96.61%)", notes)
        self.assertIn("1900/1990 mutants killed (95.47%; gate 67%, goal 95%)", notes)
        self.assertIn("Release build: 26/26 CTest suites PASS", notes)
        self.assertIn(f"{hashlib.sha256(b'appimage bytes').hexdigest()}  {asset.name}", notes)
        self.assertEqual(notes.count("- ✅"), len(NOTES.REQUIRED_STAGES) + 1)

    def test_percent_truncates_instead_of_rounding_up(self):
        self.assertEqual(NOTES.percent(9499, 10000), "94.99%")
        self.assertEqual(NOTES.percent(2, 3), "66.66%")

    def test_failed_or_missing_mandatory_stage_refuses_notes(self):
        for stage, replacement in (("Mutation", "Mutation               : FAIL see Mutation.log"),
                                   ("Sanitizers", "")):
            with self.subTest(stage=stage):
                lines = [line for line in SUMMARY.splitlines() if not line.startswith(stage + " ")]
                if replacement:
                    lines.append(replacement)
                (self.reports / "summary.txt").write_text("\n".join(lines) + "\n")
                with self.assertRaisesRegex(ValueError, stage):
                    self.write()

    def test_missing_changelog_entry_refuses_notes(self):
        with self.assertRaisesRegex(ValueError, "no entry for version 9.9.9"):
            self.write(version="9.9.9")


if __name__ == "__main__":
    unittest.main()
