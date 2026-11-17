# Dependency review — 2026-11-17

This is a fresh upstream audit performed for the owner's request to run
`updateDependencies` again for all tools. It records upstream availability and
observed local versions. It is not a successful complete gate, a fully locked
Linux environment, or qualification of a new dependency selection. No global
OS packages were upgraded by this audit.

## Explicit project and SDK selections

| Component | Project selection / local observation | Fresh upstream observation | Decision or next action |
| --- | --- | --- | --- |
| Qt modules and tools | SDK 6.12.0; CMake minimum 6.12 | Official 6.12 download directory contains 6.12.0 only | Retain newest downloadable 6.12 SDK; check security status below |
| CMake and CTest | Private provisioner 4.5.0; host `/usr/bin` 4.4.4 | 4.5.0, stable, published 2026-11-12 | Use private provisioned pair; do not substitute host CTest or broken user wrapper |
| Mull | 0.38.0 | 0.38.0, stable, published 2026-11-11 | Retain with matching LLVM20 ABI and separately qualified correction |
| LLVM compiler, coverage tools and development headers | Private 20.1.8; host Clang 23.1.1 | LLVM 23.1.6 stable published 2026-11-17 | Retain 20.1.8 for Mull ABI; host latest does not provide compatible plugin ABI |
| zlib | Private exact 1.4; host pkg-config 1.3.2 | 1.4 stable | Use checksum-pinned private prefix and upstream tests |
| libmtp | Local 1.1.23; CI distribution-selected | 1.1.23 stable | Current local version; CI package resolution still needs inventory/pinning |
| GCC | Host 16.2.1 distribution build | Official GCC releases list 16.2 | Record distribution compiler identity; qualification uses selected native compiler |

Sources checked directly:

- Qt: <https://download.qt.io/official_releases/qt/6.12/>
- CMake: <https://api.github.com/repos/Kitware/CMake/releases/latest>
- Mull: <https://api.github.com/repos/mull-project/mull/releases/latest>
- LLVM: <https://api.github.com/repos/llvm/llvm-project/releases/latest>
- zlib: <https://api.github.com/repos/madler/zlib/releases/latest>
- libmtp: <https://api.github.com/repos/libmtp/libmtp/releases/latest>
- GCC: <https://gcc.gnu.org/releases.html>

Existing installers record artifact hashes. This audit did not redownload every
large SDK/native archive and does not replace installer checksum verification.
The private call-removal correction and libirm source are intentionally exact
build inputs rather than a claim that every upstream development commit is adopted.

## Exact Python tools and resolved dependencies

Each of the following 31 `requirements-quality.txt` entries matched PyPI's
current `info.version`. The release files for every selected version were present
and not all yanked. The query used `https://pypi.org/pypi/<name>/json`; version
metadata and the dependency resolver remain subject to installation qualification
on the selected Python version. No Python lock change was indicated by this check.

| Package | Selected and latest | Exact source |
| --- | --- | --- |
| aqtinstall | 3.3.0 | <https://pypi.org/pypi/aqtinstall/json> |
| beautifulsoup4 | 4.15.0 | <https://pypi.org/pypi/beautifulsoup4/json> |
| brotli | 1.2.1 | <https://pypi.org/pypi/brotli/json> |
| bs4 | 0.0.2 | <https://pypi.org/pypi/bs4/json> |
| certifi | 2026.10.23 | <https://pypi.org/pypi/certifi/json> |
| charset-normalizer | 3.5.2 | <https://pypi.org/pypi/charset-normalizer/json> |
| colorlog | 6.12.0 | <https://pypi.org/pypi/colorlog/json> |
| defusedxml | 0.7.1 | <https://pypi.org/pypi/defusedxml/json> |
| gcovr | 8.8 | <https://pypi.org/pypi/gcovr/json> |
| humanize | 4.17.0 | <https://pypi.org/pypi/humanize/json> |
| idna | 3.20 | <https://pypi.org/pypi/idna/json> |
| inflate64 | 1.0.4 | <https://pypi.org/pypi/inflate64/json> |
| Jinja2 | 3.1.6 | <https://pypi.org/pypi/Jinja2/json> |
| lxml | 6.1.3 | <https://pypi.org/pypi/lxml/json> |
| MarkupSafe | 3.0.4 | <https://pypi.org/pypi/MarkupSafe/json> |
| multivolumefile | 0.2.3 | <https://pypi.org/pypi/multivolumefile/json> |
| patch-ng | 1.19.1 | <https://pypi.org/pypi/patch-ng/json> |
| psutil | 7.2.2 | <https://pypi.org/pypi/psutil/json> |
| py7zr | 1.1.4 | <https://pypi.org/pypi/py7zr/json> |
| pybcj | 1.0.8 | <https://pypi.org/pypi/pybcj/json> |
| pycryptodomex | 3.24.2 | <https://pypi.org/pypi/pycryptodomex/json> |
| Pygments | 2.21.0 | <https://pypi.org/pypi/Pygments/json> |
| pyppmd | 1.3.1 | <https://pypi.org/pypi/pyppmd/json> |
| requests | 2.34.2 | <https://pypi.org/pypi/requests/json> |
| semantic-version | 2.10.0 | <https://pypi.org/pypi/semantic-version/json> |
| soupsieve | 4.1.1 | <https://pypi.org/pypi/soupsieve/json> |
| texttable | 1.7.3 | <https://pypi.org/pypi/texttable/json> |
| typing_extensions | 4.16.0 | <https://pypi.org/pypi/typing_extensions/json> |
| urllib3 | 2.8.0 | <https://pypi.org/pypi/urllib3/json> |
| clang-format | 23.1.5 | <https://pypi.org/pypi/clang-format/json> |
| clang-tidy | 23.1.4 | <https://pypi.org/pypi/clang-tidy/json> |

The formatter/analyzer wheel release numbers are independent of the ABI-pinned
native LLVM20 compiler. An LLVM23 release does not imply that matching Python
wheel releases already exist.

## GitHub Actions

All selected versions were still the latest stable release. GitHub tag references
were resolved afresh and matched the workflow pins exactly. The release Action's
annotated tag was dereferenced to its commit.

| Action | Latest selected tag | Resolved commit SHA |
| --- | --- | --- |
| actions/checkout | v7.0.1 | `3d3c42e5aac5ba805825da76410c181273ba90b1` |
| actions/upload-artifact | v7.0.2 | `cf430e030ddbb5b0abf93d22962f4752f3646cd9` |
| jurplel/install-qt-action | v4.5.0 | `b344308e4483fc892e0d27b3cd1b173ebdd95f06` |
| softprops/action-gh-release | v3.0.4 | `8a0cd651e5b2365bd640854a3fa5dfc70e4875f3` |

Release and tag APIs:

- <https://api.github.com/repos/actions/checkout/releases/latest>
- <https://api.github.com/repos/actions/checkout/git/ref/tags/v7.0.1>
- <https://api.github.com/repos/actions/upload-artifact/releases/latest>
- <https://api.github.com/repos/actions/upload-artifact/git/ref/tags/v7.0.2>
- <https://api.github.com/repos/jurplel/install-qt-action/releases/latest>
- <https://api.github.com/repos/jurplel/install-qt-action/git/ref/tags/v4.5.0>
- <https://api.github.com/repos/softprops/action-gh-release/releases/latest>
- <https://api.github.com/repos/softprops/action-gh-release/git/ref/tags/v3.0.4>
- <https://api.github.com/repos/softprops/action-gh-release/git/tags/28ab305d0ffa26d60ec4bd614126329e26c7a0ec>

Runner/runtime compatibility must still be observed in actual Actions execution;
matching upstream SHAs is not evidence of a green workflow.

## Native auxiliary tools and host/runtime inventory

Host observations came from executable version commands or pkg-config. CI uses
Ubuntu 24.04 distribution packages for several entries and can therefore differ.
These entries are not locked by the Python requirements file.

| Component | Local observation | Upstream stable checked | Required follow-up |
| --- | --- | --- | --- |
| Clazy | 1.17.1 (version output labels analyzer 1.17) | 1.17.1 | Current; record compiler linkage and CI resolution |
| Cppcheck | 2.22.0 | 2.22.0 | Current local tool; inventory CI |
| Doxygen | 1.18.0 | 1.18.0 | Current local tool; inventory CI |
| ShellCheck | 0.11.0 | 0.11.0 | Current local tool; inventory CI |
| pkgconf / pkg-config | 3.0.7 | 3.0.11 | Provision exact private update and qualify discovery |
| Graphviz | 16.1.0 | 16.1.1 | Provision exact private update and qualify Doxygen rendering |
| Python interpreter | 3.14.7 | 3.14.10 maintenance; 3.15.2 newest feature release | Prefer private maintenance update first; prove wheel compatibility before selecting 3.15 |
| Xvfb / X server | Host package 21.1.24-1 | X.Org index contains stable xorg-server 21.1.26; newer 26.0.99 entries are prereleases | Newer stable exists; record CI package identity and do not adopt prerelease |
| libusb | 1.0.30 | 1.0.30 | Current local runtime; inventory linked CI library |
| libX11 | 1.8.13 | 1.8.14 | Host runtime newer release available; retain explicit unresolved environment item |
| SQLite | 3.53.4 via pkg-config | 3.54.1 | Qt SQL plugin may use a different SQLite; inspect actual linkage before replacement |
| OpenSSL | 3.6.5 via pkg-config | 4.1.0 | Major ABI change; inspect Qt TLS backend compatibility before private replacement |
| ICU | Host 78.3-1; Qt SDK requires ICU73 archive; native LLVM uses privately extracted ICU74 package | ICU 79.1 stable | Retain ABI-specific SDK/native inputs; host latest cannot replace their sonames |
| libarchive / bsdtar | Host 3.8.9-1; CI distribution-selected | 3.8.11 stable | Newer stable available; private update needs archive-format and extraction qualification |
| Ninja | Host 1.13.2-3 | 1.13.2 stable | Current; retain exact build-backend identity |
| Bash, Coreutils, Git and Make | Host packages 5.3.20-1, 9.12-2, 2.56.0-1 and 4.4.1-3 | No independent full release qualification established by this audit | Host execution utilities; capture identities rather than claiming project lock |
| build-essential and graphics development packages | CI distribution-selected | No single upstream version represents these package groups | Capture package/executable identities; no complete environment-lock claim |

Sources:

- Clazy: <https://download.kde.org/stable/clazy/>
- Cppcheck: <https://api.github.com/repos/cppcheck-opensource/cppcheck/releases/latest>
- Doxygen: <https://api.github.com/repos/doxygen/doxygen/releases/latest>
- ShellCheck: <https://api.github.com/repos/koalaman/shellcheck/releases/latest>
- pkgconf: <https://api.github.com/repos/pkgconf/pkgconf/releases/latest>
- Graphviz: <https://gitlab.com/api/v4/projects/4207231/releases/permalink/latest>
- Python: <https://www.python.org/downloads/>
- X server: <https://xorg.freedesktop.org/archive/individual/xserver/>
- libX11: <https://xorg.freedesktop.org/archive/individual/lib/>
- libusb: <https://api.github.com/repos/libusb/libusb/releases/latest>
- OpenSSL: <https://api.github.com/repos/openssl/openssl/releases/latest>
- SQLite: <https://www.sqlite.org/download.html>
- libarchive: <https://api.github.com/repos/libarchive/libarchive/releases/latest>
- ICU: <https://api.github.com/repos/unicode-org/icu/releases/latest>
- Ninja: <https://api.github.com/repos/ninja-build/ninja/releases/latest>

## Private auxiliary-tool update recommendations

For pkgconf 3.0.11, resolve its upstream release source archive, record its SHA-256,
then configure/build/install in a fresh project-owned prefix. Select its `bin`
through a scoped PATH for the pipeline. Preserve Qt, zlib and libmtp discovery
settings, record `pkgconf --version`, and compare the resulting CMake dependency
paths with the existing qualified configuration. The current host installation
must remain untouched.

For Graphviz 16.1.1, resolve the official release source or official platform
artifact and record its SHA-256. Build/install into a separate private prefix,
with dependency/plugin discovery recorded. Select that prefix's `dot` for Doxygen,
run `dot -V`, exercise a real diagram, and rerun the documentation stage followed
by the complete gate. Installing a newer executable alone does not qualify its
plugins or produce a fully pinned Graphviz dependency tree.

These are actionable update paths, not completed updates. The primary agent must
record exact artifacts, build inputs and results before calling either qualified.

## Qt security patch status

The newest downloadable SDK is still 6.12.0, but its official series directory
also contains later source fixes. This audit inspected the following patches:

- CVE-2026-116337: bound status-line and header-block accumulation in
  `QHttpNetworkReply`, relevant to the application's QtNetwork dependency:
  <https://download.qt.io/official_releases/qt/6.12/CVE-2026-116337-qtbase-6.12-0001.diff>
  and
  <https://download.qt.io/official_releases/qt/6.12/CVE-2026-116337-qtbase-6.12-0002.diff>.
- CVE-2026-105924: guard an empty CBOR URL conversion to JSON:
  <https://download.qt.io/official_releases/qt/6.12/CVE-2026-105924-qtbase-6.12.diff>.
- CVE-2026-105943: supply Windows NTLM target SPN:
  <https://download.qt.io/official_releases/qt/6.12/CVE-2026-105943-qtbase-6.12.diff>.

The audit did not establish that the binary SDK incorporates these patches and
does not claim it does. Choosing the newest numbered downloadable SDK alone is
not proof of applied security fixes. Other module patches in the series index
must not be represented as application dependencies when those modules are unused.

## Qualification boundary

A complete dependency result still needs the private tool updates or explicit
compatibility retentions, the actual CI package/runtime inventory, fresh full
native gate results, extracted-package checks, and observed green Actions. The
native correction is pending its whole-scope mutation qualification separately.
Historical coverage and targeted mutation proofs do not establish this audit's
full gate as passing.
