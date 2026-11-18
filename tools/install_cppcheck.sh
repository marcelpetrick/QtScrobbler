#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 NEW_CPPCHECK_PREFIX REPORT_DIR" >&2
    exit 2
fi

prefix=$(realpath -m -- "$1")
reports=$(realpath -m -- "$2")
if [[ -e "$prefix" || -L "$prefix" ]]; then
    echo "Cppcheck installation directory already exists: $prefix" >&2
    exit 1
fi

mkdir -p "$reports"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"
mkdir source
curl --fail --silent --show-error --location --retry 3 \
    https://codeload.github.com/cppcheck-opensource/cppcheck/tar.gz/a436ca35ed1887bee789765122b65ed2d7a7e045 \
    -o cppcheck.tar.gz
sha256sum --strict --check <<'SUMS' | tee "$reports/cppcheck-archive-checks.txt"
68ed9efb7aad635b7f4c121662689b2377d1d745dc9e76227566516a9617f343  cppcheck.tar.gz
SUMS

bsdtar -xf cppcheck.tar.gz -C source --strip-components 1
cmake -S source -B build -G Ninja -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-O1 -DBUILD_TESTING=OFF \
    -DUSE_BUNDLED_TINYXML2=ON 2>&1 | tee "$reports/cppcheck-configure.log"
cmake --build build --parallel "${QTSCROBBLER_JOBS:-4}" 2>&1 | tee "$reports/cppcheck-build.log"
cmake --install build 2>&1 | tee "$reports/cppcheck-install.log"
"$prefix/bin/cppcheck" --version | tee "$reports/cppcheck-version.txt"
grep -Fxq 'Cppcheck 2.22.0' "$reports/cppcheck-version.txt"
