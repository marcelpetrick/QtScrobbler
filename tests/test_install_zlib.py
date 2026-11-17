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

    def test_corrupt_primary_falls_back_to_same_pinned_official_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "download.tar.gz"
            expected = hashlib.sha256(b"verified source").hexdigest()
            with mock.patch.object(installer, "SHA256", expected), mock.patch.object(
                    installer.urllib.request, "urlopen", side_effect=[io.BytesIO(b"bad response"), io.BytesIO(b"verified source")]) as request:
                selected = installer.download_source(archive, root)
            self.assertEqual(selected, installer.SOURCE_URLS[1])
            self.assertEqual(archive.read_bytes(), b"verified source")
            self.assertEqual(request.call_args_list, [mock.call(url, timeout=60) for url in installer.SOURCE_URLS])
            evidence = json.loads((root / "downloads.json").read_text())
            self.assertEqual([item["status"] for item in evidence], ["checksum_mismatch", "verified"])
            self.assertEqual(evidence[0]["actual_sha256"], hashlib.sha256(b"bad response").hexdigest())
            self.assertEqual(evidence[0]["size_bytes"], len(b"bad response"))
            self.assertEqual(evidence[1]["actual_sha256"], expected)
            self.assertTrue(all(item["expected_sha256"] == expected for item in evidence))

    def test_all_mismatches_fail_with_retained_digests_and_bounded_attempts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(installer.urllib.request, "urlopen", side_effect=[io.BytesIO(b"bad first"), io.BytesIO(b"bad second")]) as request:
                with self.assertRaisesRegex(ValueError, "all pinned zlib sources failed") as error:
                    installer.download_source(root / "download.tar.gz", root)
            self.assertEqual(request.call_count, 2)
            self.assertIn(installer.SHA256, str(error.exception))
            self.assertIn(hashlib.sha256(b"bad second").hexdigest(), str(error.exception))
            evidence = json.loads((root / "downloads.json").read_text())
            self.assertEqual([item["status"] for item in evidence], ["checksum_mismatch"] * 2)
            self.assertTrue(all(item["error"] for item in evidence))
            self.assertEqual([item["url"] for item in evidence], list(installer.SOURCE_URLS))

    def test_network_failure_falls_back_without_stale_download_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "download.tar.gz"
            archive.write_bytes(b"stale bytes")
            expected = hashlib.sha256(b"verified source").hexdigest()
            with mock.patch.object(installer, "SHA256", expected), mock.patch.object(
                    installer.urllib.request, "urlopen", side_effect=[OSError("connection closed"), io.BytesIO(b"verified source")]):
                installer.download_source(archive, root)
            evidence = json.loads((root / "downloads.json").read_text())
            self.assertEqual(evidence[0]["status"], "download_error")
            self.assertEqual(evidence[0]["size_bytes"], 0)
            self.assertIsNone(evidence[0]["actual_sha256"])
            self.assertEqual(evidence[0]["error"], "connection closed")
            self.assertEqual(evidence[1]["status"], "verified")

    def test_partial_transfer_failure_preserves_partial_digest(self):
        class Interrupted(io.BytesIO):
            def read(self, size=-1):
                if self.tell():
                    raise installer.http.client.IncompleteRead(b"", 10)
                return super().read(size)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(installer.urllib.request, "urlopen", side_effect=[Interrupted(b"partial"), OSError("mirror unavailable")]):
                with self.assertRaises(ValueError):
                    installer.download_source(root / "download.tar.gz", root)
            evidence = json.loads((root / "downloads.json").read_text())
            self.assertEqual(evidence[0]["status"], "download_error")
            self.assertEqual(evidence[0]["size_bytes"], 7)
            self.assertEqual(evidence[0]["actual_sha256"], hashlib.sha256(b"partial").hexdigest())
            self.assertEqual(evidence[1]["error"], "mirror unavailable")

    def test_explicit_corrupt_archive_is_fatal_without_download_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "bin").mkdir()
            for name in ("cmake", "ctest"):
                (root / "bin" / name).write_text("fixture")
            archive = root / "explicit.tar.gz"
            archive.write_bytes(b"explicit bad archive")
            args = argparse.Namespace(prefix=root / "prefix", jobs=1, archive=archive,
                                      report_dir=root / "reports", cmake=root / "bin/cmake")
            with mock.patch.object(installer.urllib.request, "urlopen") as request:
                with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                    installer.install(args)
            request.assert_not_called()
            self.assertFalse(args.prefix.exists())
            self.assertEqual(archive.read_bytes(), b"explicit bad archive")

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
