#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Offline contracts for the checksum-pinned Windows Qt/MinGW SDK installer."""
from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location(
    "install_qt_sdk_windows", Path(__file__).resolve().parents[1] / "tools/install_qt_sdk_windows.py")
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)

# Fake 7z: "x -y -o<dest> <archive>" creates the relative paths listed in the archive.
FAKE_7Z = '''#!{python}
import sys
from pathlib import Path
dest = Path(sys.argv[3][2:])
for line in Path(sys.argv[4]).read_text().splitlines():
    target = dest / line
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("fixture")
'''


class WindowsQtInstallerTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)
        self.cache = self.work / "archives"
        self.cache.mkdir()
        self.seven_zip = self.work / "7z"
        self.seven_zip.write_text(FAKE_7Z.format(python=sys.executable))
        self.seven_zip.chmod(0o755)

    def archives(self, omit: str | None = None) -> dict[str, str]:
        """Write fake archives listing the REQUIRED files that land under their destination."""
        hashes = {}
        for name, _, relative in installer.ARCHIVES:
            contents = [str(path.relative_to(relative)) for path in installer.REQUIRED
                        if path.is_relative_to(relative) and str(path) != omit]
            data = "\n".join(contents).encode()
            (self.cache / name).write_bytes(data)
            hashes[name] = hashlib.sha256(data).hexdigest()
        return hashes

    def install(self, hashes):
        with mock.patch.object(installer, "pinned_hashes", return_value=hashes):
            return installer.install(self.work / "Qt", self.cache, str(self.seven_zip))

    def test_committed_manifest_pins_exactly_the_downloaded_archives(self):
        hashes = installer.pinned_hashes()
        self.assertEqual(set(hashes), {name for name, _, _ in installer.ARCHIVES})
        self.assertTrue(all(len(value) == 64 for value in hashes.values()))
        urls = [url for _, url, _ in installer.ARCHIVES]
        self.assertTrue(all(url.startswith("https://download.qt.io/online/qtsdkrepository/windows_x86/")
                            for url in urls))
        self.assertTrue(all(url.endswith(name) for name, url, _ in installer.ARCHIVES))

    def test_verified_archives_install_a_relocatable_kit(self):
        dest = self.install(self.archives())
        for path in installer.REQUIRED:
            self.assertTrue((dest / path).is_file(), path)
        self.assertEqual((dest / installer.QT_SUBDIR / "bin/qt.conf").read_text(), "[Paths]\nPrefix=..\n")
        self.assertEqual([path.name for path in self.work.iterdir() if path.name.startswith(".qtscrobbler")], [])

    def test_checksum_mismatch_removes_archive_and_installs_nothing(self):
        hashes = self.archives()
        name = installer.ARCHIVES[0][0]
        hashes[name] = "0" * 64
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            self.install(hashes)
        self.assertFalse((self.cache / name).exists())
        self.assertFalse((self.work / "Qt").exists())

    def test_incomplete_kit_is_rejected_before_publishing(self):
        missing = str(installer.QT_SUBDIR / "plugins/sqldrivers/qsqlite.dll")
        with self.assertRaisesRegex(ValueError, "incomplete.*qsqlite"):
            self.install(self.archives(omit=missing))
        self.assertFalse((self.work / "Qt").exists())

    def test_existing_destination_is_preserved_before_any_download(self):
        dest = self.work / "Qt"
        dest.mkdir()
        (dest / "keep").write_text("user data")
        with mock.patch.object(installer.urllib.request, "urlopen") as network:
            with self.assertRaisesRegex(ValueError, "already exists"):
                installer.install(dest, None, str(self.seven_zip))
            network.assert_not_called()
        self.assertEqual((dest / "keep").read_text(), "user data")

    def test_downloads_send_a_neutral_user_agent(self):
        target = self.work / "download.7z"
        with mock.patch.object(installer.urllib.request, "urlopen", return_value=io.BytesIO(b"bytes")) as network:
            installer.fetch("https://download.qt.io/fixture.7z", target)
        request = network.call_args.args[0]
        self.assertEqual(request.get_header("User-agent"), "qtscrobbler-tool-installer")
        self.assertEqual(target.read_bytes(), b"bytes")


if __name__ == "__main__":
    unittest.main()
