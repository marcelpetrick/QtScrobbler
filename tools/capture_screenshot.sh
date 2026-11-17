#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Capture the actual Qt window after the fictional fixture is imported.
# Usage: tools/capture_screenshot.sh /path/to/build [output.png]
# Requires the built test_gui executable. No live account, service or desktop is used.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 BUILD_DIR [OUTPUT.png]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
screenshot_output="${2:-${repo_root}/documents/images/qtscrobbler.png}"
mkdir -p "$(dirname "${screenshot_output}")"
screenshot_output="$(realpath "${screenshot_output}")"
QT_QPA_PLATFORM=offscreen QTSCROBBLER_SCREENSHOT_PATH="${screenshot_output}" \
    "$1/test_gui" importEditAndClose
echo "Captured real Qt UI: ${screenshot_output}"
