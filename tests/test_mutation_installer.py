"""Offline native toolchain installation, preservation and integrity contracts."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location(
    "install_mutation_toolchain", Path(__file__).resolve().parents[1] / "tools/install_mutation_toolchain.py")
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)


class MutationInstallerTest(unittest.TestCase):
    def native_package(self, root, *, version="20.1.8 0.38.0"):
        self.assertIsNotNone(shutil.which("bsdtar"), "native installer contracts require bsdtar")
        self.assertIsNotNone(shutil.which("ar"), "native installer contracts require ar")
        data = root / "data.tar.gz"
        with tarfile.open(data, "w:gz") as archive:
            for name in ("clang-20", "mull-runner", "llvm-cov-20", "llvm-profdata-20"):
                content = f'#!/bin/sh\nprintf "%s\\n" "{version}"\n'.encode()
                member = tarfile.TarInfo("usr/bin/" + name)
                member.mode = 0o755
                member.size = len(content)
                archive.addfile(member, io.BytesIO(content))
            member = tarfile.TarInfo("usr/bin/clang++-20")
            member.type = tarfile.SYMTYPE
            member.linkname = "clang-20"
            archive.addfile(member)
        package = root / "fixture.deb"
        subprocess.run(["ar", "rc", str(package), str(data)], check=True, capture_output=True)
        return package.read_bytes()

    def install_fixture(self, root, *, version="20.1.8 0.38.0", digest=None):
        package = self.native_package(root, version=version)
        artifacts = [("https://fixture.invalid/fixture.deb", digest or hashlib.sha256(package).hexdigest())]
        with mock.patch.object(installer, "ARTIFACTS", artifacts), \
                mock.patch.object(installer.urllib.request, "urlopen", return_value=io.BytesIO(package)):
            result = installer.install(root / "prefix")
        return result, artifacts

    def test_downloads_send_a_neutral_user_agent(self):
        # archive.ubuntu.com answers the default Python-urllib agent with HTTP 403.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = self.native_package(root)
            artifacts = [("https://fixture.invalid/fixture.deb", hashlib.sha256(package).hexdigest())]
            with mock.patch.object(installer, "ARTIFACTS", artifacts), \
                    mock.patch.object(installer.urllib.request, "urlopen", return_value=io.BytesIO(package)) as download:
                installer.install(root / "prefix")
            request = download.call_args.args[0]
            self.assertEqual(request.full_url, "https://fixture.invalid/fixture.deb")
            self.assertEqual(request.get_header("User-agent"), "qtscrobbler-tool-installer")
            self.assertNotIn("python", request.get_header("User-agent").lower())

    def test_unrelated_nonempty_prefix_is_preserved_before_any_download(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "prefix"
            marker = prefix / "usr/bin/clang-20"
            marker.parent.mkdir(parents=True)
            marker.write_text("original user tool")
            before = installer.installed_inventory(prefix)
            with mock.patch.object(installer.urllib.request, "urlopen") as download:
                with self.assertRaisesRegex(ValueError, "prefix must be empty"):
                    installer.install(prefix)
                download.assert_not_called()
            self.assertEqual(installer.installed_inventory(prefix), before)
            self.assertEqual(marker.read_text(), "original user tool")

    def test_native_package_installs_and_reuses_without_download_or_extraction(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest, artifacts = self.install_fixture(root)
            prefix = root / "prefix"
            self.assertEqual(manifest["schema_version"], 2)
            self.assertIn(str(prefix / "usr/bin/clang-20"), (prefix / "env.sh").read_text())
            self.assertNotIn(".qtscrobbler-native-", (prefix / "env.sh").read_text())
            self.assertTrue((prefix / "usr/bin/clang++-20").is_symlink())
            with mock.patch.object(installer, "ARTIFACTS", artifacts), \
                    mock.patch.object(installer.urllib.request, "urlopen") as download, \
                    mock.patch.object(installer.subprocess, "run") as extract:
                self.assertEqual(installer.install(prefix), manifest)
                download.assert_not_called()
                extract.assert_not_called()

    def test_changed_missing_added_or_retargeted_artifact_is_preserved_and_rejected(self):
        for change in ("changed", "missing", "added", "symlink", "permissions"):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                _, artifacts = self.install_fixture(root)
                prefix = root / "prefix"
                tool = prefix / "usr/bin/clang-20"
                if change == "changed":
                    tool.write_text("changed tool")
                elif change == "missing":
                    tool.unlink()
                elif change == "added":
                    (prefix / "usr/bin/unqualified").write_text("unexpected tool")
                elif change == "symlink":
                    link = prefix / "usr/bin/clang++-20"
                    link.unlink()
                    link.symlink_to("mull-runner")
                else:
                    tool.chmod(0o644)
                before = installer.installed_inventory(prefix)
                with mock.patch.object(installer, "ARTIFACTS", artifacts), \
                        mock.patch.object(installer.urllib.request, "urlopen") as download:
                    with self.assertRaisesRegex(ValueError, "inventory differs"):
                        installer.install(prefix)
                    download.assert_not_called()
                self.assertEqual(installer.installed_inventory(prefix), before)

    def test_old_manifest_cannot_authorize_overwriting_existing_tools(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory)
            (prefix / installer.MANIFEST).write_text(json.dumps({"schema_version": 1}))
            (prefix / "keep").write_text("preserved")
            with self.assertRaisesRegex(ValueError, "unrecognized native toolchain manifest"):
                installer.install(prefix)
            self.assertEqual((prefix / "keep").read_text(), "preserved")

    def test_failed_checksum_or_qualification_does_not_publish_partial_installation(self):
        for failure in ("checksum", "qualification"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                arguments = {"digest": "0" * 64} if failure == "checksum" else {"version": "wrong"}
                with self.assertRaisesRegex(ValueError, "checksum mismatch|unexpected executable version"):
                    self.install_fixture(root, **arguments)
                self.assertFalse((root / "prefix").exists())
                self.assertEqual(list(root.glob(".qtscrobbler-native-*")), [])

    def test_empty_prefix_can_be_published_without_replacing_user_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "prefix").mkdir()
            self.install_fixture(root)
            self.assertTrue((root / "prefix" / installer.MANIFEST).is_file())


if __name__ == "__main__":
    unittest.main()
