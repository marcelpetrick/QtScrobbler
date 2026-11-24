#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Build, test, deploy and smoke-test the Windows x86-64 release ZIP (Git Bash on Windows).
#
# Usage: tools/build_windows_package.sh QT_PREFIX MINGW_BIN ZLIB_PREFIX VERSION OUT_DIR
#   QT_PREFIX   Qt 6.12.1 MinGW kit, e.g. <dest>/6.12.1/mingw_64 (tools/install_qt_sdk_windows.py)
#   MINGW_BIN   matching MinGW 15.1 bin directory, e.g. <dest>/Tools/mingw1510_64/bin
#   ZLIB_PREFIX zlib 1.4 built by tools/install_zlib.py with that compiler
# Prints the ZIP path on its last line. MTP support is Linux-only and disabled here.
set -euo pipefail

if [[ $# -ne 5 ]]; then
    echo "Usage: $0 QT_PREFIX MINGW_BIN ZLIB_PREFIX VERSION OUT_DIR" >&2
    exit 2
fi
# Accept native Windows paths (e.g. D:\a\_temp): CMake gets C:/x form, PATH gets /c/x form.
qt=$(cygpath -m "$1") mingw=$(cygpath -m "$2") zlib=$(cygpath -m "$3") version=$4 out=$(cygpath -m "$5")
build=${QTSCROBBLER_WINDOWS_BUILD_DIR:-build-windows}
jobs=${QTSCROBBLER_JOBS:-4}
name="qtscrobbler-${version}-windows-x86_64"
# Suites qualified on Windows gate the package. The others assume Linux behavior
# (INI-file settings instead of the registry, POSIX permissions, process output) and are measured
# separately by the workflow until they are ported; see documents/GITHUB_ACTIONS.md.
gated='^(common|parse_log|parse_ipod|gettrackinfo|submit)$'

mingw_path=$(cygpath -u "$mingw")
qt_path=$(cygpath -u "$qt")
export PATH="$mingw_path:$qt_path/bin:$PATH"
mkdir -p "$out"
cmake -S . -B "$build" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH="$qt" \
    -DQTSCROBBLER_MTP=OFF -DZLIB_ROOT="$zlib" -DZLIB_LIBRARY="$zlib/lib/libzs.a" >&2
cmake --build "$build" --parallel "$jobs" >&2
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure --no-tests=error \
    --timeout 120 -R "$gated" >&2

package="$out/$name"
rm -rf "$package" "$out/$name.zip" "$out/$name.zip.sha256"
mkdir -p "$package/examples"
cp "$build/qtscrob.exe" "$build/scrobbler.exe" "$package/"
strip --strip-unneeded "$package/qtscrob.exe" "$package/scrobbler.exe"
# The application's own catalogs are compiled into its resources.
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw \
    --compiler-runtime "$package/qtscrob.exe" "$package/scrobbler.exe" >&2
cp README.md LICENSE COPYING "$package/"
cp examples/scrobble.log examples/README.md "$package/examples/"
cp -R documents "$package/"
(cd "$out" && 7z a -tzip -bso0 -bsp0 "$name.zip" "$name" && sha256sum "$name.zip" > "$name.zip.sha256")

# Smoke-test the extracted ZIP as a user would: no Qt or MinGW on PATH.
check=$(mktemp -d)
trap 'rm -rf "$check"' EXIT
(cd "$out" && sha256sum -c "$name.zip.sha256") >&2
7z x -bso0 -bsp0 "-o$check" "$out/$name.zip"
clean_path="/usr/bin:$(cygpath -u "${SYSTEMROOT:-C:\\Windows}")/System32"
cli=$(PATH="$clean_path" timeout 30 "$check/$name/scrobbler.exe" --help)
# The GUI creates its QApplication first, so this also loads the shipped qwindows plugin.
gui=$(PATH="$clean_path" timeout 30 "$check/$name/qtscrob.exe" --help)
grep -q "scrobbler (v$version)" <<<"$cli" || { echo "CLI banner missing: $cli" >&2; exit 1; }
grep -q "QTScrob (v$version)" <<<"$gui" || { echo "GUI banner missing: $gui" >&2; exit 1; }
test -s "$check/$name/LICENSE"
test -s "$check/$name/examples/scrobble.log"
test -f "$check/$name/platforms/qwindows.dll"
test -f "$check/$name/sqldrivers/qsqlite.dll"
echo "Packaged and smoke-tested $out/$name.zip" >&2
echo "$out/$name.zip"
