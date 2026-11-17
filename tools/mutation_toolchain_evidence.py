#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Read-only validation tying a native mutation campaign to its semantic tool proof.

Freshness means the exact proved binaries, compiler invocation, support files and
Qt probe artifacts remain unchanged. No arbitrary expiry replaces those checks.
"""
from __future__ import annotations

from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil

from check_mutation_toolchain import check_compile_inventory, check_report


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def invocation(path: Path | str) -> Path:
    # Resolving the final symlink loses clang++'s driver mode. Keep the exact name.
    return Path(os.path.abspath(path))


def cache_values(build: Path) -> dict[str, str]:
    result = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and ":" in line and "=" in line:
            key, value = line.split("=", 1)
            result[key.split(":", 1)[0]] = value
    return result


def verified_record(record: dict, label: str) -> dict:
    if not isinstance(record, dict) or not record.get("path") or not record.get("sha256"):
        raise ValueError(f"missing {label} path/hash in semantic evidence")
    path = invocation(record["path"])
    if not path.is_file() or digest(path) != record["sha256"]:
        raise ValueError(f"{label} changed since semantic preflight: {path}")
    return {"path": str(path), "sha256": record["sha256"]}


def verify_callfix_manifest(path: Path, compiler: Path, root: Path) -> dict:
    """Verify installed shim closure and the upstream compiler/library it interposes."""
    from install_mull_callfix import MANIFEST, PATCH_ID, artifact_resolution, inventory

    if path.name != MANIFEST or path.is_symlink():
        raise ValueError("compiler wrapper requires the recognized callfix manifest")
    manifest = json.loads(path.read_text())
    if not isinstance(manifest, dict) or not isinstance(manifest.get("build_inputs"), dict):
        raise ValueError("unrecognized callfix dependency manifest")
    inputs = manifest["build_inputs"]
    if (manifest.get("schema_version") != 1 or inputs.get("patch_id") != PATCH_ID
            or inputs.get("artifacts") != artifact_resolution()):
        raise ValueError("unrecognized callfix dependency resolution")
    for key, relative in (("native_source_sha256", "tools/native/void_call_fix.cpp"),
                          ("cfg_test_sha256", "tools/native/callfix_cfg_test.cpp"),
                          ("installer_sha256", "tools/install_mull_callfix.py")):
        if inputs.get(key) != digest(root / relative):
            raise ValueError(f"callfix source/build input changed: {relative}")
    prefix = path.parent
    installed = manifest.get("installed_files")
    if not isinstance(installed, dict) or not installed or inventory(prefix) != installed:
        raise ValueError("callfix installed artifact inventory differs from its semantic proof")
    if invocation(prefix / "bin/clang++-20-callfix") != compiler:
        raise ValueError("build compiler is not the wrapper belonging to the proved callfix manifest")
    for relative in ("bin/clang++-20-callfix", "lib/libmull-callfix.so", "native/void_call_fix.cpp",
                     "native/callfix_cfg_test.cpp", "native/callfix-cfg-test", "env.sh"):
        artifact = prefix / relative
        if artifact.is_symlink() or not artifact.is_file():
            raise ValueError(f"required callfix artifact missing: {relative}")
    official = inputs.get("official_toolchain")
    expected = {"usr/bin/clang++-20", "usr/lib/mull-ir-frontend-20",
                "usr/lib/x86_64-linux-gnu/libLLVM.so.20.1", "env.sh"}
    if not isinstance(official, dict) or set(official) != expected:
        raise ValueError("callfix official toolchain closure is incomplete")
    official_prefix = invocation(official["usr/bin/clang++-20"]["path"]).parents[2]
    for relative, record in official.items():
        verified_record(record, "official toolchain " + relative)
        official_path = invocation(record["path"])
        if (official_path != official_prefix / relative
                or not official_path.resolve().is_relative_to(official_prefix)
                or str(official_path.resolve()) != record.get("resolved")):
            raise ValueError(f"official toolchain path or symlink changed: {relative}")
    inventory_hash = hashlib.sha256(json.dumps(installed, sort_keys=True).encode()).hexdigest()
    return {"path": str(path), "sha256": digest(path), "build_inputs": inputs,
            "installed_inventory_sha256": inventory_hash, "installed_entry_count": len(installed)}


def validate(summary_path: Path, build: Path, runner: Path, production_inventory: dict,
             source_root: Path) -> dict:
    """Return immutable provenance or fail before executing any campaign command."""
    summary_path = summary_path.resolve()
    summary = json.loads(summary_path.read_text())
    if not isinstance(summary, dict) or summary.get("schema_version") != 1 or summary.get("status") != "PASS":
        raise ValueError("mutation campaign requires a passing native semantic preflight")
    loader_keys = {"LD_LIBRARY_PATH", "LD_PRELOAD"}
    loader_environment = summary.get("native_environment")
    if (not isinstance(loader_environment, dict) or set(loader_environment) != loader_keys
            or any(not isinstance(value, str) for value in loader_environment.values())):
        raise ValueError("semantic preflight native loader environment is missing or invalid")
    if loader_environment != {name: os.environ.get(name, "") for name in loader_keys}:
        raise ValueError("native loader environment differs from the semantic preflight")
    try:
        started = datetime.fromisoformat(summary["started_utc"])
        finished = datetime.fromisoformat(summary["finished_utc"])
        if started.tzinfo is None or finished.tzinfo is None or finished < started:
            raise ValueError("invalid semantic preflight timestamps")
    except (KeyError, TypeError) as error:
        raise ValueError("semantic preflight completion timestamps are missing") from error
    cache = cache_values(build)
    if not cache.get("CMAKE_CXX_COMPILER") or not cache.get("QTSCROBBLER_MULL_PLUGIN"):
        raise ValueError("mutation build must select explicit compiler and plugin")
    selected = {"compiler": invocation(cache["CMAKE_CXX_COMPILER"]),
                "plugin": Path(cache["QTSCROBBLER_MULL_PLUGIN"]).resolve(), "runner": invocation(runner)}
    recorded = summary.get("toolchain", {})
    tools = {}
    for name, selected_path in selected.items():
        record = verified_record(recorded.get(name), name)
        proved_path = Path(record["path"])
        if name == "plugin":
            proved_path = proved_path.resolve()
        if proved_path != selected_path:
            raise ValueError(f"selected {name} differs from the semantic preflight")
        tools[name] = record
    if not production_inventory:
        raise ValueError("mutation compiler proof requires the complete production compile inventory")
    for filename, command in production_inventory.items():
        tokens = command.get("arguments") or shlex.split(command["command"])
        executable = shutil.which(tokens[0]) if tokens else None
        if executable is None or invocation(executable) != selected["compiler"]:
            raise ValueError(f"production compile command bypasses the proved compiler: {filename}")
    reports = summary_path.parent
    hashes = summary.get("source_sha256")
    expected_sources = {"CMakeLists.txt", "mull.yml", "probe.h", "probe.cpp", "test_probe.cpp"}
    if not isinstance(hashes, dict) or set(hashes) != expected_sources:
        raise ValueError("semantic preflight fixture fingerprint inventory is incomplete")
    for name, expected in hashes.items():
        path = reports / "source" / name
        if not path.is_file() or digest(path) != expected:
            raise ValueError(f"semantic preflight fixture changed: {name}")
    binary = reports / "build/test_probe"
    if not binary.is_file() or digest(binary) != summary.get("binary_sha256"):
        raise ValueError("semantic preflight probe binary changed")
    compile_commands = reports / "build/compile_commands.json"
    if digest(compile_commands) != summary.get("compile_commands_sha256"):
        raise ValueError("semantic preflight compile inventory changed")
    inventory = check_compile_inventory(compile_commands, reports / "source/probe.cpp", selected["plugin"])
    if inventory != summary.get("compile_inventory"):
        raise ValueError("semantic preflight compile commands differ from recorded proof")
    raw_report = reports / "signal-removal.sqlite"
    proof = check_report(raw_report, reports / "source/probe.cpp")
    if proof != summary.get("semantic_proof"):
        raise ValueError("semantic proof differs from its native assertion-kill report")
    support = summary.get("support_artifacts")
    if not isinstance(support, list):
        raise ValueError("semantic preflight support artifact inventory is missing")
    artifacts = [verified_record(record, "native support artifact") for record in support]
    manifests = [Path(record["path"]) for record in artifacts
                 if Path(record["path"]).name == "mull-callfix-manifest.json"]
    if len(manifests) > 1:
        raise ValueError("semantic proof contains multiple native callfix manifests")
    callfix = verify_callfix_manifest(manifests[0], selected["compiler"], source_root) if manifests else None
    with selected["compiler"].open("rb") as stream:
        wrapper = stream.read(2) == b"#!"
    if wrapper and callfix is None:
        raise ValueError("compiler wrapper semantic proof must include its qualified callfix manifest")
    return {"summary_path": str(summary_path), "summary_sha256": digest(summary_path),
            "semantic_proof": proof, "native_report_sha256": digest(raw_report),
            "tools": tools, "native_environment": loader_environment,
            "support_artifacts": artifacts, "callfix": callfix,
            "source_sha256": hashes, "probe_binary_sha256": summary["binary_sha256"],
            "probe_compile_commands_sha256": summary["compile_commands_sha256"]}
