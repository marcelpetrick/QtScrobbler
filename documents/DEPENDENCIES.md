# Dependency inventory

Reviewed 2026-11-17 using upstream release APIs and registries. `plan.md` tracks the
remaining qualification work. Build compatibility minima and exact tested SDK versions
serve different purposes: CMake permits compatible consumers; provisioning pins the SDK.

| Component | Previous project selection | Selected or measured | Source / status |
| --- | --- | --- | --- |
| Qt Core, Network, Sql, Xml, Widgets, Test and tools | >=6.8, CI 6.8.* | 6.12.0 | [Official downloads](https://download.qt.io/official_releases/qt/6.12/); 6.12.1 not yet in download index at review |
| Qt installer | Unpinned indirect tool | aqtinstall 3.3.0 | [PyPI](https://pypi.org/project/aqtinstall/3.3.0/); Linux archive set qtbase, qttools, qttranslations, icu |
| CMake / CTest | >=3.28; local 4.4.4 | 4.5.0 qualified toolchain; compatibility floor retained | [Upstream release](https://github.com/Kitware/CMake/releases/tag/v4.5.0) |
| libmtp | Unversioned pkg-config | 1.1.23 locally | [Upstream releases](https://github.com/libmtp/libmtp/releases); current upstream stable; CI distro package resolution still needs a reproducible pin |
| Compiler | OS-selected | GCC 16.2.1 locally | Exact CI compiler/environment lock pending |
| pkg-config implementation | OS-selected | pkgconf 3.0.7 locally | Exact CI environment lock pending |
| Python | No project tooling dependency | 3.14.7 locally; scripts support 3.12+ | Used for tooling and pipeline contract tests; CI environment pin pending |
| Coverage frontend | Absent | gcovr 8.8 installed for qualification | [PyPI](https://pypi.org/project/gcovr/8.8/); measurement/gate pending |
| Mutation engine | Absent | Mull 0.38.0 candidate | [Upstream release](https://github.com/mull-project/mull/releases/tag/0.38.0); published Linux LLVM-specific packages currently stop at LLVM 20, local Clang is 23.1.1; compatible toolchain spike pending |
| checkout Action | v4 | v7.0.1, immutable SHA | CI/release workflows |
| upload-artifact Action | v4 | v7.0.2, immutable SHA | CI/release workflows |
| install-qt-action | v3 | v4.5.0, immutable SHA | CI/release workflows |
| action-gh-release | v2 | v3.0.4, immutable SHA | Release workflow; publishing still requires authorization |

`requirements-quality.txt` pins the installed installer/coverage tools and their complete
resolved Python dependency set. Re-review upstream releases before changing this file;
never regenerate it from an unrelated global environment. Source and runtime dependencies
such as OpenSSL, ICU, SQLite, libusb and graphics plugins require environment inventory
and packaging checks; they are not silently claimed to be locked by a Python file.

## Provisioning and local checks

The official Qt Linux SDK requires ICU 73; include the installer's **icu** archive even
on hosts with a newer system ICU. No OS-wide package replacement is needed.

```sh
python3 -m venv /path/to/quality-tools
/path/to/quality-tools/bin/python -m pip install -r requirements-quality.txt
/path/to/quality-tools/bin/aqt install-qt linux desktop 6.12.0 linux_gcc_64 \
  --outputdir /path/to/Qt --archives qtbase qttools qttranslations icu

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
7. Commit only after mandatory checks pass. Never publish/push as part of the review.
