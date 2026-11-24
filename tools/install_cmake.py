#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Marcel Petrick
"""Install the reviewed CMake/CTest Linux or Windows x86-64 toolchain in an isolated prefix.

Usage: python3 tools/install_cmake.py --prefix /path/to/cmake
Then prepend /path/to/cmake/bin to PATH. Downloads are checksum-verified against
our reviewed upstream release digest. Existing unrelated prefixes are never replaced.
Requires Python 3.12+; does not modify OS packages or invoke a shell.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import platform
import subprocess
import tarfile
import tempfile
import urllib.request
import zipfile

# archive.ubuntu.com rejects the default "Python-urllib" User-Agent with HTTP 403;
# a neutral client name identifies these downloads without personal details.
DOWNLOAD_HEADERS = {"User-Agent": "qtscrobbler-tool-installer"}


VERSION = "4.5.1"
# Digests from upstream's cmake-4.5.1-SHA-256.txt.
ARCHIVES = {
    "Linux": (f"cmake-{VERSION}-linux-x86_64", ".tar.gz",
              "83b71181fb62b4fe8f26067a9321448572e7a43bd04ff6c7f54a24c33698f16d"),
    "Windows": (f"cmake-{VERSION}-windows-x86_64", ".zip",
                "bfe6eadb245dc8213e0d3feea316b520cb2aae92f5667ffd76059ba6b05c430c"),
}
ARCHIVE_STEM, ARCHIVE_SUFFIX, ARCHIVE_SHA256 = ARCHIVES["Linux"]
ARCHIVE_NAME = ARCHIVE_STEM + ARCHIVE_SUFFIX
RELEASE_URL = f"https://github.com/Kitware/CMake/releases/download/v{VERSION}/"
DOWNLOAD_URL = RELEASE_URL + ARCHIVE_NAME
STAMP_NAME = ".qtscrobbler-archive.sha256"


def verify_installation(prefix: Path) -> None:
    """Check that both executable names resolve to the selected release."""
    for tool in ("cmake", "ctest"):
        result = subprocess.run(
            [str(prefix / "bin" / tool), "--version"],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            timeout=15,
        )
        if result.stdout.splitlines()[0] != f"{tool} version {VERSION}":
            raise RuntimeError(f"Unexpected {tool} version in {prefix}")


def install(prefix: Path) -> None:
    """Download, verify, extract and qualify CMake without replacing user data."""
    system = platform.system()
    if system not in ARCHIVES or platform.machine() not in ("x86_64", "AMD64"):
        raise RuntimeError("The reviewed archives support Linux and Windows x86-64 only")
    stem, suffix, archive_sha256 = ARCHIVES[system]
    if prefix.exists():
        stamp = prefix / STAMP_NAME
        if not stamp.is_file() or stamp.read_text().strip() != archive_sha256:
            raise FileExistsError(f"Refusing to replace an unrecognized prefix: {prefix}")
        verify_installation(prefix)
        print(f"CMake/CTest {VERSION} already installed: {prefix / 'bin'}")
        return

    prefix.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".qtscrobbler-cmake-", dir=prefix.parent) as temporary:
        work = Path(temporary)
        archive = work / (stem + suffix)
        digest = hashlib.sha256()
        with urllib.request.urlopen(urllib.request.Request(RELEASE_URL + stem + suffix, headers=DOWNLOAD_HEADERS), timeout=60) as response, archive.open("wb") as output:
            while chunk := response.read(1024 * 1024):
                digest.update(chunk)
                output.write(chunk)
        if digest.hexdigest() != archive_sha256:
            raise RuntimeError("CMake archive checksum mismatch; nothing installed")
        if suffix == ".zip":
            with zipfile.ZipFile(archive) as contents:
                contents.extractall(work)
        else:
            with tarfile.open(archive, "r:gz") as contents:
                contents.extractall(work, filter="data")
        extracted = work / stem
        verify_installation(extracted)
        (extracted / STAMP_NAME).write_text(archive_sha256 + "\n")
        extracted.rename(prefix)
    print(f"Installed checksum-verified CMake/CTest {VERSION}: {prefix / 'bin'}")


def main() -> None:
    """Parse an explicit install destination and provision the pinned toolchain."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True, help="new isolated installation directory")
    arguments = parser.parse_args()
    install(arguments.prefix.expanduser().resolve())


if __name__ == "__main__":
    main()
