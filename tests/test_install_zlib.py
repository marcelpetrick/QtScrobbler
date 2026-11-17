"""Offline integrity and failure-propagation contracts for private zlib provisioning."""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location(
    "install_zlib", Path(__file__).resolve().parents[1] / "tools/install_zlib.py")
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)


class ZlibInstallerTest(unittest.TestCase):
    def test_corrupt_archive_does_not_extract(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "bad.tar.gz"
            archive.write_bytes(b"untrusted download")
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                installer.unpack(archive, root / "output")
            self.assertFalse((root / "output").exists())

    def test_archive_path_traversal_and_links_are_rejected(self):
        for name, link in (("../../escaped", False), ("zlib-1.4/escape", True)):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                archive = root / "archive.tar.gz"
                with tarfile.open(archive, "w:gz") as output:
                    info = tarfile.TarInfo(name)
                    if link:
                        info.type = tarfile.SYMTYPE
                        info.linkname = "/etc/passwd"
                        output.addfile(info)
                    else:
                        info.size = 1
                        output.addfile(info, io.BytesIO(b"x"))
                with mock.patch.object(installer, "SHA256", installer.digest(archive)):
                    with self.assertRaisesRegex(ValueError, "unsafe zlib archive"):
                        installer.unpack(archive, root / "output")
                self.assertFalse((root / "output").exists())

    def test_failed_stage_is_fatal_and_retains_output(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "stage.log"
            with self.assertRaisesRegex(RuntimeError, "command failed \\(7\\)"):
                installer.run([sys.executable, "-c", "print('deliberate failure'); raise SystemExit(7)"], log)
            self.assertIn("deliberate failure", log.read_text())

    def test_install_manifest_detects_changes_and_missing_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory)
            files = {}
            for relative in ("include/zlib.h", "include/zconf.h", "lib/libz.a"):
                path = prefix / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"qualified contents")
                files[relative] = installer.digest(path)
            manifest = {"version": installer.VERSION, "source_sha256": installer.SHA256, "installed_files": files}
            (prefix / installer.MANIFEST).write_text(json.dumps(manifest))
            self.assertTrue(installer.verified_install(prefix))
            (prefix / "lib/libz.a").write_bytes(b"changed")
            with self.assertRaisesRegex(ValueError, "missing or changed"):
                installer.verified_install(prefix)
            (prefix / "lib/libz.a").unlink()
            with self.assertRaisesRegex(ValueError, "inventory differs"):
                installer.verified_install(prefix)

    def test_untracked_library_cannot_override_qualified_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory)
            files = {}
            for relative in ("include/zlib.h", "include/zconf.h", "lib/libz.a"):
                path = prefix / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"qualified")
                files[relative] = installer.digest(path)
            data = {"version": installer.VERSION, "source_sha256": installer.SHA256, "installed_files": files}
            (prefix / installer.MANIFEST).write_text(json.dumps(data))
            (prefix / "lib/libz.so").write_bytes(b"unqualified shared library")
            with self.assertRaisesRegex(ValueError, "inventory differs"):
                installer.verified_install(prefix)

    def test_manifest_with_wrong_resolution_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory)
            (prefix / installer.MANIFEST).write_text(json.dumps({"version": "1.3.2"}))
            with self.assertRaisesRegex(ValueError, "different resolution"):
                installer.verified_install(prefix)

    def test_nonempty_prefix_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory)
            marker = prefix / "keep"
            marker.write_text("user data")
            args = argparse.Namespace(prefix=prefix, jobs=1)
            with self.assertRaisesRegex(ValueError, "prefix must be empty"):
                installer.install(args)
            self.assertEqual(marker.read_text(), "user data")

    def test_global_prefix_is_rejected(self):
        args = argparse.Namespace(prefix=Path("/"), jobs=1)
        with self.assertRaisesRegex(ValueError, "dedicated private"):
            installer.install(args)


if __name__ == "__main__":
    unittest.main()
