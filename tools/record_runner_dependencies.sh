#!/usr/bin/env bash
set -euo pipefail

report_dir=$1
mkdir -p "$report_dir"
{
    printf 'Runner image: %s %s\n' "${ImageOS:-}" "${ImageVersion:-}"
    cat /etc/os-release
    uname -a
    dpkg-query -W -f='${binary:Package}\t${Version}\n' | sort
} > "$report_dir/runner-packages.txt"

{
    python3 -VV
    python3 -m pip list --format=freeze
    for tool in cmake ctest pkg-config gcc g++ clang-20 llvm-cov-20 mull-runner-20 \
        clang-format clang-tidy clazy-standalone cppcheck doxygen shellcheck; do
        if command -v "$tool"; then
            version=$("$tool" --version 2>&1)
            printf '%s\n' "$version"
        fi
    done
    if command -v dot; then
        dot -V
    fi
    libmtp_version=$(pkg-config --modversion libmtp)
    printf 'libmtp: %s\n' "$libmtp_version"
} > "$report_dir/selected-tools.txt" 2>&1
