#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Run native static, documentation or extracted-package checks with retained evidence."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_DOCS = (
    "README.md", "AGENTS.md", "documents/plan.md", "documents/visions.md", "examples/README.md",
    "documents/API.md", "documents/DEPENDENCIES.md", "documents/MUTATION_TESTING.md",
    "documents/QUALITY.md", "documents/ARCHITECTURE.md", "documents/README.md", "documents/REVIEW.md",
)


def source_fingerprint() -> dict[str, str]:
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted((ROOT / "src").rglob("*"))
            if path.is_file() and path.suffix in {".cpp", ".h"}}


def production_commands(build: Path, root: Path = ROOT) -> list[dict]:
    """Select every maintained production translation unit; never analyze Qt generated files."""
    rows = json.loads((build / "compile_commands.json").read_text())
    expected = {path.resolve() for directory in ("src/lib", "src/cli", "src/qt/src")
                for path in (root / directory).glob("*.cpp") if path.name != "parse-mtp-win32.cpp"}
    selected = {}
    for row in rows:
        path = Path(row["file"])
        if not path.is_absolute():
            path = Path(row["directory"]) / path
        if path.resolve() in expected:
            selected[str(path.resolve())] = dict(row, file=str(path.resolve()))
    if not expected or expected != {Path(name) for name in selected}:
        raise ValueError("compile inventory omits maintained production files (MTP must be enabled)")
    return [selected[name] for name in sorted(selected)]


def validate_docs(root: Path = ROOT) -> list[str]:
    checked = []
    names = dict.fromkeys((*REQUIRED_DOCS, *(str(path.relative_to(root))
                            for path in sorted((root / "documents").rglob("*.md")))))
    for name in names:
        path = root / name
        content = path.read_text()
        if not re.search(r"(?m)^# ", content):
            raise ValueError(f"missing document title: {name}")
        if len(re.findall(r"(?m)^```", content)) % 2:
            raise ValueError(f"unclosed Markdown code fence: {name}")
        for target in re.findall(r"\]\(([^)]+)\)", content):
            target = target.split("#", 1)[0].strip("<>")
            if not target or re.match(r"[a-z]+:", target):
                continue
            if not (path.parent / unquote(target)).exists():
                raise ValueError(f"broken local document link in {name}: {target}")
        checked.append(name)
    if not (root / "documents/images/qtscrobbler.png").is_file():
        raise ValueError("missing actual GUI screenshot")
    if "version 2" not in (root / "LICENSE").read_text().lower():
        raise ValueError("GPLv2 license evidence missing")
    return checked


def require_runtime_version(output: str, expected: str, program: str) -> None:
    """Compare the advertised version exactly, including the final patch component."""
    match = re.search(r"(?m)^" + re.escape(program) + r" \(v([^)]+)\)", output)
    if not match or match[1] != expected:
        raise ValueError(f"packaged {program} version differs from source")


class Checks:
    def __init__(self, reports: Path, timeout: int):
        self.reports = reports
        self.timeout = timeout
        self.commands = []
        self.versions = {}

    def command(self, name: str, args: list[str], *, environment=None, input_text=None, timeout=None) -> str:
        log = self.reports / f"{len(self.commands):03d}-{name}.log"
        bound = self.timeout if timeout is None else min(self.timeout, timeout)
        record = {"name": name, "command": args, "log": log.name, "status": "FAIL", "timeout_seconds": bound}
        self.commands.append(record)
        try:
            result = subprocess.run(args, cwd=ROOT, env=environment, input=input_text,
                                    capture_output=True, text=True, timeout=bound, check=False)
            output = result.stdout + result.stderr
            log.write_text(output)
            record["exit_code"] = result.returncode
            if result.returncode:
                raise ValueError(f"{name} exited {result.returncode}; inspect {log}")
            record["status"] = "PASS"
            return output
        except subprocess.TimeoutExpired as error:
            record["timed_out"] = True
            output = error.stdout or b""
            log.write_bytes(output.encode() if isinstance(output, str) else output)
            raise ValueError(f"{name} exceeded {bound}s; inspect {log}") from error
        except OSError as error:
            log.write_text(str(error) + "\n")
            raise

    def tool(self, name: str) -> str:
        path = shutil.which(name)
        if not path:
            raise ValueError(f"mandatory native tool is missing: {name}")
        if name not in self.versions:
            output = self.command(name + "-version", [path, "--version"])
            self.versions[name] = {"path": path, "version": output.strip()}
        return path

    def static(self, build: Path):
        rows = production_commands(build)
        inventory = self.reports / "production-compile-commands.json"
        inventory.write_text(json.dumps(rows, indent=2) + "\n")
        files = [str(Path(row["file"]).resolve()) for row in rows]
        errors = []
        commands = (
            ("clang-tidy", ["-p", str(build), "--checks=-*,clang-analyzer-core.*,clang-analyzer-unix.Malloc",
                            "--warnings-as-errors=*", *files]),
            ("clazy-standalone", ["-p", str(build), "-checks=connect-non-signal,incorrect-emit", *files,
                                  "--extra-arg=-Werror"]),
            ("cppcheck", [f"--project={inventory}", "--library=qt", "--error-exitcode=1", "--quiet",
                          "--suppress=missingIncludeSystem"]),
        )
        for name, args in commands:
            try:
                self.command(name, [self.tool(name), *args])
            except (ValueError, OSError) as error:
                errors.append(str(error))
        if errors:
            raise ValueError("; ".join(errors))

    def docs(self):
        checked = validate_docs()
        (self.reports / "documents.json").write_text(json.dumps(checked, indent=2) + "\n")
        # The repository configuration owns warning policy. Only relocate its output.
        config = (ROOT / "Doxyfile").read_text()
        config += f'\nOUTPUT_DIRECTORY = "{self.reports / "api"}"\nWARN_AS_ERROR = YES\n'
        self.command("doxygen", [self.tool("doxygen"), "-"], input_text=config)
        if not (self.reports / "api/html/index.html").is_file():
            raise ValueError("Doxygen did not generate its required HTML index")

    def package(self, build: Path):
        members = [build / "qtscrob", build / "scrobbler", ROOT / "LICENSE", ROOT / "README.md",
                   ROOT / "COPYING", ROOT / "documents", ROOT / "examples/scrobble.log", ROOT / "examples/README.md"]
        if any(not path.exists() for path in members):
            raise ValueError("required package binaries/documentation/example are missing")
        archive = self.reports / "qtscrobbler-linux.tar.gz"
        with tarfile.open(archive, "w:gz") as stream:
            for member in members:
                name = f"examples/{member.name}" if member.parent == ROOT / "examples" else member.name
                stream.add(member, arcname=name)
        with tempfile.TemporaryDirectory(prefix="qtscrobbler-package-") as temporary:
            destination = Path(temporary)
            with tarfile.open(archive) as stream:
                stream.extractall(destination, filter="data")
            environment = os.environ.copy()
            home = destination / "home"
            home.mkdir(mode=0o700)
            environment.update(HOME=str(home), XDG_CONFIG_HOME=str(home), XDG_CACHE_HOME=str(home),
                               XDG_RUNTIME_DIR=str(home), QT_QPA_PLATFORM="offscreen")
            for name in ("qtscrob", "scrobbler"):
                output = self.command(name + "-ldd", [self.tool("ldd"), str(destination / name)])
                if "not found" in output:
                    raise ValueError(f"extracted {name} has unresolved runtime dependencies")
            version = re.search(r'#define CLIENT_VERSION "([^"]+)"', (ROOT / "src/lib/common.h").read_text())[1]
            output = self.command("packaged-cli", [str(destination / "scrobbler"), "--help"],
                                  environment=environment, timeout=5)
            require_runtime_version(output, version, "scrobbler")
            output = self.command("packaged-gui-help", [str(destination / "qtscrob"), "--help"],
                                  environment=environment, timeout=5)
            require_runtime_version(output, version, "QTScrob")
            log = self.reports / "packaged-gui.log"
            gui_record = {"name": "packaged-gui", "command": [str(destination / "qtscrob")],
                          "status": "FAIL", "log": log.name, "startup_observation_seconds": 2}
            self.commands.append(gui_record)
            with log.open("w") as output_stream:
                process = subprocess.Popen([str(destination / "qtscrob")], cwd=destination,
                                           env=environment, stdout=output_stream, stderr=subprocess.STDOUT)
                try:
                    result = process.wait(timeout=2)
                    raise ValueError(f"packaged GUI exited before startup observation completed: {result}")
                except subprocess.TimeoutExpired:
                    process.terminate()
                    try:
                        gui_record["exit_code"] = process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
                        raise ValueError("packaged GUI did not terminate after startup observation")
            # Qt's offscreen plugin cannot negotiate native widget size hints.
            unexpected = [line for line in log.read_text().splitlines()
                          if line.strip() and line != "This plugin does not support propagateSizeHints()"]
            if unexpected:
                raise ValueError("unexpected packaged GUI diagnostics; inspect packaged-gui.log")
            gui_record["status"] = "PASS"
            gui_record["termination"] = "terminated intentionally after surviving startup observation"
        (self.reports / "package.json").write_text(json.dumps({
            "archive": archive.name, "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
            "version": version, "gui_startup_seconds": 2,
            "runtime": "uses selected installed Qt SDK and host libraries; not a bundled standalone distribution",
            "workflow_limit": "startup and CLI help only; real import/edit/submission covered by Qt Test suite",
        }, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=("static", "docs", "package"), required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--report-dir", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()
    reports = args.report_dir.resolve()
    if args.timeout <= 0 or (reports.exists() and any(reports.iterdir())):
        parser.error("timeout must be positive and report directory must be new or empty")
    reports.mkdir(parents=True, exist_ok=True)
    checks = Checks(reports, args.timeout)
    sources = source_fingerprint()
    manifest = {"stage": args.stage, "status": "FAIL", "started_utc": datetime.now(timezone.utc).isoformat(),
                "commands": checks.commands, "tools": checks.versions,
                "host": os.uname().sysname + " " + os.uname().machine,
                "source_sha256": sources}
    try:
        if args.stage == "docs":
            checks.docs()
        else:
            getattr(checks, args.stage)(args.build_dir.resolve())
        if source_fingerprint() != sources:
            raise ValueError("production source changed during quality stage; run again on a stable checkout")
        manifest["status"] = "PASS"
    except (ValueError, OSError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        manifest["error"] = str(error)
        print(f"{args.stage}: FAIL: {error}", flush=True)
    finally:
        manifest["finished_utc"] = datetime.now(timezone.utc).isoformat()
        (reports / "summary.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"{args.stage}: {manifest['status']}", flush=True)
    return 0 if manifest["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
