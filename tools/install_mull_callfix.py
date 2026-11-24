#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build the reviewed Mull 0.38.0/libirm call-removal correction in a new prefix.

Uses checksum-pinned LLVM 20.1.8 headers and libirm dfaf966 headers. The official
compiler/plugin/runtime stay unchanged. A compiler wrapper preloads the correction
only for compilation with the selected Mull plugin; applications never preload it.
Run tools/check_mutation_toolchain.py after installation to qualify Qt semantics.
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
import tempfile
import urllib.request
import zipfile

# archive.ubuntu.com rejects the default "Python-urllib" User-Agent with HTTP 403;
# a neutral client name identifies these downloads without personal details.
DOWNLOAD_HEADERS = {"User-Agent": "qtscrobbler-tool-installer"}

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools/native/void_call_fix.cpp"
CFG_TEST = ROOT / "tools/native/callfix_cfg_test.cpp"
MANIFEST = "mull-callfix-manifest.json"
PATCH_ID = "mull-0.38.0-libirm-dfaf966-void-invoke-v1"
LLVM_VERSION = "20.1.8~++20250804090239+87f0227cb601-1~exp1~20250804210352.139"
LLVM_URL = ("https://apt.llvm.org/noble/pool/main/l/llvm-toolchain-20/"
            f"llvm-20-dev_{LLVM_VERSION}_amd64.deb")
LLVM_SHA256 = "3c5f27bb70dcea4a7bbf3b10d6c9a72ca4c6acf58719cebcf5cd1c17bf7bc0b8"
IRM_COMMIT = "dfaf9660867bad1814b0cae8e38f251a106fe858"
IRM_URL = f"https://github.com/mull-project/libirm/archive/{IRM_COMMIT}.zip"
IRM_SHA256 = "172172e3135b80d40ea0e996f3eabce23c1be54d9ab44cfb52fe699417a7e9a6"


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def artifact_resolution() -> list[dict]:
    return [{"url": LLVM_URL, "sha256": LLVM_SHA256}, {"url": IRM_URL, "sha256": IRM_SHA256}]


def toolchain_inputs(prefix: Path) -> dict:
    relative = ("usr/bin/clang++-20", "usr/lib/mull-ir-frontend-20",
                "usr/lib/x86_64-linux-gnu/libLLVM.so.20.1", "env.sh")
    result = {}
    for name in relative:
        path = prefix / name
        if not path.is_file() or not path.resolve().is_relative_to(prefix):
            raise ValueError(f"required official toolchain input missing or outside prefix: {name}")
        result[name] = {"path": str(path), "resolved": str(path.resolve()), "sha256": digest(path)}
    return result


def environment_for(prefix: Path) -> dict:
    environment = os.environ.copy()
    libraries = f"{prefix}/usr/lib/x86_64-linux-gnu:{prefix}/usr/lib/llvm-20/lib"
    environment["LD_LIBRARY_PATH"] = libraries + (":" + environment["LD_LIBRARY_PATH"]
                                                if environment.get("LD_LIBRARY_PATH") else "")
    return environment


def inventory(prefix: Path) -> dict:
    result = {}
    for directory, directories, files in os.walk(prefix, followlinks=False):
        for name in sorted(directories + files):
            path = Path(directory) / name
            relative = str(path.relative_to(prefix))
            if relative == MANIFEST:
                continue
            if path.is_symlink():
                result[relative] = {"symlink": os.readlink(path)}
            elif path.is_dir():
                result[relative] = {"directory": True}
            elif path.is_file():
                result[relative] = {"sha256": digest(path), "mode": path.stat().st_mode & 0o777}
            else:
                raise ValueError(f"unsupported installed artifact: {relative}")
    return result


def build_inputs(toolchain: Path) -> dict:
    return {"patch_id": PATCH_ID, "artifacts": artifact_resolution(),
            "native_source_sha256": digest(SOURCE), "cfg_test_sha256": digest(CFG_TEST),
            "installer_sha256": digest(Path(__file__)),
            "official_toolchain": toolchain_inputs(toolchain)}


def verified_install(prefix: Path, expected: dict) -> dict | None:
    path = prefix / MANIFEST
    if not path.is_file() or path.is_symlink():
        return None
    manifest = json.loads(path.read_text())
    if (not isinstance(manifest, dict) or manifest.get("schema_version") != 1
            or manifest.get("build_inputs") != expected or not manifest.get("installed_files")):
        raise ValueError("unrecognized or changed callfix build inputs; select a new empty prefix")
    if inventory(prefix) != manifest["installed_files"]:
        raise ValueError("callfix installed artifact inventory differs from manifest")
    for relative in ("bin/clang++-20-callfix", "lib/libmull-callfix.so", "native/void_call_fix.cpp", "env.sh"):
        path = prefix / relative
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"callfix required installed artifact missing: {relative}")
    return manifest


def download(url: str, expected: str, destination: Path, local: Path | None) -> None:
    if local is not None:
        shutil.copyfile(local, destination)
    else:
        with urllib.request.urlopen(urllib.request.Request(url, headers=DOWNLOAD_HEADERS), timeout=60) as response, destination.open("wb") as output:
            shutil.copyfileobj(response, output)
    if digest(destination) != expected:
        raise ValueError(f"callfix dependency checksum mismatch: {destination.name}")


def command(arguments: list[str], environment: dict, log: Path) -> str:
    result = subprocess.run(arguments, env=environment, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=120, check=False)
    log.write_text(result.stdout)
    if result.returncode:
        raise ValueError(f"callfix command exited {result.returncode}: {result.stdout.strip()}")
    return result.stdout


def populate(staged: Path, final: Path, toolchain: Path, inputs: dict,
             llvm_archive: Path | None, irm_archive: Path | None) -> dict:
    staged.mkdir()
    downloads = staged / "downloads"
    downloads.mkdir()
    llvm = downloads / "llvm-20-dev.deb"
    irm = downloads / "libirm.zip"
    download(LLVM_URL, LLVM_SHA256, llvm, llvm_archive)
    download(IRM_URL, IRM_SHA256, irm, irm_archive)
    environment = environment_for(toolchain)
    compiler = str(toolchain / "usr/bin/clang++-20")
    version = command([compiler, "--version"], environment, staged / "compiler-version.log")
    if "20.1.8" not in version:
        raise ValueError("callfix requires the qualified Clang 20.1.8 compiler")
    with tempfile.TemporaryDirectory(prefix="qtscrobbler-callfix-headers-") as temporary:
        work = Path(temporary)
        members = subprocess.check_output(["bsdtar", "-tf", str(llvm)], text=True, timeout=120).splitlines()
        member = next((name for name in members if name.startswith("data.tar")), None)
        if member is None:
            raise ValueError("LLVM development package has no data archive")
        payload = subprocess.check_output(["bsdtar", "-xOf", str(llvm), member], timeout=120)
        subprocess.run(["bsdtar", "-xf", "-", "-C", str(work)], input=payload, check=True, timeout=120)
        headers = staged / "include"
        shutil.copytree(work / "usr/lib/llvm-20/include", headers, symlinks=False)
        with zipfile.ZipFile(irm) as archive:
            archive.extractall(work / "irm")
        upstream = work / "irm" / f"libirm-{IRM_COMMIT}"
        shutil.copytree(upstream / "include/irm", headers / "irm")
        license_directory = staged / "licenses"
        license_directory.mkdir()
        shutil.copyfile(upstream / "LICENSE", license_directory / "libirm-Apache-2.0.txt")
        native = staged / "native"
        native.mkdir()
        source = native / SOURCE.name
        shutil.copyfile(SOURCE, source)
        shutil.copyfile(CFG_TEST, native / CFG_TEST.name)
        library_directory = staged / "lib"
        library_directory.mkdir()
        library = library_directory / "libmull-callfix.so"
        args = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-fPIC", "-shared", "-Wl,-soname,libmull-callfix.so",
                "-isystem", str(headers), str(source),
                str(toolchain / "usr/lib/x86_64-linux-gnu/libLLVM.so.20.1"), "-o", str(library)]
        command(args, environment, staged / "build.log")
        if not library.is_file():
            raise ValueError("callfix compiler did not produce the required shared library")
        cfg_test = native / "callfix-cfg-test"
        cfg_arguments = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-isystem", str(headers),
                         str(native / CFG_TEST.name), str(library),
                         str(toolchain / "usr/lib/x86_64-linux-gnu/libLLVM.so.20.1"),
                         "-Wl,-rpath,$ORIGIN/../lib", "-o", str(cfg_test)]
        command(cfg_arguments, environment, staged / "cfg-build.log")
        # Link by SONAME-style library lookup so publication does not retain a staging path.
        cfg_output = command([str(cfg_test)], environment, staged / "cfg-test.log")
        if not cfg_output.startswith("PASS: invoke normal edges, unwind PHIs"):
            raise ValueError("native CFG qualification did not report the required invariants")
    binaries = staged / "bin"
    binaries.mkdir()
    wrapper = binaries / "clang++-20-callfix"
    wrapper.write_text(
        "#!/usr/bin/env python3\nimport os\nimport sys\nfrom pathlib import Path\n"
        f"compiler = {compiler!r}\nplugin = {str(toolchain / 'usr/lib/mull-ir-frontend-20')!r}\n"
        "environment = os.environ.copy()\n"
        "selected = [argument.split('=', 1)[1] for argument in sys.argv[1:] if argument.startswith('-fpass-plugin=')]\n"
        "if selected:\n"
        "    if any(Path(path).resolve() != Path(plugin).resolve() for path in selected):\n"
        "        raise SystemExit('callfix wrapper requires its qualified Mull plugin')\n"
        "    shim = str(Path(__file__).resolve().parent.parent / 'lib/libmull-callfix.so')\n"
        "    environment['LD_PRELOAD'] = shim + (':' + environment['LD_PRELOAD'] if environment.get('LD_PRELOAD') else '')\n"
        "os.execve(compiler, [compiler, *sys.argv[1:]], environment)\n")
    wrapper.chmod(0o755)
    (staged / "env.sh").write_text(
        "# Source the official environment, then select the private call-removal correction.\n"
        f". {shlex.quote(str(toolchain / 'env.sh'))}\n"
        f"export CXX={shlex.quote(str(final / 'bin/clang++-20-callfix'))}\n"
        f"export QTSCROBBLER_MULL_CALLFIX_MANIFEST={shlex.quote(str(final / MANIFEST))}\n"
        f"export QTSCROBBLER_MULL_CALLFIX_SHIM={shlex.quote(str(final / 'lib/libmull-callfix.so'))}\n"
        f"export QTSCROBBLER_MULL_CALLFIX_SOURCE={shlex.quote(str(final / 'native/void_call_fix.cpp'))}\n")
    manifest = {"schema_version": 1, "build_inputs": inputs, "compile_command": args,
                "qualification": "native CFG invariants passed; mandatory Qt semantic preflight remains separate",
                "cfg_compile_command": cfg_arguments,
                "installed_files": inventory(staged)}
    (staged / MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def install(toolchain: Path, prefix: Path, *, llvm_archive: Path | None = None,
            irm_archive: Path | None = None) -> dict:
    toolchain, prefix = toolchain.resolve(), prefix.resolve()
    if (prefix in {Path("/"), Path.home().resolve()} or prefix.is_relative_to(toolchain)
            or toolchain.is_relative_to(prefix)
            or any(prefix.is_relative_to(Path(root)) for root in ("/usr", "/etc", "/bin", "/lib", "/sbin"))):
        raise ValueError("select a separate isolated callfix prefix")
    if prefix.exists() and (not prefix.is_dir() or (any(prefix.iterdir()) and not (prefix / MANIFEST).is_file())):
        raise ValueError("callfix prefix must be empty or contain a recognized install")
    inputs = build_inputs(toolchain)
    if prefix.exists():
        existing = verified_install(prefix, inputs)
        if existing is not None:
            return existing
    if platform.system() != "Linux" or platform.machine() not in {"x86_64", "amd64"}:
        raise ValueError("callfix supports the pinned Linux x86-64 toolchain only")
    if not shutil.which("bsdtar"):
        raise ValueError("bsdtar is required for the pinned LLVM development archive")
    prefix.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".qtscrobbler-callfix-", dir=prefix.parent) as temporary:
        staged = Path(temporary) / "prefix"
        manifest = populate(staged, prefix, toolchain, inputs, llvm_archive, irm_archive)
        if build_inputs(toolchain) != inputs:
            raise ValueError("callfix source or official toolchain changed during build")
        if prefix.exists():
            prefix.rmdir()
        staged.rename(prefix)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain-prefix", type=Path, required=True)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--llvm-dev-archive", type=Path, help="optional local archive; pinned checksum still required")
    parser.add_argument("--libirm-archive", type=Path, help="optional local archive; pinned checksum still required")
    args = parser.parse_args()
    try:
        install(args.toolchain_prefix, args.prefix, llvm_archive=args.llvm_dev_archive, irm_archive=args.libirm_archive)
        print(f"Built call-removal correction: source {args.prefix.resolve() / 'env.sh'}; run the Qt semantic preflight")
        return 0
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print(f"Callfix installation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
