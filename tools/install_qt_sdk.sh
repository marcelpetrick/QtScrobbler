#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 NEW_QT_INSTALL_DIR REPORT_DIR" >&2
    exit 2
fi

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
install_dir=$1
report_dir=$2
if [[ -e "$install_dir" || -L "$install_dir" ]]; then
    echo "Qt installation directory already exists: $install_dir" >&2
    exit 1
fi

mkdir -p "$report_dir"
archive_dir=$(mktemp -d)
trap 'rm -rf "$archive_dir"' EXIT

# The locked aqt verifies upstream SHA-256 before extracting each archive.
python3 -c 'import py7zr'
python3 -m aqt install-qt linux desktop 6.12.0 linux_gcc_64 \
    --base https://download.qt.io --timeout 30 --outputdir "$install_dir" \
    --keep --archive-dest "$archive_dir" \
    --archives qtbase qtdeclarative qttools qttranslations icu 2>&1 | tee "$report_dir/qt-install.log"

# Verify the resulting downloads against the exact bytes qualified for this project.
(
    cd "$archive_dir"
    sha256sum --strict --check "$script_dir/qt-sdk-linux-x86_64.sha256"
) | tee "$report_dir/qt-archive-checks.txt"

qt_prefix="$install_dir/6.12.0/gcc_64"
test "$("$qt_prefix/bin/qmake" -query QT_VERSION)" = "6.12.0"
{
    "$qt_prefix/bin/qmake" -query
    "$qt_prefix/bin/lrelease" -version
    for binary in "$qt_prefix/bin/lrelease" "$qt_prefix/plugins/platforms/libqxcb.so" \
        "$qt_prefix/plugins/sqldrivers/libqsqlite.so" "$qt_prefix/plugins/tls/libqopensslbackend.so"; do
        printf '\n%s\n' "$binary"
        ldd "$binary"
    done
} | tee "$report_dir/qt-runtime.txt"
if grep -q '=> not found' "$report_dir/qt-runtime.txt"; then
    echo "The Qt SDK has unresolved runtime libraries." >&2
    exit 1
fi
