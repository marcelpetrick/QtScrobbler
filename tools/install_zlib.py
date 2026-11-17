#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build checksum-pinned zlib 1.4 as a tested static PIC library in a private prefix.

Example: python3 tools/install_zlib.py --prefix /tmp/qtscrobbler-zlib-1.4 \
    --cmake /path/to/qualified/bin/cmake
Then pass -DZLIB_ROOT=/tmp/qtscrobbler-zlib-1.4 to the application configuration.
No OS packages or global locations are changed. Existing installs are reused only
when their manifest and every installed artifact still match.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import xml.etree.ElementTree as ET

VERSION = "1.4"
URL = "https://zlib.net/zlib-1.4.tar.gz"
SHA256 = "02dd159783677a749c04adbdd02b55b8a2bcc7e054f51b76b549199a916ad584"
MANIFEST = "zlib-manifest.json"


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def unpack(archive: Path, destination: Path) -> Path:
    if digest(archive) != SHA256:
        raise ValueError("zlib source SHA-256 mismatch")
    with tarfile.open(archive, "r:gz") as source:
        # The expected source release uses regular files/directories only.
        for member in source.getmembers():
            target = destination / member.name
            if not target.resolve().is_relative_to(destination.resolve()) or not (member.isfile() or member.isdir()):
                raise ValueError(f"unsafe zlib archive member: {member.name}")
        source.extractall(destination, filter="data")
    root = destination / f"zlib-{VERSION}"
    if not (root / "CMakeLists.txt").is_file():
        raise ValueError("zlib archive does not contain the expected source root")
    return root


def verified_install(prefix: Path) -> bool:
    manifest = prefix / MANIFEST
    if not manifest.exists():
        return False
    data = json.loads(manifest.read_text())
    if not isinstance(data, dict):
        raise ValueError("existing zlib manifest is malformed")
    if data.get("version") != VERSION or data.get("source_sha256") != SHA256:
        raise ValueError("existing zlib manifest has a different resolution")
    files = data.get("installed_files", {})
    if not isinstance(files, dict) or not {"include/zlib.h", "include/zconf.h", "lib/libz.a"} <= files.keys():
        raise ValueError("existing zlib manifest is incomplete")
    actual = {str(path.relative_to(prefix)) for path in prefix.rglob("*")
              if path.is_file() and path != manifest}
    if actual != set(files):
        raise ValueError("installed zlib artifact inventory differs from the manifest")
    for relative, expected in files.items():
        path = prefix / relative
        if (not path.resolve().is_relative_to(prefix.resolve()) or path.is_symlink()
                or not path.is_file() or digest(path) != expected):
            raise ValueError(f"installed zlib artifact is missing or changed: {relative}")
    return True


def run(command: list[str], log: Path) -> None:
    print("Running:", " ".join(command), flush=True)
    with log.open("w") as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=600, check=False)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}); inspect {log}")


def install(args: argparse.Namespace) -> Path:
    prefix = args.prefix.resolve()
    if (prefix in (Path("/"), Path.home().resolve(), Path(__file__).resolve().parents[1])
            or any(prefix.is_relative_to(root) for root in map(Path, ("/usr", "/etc", "/bin", "/lib", "/sbin")))):
        raise ValueError("select a dedicated private zlib prefix")
    if args.jobs < 1:
        raise ValueError("jobs must be positive")
    if verified_install(prefix):
        print(f"Verified existing zlib {VERSION}: {prefix}")
        return prefix
    if prefix.exists() and any(prefix.iterdir()):
        raise ValueError("zlib prefix must be empty or contain a verified install")
    cmake = Path(shutil.which(str(args.cmake)) or args.cmake).resolve()
    ctest = cmake.with_name("ctest")
    if not cmake.is_file() or not ctest.is_file():
        raise ValueError("select qualified CMake and its sibling CTest")
    reports = (args.report_dir or prefix.with_name(prefix.name + "-reports")).resolve()
    if reports == prefix or reports.is_relative_to(prefix) or prefix.is_relative_to(reports):
        raise ValueError("report directory must be separate from the install prefix")
    if reports.exists() and any(reports.iterdir()):
        raise ValueError("report directory must be new or empty")
    reports.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="qtscrobbler-zlib-build-") as temporary:
        work = Path(temporary)
        archive = args.archive
        if archive is None:
            archive = work / "zlib.tar.gz"
            with urllib.request.urlopen(URL, timeout=60) as response, archive.open("wb") as output:
                shutil.copyfileobj(response, output)
        source = unpack(archive, work)
        build = work / "build"
        run([str(cmake), "-S", str(source), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
             f"-DCMAKE_INSTALL_PREFIX={prefix}", "-DCMAKE_INSTALL_LIBDIR=lib",
             "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", "-DZLIB_BUILD_SHARED=OFF",
             "-DZLIB_BUILD_STATIC=ON", "-DZLIB_BUILD_TESTING=ON"], reports / "configure.log")
        run([str(cmake), "--build", str(build), "--parallel", str(args.jobs)], reports / "build.log")
        junit = reports / "junit.xml"
        run([str(ctest), "--test-dir", str(build), "--output-on-failure", "--no-tests=error",
             "--output-junit", str(junit), "--timeout", "30"], reports / "tests.log")
        suite = ET.parse(junit).getroot()
        count = int(suite.attrib.get("tests", "0"))
        if count == 0 or any(int(suite.attrib.get(key, "0")) for key in ("failures", "errors", "skipped")):
            raise ValueError("zlib upstream test report is empty or failed")
        run([str(cmake), "--install", str(build)], reports / "install.log")
        installed = {str(path.relative_to(prefix)): digest(path) for path in prefix.rglob("*") if path.is_file()}
        cache = (build / "CMakeCache.txt").read_text()
        (reports / "CMakeCache.txt").write_text(cache)
        tools = {tool.name: subprocess.check_output([str(tool), "--version"], text=True).splitlines()[0]
                 for tool in (cmake, ctest)}
        compiler_match = re.search(r"^CMAKE_C_COMPILER:FILEPATH=(.+)$", cache, re.MULTILINE)
        if not compiler_match:
            raise ValueError("zlib build did not record a C compiler")
        compiler_path = compiler_match.group(1)
        compiler = {"path": compiler_path,
                    "version": subprocess.check_output([compiler_path, "--version"], text=True).splitlines()[0]}
        manifest = {"version": VERSION, "source_url": URL, "source_sha256": SHA256,
                    "static": True, "position_independent_code": True,
                    "upstream_tests_passed": count, "tools": tools, "compiler": compiler,
                    "qualification_reports": str(reports), "installed_files": installed}
        (prefix / MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n")
        verified_install(prefix)
    print(f"Qualified zlib {VERSION}: {prefix}; {count} upstream tests passed")
    print(f"Application CMake option: -DZLIB_ROOT={prefix}")
    return prefix


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--report-dir", type=Path)
    parser.add_argument("--archive", type=Path, help="local checksum-verified source archive; otherwise download")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--jobs", type=int, default=4)
    try:
        install(parser.parse_args())
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, tarfile.TarError, ET.ParseError) as error:
        print(f"zlib installation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
