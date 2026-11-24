#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Install the checksum-pinned Qt 6.12.1 MinGW SDK and its MinGW 15.1 toolchain.

Usage: python3 tools/install_qt_sdk_windows.py --dest NEW_DIR [--archive-dir DIR]
Afterwards NEW_DIR/6.12.1/mingw_64 is the Qt prefix and NEW_DIR/Tools/mingw1510_64/bin
holds the matching compiler. Every archive is verified against
tools/qt-sdk-windows-x86_64.sha256 before extraction; the hashes match the official
Qt repository sidecars. aqtinstall 3.3.0 cannot resolve Qt's architecture-suffixed
Windows repository folders, so the fixed official archive URLs are used directly.
The 7z executable extracts the archives. Runs on Windows and, for local cross
builds, on Linux.
"""
from __future__ import annotations

import argparse
import hashlib
import http.client
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.request

# archive.ubuntu.com rejects the default "Python-urllib" User-Agent with HTTP 403;
# a neutral client name identifies these downloads without personal details.
DOWNLOAD_HEADERS = {"User-Agent": "qtscrobbler-tool-installer"}

QT_VERSION = "6.12.1"
QT_BUILD = "6.12.1-0-202611180427"
MINGW_BUILD = "15.1.0-202610271023"
REPOSITORY = "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop"
KIT_URL = f"{REPOSITORY}/qt6_6121/qt6_6121_mingw/qt.qt6.6121.win64_mingw/{QT_BUILD}"
MINGW_URL = f"{REPOSITORY}/tools_mingw1510/qt.tools.win64_mingw1510/{MINGW_BUILD}"
KIT_SUFFIX = "-Windows-Windows_11_24H2-Mingw-Windows-Windows_11_24H2-X86_64.7z"
RUNTIME = "x86_64-15.1.0-release-posix-seh-msvcrt-rt_v12-rev0-runtime.7z"
QT_SUBDIR = Path(QT_VERSION) / "mingw_64"
MINGW_SUBDIR = Path("Tools") / "mingw1510_64"
# (archive name, download URL, destination relative to --dest)
ARCHIVES = tuple(
    [(f"{module}{KIT_SUFFIX}", f"{KIT_URL}{module}{KIT_SUFFIX}", QT_SUBDIR)
     for module in ("qtbase", "qttools", "qttranslations")]
    + [(RUNTIME, f"{KIT_URL}{RUNTIME}", QT_SUBDIR / "bin"),
       ("mingw1510.7z", f"{MINGW_URL}mingw1510.7z", MINGW_SUBDIR)])
MANIFEST = Path(__file__).resolve().with_name("qt-sdk-windows-x86_64.sha256")
REQUIRED = (QT_SUBDIR / "bin/Qt6Core.dll", QT_SUBDIR / "bin/windeployqt.exe",
            QT_SUBDIR / "bin/lrelease.exe", QT_SUBDIR / "bin/libstdc++-6.dll",
            QT_SUBDIR / "lib/cmake/Qt6/Qt6Config.cmake", QT_SUBDIR / "lib/cmake/Qt6Test/Qt6TestConfig.cmake",
            QT_SUBDIR / "plugins/platforms/qwindows.dll", QT_SUBDIR / "plugins/sqldrivers/qsqlite.dll",
            MINGW_SUBDIR / "bin/g++.exe", MINGW_SUBDIR / "bin/mingw32-make.exe")


def pinned_hashes(manifest: Path = MANIFEST) -> dict[str, str]:
    hashes = {}
    for line in manifest.read_text().splitlines():
        digest_text, name = line.split(maxsplit=1)
        hashes[name.strip()] = digest_text
    expected = {name for name, _, _ in ARCHIVES}
    if set(hashes) != expected:
        raise ValueError(f"Windows SDK manifest must pin exactly {sorted(expected)}")
    return hashes


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def fetch(url: str, target: Path) -> None:
    print(f"Downloading {url}", flush=True)
    with urllib.request.urlopen(urllib.request.Request(url, headers=DOWNLOAD_HEADERS), timeout=120) as response, target.open("wb") as output:
        shutil.copyfileobj(response, output)


def verified_archive(name: str, url: str, expected: str, cache: Path) -> Path:
    path = cache / name
    if not path.is_file():
        fetch(url, path)
    actual = digest(path)
    if actual != expected:
        path.unlink()
        raise ValueError(f"{name}: SHA-256 mismatch: expected {expected}, got {actual}")
    print(f"Verified {name} {actual}", flush=True)
    return path


def extract(archive: Path, destination: Path, seven_zip: str) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([seven_zip, "x", "-y", f"-o{destination}", str(archive)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, check=False)
    if result.returncode:
        raise RuntimeError(f"extracting {archive.name} failed: {result.stderr.strip()}")


def install(dest: Path, archive_dir: Path | None, seven_zip: str) -> Path:
    if dest.exists() or dest.is_symlink():
        raise ValueError(f"Qt installation directory already exists: {dest}")
    hashes = pinned_hashes()
    tool = shutil.which(seven_zip)
    if tool is None:
        raise ValueError(f"7z executable not found: {seven_zip}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    # Stage beside the destination: a same-volume rename publishes it atomically, while a
    # cross-drive move (C: temp to D: on runners) fails deleting 7z's read-only files.
    with tempfile.TemporaryDirectory(prefix=".qtscrobbler-qt-win-", dir=dest.parent,
                                     ignore_cleanup_errors=True) as temporary:
        cache = archive_dir or Path(temporary)
        cache.mkdir(parents=True, exist_ok=True)
        staging = Path(temporary) / "staging"
        for name, url, relative in ARCHIVES:
            extract(verified_archive(name, url, hashes[name], cache), staging / relative, tool)
        missing = [str(path) for path in REQUIRED if not (staging / path).is_file()]
        if missing:
            raise ValueError(f"extracted Windows SDK is incomplete: {missing}")
        # The archives carry the build machine's prefix; make the kit relocatable.
        (staging / QT_SUBDIR / "bin/qt.conf").write_text("[Paths]\nPrefix=..\n")
        staging.rename(dest)
    print(f"Qt {QT_VERSION} MinGW prefix: {dest / QT_SUBDIR}")
    print(f"MinGW toolchain: {dest / MINGW_SUBDIR / 'bin'}")
    return dest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dest", type=Path, required=True, help="new installation directory")
    parser.add_argument("--archive-dir", type=Path, help="keep or reuse verified archives here")
    parser.add_argument("--7z", dest="seven_zip", default="7z", help="7z executable")
    args = parser.parse_args(argv)
    try:
        install(args.dest.resolve(), args.archive_dir, args.seven_zip)
        return 0
    except (OSError, ValueError, RuntimeError, http.client.HTTPException) as error:
        print(f"Windows Qt SDK installation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
