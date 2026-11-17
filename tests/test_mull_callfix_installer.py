"""Offline preservation, integrity and compile-only preload contracts for the Mull correction."""
import importlib.util
import io
from pathlib import Path
import os
import runpy
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock
import zipfile

SPEC = importlib.util.spec_from_file_location(
    "install_mull_callfix", Path(__file__).resolve().parents[1] / "tools/install_mull_callfix.py")
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)


class MullCallfixInstallerTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.toolchain = self.root / "official"
        self.prefix = self.root / "callfix"
        compiler = self.toolchain / "usr/bin/clang++-20"
        compiler.parent.mkdir(parents=True)
        cfg_program = "#!/usr/bin/env python3\nprint('PASS: invoke normal edges, unwind PHIs fixture')\n"
        compiler.write_text(f"#!{sys.executable}\n" +
                            "from pathlib import Path\nimport sys\n"
                            "if '--version' in sys.argv: print('fixture clang version 20.1.8'); raise SystemExit(0)\n"
                            "output = Path(sys.argv[sys.argv.index('-o')+1])\n"
                            "if output.name == 'callfix-cfg-test':\n"
                            f" output.write_text({cfg_program!r}); output.chmod(0o755)\n"
                            "else: output.write_bytes(b'fixture ABI library')\n")
        compiler.chmod(0o755)
        for relative in ("usr/lib/mull-ir-frontend-20", "usr/lib/x86_64-linux-gnu/libLLVM.so.20.1", "env.sh"):
            path = self.toolchain / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("unchanged qualified fixture input: " + relative)
        self.assertIsNotNone(shutil.which("bsdtar"), "installer contract requires bsdtar")
        self.assertIsNotNone(shutil.which("ar"), "installer contract requires ar")
        data = self.root / "data.tar.gz"
        with tarfile.open(data, "w:gz") as archive:
            content = b"fixture LLVM header"
            header = tarfile.TarInfo("usr/include/llvm-20/llvm/probe.h")
            header.size = len(content)
            archive.addfile(header, io.BytesIO(content))
            link = tarfile.TarInfo("usr/lib/llvm-20/include/llvm")
            link.type = tarfile.SYMTYPE
            link.linkname = "../../../include/llvm-20/llvm"
            archive.addfile(link)
        self.llvm = self.root / "llvm.deb"
        subprocess.run(["ar", "rc", str(self.llvm), str(data)], check=True, capture_output=True)
        self.irm = self.root / "irm.zip"
        with zipfile.ZipFile(self.irm, "w") as archive:
            archive.writestr(f"libirm-{installer.IRM_COMMIT}/include/irm/probe.h", "fixture IR mutation header")
            archive.writestr(f"libirm-{installer.IRM_COMMIT}/LICENSE", "Apache-2.0 fixture license")
        self.patches = [mock.patch.object(installer, "LLVM_SHA256", installer.digest(self.llvm)),
                        mock.patch.object(installer, "IRM_SHA256", installer.digest(self.irm))]
        for patch in self.patches:
            patch.start()
            self.addCleanup(patch.stop)

    def install(self):
        return installer.install(self.toolchain, self.prefix, llvm_archive=self.llvm, irm_archive=self.irm)

    def test_unrelated_prefix_is_preserved_before_dependency_inspection(self):
        self.prefix.mkdir()
        marker = self.prefix / "keep"
        marker.write_text("user data")
        with mock.patch.object(installer, "build_inputs") as inspect:
            with self.assertRaisesRegex(ValueError, "prefix must be empty"):
                self.install()
            inspect.assert_not_called()
        self.assertEqual(marker.read_text(), "user data")

    def test_toolchain_and_callfix_prefix_must_be_disjoint(self):
        for prefix in (self.toolchain, self.toolchain / "inside", self.root):
            with self.subTest(prefix=prefix), self.assertRaisesRegex(ValueError, "separate isolated"):
                installer.install(self.toolchain, prefix)

    def test_installation_records_inputs_and_reuses_without_rebuilding(self):
        before = installer.inventory(self.toolchain)
        manifest = self.install()
        self.assertEqual(installer.inventory(self.toolchain), before)
        self.assertEqual(manifest["build_inputs"]["patch_id"], installer.PATCH_ID)
        self.assertIn("-Werror", manifest["compile_command"])
        self.assertEqual(manifest["build_inputs"]["official_toolchain"]["usr/bin/clang++-20"]["sha256"],
                         installer.digest(self.toolchain / "usr/bin/clang++-20"))
        self.assertEqual((self.prefix / "native/void_call_fix.cpp").read_bytes(), installer.SOURCE.read_bytes())
        self.assertIn(str(self.prefix / "bin/clang++-20-callfix"), (self.prefix / "env.sh").read_text())
        with mock.patch.object(installer, "populate") as populate, \
                mock.patch.object(installer.urllib.request, "urlopen") as network:
            self.assertEqual(self.install(), manifest)
            populate.assert_not_called()
            network.assert_not_called()

    def test_changed_or_added_installed_artifact_is_preserved_and_rejected(self):
        self.install()
        (self.prefix / "lib/libmull-callfix.so").write_bytes(b"changed library")
        before = installer.inventory(self.prefix)
        with self.assertRaisesRegex(ValueError, "inventory differs"):
            self.install()
        self.assertEqual(installer.inventory(self.prefix), before)

    def test_new_untracked_wrapper_cannot_enter_recognized_prefix(self):
        self.install()
        (self.prefix / "bin/unqualified").write_text("unexpected tool")
        with self.assertRaisesRegex(ValueError, "inventory differs"):
            self.install()
        self.assertEqual((self.prefix / "bin/unqualified").read_text(), "unexpected tool")

    def test_changed_official_input_requires_new_prefix(self):
        self.install()
        (self.toolchain / "usr/lib/mull-ir-frontend-20").write_bytes(b"different plugin")
        with self.assertRaisesRegex(ValueError, "changed callfix build inputs"):
            self.install()

    def test_corrupt_archive_does_not_publish_partial_installation(self):
        self.llvm.write_bytes(b"corrupt dependency")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            self.install()
        self.assertFalse(self.prefix.exists())
        self.assertEqual(list(self.root.glob(".qtscrobbler-callfix-*")), [])

    def test_failed_compiler_does_not_publish_partial_installation(self):
        compiler = self.toolchain / "usr/bin/clang++-20"
        compiler.write_text(f"#!{sys.executable}\nraise SystemExit(7)\n")
        compiler.chmod(0o755)
        with self.assertRaisesRegex(ValueError, "exited 7"):
            self.install()
        self.assertFalse(self.prefix.exists())

    def test_incorrect_native_cfg_output_does_not_publish_installation(self):
        compiler = self.toolchain / "usr/bin/clang++-20"
        compiler.write_text(compiler.read_text().replace("PASS: invoke normal edges", "WRONG: invoke normal edges"))
        with self.assertRaisesRegex(ValueError, "CFG qualification"):
            self.install()
        self.assertFalse(self.prefix.exists())

    def test_wrapper_preloads_only_for_the_qualified_plugin(self):
        self.install()
        wrapper = self.prefix / "bin/clang++-20-callfix"
        plugin = str(self.toolchain / "usr/lib/mull-ir-frontend-20")
        for arguments, preload in [(["-c", "file.cpp"], False),
                                   ([f"-fpass-plugin={plugin}", "-c", "file.cpp"], True),
                                   (["--version"], False)]:
            with self.subTest(arguments=arguments), \
                    mock.patch.object(sys, "argv", [str(wrapper), *arguments]), \
                    mock.patch.dict(os.environ, {"LD_PRELOAD": "user-preload.so"}), \
                    mock.patch.object(os, "execve") as execute:
                runpy.run_path(str(wrapper), run_name="__main__")
                compiler, passed, environment = execute.call_args.args
                self.assertEqual(compiler, str(self.toolchain / "usr/bin/clang++-20"))
                self.assertEqual(passed[1:], arguments)
                expected = str(self.prefix / "lib/libmull-callfix.so") + ":user-preload.so" if preload else "user-preload.so"
                self.assertEqual(environment["LD_PRELOAD"], expected)
        with mock.patch.object(sys, "argv", [str(wrapper), "-fpass-plugin=/other/plugin"]), \
                mock.patch.object(os, "execve") as execute:
            with self.assertRaisesRegex(SystemExit, "qualified Mull plugin"):
                runpy.run_path(str(wrapper), run_name="__main__")
            execute.assert_not_called()


if __name__ == "__main__":
    unittest.main()
