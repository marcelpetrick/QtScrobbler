# Dependency inventory

Reviewed 2026-11-17 using upstream release APIs and registries. `plan.md` tracks the
remaining qualification work. Build compatibility minima and exact tested SDK versions
serve different purposes: CMake permits compatible consumers; provisioning pins the SDK.

| Component | Previous project selection | Selected or measured | Source / status |
| --- | --- | --- | --- |
| Qt Core, Network, Sql, Xml, Widgets, Test and tools | >=6.8, CI 6.8.* | 6.12.0 | [Official downloads](https://download.qt.io/official_releases/qt/6.12/); 6.12.1 not yet in download index at review |
| Qt installer | Unpinned indirect tool | aqtinstall 3.3.0 | [PyPI](https://pypi.org/project/aqtinstall/3.3.0/); Linux archive set qtbase, qtdeclarative, qttools, qttranslations, icu |
| CMake / CTest | >=3.28; local 4.4.4 | 4.5.0 qualified toolchain; compatibility floor retained | [Upstream release](https://github.com/Kitware/CMake/releases/tag/v4.5.0) |
| libmtp | Unversioned pkg-config | 1.1.23 locally | [Upstream releases](https://github.com/libmtp/libmtp/releases); current upstream stable; CI distro package resolution still needs a reproducible pin |
| Compiler | OS-selected | GCC 16.2.1 locally | Exact CI compiler/environment lock pending |
| pkg-config implementation | OS-selected | pkgconf 3.0.7 locally | Exact CI environment lock pending |
| Python | No project tooling dependency | 3.14.7 locally; scripts support 3.12+ | Used for tooling and pipeline contract tests; CI environment pin pending |
| Coverage frontend | Absent | gcovr 8.8 installed for qualification | [PyPI](https://pypi.org/project/gcovr/8.8/); measurement/gate pending |
| Mutation engine | Absent | Mull 0.38.0 qualified with LLVM 20.1.8 | [Upstream release](https://github.com/mull-project/mull/releases/tag/0.38.0); published selected release provides the LLVM 20 ABI, while the local system Clang is 23.1.1; native Qt Test mutation spike qualified; full campaign target remains open |
| checkout Action | v4 | v7.0.1, immutable SHA | CI/release workflows |
| upload-artifact Action | v4 | v7.0.2, immutable SHA | CI/release workflows |
| install-qt-action | v3 | v4.5.0, immutable SHA | CI/release workflows |
| action-gh-release | v2 | v3.0.4, immutable SHA | Release workflow; publishing still requires authorization |

`requirements-quality.txt` pins the installed installer/coverage tools and their complete
resolved Python dependency set. Re-review upstream releases before changing this file;
never regenerate it from an unrelated global environment. The native installer rejects unrelated nonempty prefixes. Older manifests without installed-file
fingerprints require a new empty prefix; do not overwrite an existing installation.

Source and runtime dependencies
such as OpenSSL, ICU, SQLite, libusb and graphics plugins require environment inventory
and packaging checks; they are not silently claimed to be locked by a Python file.

## Provisioning and local checks

The official Qt Linux SDK requires ICU 73; include the installer's **icu** archive even
on hosts with a newer system ICU. No OS-wide package replacement is needed.

```sh
python3 -m venv /path/to/quality-tools
/path/to/quality-tools/bin/python -m pip install -r requirements-quality.txt
/path/to/quality-tools/bin/aqt install-qt linux desktop 6.12.0 linux_gcc_64 \
  --outputdir /path/to/Qt --archives qtbase qtdeclarative qttools qttranslations icu

QTSCROBBLER_QT_PREFIX=/path/to/Qt/6.12.0/gcc_64 \
QTSCROBBLER_JOBS=4 ./localPipeline.sh --noRun --noOpen --report-dir /tmp/qtscrobbler-reports
```

Use CMake and CTest from the same installation. The verified Linux x86-64 upstream
CMake 4.5.0 tarball is `cmake-4.5.0-linux-x86_64.tar.gz` with SHA-256
`81f0d5b0960fb6adc8197030ffc827fba937640d51e73e39e0e9e40d88c66f6f`.
It was checked against upstream's `cmake-4.5.0-SHA-256.txt`; the PyPI CMake wrapper did
not yet provide 4.5.0, so it must not be presented as an installable PyPI pin.

## Repeatable dependency review

1. Read CMake, qmake remnants, workflow `uses` references, Python lock and this inventory.
2. Query upstream stable releases; reject prerelease/yanked candidates and verify platform support.
3. Resolve tags to immutable commit SHAs; review Action runner/runtime and permission requirements.
4. Provision an isolated environment and capture actual compiler, Qt, plugins and runtime library versions.
5. Update one coherent dependency group, then run the entire local pipeline and package/runtime checks.
6. Record exact old/new versions, measured results and outstanding limitations in `plan.md`.
7. Commit only after the applicable gate passes. The owner authorizes continuous pushes
   after green checks; release publication follows complete qualification.

## iPod decompression dependency

The iPod parser links checksum-pinned zlib 1.4 for bounded streaming
decompression. Provision it with `tools/install_zlib.py` in a private prefix
and set `ZLIB_ROOT` explicitly. Its qualification runs all 16 upstream tests;
see the native resolution below for the artifact hash.

## Native quality resolution added during qualification

- `clang-format==23.1.5` and `clang-tidy==23.1.4` are exact Python wheel pins;
  both were looked up on PyPI and installed into the isolated quality environment.
  Formatting and analyzer versions are independent of the ABI-matched LLVM 20
  compiler used for coverage/Mull.
- `tools/install_mutation_toolchain.py` checks fixed official package SHA-256 hashes
  and qualifies Clang/LLVM 20.1.8 with Mull 0.38.0. The LLVM 20 ABI is required by
  the selected Mull plugin; the system Clang 23 cannot substitute for it.
- `tools/install_zlib.py` pins zlib 1.4 from the official release tarball,
  SHA-256 `02dd159783677a749c04adbdd02b55b8a2bcc7e054f51b76b549199a916ad584`.
  It builds a static PIC library in a private prefix, runs all 16 upstream tests,
  and fingerprints installed artifacts/compiler provenance before allowing reuse.
  Set `ZLIB_ROOT` to this prefix; the complete pipeline and Actions use it.
- Native Clazy, Cppcheck, Doxygen, Xvfb and libX11 are required by the complete
  gate. The quality reports record their actual versions. Ubuntu distribution
  analyzer/runtime packages still depend on the runner repository resolution;
  those packages are not falsely claimed to be locked by requirements-quality.txt.

The default local pipeline is the complete qualification entry point. `--quick`
is an explicit incremental build/test subset; `--baseline` measures incomplete
coverage/mutation and cannot qualify a release. See [quality commands](QUALITY.md).
