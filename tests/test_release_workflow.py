# SPDX-License-Identifier: GPL-2.0-only
"""Published checksum files must verify with `sha256sum -c` next to the download.

Release v0.13.85 shipped an AppImage checksum naming the absolute runner path.
These tests execute the workflow's own checksum commands with absolute paths
and require verification both before and after publishing.
"""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

WORKFLOW = Path(__file__).resolve().parents[1] / ".github/workflows/release.yml"


def step_script(text: str, name: str) -> str:
    """Return the dedented `run: |` block of the step with the given name."""
    match = re.search(rf"^      - name: {re.escape(name)}\n(.*?)(?=^      - name: |\Z)", text, re.M | re.S)
    if match is None:
        raise AssertionError(f"missing workflow step: {name}")
    run = re.search(r"^        run: \|\n((?:^          .*\n|^\s*\n)+)", match[1], re.M)
    if run is None:
        raise AssertionError(f"step without a run block: {name}")
    return "".join(line[10:] if line.strip() else "\n" for line in run[1].splitlines(keepends=True))


def checksum_line(script: str, target: str) -> str:
    lines = [line for line in script.splitlines() if line.rstrip().endswith(f'> "{target}"')]
    if len(lines) != 1:
        raise AssertionError(f"expected one checksum command writing {target}, found {lines}")
    return lines[0]


class ReleaseWorkflowChecksumTest(unittest.TestCase):
    def setUp(self):
        self.text = WORKFLOW.read_text()
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.dist = self.root / "dist"
        self.dist.mkdir()

    def tearDown(self):
        self.tmp.cleanup()

    def run_bash(self, command: str, cwd: Path, **env: str) -> None:
        subprocess.run(["bash", "-euo", "pipefail", "-c", command], cwd=cwd, check=True,
                       env={**os.environ, **env}, timeout=20)

    def verify_like_a_user(self, asset: Path) -> None:
        download = self.root / "download"
        download.mkdir(exist_ok=True)
        for path in (asset, asset.with_name(asset.name + ".sha256")):
            (download / path.name).write_bytes(path.read_bytes())
        result = subprocess.run(["sha256sum", "-c", "--", asset.name + ".sha256"], cwd=download,
                                capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, f"{asset.name}: OK\n")

    def test_appimage_checksum_names_the_bare_file_for_an_absolute_path(self):
        command = checksum_line(step_script(self.text, "Build the AppImage"), "$APPIMAGE.sha256")
        appimage = self.dist / "QtScrobbler-9.9.9-x86_64.AppImage"
        appimage.write_bytes(b"appimage bytes")
        elsewhere = self.root / "workspace"
        elsewhere.mkdir()
        self.run_bash(command, elsewhere, APPIMAGE=str(appimage))
        expected = f"{hashlib.sha256(b'appimage bytes').hexdigest()}  {appimage.name}\n"
        self.assertEqual(Path(f"{appimage}.sha256").read_text(), expected)
        self.verify_like_a_user(appimage)

    def test_archive_checksum_names_the_bare_file(self):
        script = step_script(self.text, "Strip and package release binaries")
        command = checksum_line(script, "dist/$ARCHIVE.tar.gz.sha256")
        archive = self.dist / "qtscrobbler-9.9.9-linux-x86_64.tar.gz"
        archive.write_bytes(b"archive bytes")
        sha = hashlib.sha256(b"archive bytes").hexdigest()
        self.run_bash(command, self.root, ARCHIVE=archive.name.removesuffix(".tar.gz"), SHA=sha)
        self.assertEqual(Path(f"{archive}.sha256").read_text(), f"{sha}  {archive.name}\n")
        self.verify_like_a_user(archive)

    def test_windows_zip_checksum_names_the_bare_file(self):
        script = (WORKFLOW.parents[2] / "tools/build_windows_package.sh").read_text()
        line = next(line for line in script.splitlines() if '> "$name.zip.sha256"' in line)
        command = line[line.index('sha256sum "$name.zip"'):].rstrip(")")
        package = self.dist / "qtscrobbler-9.9.9-windows-x86_64.zip"
        package.write_bytes(b"zip bytes")
        self.assertTrue(line.startswith('(cd "$out" && '))
        self.run_bash(command, self.dist, name=package.name.removesuffix(".zip"))
        expected = f"{hashlib.sha256(b'zip bytes').hexdigest()}  {package.name}\n"
        self.assertEqual(Path(f"{package}.sha256").read_text(), expected)
        self.verify_like_a_user(package)

    def test_checksums_are_verified_before_and_after_publishing(self):
        steps = re.findall(r"^      - name: (.+)$", self.text, re.M)
        publish = steps.index("Create GitHub Release")
        before = step_script(self.text, "Check extracted release entry points")
        self.assertIn("(cd dist && sha256sum -c -- ./*.sha256)", before)
        self.assertLess(steps.index("Check extracted release entry points"), publish)
        after = step_script(self.text, "Verify the published release assets")
        self.assertGreater(steps.index("Verify the published release assets"), publish)
        self.assertIn('gh release download "$TAG"', after)
        self.assertIn("-eq 3", after)
        self.assertIn("-windows-x86_64.zip", after)
        self.assertIn('sha256sum -c -- ./*.sha256', after)


if __name__ == "__main__":
    unittest.main()
