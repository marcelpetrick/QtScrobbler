#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Marcel Petrick
"""Install the reviewed CMake/CTest Linux x86-64 toolchain in an isolated prefix.

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


VERSION = "4.5.0"
ARCHIVE_NAME = f"cmake-{VERSION}-linux-x86_64.tar.gz"
ARCHIVE_SHA256 = "81f0d5b0960fb6adc8197030ffc827fba937640d51e73e39e0e9e40d88c66f6f"
DOWNLOAD_URL = f"https://github.com/Kitware/CMake/releases/download/v{VERSION}/{ARCHIVE_NAME}"
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
    if platform.system() != "Linux" or platform.machine() not in ("x86_64", "AMD64"):
        raise RuntimeError("This reviewed archive supports Linux x86-64 only")
    if prefix.exists():
        stamp = prefix / STAMP_NAME
        if not stamp.is_file() or stamp.read_text().strip() != ARCHIVE_SHA256:
            raise FileExistsError(f"Refusing to replace an unrecognized prefix: {prefix}")
        verify_installation(prefix)
        print(f"CMake/CTest {VERSION} already installed: {prefix / 'bin'}")
        return

    prefix.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".qtscrobbler-cmake-", dir=prefix.parent) as temporary:
        work = Path(temporary)
        archive = work / ARCHIVE_NAME
        digest = hashlib.sha256()
        with urllib.request.urlopen(DOWNLOAD_URL, timeout=60) as response, archive.open("wb") as output:
            while chunk := response.read(1024 * 1024):
                digest.update(chunk)
                output.write(chunk)
        if digest.hexdigest() != ARCHIVE_SHA256:
            raise RuntimeError("CMake archive checksum mismatch; nothing installed")
        with tarfile.open(archive, "r:gz") as contents:
            contents.extractall(work, filter="data")
        extracted = work / f"cmake-{VERSION}-linux-x86_64"
        verify_installation(extracted)
        (extracted / STAMP_NAME).write_text(ARCHIVE_SHA256 + "\n")
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
