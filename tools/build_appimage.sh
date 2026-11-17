#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Build a self-contained x86_64 AppImage of the QtScrobbler GUI (with the CLI inside).
#
# Usage: tools/build_appimage.sh BUILD_DIR OUTPUT_DIR VERSION
#
# Requires: QMAKE (Qt 6.12 qmake used for the build), LINUXDEPLOY (path to the pinned
# linuxdeploy AppImage) and linuxdeploy-plugin-qt-x86_64.AppImage next to it or on PATH.
# The release workflow downloads both tools and verifies their SHA-256 first.
set -euo pipefail

if [[ "$#" -ne 3 ]]; then
    echo "usage: $0 BUILD_DIR OUTPUT_DIR VERSION" >&2
    exit 2
fi
build_dir=$(realpath "$1")
output_dir=$(realpath -m "$2")
version=$3
root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
: "${QMAKE:?set QMAKE to the Qt 6.12 qmake used for the build}"
: "${LINUXDEPLOY:?set LINUXDEPLOY to the pinned linuxdeploy AppImage}"

for binary in qtscrob scrobbler; do
    [[ -x "${build_dir}/${binary}" ]] || { echo "missing executable: ${build_dir}/${binary}" >&2; exit 1; }
done

work=$(mktemp -d)
trap 'rm -rf "${work}"' EXIT
appdir="${work}/AppDir"
mkdir -p "${output_dir}"

# The source file has CRLF line endings; the desktop specification wants LF and an
# icon name without an extension.
tr -d '\r' < "${root_dir}/src/qt/qtscrob.desktop" | sed -e 's/^Icon=.*/Icon=qtscrob/' > "${work}/qtscrob.desktop"
cp "${root_dir}/src/qt/resources/icons/256x256/qtscrob.png" "${work}/qtscrob.png"

# GitHub runners have no FUSE; run the tool AppImages from their extracted payload.
export APPIMAGE_EXTRACT_AND_RUN=1
# linuxdeploy's bundled strip cannot read newer RELR sections; our binaries are stripped already.
export NO_STRIP=1
export LDAI_OUTPUT="${output_dir}/QtScrobbler-${version}-x86_64.AppImage"
tool_dir=$(dirname "$(realpath "${LINUXDEPLOY}")")
export PATH="${tool_dir}:${PATH}"
qt_prefix=$("${QMAKE}" -query QT_INSTALL_PREFIX)
qt_plugins=$("${QMAKE}" -query QT_INSTALL_PLUGINS)
export LD_LIBRARY_PATH="${qt_prefix}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

# linuxdeploy-plugin-qt bundles every SQL driver Qt ships (PostgreSQL, MySQL, ODBC, ...),
# which need client libraries the app never uses. QtScrobbler only uses QSQLITE, so
# present a plugin tree whose sqldrivers holds SQLite only, through a qmake wrapper.
plugins="${work}/plugins"
mkdir -p "${plugins}/sqldrivers"
for category in "${qt_plugins}"/*; do
    [[ "$(basename "${category}")" == sqldrivers ]] || ln -s "${category}" "${plugins}/"
done
ln -s "${qt_plugins}/sqldrivers/libqsqlite.so" "${plugins}/sqldrivers/"
cat > "${work}/qmake" <<WRAPPER
#!/usr/bin/env bash
"${QMAKE}" "\$@" | sed -e 's#^\(QT_INSTALL_PLUGINS[^:]*\):.*#\1:${plugins}#'
WRAPPER
chmod +x "${work}/qmake"
export QMAKE="${work}/qmake"

"${LINUXDEPLOY}" --appdir "${appdir}" \
    --executable "${build_dir}/qtscrob" --executable "${build_dir}/scrobbler" \
    --desktop-file "${work}/qtscrob.desktop" --icon-file "${work}/qtscrob.png" \
    --plugin qt --output appimage

[[ -s "${LDAI_OUTPUT}" ]] || { echo "AppImage was not created: ${LDAI_OUTPUT}" >&2; exit 1; }
echo "${LDAI_OUTPUT}"
