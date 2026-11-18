# Native quality checks

Coverage gates are 95% source line and 95% source branch coverage. Mutation testing
gates on a ratchet (`MUTATION_MINIMUM` in `localPipeline.sh`, currently 67%; owner
decision 2026-11-17) so the assertion-killed score cannot drop; 95% remains the goal
and the ratchet is raised as tests kill more mutants. Historical measurements are recorded in [the execution
plan](plan.md); mutation accounting is described in [Mutation testing](MUTATION_TESTING.md).

The local pipeline orchestrates native builds, CTest, coverage, sanitizers and mutation
campaigns. The following additional stages can run independently against a configured
build with `CMAKE_EXPORT_COMPILE_COMMANDS=ON` and `QTSCROBBLER_MTP=ON`:

```sh
python3 tools/run_quality_checks.py --stage static --build-dir /tmp/qtscrobbler-build --report-dir /tmp/qtscrobbler-static
python3 tools/run_quality_checks.py --stage docs --build-dir /tmp/qtscrobbler-build --report-dir /tmp/qtscrobbler-docs
python3 tools/run_quality_checks.py --stage package --build-dir /tmp/qtscrobbler-build --report-dir /tmp/qtscrobbler-package
```

Each report directory must be new or empty. Missing tools, missing files, analyzer
errors, tool failures and timeouts fail the stage. Commands, actual tool versions,
exit statuses and output logs are retained beside `summary.json`. The default bound
is 600 seconds per tool command; `--timeout` changes it explicitly. The GUI startup
observation lasts two seconds, followed by bounded termination and process reaping.

## Static analysis scope

The compilation inventory must contain every maintained Linux production translation
unit, including the MTP adapter. Qt generated code, tests, system headers, Windows-only
adapter code and vendored xgetopt are outside the Linux source inventory. Source
compilation arguments come from the real build.

Clang-tidy enables `clang-analyzer-core.*` and `clang-analyzer-unix.Malloc`, with
findings treated as errors. Clazy enables `connect-non-signal` and `incorrect-emit`
with compiler warnings fatal. Cppcheck enables its default error checks and uses its
Qt library model. Missing system includes are suppressed because Cppcheck cannot model
the complete compiler SDK; production parse errors and other diagnostics remain fatal.
These selected checks are incremental and do not constitute all available checks.
`QTSCROBBLER_CLAZY_EXTRA_ARGS` adds shell-split compiler arguments for Clazy only.
CI uses it because Ubuntu 24.04's Clazy 1.11 (Clang 15) cannot parse GCC 14
libstdc++; it points Clazy at the GCC 13 headers. The build is unchanged.

Qualified host tools are recorded per report. The initial native host supplies
clang-tidy/Clazy built on LLVM 23.1.1, Clazy 1.17.1, Cppcheck 2.22.0 and Doxygen 1.18.0.
These analyzers are separate from the LLVM 20 Mull compiler. Another host must provide
compatible native tools; the runner never installs or upgrades OS packages. Tool version
capture is provenance, and does not imply a reproducible toolchain lock on another host.

## Documentation and package evidence

Documentation checks require current instructions, vision, API, dependency, mutation and
quality documentation; balanced code fences; valid local links; GPLv2 license evidence;
and the screenshot captured from the real GUI. External links are not fetched by this
stage. Doxygen runs with warnings fatal and must produce its HTML index. API documentation
is indexed from the actual source declarations and [API contracts](API.md).

Package checks produce a tar archive, extract it into a disposable directory, check
both binaries with `ldd`, run both entry-point help commands with exact source-version verification, and observe
real offscreen GUI startup. HOME and XDG paths are temporary. The narrow allowance for
`This plugin does not support propagateSizeHints()` reflects Qt's offscreen platform.
Other GUI diagnostics fail the check. The package uses the selected installed Qt SDK
and host libraries; it is not a bundled standalone distribution. Import, edit, settings,
submission and cancellation workflows remain the responsibility of the real Qt Test suite.

## Complete pipeline and diagnostic modes

Install the exact Python requirements, qualified CMake/CTest and native LLVM/Mull
prefix, install the reviewed private call-removal correction and source its `env.sh`. Set `QTSCROBBLER_QT_PREFIX` and `ZLIB_ROOT` to the
qualified Qt 6.12.0 and zlib 1.4 prefixes. Native Clazy, Cppcheck, Doxygen, Xvfb and
libX11 must be available. Run:

```sh
QTSCROBBLER_JOBS=4 ./localPipeline.sh --noRun --noOpen --report-dir /path/to/new-reports
```

The default gate requires separate 95% raw line/branch thresholds and the current
67% native mutation ratchet, alongside every other mandatory stage. The mutation
goal is 95% proven assertion kills. `--quick` runs the explicit
build/test subset while implementation proceeds. `--baseline` runs all stages but
records subthreshold scores as measurements. Neither diagnostic mode establishes
complete qualification. CI runs the default complete gate. The release workflow
requires a matching version tag on a commit that passed complete CI on main and
repeats the build, tests, and extracted-package checks before publishing. Manual
dispatch can also publish an existing tag after those checks.

A native Qt Test wrapper also launches the actual GUI on a private Xvfb display,
finds its main window with libX11 and requests WM_DELETE_WINDOW. Normal termination
checks cleanup and flushes subprocess LLVM profiles. Display helpers are rejected
unless they advertise the window-close protocol. These tests use no existing user
display, settings, credentials or devices.

## Measured results

Commit `d07b90e` / client `0.13.69`, measured on 2026-11-17 with Qt6.12.0,
Clang/LLVM20.1.8, MTP enabled and pinned zlib1.4, by the complete local gate
(`~/.cache/qtscrobbler-runs/69/reports`) and Actions run 44167982834:

| Check | Observed result |
| --- | --- |
| Source lines | 2722 /2749, 99.017824663514%; 95% gate PASS (local and CI) |
| Source branches | 1027 /1063, 96.61335841956726%; 95% gate PASS (local and CI) |
| Missing production files | None; library, CLI, GUI, and Linux MTP included |
| Normal CTest | 26 /26 PASS (local and CI) |
| ASan/UBSan/leak CTest | 26 /26 PASS locally; CI failed only the Xvfb display-pipe reset, fixed in 0.13.74 |
| Format, static, documentation | PASS (local and CI) |
| Extracted package checks | PASS (local and CI) |
| Native mutation | 1195 /1766 assertion-killed, 67.667% locally; 1193 /1766, 67.554% on CI; ratchet 67% PASS, 95% goal open |

Earlier measurement:

Frozen commit `20bd7f8` / client `0.13.59`, measured on 2026-11-17 with Qt6.12.0,
Clang/LLVM20.1.8, MTP enabled, and pinned zlib1.4:

| Check | Observed result |
| --- | --- |
| Source lines | 2722 /2749, 99.017824663514%;95% gate PASS |
| Source branches | 1017 /1063, 95.67262464722484%;95% gate PASS |
| Missing production files | None; library, CLI, GUI, and Linux MTP included |
| Normal CTest | 20 /20 PASS |
| ASan/UBSan/leak CTest | 20 /20 PASS |
| Format, static, documentation | PASS |
| Extracted package checks | PASS (separate package stage on the same frozen build) |
| Native mutation | Interrupted after verified tool defects; no qualified complete score |

Coverage reports: `/tmp/qtscrobbler-frozen-59-full-reports/coverage/summary.json`,
with hashed exports, raw profiles, HTML, JUnit, source/test/tool inventories, and no
missing production files. Package evidence: `/tmp/qtscrobbler-frozen-59-package`.
README badges display the newest measured values truncated to two decimals;
passing uses integer totals. Historical measurements remain associated with their own commit.

The mutation investigation found both false-uncovered filtering and invalid native
void-call replacements around C++ exception cleanup. LLVM IR shows that some advertised
signal removals leave the signal intact while removing a destructor instead. These
are tool defects, not equivalent source mutations or missing signal assertions.
See [mutation qualification](MUTATION_TESTING.md); publication remains pending the
corrected native tool and a complete passing campaign.

The complete gate requires a native semantic preflight before mutation: an actual
Qt signal call must disappear and yield a normal QSignalSpy assertion failure.
Old or misconfigured native tools fail this stage. The build-only correction and
its CFG verifier under `tools/native` are outside the application coverage/mutation
scope; they are qualified with native LLVM invariants, the Qt sentinel and dedicated
installer/provenance contracts. Their exact source and binary fingerprints are
retained in each application campaign. No GUI/CLI/library/MTP code is excluded.

## Enforced offline execution

Normal and sanitizer CTest runs, coverage test collection, the complete native Mull
campaign and extracted-package checks use `tools/run_offline.py`. It creates fresh
Linux user, network, PID and mount namespaces with a matching `/proc` (LeakSanitizer
reads `/proc/<pid>/task`), enables only loopback and starts a private
DBus session with no desktop service directories. Descendant GUI/CLI/network tests
retain their real event loops and loopback fixtures while external routes and
host desktop service activation are unavailable. Missing namespace support, `ip`,
`unshare` or `dbus-daemon` fails the stage; there is no host-network fallback.
Dependency downloads must finish before qualification. Run an individual suite with:

```sh
python3 tools/run_offline.py -- ctest --test-dir build --output-on-failure --no-tests=error
```

The wrapper's contracts verify loopback HTTP, absence of an external route, private
bus activation inventory, exact argument/exit/signal forwarding and direct-child
cleanup on supervisor death. Whole-project checks must still qualify the final
integration in the selected local and GitHub runner environments.
