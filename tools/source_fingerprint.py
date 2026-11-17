#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""One fingerprint scope for every report that must describe the same source tree.

Coverage and mutation evidence are cross-checked by comparing these maps, so both
runners import this function instead of keeping their own file lists.
"""
from __future__ import annotations

import hashlib
from pathlib import Path

FOLDERS = ("src", "tests", "cmake", "tools")
TOP_LEVEL_FILES = ("CMakeLists.txt", "localPipeline.sh", "mull.yml", "Doxyfile",
                   "requirements-quality.txt", "requirements-bootstrap.txt", ".clang-format")


def source_hashes(root: Path) -> dict[str, str]:
    """Fingerprint source, resources, test contracts, tooling and build definitions."""
    paths = {path for folder in FOLDERS for path in (root / folder).rglob("*")
             if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc"}
    paths.update(path for name in TOP_LEVEL_FILES if (path := root / name).is_file())
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(paths)}
