#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Marcel Petrick
"""Install checksum-pinned LLVM 20.1.8/Mull 0.38.0 in an isolated Linux x86-64 prefix.

Usage: python3 tools/install_mutation_toolchain.py --prefix /path/to/native-tools
Then source /path/to/native-tools/env.sh before a Clang coverage or mutation build.

Requires Python 3.12+, bsdtar and a compatible Linux host (glibc >= 2.38). No root,
OS package installation, shell download scripts or container is used. All downloads
are verified against fixed SHA256 digests before extraction. LLVM artifact hashes
were obtained from apt.llvm.org's noble package index; supporting runtime hashes from
Ubuntu noble's main/updates/security package indexes; Mull hashes from GitHub release asset digests.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import urllib.request


LLVM_VERSION = "20.1.8~++20250804090239+87f0227cb601-1~exp1~20250804210352.139"
LLVM_BASE = "https://apt.llvm.org/noble/pool/main/l/llvm-toolchain-20/"
MULL_BASE = "https://github.com/mull-project/mull/releases/download/0.38.0/"
LLVM_PACKAGES = {
    "clang-20": "b3478e87705e68850d5cceeeb85ad64fff78ed0cb05a9fe7f0648ccf88c5b1f3",
    "libclang-common-20-dev": "7a945a2c284f50328fcacb34b3f507264d7bbba9fda22354cb1de61959f5265b",
    "libclang-cpp20": "4c3ea6fe4b459ba8c154a9a0f841500b4ddda341bfd55e32548eb1275c0f36b1",
    "libclang-rt-20-dev": "061ff449899acd20ac800aef25126b66a53d9808ecc529392dbb47a3046f79ef",
    "libclang1-20": "7f79dde122860815eacbadb66130dfb42a8d42a637e906d1d4ba7dc0e564e780",
    "libllvm20": "d9be126777d635a040d1b3627dd14b04dc10573c46fd8d00b3a7632e5de0d0fc",
    "llvm-20-linker-tools": "c11f46cbd732b3c4e0aa9df8c964f7b96b93b7b3a9e8a4dd939182d0282fc5f9",
    "llvm-20": "48911311c972fa02d2d20978d0f7b8360f1681fbd0308560bdc06632583e60e5",
}
ARTIFACTS = [
    (LLVM_BASE + f"{name}_{LLVM_VERSION}_amd64.deb", digest)
    for name, digest in LLVM_PACKAGES.items()
] + [
    ("https://archive.ubuntu.com/ubuntu/pool/main/libe/libedit/"
     "libedit2_3.1-20230828-1build1_amd64.deb",
     "3afbc8ddf835049dd4d88275b9d13680e4c4cdd59e191cbd63424152cc08e00e"),
    ("https://archive.ubuntu.com/ubuntu/pool/main/libx/libxml2/"
     "libxml2_2.9.14+dfsg-1.3ubuntu3.10_amd64.deb",
     "c5a065f85c1e6f605741e365e40cad71c3ceb0e85e2791400bd35b8896e12ad1"),
    ("https://archive.ubuntu.com/ubuntu/pool/main/i/icu/libicu74_74.2-1ubuntu3.1_amd64.deb",
     "c9a70989678660eed9a1e904c74fa043da8bec8e2036856fc16e31ced79b04f8"),
    (MULL_BASE + "Mull-0.38.0-ubuntu-amd64-24.04.deb",
     "83ebc3c49f173b83dc0d991fff4b3ac8d6a2a7ecf51244d598507f85b04d5a29"),
    (MULL_BASE + "Mull-20-0.38.0-LLVM-20.1.2-ubuntu-amd64-24.04.deb",
     "73360881642837c553e2e28a66de792a6189d3609dd7e39b9e6216e6048997d5"),
]


def install(prefix: Path) -> dict:
    if platform.system() != "Linux" or platform.machine() not in {"x86_64", "amd64"}:
        raise ValueError("this pinned artifact set supports Linux x86-64 only")
    if not shutil.which("bsdtar"):
        raise ValueError("bsdtar is required to extract Debian package archives")
    if prefix in {Path("/"), Path("/usr"), Path("/usr/local")}:
        raise ValueError("choose an isolated user-owned tool prefix")
    prefix.mkdir(parents=True, exist_ok=True)
    downloads = prefix / "downloads"
    downloads.mkdir(exist_ok=True)
    for url, digest in ARTIFACTS:
        archive = downloads / url.rsplit("/", 1)[1]
        if not archive.exists():
            partial = archive.with_suffix(".part")
            with urllib.request.urlopen(url, timeout=60) as response, partial.open("wb") as output:
                shutil.copyfileobj(response, output)
            partial.rename(archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != digest:
            raise ValueError(f"checksum mismatch; remove and retry: {archive}")
        members = subprocess.check_output(["bsdtar", "-tf", str(archive)], text=True).splitlines()
        member = next((name for name in members if name.startswith("data.tar")), None)
        if not member:
            raise ValueError(f"Debian archive has no data member: {archive}")
        data = subprocess.check_output(["bsdtar", "-xOf", str(archive), member])
        subprocess.run(["bsdtar", "-xf", "-", "-C", str(prefix)], input=data, check=True)
        print(f"Verified and extracted {archive.name}", flush=True)

    libpath = f"{prefix}/usr/lib/x86_64-linux-gnu:{prefix}/usr/lib/llvm-20/lib"
    environment = os.environ.copy()
    environment["LD_LIBRARY_PATH"] = libpath + ":" + environment.get("LD_LIBRARY_PATH", "")
    checks = {}
    for executable, required in [
        ("usr/bin/clang-20", "20.1.8"), ("usr/bin/mull-runner", "0.38.0"),
        ("usr/bin/llvm-cov-20", "20.1.8"), ("usr/bin/llvm-profdata-20", "20.1.8"),
    ]:
        output = subprocess.check_output([str(prefix / executable), "--version"],
                                         env=environment, text=True, stderr=subprocess.STDOUT)
        if required not in output:
            raise ValueError(f"unexpected executable version: {executable}: {output}")
        checks[executable] = output.strip()
    prefix_quote = shlex.quote(str(prefix))
    (prefix / "env.sh").write_text(
        "# Generated native LLVM/Mull environment; source this file.\n"
        f"export QTSCROBBLER_LLVM_PREFIX={prefix_quote}\n"
        f"export QTSCROBBLER_MULL_PLUGIN={shlex.quote(str(prefix / 'usr/lib/mull-ir-frontend-20'))}\n"
        f"export CC={shlex.quote(str(prefix / 'usr/bin/clang-20'))}\n"
        f"export CXX={shlex.quote(str(prefix / 'usr/bin/clang++-20'))}\n"
        f"export PATH={shlex.quote(str(prefix / 'usr/bin'))}:\"$PATH\"\n"
        f"export LD_LIBRARY_PATH={shlex.quote(libpath)}\"${{LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}}\"\n"
    )
    manifest = {"schema_version": 1, "artifacts": [
        {"url": url, "sha256": digest} for url, digest in ARTIFACTS
    ], "versions": checks}
    (prefix / "toolchain-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        install(arguments.prefix.resolve())
        print(f"Ready: source {arguments.prefix.resolve() / 'env.sh'}")
        return 0
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"Native toolchain installation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
