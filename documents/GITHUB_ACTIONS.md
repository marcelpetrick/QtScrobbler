# GitHub Actions — QtScrobbler

CI uses the same quality entry point as local development:

```sh
QTSCROBBLER_QT_PREFIX=/path/to/Qt/6.12.1/gcc_64 \
  ./localPipeline.sh --noRun --noOpen --report-dir build-reports
```

`--noRun` suppresses the optional interactive GUI launch. Automated GUI behavior is
still exercised by CTest. `--noOpen` leaves reports closed. `--report-dir` retains
stage logs and `junit.xml`. Mandatory stage failures cause a nonzero pipeline exit.
The pipeline's current and planned gates are recorded in [plan.md](plan.md); the
workflow adds no alternative definition of a passing test suite or coverage score.

## CI

[ci.yml](../.github/workflows/ci.yml) runs on every branch push except version tags,
on pull requests to `main` (and `master` during migration), and on manual dispatch.
The Ubuntu 24.04 runner installs the required system libraries, checksum-verified
CMake/CTest 4.5.1, and the Qt 6.12.1 Linux SDK with qtbase, qtdeclarative, qttools, qttranslations
and ICU archives. Both workflows install the exact bootstrap and quality locks
into an isolated Python 3.15.2 virtual environment before invoking its aqt installer.
The shared Qt installer checks all five SDK archives against committed SHA-256 hashes.
GitHub Actions are referenced by immutable commit SHAs.

A parallel `windows` job on Windows Server 2025 installs CMake 4.5.1, the
checksum-pinned Qt 6.12.1 MinGW kit with MinGW 15.1.0
(`tools/install_qt_sdk_windows.py`) and zlib 1.4, then runs
`tools/build_windows_package.sh`. It gates on building both programs, all eleven
Qt Test suites, `windeployqt` packaging and a smoke test of the extracted ZIP with no Qt or
MinGW on `PATH`. The Python contracts check the Linux pipeline tooling and run in the Linux job.

Six suites were first measured without gating (run 45412165069, 2026-11-24) and
then ported. Their failures were test-side assumptions, not product defects:

| Suite | Before porting (passed / failed) | Cause | Port |
| --- | --- | --- | --- |
| `conf` | 15 / 6 | Settings fixtures used the native format; Windows Conf uses INI, legacy settings use the registry | Fixtures use Conf's format; the legacy registry keys are cleared per test |
| `dbcache` | 23 / 1 | Child-process output ends in `\r\n` (text-mode stdout) | Normalize line endings |
| `scrobble` | 48 / 17 | Settings and cache written to `%APPDATA%` leaked between tests | INI locations redirected into the test home |
| `gui` | 30 / 16 | `\r\n` output and the offscreen font warning | Normalize; `QT_QPA_FONTDIR` set to the system fonts |
| `cli` | 9 / 52 | Expected output compared byte-for-byte with `\n` line endings | Normalize line endings |
| `translations` | 5 / 1 | The offscreen platform warns that Qt ships no font directory | `QT_QPA_FONTDIR` set to the system fonts |

`tests/test_environment.h` holds these isolations. Windows does not redirect the
registry, so the `conf` suite skips itself unless `QTSCROBBLER_TEST_WINDOWS_REGISTRY=1`
allows it to clear the legacy `qtscrob` keys; the disposable CI runner sets it. After
porting, validation run 45835640630 passed all six (conf 21, dbcache 24, scrobble 65,
cli 61, gui 46, translations 6). The Linux job remains the complete quality gate
(coverage, sanitizers, mutation).

The installer step's explicit `qt-root-dir` output selects the SDK for the local
pipeline. This prevents a system `qmake6` from selecting an older Qt installation.
CMake and CTest are taken from the same installed prefix.

Ubuntu 24.04 runners restrict unprivileged user namespaces through AppArmor, which
makes `tools/run_offline.py` fail closed (`write failed /proc/self/uid_map`). Both
workflows set `kernel.apparmor_restrict_unprivileged_userns=0` and probe the exact
namespace set before the pipeline. CI provisions Clazy 1.17.1 against the
same pinned LLVM 20.1.8 as the C++23 build. See [quality](QUALITY.md).

The quality step uses `pipefail` while teeing output, so saving its log cannot hide
a failure. The always-run report includes actual pipeline output, including failures,
or states that provisioning prevented the pipeline from running. It does not print
hardcoded successful stage results.

Artifacts are available in the workflow run's **Artifacts** section:

- `quality-reports` contains pipeline logs, JUnit XML, the build report, and
  dependency reports with Qt archive checks, runtime links, runner packages,
  and selected tool versions. Retained for 30 days.
- `binaries-linux-x86_64`: GUI `qtscrob` and CLI `scrobbler`, uploaded only on success;
  retained 7 days.

The workflow is bounded to 360 minutes. System/compiler packages are currently
resolved by Ubuntu apt; the remaining environment lock work is tracked in
[the dependency inventory](DEPENDENCIES.md).

## Repository security

Dependabot alerts and automatic security-update pull requests are enabled in the
repository settings. [Dependabot configuration](../.github/dependabot.yml)
also requests weekly version checks for the Python pins and GitHub Actions.
Review changes to native SDKs, distribution packages and checksum-pinned tools
through the [dependency inventory](DEPENDENCIES.md).

[Dependency Review](../.github/workflows/dependency-review.yml) checks dependency
changes in pull requests targeting main. GitHub-managed CodeQL scans C/C++,
Python and Actions separately from the native pipeline. Secret scanning and
push protection are enabled in the repository settings.

## Pinned CMake installer

[`tools/install_cmake.py`](../tools/install_cmake.py) installs the reviewed Linux x86-64
CMake 4.5.1 release into a new, explicit directory without changing OS packages:

```sh
python3 tools/install_cmake.py --prefix /path/to/isolated/cmake
export PATH="/path/to/isolated/cmake/bin:$PATH"
cmake --version
ctest --version
```

It requires Python 3.12+, downloads the official archive, checks the committed SHA-256,
extracts with Python's safe data filter, and verifies both tool versions before installation.
Repeated calls reuse only a recognized installation bearing the same archive digest;
unrelated existing directories are rejected. A checksum mismatch installs nothing.
Update the release URL and reviewed digest together during dependency review.

## Releases

[release.yml](../.github/workflows/release.yml) publishes only commits that CI already
qualified. Push a version tag matching `v[0-9]*`, or run the workflow manually with an
existing tag. Ordinary pushes to `main` never publish a release.

The workflow is deliberately small (minutes instead of the full gate's hours):

1. Check out the tag and require it to equal `CLIENT_VERSION` in `src/lib/common.h`.
2. Require a **successful CI run on that exact commit**; otherwise stop. CI on `main`
   is the full gate: static analysis, sanitizers, 99% line coverage, 95% branch coverage and the native mutation
   ratchet. The release does not repeat them; mutation testing is never run per release.
3. Build and run all unit tests with `./localPipeline.sh --quick` (GCC, pinned Qt,
   CMake and zlib, network-isolated CTest).
4. Strip both executables and package them with README, the unchanged GPLv2
   `LICENSE`/`COPYING`, documents and the fictional example log as a `.tar.gz`.
5. Build `QtScrobbler-<version>-x86_64.AppImage` with `tools/build_appimage.sh`, using
   the SHA-256-pinned linuxdeploy and linuxdeploy-plugin-qt. It bundles Qt 6.12 and its
   plugins, including both desktop platform plugins (xcb and Wayland with its shell,
   decoration and graphics integrations; `tools/build_appimage.sh` fails if one is
   missing); the CLI is included as `usr/bin/scrobbler`. Each file gets a `.sha256`
   that names the bare file, so `sha256sum -c` works next to the download.
6. Verify every `.sha256` with `sha256sum -c` inside `dist/`. Extract the archive and run both `--help` entry points, and start the AppImage under
   Xvfb with its bundled xcb plugin and no Qt on the library path.
7. Write the release notes with `tools/release_notes.py`: the CHANGELOG entry, the
   checks that passed in the qualifying CI run (read from its `quality-reports`
   artifact; any non-PASS mandatory stage stops the release), the release build's
   unit tests and the SHA-256 of every file. Then publish, download the published
   assets again and verify both `.sha256` files with `sha256sum -c`. The
   `release-logs` artifact includes the build and dependency reports.
   `tests/test_release_workflow.py` runs the workflow's own checksum commands
   with absolute paths and requires both verifications (v0.13.85 first shipped
   an AppImage checksum naming the runner path; its asset was replaced).

Release flow: push to `main`, wait for green CI, then
`git tag -a vX.Y.Z <commit> && git push origin vX.Y.Z`.

These are dynamically linked Linux executables. **Qt >= 6.12**, its runtime plugins
(including SQLite and a suitable platform plugin), and libmtp must be installed on
the target machine. An older distro package such as Ubuntu 24.04's default Qt is
insufficient; use the reviewed SDK or another compatible runtime. Shared libraries
and Qt plugins are not bundled in the tarball. See [README.md](../README.md) and
[the dependency inventory](DEPENDENCIES.md).

macOS and Windows are not qualified by these workflows. No Docker build or image
publication is part of this initiative.
