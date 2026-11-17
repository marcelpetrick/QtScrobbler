# GitHub Actions — QtScrobbler

CI uses the same quality entry point as local development:

```sh
QTSCROBBLER_QT_PREFIX=/path/to/Qt/6.12.0/gcc_64 \
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
CMake/CTest 4.5.0, and the Qt 6.12.0 Linux SDK with qtbase, qtdeclarative, qttools, qttranslations
and ICU archives. The Qt installer and py7zr versions are explicit.
The complete Python quality tool resolution in `requirements-quality.txt` is installed
into an isolated virtual environment for pipeline stages such as coverage. GitHub Actions
are referenced by immutable commit SHAs.

The Qt install action's explicit `qt-root-dir` output selects the SDK for the local
pipeline. This prevents a system `qmake6` from selecting an older Qt installation.
CMake and CTest are taken from the same installed prefix.

Ubuntu 24.04 runners restrict unprivileged user namespaces through AppArmor, which
makes `tools/run_offline.py` fail closed (`write failed /proc/self/uid_map`). Both
workflows set `kernel.apparmor_restrict_unprivileged_userns=0` and probe the exact
namespace set before the pipeline. They also set `QTSCROBBLER_CLAZY_EXTRA_ARGS` so
Ubuntu's Clazy 1.11 analyzes against GCC 13 libstdc++ (see [quality](QUALITY.md)).

The quality step uses `pipefail` while teeing output, so saving its log cannot hide
a failure. The always-run report includes actual pipeline output, including failures,
or states that provisioning prevented the pipeline from running. It does not print
hardcoded successful stage results.

Artifacts are available in the workflow run's **Artifacts** section:

- `quality-reports`: pipeline/stage logs, JUnit XML, and build report; retained 30 days.
- `binaries-linux-x86_64`: GUI `qtscrob` and CLI `scrobbler`, uploaded only on success;
  retained 7 days.

The workflow is bounded to 360 minutes. System/compiler packages are currently
resolved by Ubuntu apt; the remaining environment lock work is tracked in
[the dependency inventory](DEPENDENCIES.md).

## Pinned CMake installer

[`tools/install_cmake.py`](../tools/install_cmake.py) installs the reviewed Linux x86-64
CMake 4.5.0 release into a new, explicit directory without changing OS packages:

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
   is the full gate: static analysis, sanitizers, 95% coverage and native mutation
   testing. The release does not repeat them; mutation testing is never run per release.
3. Build and run all unit tests with `./localPipeline.sh --quick` (GCC, pinned Qt,
   CMake and zlib, network-isolated CTest).
4. Strip both executables and package them with README, the unchanged GPLv2
   `LICENSE`/`COPYING`, documents and the fictional example log as a `.tar.gz`.
5. Build `QtScrobbler-<version>-x86_64.AppImage` with `tools/build_appimage.sh`, using
   the SHA-256-pinned linuxdeploy and linuxdeploy-plugin-qt. It bundles Qt 6.12 and its
   plugins; the CLI is included as `usr/bin/scrobbler`. Each file gets a `.sha256`.
6. Extract the archive and run both `--help` entry points, and start the AppImage under
   Xvfb with its bundled xcb plugin and no Qt on the library path.
7. Write the release notes with `tools/release_notes.py`: the CHANGELOG entry, the
   checks that passed in the qualifying CI run (read from its `quality-reports`
   artifact; any non-PASS mandatory stage stops the release), the release build's
   unit tests and the SHA-256 of every file. Then publish; logs are kept as an artifact.

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
