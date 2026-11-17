#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build pinned pkgconf and Graphviz in a private Linux prefix; never change the OS.

Prepend PREFIX/bin to PATH after successful qualification. Graphviz is limited to
native layout/SVG output used by Doxygen; optional GUI and language bindings are
not installed. Existing installations are reused only after inventory verification.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
import xml.etree.ElementTree as ET

RESOLUTION = {
    "pkgconf": {
        "version": "3.0.11",
        "url": "https://distfiles.ariadne.space/pkgconf/pkgconf-3.0.11.tar.xz",
        "sha256": "d0cd841bc8e55b9da888106c00dc256d05b7d1d8e5a46f4be4eb4c5125bd2dc4",
    },
    "graphviz": {
        "version": "16.1.1",
        "url": "https://gitlab.com/api/v4/projects/4207231/packages/generic/graphviz-releases/16.1.1/graphviz-16.1.1.tar.xz",
        "sha256": "52e7fb9a57b52b5c67f41dba6504ba3231e118258779421cb8f4f99104de71f6",
    },
}
MANIFEST = "aux-tools-manifest.json"


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inventory(prefix: Path) -> dict[str, str]:
    result = {}
    for path in prefix.rglob("*"):
        if path == prefix / MANIFEST:
            continue
        if path.is_symlink():
            if not path.resolve().is_relative_to(prefix.resolve()):
                raise ValueError(f"installed symlink escapes prefix: {path}")
            result[str(path.relative_to(prefix))] = "link:" + os.readlink(path)
        elif path.is_file():
            result[str(path.relative_to(prefix))] = digest(path)
    return result


def verify(prefix: Path) -> bool:
    manifest = prefix / MANIFEST
    if not manifest.exists():
        return False
    data = json.loads(manifest.read_text())
    if not isinstance(data, dict) or data.get("resolution") != RESOLUTION:
        raise ValueError("auxiliary tools resolution differs")
    files = data.get("installed_files")
    if not isinstance(files, dict) or not {"bin/pkgconf", "bin/pkg-config", "bin/dot"} <= files.keys():
        raise ValueError("auxiliary tools inventory incomplete")
    if inventory(prefix) != files:
        raise ValueError("auxiliary tools installed files changed")
    qualify(prefix)
    return True


def unpack(archive: Path, target: Path, package: str) -> Path:
    pin = RESOLUTION[package]
    if digest(archive) != pin["sha256"]:
        raise ValueError(f"{package} source checksum mismatch")
    with tarfile.open(archive) as contents:
        for entry in contents.getmembers():
            if not (entry.isfile() or entry.isdir() or entry.issym() or entry.islnk()):
                raise ValueError("unsupported source archive entry")
            if not (target / entry.name).resolve().is_relative_to(target.resolve()):
                raise ValueError("source archive escapes destination")
        contents.extractall(target, filter="data")
    return target / f"{package}-{pin['version']}"


def check_plugin_override(prefix: Path) -> None:
    override = os.environ.get("GVBINDIR")
    if override and Path(override).resolve() != (prefix / "lib/graphviz").resolve():
        raise ValueError("GVBINDIR must select the private Graphviz plugin directory")


def qualify(prefix: Path) -> None:
    check_plugin_override(prefix)
    def output(args: list[str], **kwargs) -> str:
        return subprocess.check_output(args, stderr=subprocess.STDOUT, text=True, timeout=30, **kwargs)
    if output([str(prefix / "bin/pkgconf"), "--version"]).strip() != RESOLUTION["pkgconf"]["version"]:
        raise ValueError("pkgconf runtime version mismatch")
    if f"graphviz version {RESOLUTION['graphviz']['version']} " not in output([str(prefix / "bin/dot"), "-V"]):
        raise ValueError("Graphviz runtime version mismatch")
    # Exercise installed plugins/layout and XML output, beyond executable presence.
    rendered = subprocess.run([str(prefix / "bin/dot"), "-v", "-Tsvg"],
                              input="digraph G { source -> destination; }\n",
                              capture_output=True, text=True, timeout=30, check=True)
    plugin_dir = prefix / "lib/graphviz"
    if (f'libdir = "{plugin_dir}"' not in rendered.stderr
            or f"{plugin_dir}/config8" not in rendered.stderr
            or "was successfully loaded." not in rendered.stderr):
        raise ValueError("Graphviz did not load the private plugin configuration")
    svg = rendered.stdout
    root = ET.fromstring(svg)
    text = " ".join(root.itertext())
    groups = root.findall(".//{http://www.w3.org/2000/svg}g")
    nodes = [group for group in groups if group.get("class") == "node"]
    edges = [group for group in groups if group.get("class") == "edge"]
    if (root.tag != "{http://www.w3.org/2000/svg}svg" or "source" not in text or "destination" not in text
            or len(nodes) != 2 or len(edges) != 1):
        raise ValueError("Graphviz SVG layout contract failed")
    with tempfile.TemporaryDirectory(prefix="qtscrobbler-pkgconf-check-") as work:
        Path(work, "qualification.pc").write_text("Name: qualification\nDescription: isolated check\nVersion: 1.2.3\nLibs: -lqualification\n")
        env = dict(os.environ, PKG_CONFIG_LIBDIR=work, PKG_CONFIG_PATH="")
        if output([str(prefix / "bin/pkg-config"), "--modversion", "qualification"], env=env).strip() != "1.2.3":
            raise ValueError("pkgconf package resolution contract failed")


def install(args: argparse.Namespace) -> Path:
    prefix = args.prefix.absolute()
    # Do not follow a user-provided symlink to an unrelated installation.
    if prefix.is_symlink() or prefix != prefix.resolve():
        raise ValueError("select an ordinary absolute private prefix")
    if prefix in (Path("/"), Path.home(), Path(__file__).resolve().parents[1]) or any(
            prefix.is_relative_to(Path(root)) for root in ("/usr", "/etc", "/bin", "/lib", "/sbin")):
        raise ValueError("select a dedicated private prefix")
    if args.jobs < 1 or args.jobs > 4:
        raise ValueError("jobs must be between 1 and 4")
    # dot -c also honors GVBINDIR; reject a conflicting directory before any
    # build/publication command could update an unrelated plugin configuration.
    check_plugin_override(prefix)
    if verify(prefix):
        print(f"Verified auxiliary tools: {prefix}")
        return prefix
    if prefix.exists() and any(prefix.iterdir()):
        raise ValueError("refusing unrelated nonempty prefix")
    reports = args.report_dir.resolve() if args.report_dir else prefix.with_name(prefix.name + "-reports")
    if reports == prefix or reports.is_relative_to(prefix) or prefix.is_relative_to(reports):
        raise ValueError("reports must be separate from prefix")
    if reports.exists() and any(reports.iterdir()):
        raise ValueError("reports must be new or empty")
    reports.mkdir(parents=True, exist_ok=True)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    commands = []
    with tempfile.TemporaryDirectory(prefix=".qtscrobbler-aux-", dir=prefix.parent) as work_name:
        work = Path(work_name)
        staging = work / "staging"
        env = dict(os.environ)
        def run(name: str, command: list[str], cwd: Path) -> None:
            print("Running:", " ".join(command), flush=True)
            with (reports / f"{name}.log").open("w") as log:
                result = subprocess.run(command, cwd=cwd, env=dict(env, PWD=str(cwd)), stdout=log,
                                        stderr=subprocess.STDOUT, timeout=900, check=False)
            commands.append({"command": command, "exit_code": result.returncode})
            if result.returncode:
                raise RuntimeError(f"{name} failed; inspect {reports}")
        for package, pin in RESOLUTION.items():
            archive = work / f"{package}.tar.xz"
            with urllib.request.urlopen(pin["url"], timeout=60) as response, archive.open("wb") as out:
                shutil.copyfileobj(response, out)
            source = unpack(archive, work, package)
            options = [f"--prefix={prefix}", f"--libdir={prefix}/lib"]
            if package == "pkgconf":
                # Keep the host pkg-config search directories for distribution .pc files.
                original = shutil.which("pkg-config")
                if not original:
                    raise ValueError("host pkg-config required to retain native search paths")
                search = subprocess.check_output([original, "--variable=pc_path", "pkg-config"], text=True).strip()
                if not search:
                    raise ValueError("host pkg-config search path is empty")
                options += [f"--with-pkg-config-dir={search}", "--with-system-libdir=/lib:/usr/lib",
                            "--with-system-includedir=/usr/include"]
            else:
                options += ["--disable-swig", "--disable-sharp", "--disable-tcl", "--disable-man-pdfs",
                            "--without-x", "--without-qt", "--without-smyrna",
                            "--without-gdk", "--without-gtk", "--without-gtkgl", "--without-gtkglext",
                            "--without-gdk-pixbuf", "--without-webp", "--without-poppler",
                            "--without-rsvg", "--without-ghostscript", "--without-pangocairo",
                            "--without-lasi", "--without-freetype2", "--without-libgd",
                            "--without-gts", "--without-ann", "--without-aalib"]
            run(package + "-configure", [str(source / "configure"), *options], source)
            run(package + "-build", ["make", f"-j{args.jobs}"], source)
            if package == "pkgconf":
                run(package + "-check", ["make", "check"], source)
            run(package + "-install", ["make", "install", f"DESTDIR={staging}"], source)
        staged = staging / prefix.relative_to("/")
        if not (staged / "bin/pkg-config").exists():
            (staged / "bin/pkg-config").symlink_to("pkgconf")
        # Plugins embed the final prefix, so publish then qualify; failure removes
        # only this new installation, never an existing recognized/unrelated tree.
        if prefix.exists():
            prefix.rmdir()
        staged.rename(prefix)
        try:
            subprocess.run([str(prefix / "bin/dot"), "-c"], check=True, timeout=30,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            qualify(prefix)
            data = {"resolution": RESOLUTION, "commands": commands, "installed_files": inventory(prefix)}
            (prefix / MANIFEST).write_text(json.dumps(data, indent=2) + "\n")
            verify(prefix)
        except BaseException:
            shutil.rmtree(prefix)
            raise
    print(f"Qualified pkgconf 3.0.11 and Graphviz 16.1.1: {prefix}")
    return prefix


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--report-dir", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    try:
        install(parser.parse_args())
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, tarfile.TarError) as error:
        print(f"Auxiliary tool installation failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
