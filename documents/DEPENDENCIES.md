# Dependency inventory

Reviewed 2026-11-17 and again 2026-11-24 using upstream release APIs and registries. `plan.md` tracks the
remaining qualification work. Build compatibility minima and exact tested SDK versions
serve different purposes: CMake permits compatible consumers; provisioning pins the SDK.

| Component | Previous project selection | Selected or measured | Source / status |
| --- | --- | --- | --- |
| Qt Core, Network, Sql, Xml, Widgets, Test and tools | >=6.8, CI 6.8.* | 6.12.1 (`6.12.1-0-202611180427`) | [Official open-source release](https://download.qt.io/official_releases/qt/6.12/6.12.1/); newest stable 2026-11-24, 6.13 is pre-release only |
| Qt for Windows (release ZIP) | Absent | 6.12.1 MinGW kit with MinGW 15.1.0 (`15.1.0-202610271023`) | [Qt repository](https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6121/qt6_6121_mingw/); five archives SHA-256 pinned in `tools/qt-sdk-windows-x86_64.sha256` |
| Qt installer | Indirect Action provisioning | aqtinstall 3.3.0 from both Python locks | [PyPI](https://pypi.org/project/aqtinstall/3.3.0/); all five Linux SDK archives are SHA-256 pinned |
| CMake / CTest | >=3.28; local 4.4.4 | 4.5.1 (Linux and Windows archives) qualified; compatibility floor retained | [Upstream release](https://github.com/Kitware/CMake/releases/tag/v4.5.1) |
| libmtp | Unversioned pkg-config | 1.1.23 locally | [Upstream releases](https://github.com/libmtp/libmtp/releases); current upstream stable; CI distro package resolution still needs a reproducible pin |
| Compiler | OS-selected | GCC 16.2.1 locally | Exact CI compiler/environment lock pending |
| pkg-config implementation | Host pkgconf 3.0.7 | Private pkgconf 3.0.11 | Checksum-pinned installer and 429 upstream checks; final project qualification pending |
| Python | Host 3.14.7; scripts support 3.12+ | Private / Actions 3.15.2 | [Private interpreter qualification](PYTHON_TOOLCHAIN.md); exact bootstrap/source backends in requirements-bootstrap.txt |
| Coverage frontend | Absent | gcovr 8.8 installed for qualification | [PyPI](https://pypi.org/project/gcovr/8.8/); measurement/gate pending |
| Mutation engine | Absent | Mull 0.38.0 qualified with LLVM 20.1.8; 0.39.1 reviewed 2026-11-24 and deferred | [Upstream release](https://github.com/mull-project/mull/releases/tag/0.38.0); published selected release provides the LLVM 20 ABI, while the local system Clang is 23.1.1; native Qt Test mutation spike qualified; full campaign target remains open. 0.39.x (2026-11-19) changes which mutants are generated (templates, default member initializers) and the qualified call-removal correction is bound to the 0.38.0 plugin, so the upgrade needs its own correction requalification and ratchet review |
| Clazy standalone | Ubuntu Clazy 1.11 / LLVM 15 | CI builds 1.17.1 against LLVM 20.1.8 | [Upstream source tag](https://invent.kde.org/sdk/clazy/-/tags/v1.17.1); checksum-pinned source and development archives support C++23 analysis |
| Cppcheck | Ubuntu Cppcheck 2.13.0 | CI builds 2.22.0 | [Upstream release](https://github.com/cppcheck-opensource/cppcheck/releases/tag/2.22.0); source is SHA-256 pinned |
| checkout Action | v4 | v7.0.1, immutable SHA | CI/release workflows |
| upload-artifact Action | v4 | v7.0.2, immutable SHA | CI/release workflows |
| download-artifact Action | Absent | v8.0.2, immutable SHA `9000827c…8333` | Release workflow; moves the Windows ZIP to the publishing job |
| action-gh-release | v2 | v3.0.4, immutable SHA | Release workflow; publishing still requires authorization |
| linuxdeploy (AppImage) | Absent | 1-alpha-20251107-1, SHA-256 pinned | [Upstream release](https://github.com/linuxdeploy/linuxdeploy/releases/tag/1-alpha-20251107-1); newest non-continuous release; release workflow only |
| linuxdeploy-plugin-qt | Absent | 1-alpha-20250213-1, SHA-256 `15106be8…c92724` pinned | [Upstream release](https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/tag/1-alpha-20250213-1); newest non-continuous release; upstream publishes no digest, so the hash was measured on download (12317424 bytes, two consistent fetches); bundles Qt 6.12 into the AppImage |

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
/path/to/quality-tools/bin/python -m pip install --no-deps -r requirements-bootstrap.txt
/path/to/quality-tools/bin/python -m pip install --no-deps --no-build-isolation -r requirements-quality.txt
/path/to/quality-tools/bin/python -m pip check
export PATH=/path/to/quality-tools/bin:$PATH
tools/install_qt_sdk.sh /path/to/new/Qt /path/to/dependency-reports

QTSCROBBLER_QT_PREFIX=/path/to/new/Qt/6.12.1/gcc_64 \
QTSCROBBLER_JOBS=4 ./localPipeline.sh --noRun --noOpen --report-dir /tmp/qtscrobbler-reports
```

The installer requires a new directory and verifies the five Qt 6.12.1 Linux
x86-64 archives against the committed [SHA-256 manifest](../tools/qt-sdk-linux-x86_64.sha256).
The hashes match the official Qt package repository's
`6.12.1-0-202611180427` archive sidecars.
[Qt SDK archive index](https://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt6_6121/qt6_6121/qt.qt6.6121.linux_gcc_64/)

Use CMake and CTest from the same installation. The verified upstream CMake 4.5.1 archives are `cmake-4.5.1-linux-x86_64.tar.gz`
(SHA-256 `83b71181fb62b4fe8f26067a9321448572e7a43bd04ff6c7f54a24c33698f16d`) and
`cmake-4.5.1-windows-x86_64.zip` (SHA-256
`bfe6eadb245dc8213e0d3feea316b520cb2aae92f5667ffd76059ba6b05c430c`), both checked
against upstream's `cmake-4.5.1-SHA-256.txt`. `tools/install_cmake.py` selects the
archive for the host.

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
- CI builds Clazy 1.17.1 in a private prefix with
  [`tools/install_clazy.sh`](../tools/install_clazy.sh). The installer binds Clazy
  source to commit `55a7af428ea793c5b88962eafb8a59f9f5a78d9a` and checks
  the SHA-256 of each matching LLVM/Clang 20.1.8 development package. CMake and
  Ninja build the standalone analyzer using the existing LLVM prefix. The runner
  supplies the pinned Clang resource directory to Clazy at analysis time.
- CI builds Cppcheck 2.22.0 with
  [`tools/install_cppcheck.sh`](../tools/install_cppcheck.sh) from exact commit
  `a436ca35ed1887bee789765122b65ed2d7a7e045`. This version accepts C++23 language
  constructs such as `if consteval`, which Ubuntu's Cppcheck 2.13.0 rejects.
- Native Clazy, Cppcheck, Doxygen, Xvfb and libX11 are required by the complete
  gate. The quality reports record their actual versions. Ubuntu distribution
  analyzer/runtime packages still depend on the runner repository resolution;
  those packages are not falsely claimed to be locked by requirements-quality.txt.

The default local pipeline is the complete qualification entry point. `--quick`
is an explicit incremental build/test subset; `--baseline` measures incomplete
coverage/mutation and cannot qualify a release. See [quality commands](QUALITY.md).

## Qualified native call-removal correction

The official Mull 0.38.0 plugin mislabels compiler-generated cleanup as source
void-call removal and misses exception-capable source calls. Use the separate,
reviewed correction when qualifying mutation; the original toolchain is retained.

```sh
python3 tools/install_mutation_toolchain.py --prefix /path/to/native-tools
python3 tools/install_mull_callfix.py --toolchain-prefix /path/to/native-tools --prefix /path/to/mull-callfix
. /path/to/mull-callfix/env.sh
```

The correction source is derived from libirm at exact commit
`dfaf9660867bad1814b0cae8e38f251a106fe858`, archive SHA-256
`172172e3135b80d40ea0e996f3eabce23c1be54d9ab44cfb52fe699417a7e9a6`.
LLVM 20.1.8 development headers use the exact matching official Debian package,
SHA-256 `3c5f27bb70dcea4a7bbf3b10d6c9a72ca4c6acf58719cebcf5cd1c17bf7bc0b8`.
The installer verifies both archives, stages in a new private prefix, compiles with
warnings fatal and runs a native LLVM CFG/unwind/PHI verifier before publication.
A recognized installation is reused only when every installed entry and bound
compiler/plugin/LLVM input matches its manifest.

The wrapper preloads the correction only when compiling with the selected Mull
plugin. Application, test, coverage and sanitizer processes do not preload it.
The full pipeline also requires an actual Qt signal-removal assertion kill; the
unmodified plugin fails this check. Semantic proof, wrapper/shim/source/CFG inventory,
loader environment and every production compiler command remain bound and rechecked
throughout the campaign. Full campaign qualification is separate from tool validation.
The build-only libirm correction retains Apache 2.0 attribution; application GPLv2 is
unchanged. See [native mutation details](MUTATION_TESTING.md).

## Refreshed auxiliary tools

The [fresh all-tools audit](DEPENDENCY_REVIEW_2026-11-17.md) verifies the complete
Python lock, SDK, native tooling, host/runtime inventory and Action SHAs. Install
updated pkgconf 3.0.11 and Graphviz 16.1.2 without modifying the host:

```sh
python3 tools/install_aux_tools.py --prefix /path/to/aux-tools
export PATH=/path/to/aux-tools/bin:$PATH
```

The helper checks exact source hashes, stages a new private installation, runs all
429 pkgconf upstream cases, initializes Graphviz's private plugin registry and
checks real SVG output and isolated package resolution. Reuse requires a matching
installed inventory and working executables. Graphviz includes native layout/SVG
support; optional GUI/render integrations and language bindings are disabled.
The source build requires make, a C/C++ compiler, bison and flex. These bootstrap
and host/runtime versions remain distinct from the pinned selected tools; the
fresh audit records current compatibility/environment gaps explicitly.

Local private install/reuse and nine installer contracts passed; the complete
application gate and actual Actions must qualify the updated selection before
calling the whole dependency refresh complete.

Offline qualification additionally requires Linux user/network namespaces,
util-linux `unshare`, iproute2 `ip` and `dbus-daemon`; the workflows explicitly
install these runner packages. Their exact observed runner/package versions and
namespace capability remain part of final environment evidence, not the Python
lock. A private minimal session bus avoids activating the user's desktop services.

The refreshed interpreter selection is Python **3.15.2**, with official
setup-python **v7.1.0** pinned to
`6d4fd22b20465ccc0f7354476588e8df7c9ca0e0` in both workflows. See the
[private Python recipe and qualification](PYTHON_TOOLCHAIN.md). Install exact
`requirements-bootstrap.txt` before `requirements-quality.txt`; disable pip build
isolation for the quality resolution so CPython3.15 source wheels use the reviewed
build backends instead of floating build requirements. The private selected Python
and native tools do not replace the user's host runtime libraries.

## Windows release toolchain

`tools/install_qt_sdk_windows.py --dest NEW_DIR` installs the Qt 6.12.1 MinGW kit
(qtbase, qttools, qttranslations and the MinGW runtime) plus Qt's matching MinGW
15.1.0 compiler. aqtinstall 3.3.0 cannot resolve Qt's architecture-suffixed Windows
repository folders (`qt6_6121_mingw`), so the script downloads the fixed official
archive URLs and checks every archive against `tools/qt-sdk-windows-x86_64.sha256`
before extracting it with 7-Zip. The hashes equal the official `.sha256` sidecars and
were re-measured locally on 2026-11-24. `tools/install_zlib.py` builds the same pinned
zlib 1.4 with that compiler (`CMAKE_GENERATOR="MinGW Makefiles"`); MinGW names the
static archive `libzs.a`. `tools/build_windows_package.sh` builds with MTP disabled,
runs the Windows-qualified suites, deploys with `windeployqt` and smoke-tests the
extracted ZIP without Qt or MinGW on `PATH`.

The host MinGW cross compiler on the development machine targets UCRT while Qt's kit
uses msvcrt, so local Wine runs of a cross build are only a portability check; the
GitHub Windows runner provides the qualifying Windows evidence.

All installers send the neutral User-Agent `qtscrobbler-tool-installer`: since
2026-11 archive.ubuntu.com answers Python's default agent with HTTP 403.
