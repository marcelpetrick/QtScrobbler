#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 LLVM_PREFIX NEW_CLAZY_PREFIX REPORT_DIR" >&2
    exit 2
fi

llvm_prefix=$(realpath -- "$1")
prefix=$(realpath -m -- "$2")
reports=$(realpath -m -- "$3")
if [[ -e "$prefix" || -L "$prefix" ]]; then
    echo "Clazy installation directory already exists: $prefix" >&2
    exit 1
fi

llvm_version='20.1.8~++20250804090239+87f0227cb601-1~exp1~20250804210352.139'
llvm_repository='https://apt.llvm.org/noble/pool/main/l/llvm-toolchain-20'
clazy_commit=55a7af428ea793c5b88962eafb8a59f9f5a78d9a

mkdir -p "$reports"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"
mkdir source
cp -a --reflink=auto "$llvm_prefix/usr" .

curl --fail --silent --show-error --location --retry 3 \
    "https://codeload.github.com/KDE/clazy/tar.gz/$clazy_commit" -o clazy.tar.gz
for package in llvm-20-dev libclang-20-dev libclang-cpp20-dev; do
    curl --fail --silent --show-error --location --retry 3 \
        "$llvm_repository/${package}_${llvm_version}_amd64.deb" -o "$package.deb"
done
sha256sum --strict --check <<'SUMS' | tee "$reports/clazy-archive-checks.txt"
c197065f135af896685869b7ef90022a7ed9911a856f84021d47570f3958e061  clazy.tar.gz
3c5f27bb70dcea4a7bbf3b10d6c9a72ca4c6acf58719cebcf5cd1c17bf7bc0b8  llvm-20-dev.deb
69b1f28a8e452bcc12e9721c6b37ea00b9e824b757124d34a24c5ed87db4e430  libclang-20-dev.deb
478fe7f6f3955590b7084c1687d53b4271070799dfad5870b49694d638dbc479  libclang-cpp20-dev.deb
SUMS

bsdtar -xf clazy.tar.gz -C source --strip-components 1
for package in llvm-20-dev libclang-20-dev libclang-cpp20-dev; do
    member=$(ar t "$package.deb" | grep '^data\.tar')
    ar p "$package.deb" "$member" | bsdtar -xf -
done

export LD_LIBRARY_PATH="$llvm_prefix/usr/lib/x86_64-linux-gnu:$llvm_prefix/usr/lib/llvm-20/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
compiler="$llvm_prefix/usr/bin/clang-20"
cmake -S source -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-O1 \
    -DCMAKE_C_COMPILER="$compiler" -DCMAKE_CXX_COMPILER="$llvm_prefix/usr/bin/clang++-20" \
    -DLLVM_CONFIG_EXECUTABLE="$work/usr/lib/llvm-20/bin/llvm-config" \
    -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
    -DCLAZY_MAN_PAGE=OFF -DCLAZY_BUILD_CLANG_TIDY=OFF 2>&1 | tee "$reports/clazy-configure.log"
cmake --build build --target clazy-standalone --parallel "${QTSCROBBLER_JOBS:-4}" 2>&1 | tee "$reports/clazy-build.log"
cmake --install build 2>&1 | tee "$reports/clazy-install.log"
"$prefix/bin/clazy-standalone" --version | tee "$reports/clazy-version.txt"
grep -q 'LLVM version 20\.1\.8' "$reports/clazy-version.txt"
grep -q 'clazy version 1\.17' "$reports/clazy-version.txt"
