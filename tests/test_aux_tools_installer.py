#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Offline contracts for private auxiliary-tool installation integrity."""
from __future__ import annotations

import argparse
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("install_aux_tools", Path(__file__).resolve().parents[1] / "tools/install_aux_tools.py")
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)


class AuxiliaryToolInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)
        self.prefix = self.work / "prefix"

    def args(self, **kwargs):
        return argparse.Namespace(prefix=self.prefix, report_dir=self.work / "reports", jobs=4, **kwargs)

    def recognized_prefix(self):
        (self.prefix / "bin").mkdir(parents=True)
        for name in ("pkgconf", "dot"):
            (self.prefix / "bin" / name).write_text("fixture " + name)
        (self.prefix / "bin/pkg-config").symlink_to("pkgconf")
        data = {"resolution": installer.RESOLUTION, "installed_files": installer.inventory(self.prefix)}
        (self.prefix / installer.MANIFEST).write_text(json.dumps(data))

    def test_unrelated_prefix_preserved_before_network(self):
        self.prefix.mkdir()
        original = self.prefix / "important"
        original.write_text("keep")
        with patch.object(installer.urllib.request, "urlopen") as network:
            with self.assertRaisesRegex(ValueError, "nonempty"):
                installer.install(self.args())
            network.assert_not_called()
        self.assertEqual(original.read_text(), "keep")

    def test_symlinked_prefix_preserved(self):
        target = self.work / "real"
        target.mkdir()
        self.prefix.symlink_to(target, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "ordinary absolute"):
            installer.install(self.args())
        self.assertTrue(self.prefix.is_symlink())
        self.assertTrue(target.is_dir())

    def test_recognized_prefix_requalified_without_network(self):
        self.recognized_prefix()
        with patch.object(installer, "qualify") as qualify, patch.object(installer.urllib.request, "urlopen") as network:
            self.assertEqual(installer.install(self.args()), self.prefix)
            qualify.assert_called_once_with(self.prefix)
            network.assert_not_called()

    def test_host_plugin_override_rejected_before_execution(self):
        self.recognized_prefix()
        before = installer.inventory(self.prefix)
        with patch.dict(installer.os.environ, GVBINDIR="/usr/lib/graphviz"), \
                patch.object(installer.subprocess, "check_output") as command, \
                patch.object(installer.urllib.request, "urlopen") as network:
            with self.assertRaisesRegex(ValueError, "GVBINDIR"):
                installer.install(self.args())
            with self.assertRaisesRegex(ValueError, "GVBINDIR"):
                installer.qualify(self.prefix)
            command.assert_not_called()
            network.assert_not_called()
        self.assertEqual(installer.inventory(self.prefix), before)

    def test_plugin_override_rejected_before_fresh_install(self):
        with patch.dict(installer.os.environ, GVBINDIR="/usr/lib/graphviz"), \
                patch.object(installer.urllib.request, "urlopen") as network:
            with self.assertRaisesRegex(ValueError, "GVBINDIR"):
                installer.install(self.args())
            network.assert_not_called()
        self.assertFalse(self.prefix.exists())

    def test_private_plugin_override_allowed(self):
        with patch.dict(installer.os.environ, GVBINDIR=str(self.prefix / "lib/graphviz")):
            installer.check_plugin_override(self.prefix)

    def test_host_plugin_configuration_cannot_satisfy_layout_qualification(self):
        host = installer.subprocess.CompletedProcess([], 0, "unused SVG",
                   'libdir = "/usr/lib/graphviz"\n/usr/lib/graphviz/config8\nwas successfully loaded.\n')
        with patch.dict(installer.os.environ, GVBINDIR=""), \
                patch.object(installer.subprocess, "check_output", side_effect=["3.0.11", "graphviz version 16.1.1 "]), \
                patch.object(installer.subprocess, "run", return_value=host):
            with self.assertRaisesRegex(ValueError, "private plugin configuration"):
                installer.qualify(self.prefix)

    def test_changed_and_missing_files_rejected(self):
        for action in ("modify", "remove", "extra"):
            with self.subTest(action=action):
                if self.prefix.exists():
                    installer.shutil.rmtree(self.prefix)
                self.recognized_prefix()
                if action == "modify":
                    (self.prefix / "bin/dot").write_text("changed")
                elif action == "remove":
                    (self.prefix / "bin/dot").unlink()
                else:
                    (self.prefix / "extra").write_text("injected")
                with patch.object(installer, "qualify") as qualify:
                    with self.assertRaisesRegex(ValueError, "changed"):
                        installer.verify(self.prefix)
                    qualify.assert_not_called()

    def test_escaping_installed_symlink_rejected(self):
        self.recognized_prefix()
        (self.prefix / "bin/pkg-config").unlink()
        (self.prefix / "bin/pkg-config").symlink_to("/bin/sh")
        with self.assertRaisesRegex(ValueError, "escapes prefix"):
            installer.verify(self.prefix)

    def test_incomplete_and_wrong_resolution_rejected(self):
        self.recognized_prefix()
        manifest = self.prefix / installer.MANIFEST
        for data in ([], {"resolution": {}, "installed_files": {}}, {"resolution": installer.RESOLUTION, "installed_files": {}}):
            manifest.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                installer.verify(self.prefix)

    def test_bad_checksum_never_extracts(self):
        archive = self.work / "source.tar.xz"
        archive.write_bytes(b"not a release")
        with self.assertRaisesRegex(ValueError, "checksum"):
            installer.unpack(archive, self.work / "unpacked", "pkgconf")
        self.assertFalse((self.work / "unpacked").exists())

    def test_traversal_archive_rejected(self):
        archive = self.work / "source.tar.xz"
        with tarfile.open(archive, "w:xz") as contents:
            entry = tarfile.TarInfo("../escaped")
            entry.size = 5
            contents.addfile(entry, io.BytesIO(b"unsafe"))
        with patch.dict(installer.RESOLUTION["pkgconf"], sha256=installer.digest(archive)):
            with self.assertRaisesRegex(ValueError, "escapes"):
                installer.unpack(archive, self.work / "unpacked", "pkgconf")
        self.assertFalse((self.work / "escaped").exists())

    def test_nested_report_path_rejected_before_network(self):
        args = self.args()
        args.report_dir = self.prefix / "reports"
        with patch.object(installer.urllib.request, "urlopen") as network:
            with self.assertRaisesRegex(ValueError, "separate"):
                installer.install(args)
            network.assert_not_called()


if __name__ == "__main__":
    unittest.main()
