# Native mutation testing

The target is **95% assertion-killed mutants**, independently of line and branch
coverage. A small successful compatibility spike is evidence that the tools work;
it is not a project-wide score. `plan.md` tracks the remaining test and gate work.

## Toolchain

Mull 0.38.0 is the selected stable engine. Its published Linux compiler plugin is
built against LLVM 20.1.2. The pinned LLVM 20.1.8 compiler was qualified against this
plugin using actual Qt Test assertions. The host's Clang 23 cannot load that plugin.
The installer includes matching LLVM coverage tools so coverage builds use the same
compiler family without substituting decision coverage for branch coverage.

```sh
python3 tools/install_mutation_toolchain.py --prefix /path/to/native-tools
. /path/to/native-tools/env.sh
```

The installer verifies fixed SHA-256 hashes for 13 official Debian package artifacts
and extracts them into a user-owned prefix with `bsdtar`. It installs nothing into
the OS and needs no root access or container. It generates `env.sh` and
`toolchain-manifest.json`, including every artifact URL/hash and verified tool version.
This artifact set is for Linux x86-64 with glibc 2.38 or newer. Runtime dependencies
not bundled by the installer remain host requirements; unsuccessful tool version
checks fail installation rather than silently accepting a partial toolchain.

The fixed LLVM package revision is
`20.1.8~++20250804090239+87f0227cb601-1~exp1~20250804210352.139` from
[apt.llvm.org's noble LLVM 20 repository](https://apt.llvm.org/noble/).
It includes Clang, the LLVM shared library, Clang common/runtime packages, linker tools,
`llvm-cov-20`, and `llvm-profdata-20`. Supporting libedit2, security-updated libxml2 `2.9.14+dfsg-1.3ubuntu3.10`,
and ICU 74 `74.2-1ubuntu3.1` come from
[Ubuntu noble's official archive](https://archive.ubuntu.com/ubuntu/).
The Mull engine and LLVM 20 plugin come from the
[Mull 0.38.0 release](https://github.com/mull-project/mull/releases/tag/0.38.0).
Checksums are literal reviewed inputs in `tools/install_mutation_toolchain.py`.

## Build and campaign

Use a separate build directory. Source the native environment before configuring,
and put the qualified CMake/CTest installation on PATH. Keep normal, sanitizer,
coverage and mutation output directories separate.

```sh
. /path/to/native-tools/env.sh
MULL_CONFIG="$PWD/mull.yml" cmake -S . -B /tmp/qtscrobbler-mutation-build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DQTSCROBBLER_MULL_PLUGIN:FILEPATH="$QTSCROBBLER_MULL_PLUGIN"
MULL_CONFIG="$PWD/mull.yml" cmake --build /tmp/qtscrobbler-mutation-build --parallel 4
ctest --test-dir /tmp/qtscrobbler-mutation-build --output-on-failure --no-tests=error

# Initial measurement; a command success here does not mean the 95% target passes.
python3 tools/run_mutation_tests.py --build-dir /tmp/qtscrobbler-mutation-build \
  --report-dir /tmp/qtscrobbler-mutation-baseline --baseline \
  --coverage-summary /path/to/fresh-coverage/summary.json

# Enforced score; this exits nonzero when fewer than 95% are assertion-killed.
python3 tools/run_mutation_tests.py --build-dir /tmp/qtscrobbler-mutation-build \
  --report-dir /tmp/qtscrobbler-mutation-release \
  --coverage-summary /path/to/fresh-coverage/summary.json
```

The campaign discovers native test executables through CTest JSON. It also mutates
the actual `scrobbler` executable through `test_cli`, and `qtscrob` through `test_gui`,
so application entry points are not silently omitted. If a test does not launch
the actual application process, those application's mutants remain undetected. Python
quality-tool tests are recorded as outside the native production mutation scope.

`mull.yml` enables all standard C++ mutation groups plus logical negation, across
maintained first-party library, CLI and GUI `.cpp`/`.h` sources. Tests, generated Qt
files, Qt/system headers and vendored xgetopt are outside the production path filter.
Windows-only sources not compiled on Linux remain unmeasured and must be reported as
such. There are no blanket logging exclusions and no equivalent-mutant suppressions.

## Coverage filtering correction

The qualification investigation reproduced a Mull 0.38.0 coverage-filter defect:
a mutation at historical `libscrobble.cpp:243` was reported uncovered, but activating
that exact mutant produced 20 normal Qt Test assertion failures. An unchanged
Mull runner executed and recorded it as assertion-killed when LLVM coverage-mapping
sections were removed from a private copy. Historical campaigns using that filter
are retained as historical diagnostics, not trusted classifications or release scores.

Mutation builds now retain profiling but omit `-fcoverage-mapping` throughout the
build. This makes the official runner execute **every generated mutant**, preserving
the full denominator and avoiding coverage-based skipping. Source line and branch
coverage still use a separate LLVM build with mappings enabled. Trusted LLVM export
positions classify mutants independently of their observed execution result; missing
or ambiguous mappings are reported as unknown and never removed from the denominator.
The pipeline passes the fresh coverage summary into mutation qualification and verifies
matching source/test/tool fingerprints plus export/profile digests.

## Timeout calibration

The current runner defaults to `--mutant-timeout-ms 15000`. It passes that value to
Mull's `--timeout` for original warmup/baseline execution and to `--minimum-timeout`
for mutant execution. Mull 0.38.0 actually chooses the mutant limit as
`max(minimum-timeout, 10 * measured baseline duration)`; `--timeout` alone is not a
fixed per-mutant deadline. Each campaign manifest records this policy and the
selected limits, while per-group logs retain the warmup and baseline timings.

On 2026-11-17, the 57-row Scrobble suite passed three repeated isolated LLVM 20/Qt
6.12 runs in 3,109 ms, 3,085 ms and 3,080 ms (wall times 3.116 s, 3.091 s and
3.087 s). The native GUI wrapper allows its child workflow up to 12 seconds to
report a hung startup/close through a normal Qt Test assertion. A 15-second floor
allows that assertion to complete even when the normal GUI baseline is short;
Scrobble's approximately three-second baseline gives an automatically larger limit.
Recalibrate after material suite, platform or load changes. Do not count a deadline
expiry as a behavioral kill.

Evidence for those repetitions is retained under
`/home/mpetrick/.cache/qtscrobbler-work/scrobble-timeout-calibration.json` and its
three adjacent logs. These are calibration observations, not a full mutation score.
The historical frozen client 0.13.39 campaign predates the new floor: it passes
`--timeout 5000` and leaves Mull's minimum at its default 30 ms, with automatic
baseline scaling. Its raw timeouts remain failures to detect; rerun timeout cases
with the documented floor before using that campaign for final qualification.

## Honest scoring and integrity

Mull's generic `Failed` status can include process launch failures and signal deaths.
The default Mull score also treats timeouts differently from this project's policy.
`tools/mutation_score.py` reads the actual Mull 0.38 SQLite schema and counts a kill
only when a normal nonzero process exit is accompanied by captured Qt Test `FAIL!`
and a failing test total. Fatal Qt output is not accepted as an assertion kill.

- Score is proven assertion kills divided by unique generated mutants.
- Duplicate source mutants across executables count once; a proven kill by any one
  test executable detects the mutant for the combined suite.
- Survived, uncovered, timeout, crashed and unexplained exit results count against
  the score. A dry run, invalid result, unknown schema/version, launch/wait failure,
  or unproven fail-fast result fails the campaign's integrity check.
- Scores are compared from integer counts and exact decimal thresholds; display
  rounding cannot promote a score below 95% into a pass.
- `--include-not-covered` includes generated mutants on unexecuted production lines.
  Reports retain each raw status, not only the score.
- Every report directory must start empty. The manifest fingerprints source files,
  config, CMake cache and application/test binaries, and rejects changes during a run.
- Default tests use a disposable HOME/XDG directory and offscreen Qt. Per-mutant and
  per-command timeouts are bounded. Do not run a measurement on a heavily loaded machine.

Inspect or re-score saved reports without repeating the mutation campaign:

```sh
python3 tools/mutation_score.py /path/to/reports/*.sqlite --minimum 95 \
  --output /path/to/reports/rescored-summary.json
python3 tests/test_mutation_score.py
```

When fixing a survivor, retain its source/operator/location/replacement and reproduce
the changed behavior against the original and the mutant. Add a boundary or exact
contract assertion, then repeat the targeted mutant and full relevant suite. True
equivalent mutants require a reviewed reason and a separate exclusion count; none
are currently approved. Never remove a high-risk module to improve the score.

## Measured results

This section is append-only. New evidence adds a dated row and preserves old results.

| Date | Scope | Result | Interpretation |
| --- | --- | --- | --- |
| 2026-11-17 | Compatibility spike: real `CommonTest` plus `src/lib/common.cpp`, `<` to `<=` and `<` to `>=` | 2 generated, 2 assertion-killed; original passes; no timeouts/crashes | Proves native Clang 20.1.8/Mull 0.38.0/Qt 6.12.0 compatibility; deliberately limited scope, **not** the full project baseline |
| 2026-11-17 | First broad native source snapshot, client version 0.13.30; all configured native tests and both application executables; MTP enabled | **83 / 1,396 unique mutants assertion-killed (5.945558739255014326647564470%); 95% target FAIL** | Initial baseline, not completion. Snapshot has 5 CTests and predates added storage/iPod tests. 112 raw timeouts counted against detection; no infrastructure errors or equivalent exclusions |

The spike's exact failures were `chronologicalOrdering`: reversal made
`entry_less_than(earlier, later)` false; an inclusive comparison made equal timestamps
incorrectly compare as earlier. SQLite rows contained Qt Test assertion output and
normal exit status 1 in both cases. The example MTP branch was disabled in this first
spike; the whole-project campaign must build MTP support and retain unhit mutants.

The accepted broad campaign ran from 08:40:49 to 08:42:59 UTC on 2026-11-17. Its five
SQLite reports contain 4,574 raw executions: 119 `Failed`, 4,343 `Passed`, and 112
`Timedout`. Generic failures include signal deaths, so these raw counts are not a
kill score; captured assertion evidence and source-ID deduplication produce 83 kills.
The native `test_cli` contains no production mutations itself, so its linked-mutant
report is absent; a separate campaign against the actual CLI executable uses that
test program and contributes application mutants. The Python pipeline contract test
is recorded outside the C++ mutation scope. Unexecuted source mutants were included.

Evidence is retained locally at `/tmp/qtscrobbler-mutation-baseline-v3/`, with the
immutable source copy at `/tmp/qtscrobbler-mutation-snapshot/`. The completed
`manifest.json` SHA-256 is
`45306fd4e7f6d6fe77985afc1dc465c888d8bf0b1aba0b676af151fc072d3965`;
its source inventory fingerprints 39 `.cpp`/`.h` files, including platform sources
that are uncompiled on Linux. The mutation config fingerprint is
`2e5a89c13a0cf6832290533ba4ef4cd8a11534cd96738cb1fa6f1bb5815b9e48`.
These paths are working evidence, not portable published artifacts; attach/copy the
reports to retained CI/release storage before relying on them outside this machine.

Two preliminary campaigns were intentionally discarded: one treated the external
Python interpreter as a project ELF executable; another allowed generated moc paths
through the broad `/src/lib/` path match. The runner now requires test executables
inside the project build directory, and the config plus score reader reject generated
Qt paths. Only the corrected campaign's numbers appear as the broad baseline above.

Primary tool references:
[installation](https://mull-project.com/getting-started/installation/),
[compiler/plugin setup](https://mull-project.com/getting-started/setup/),
[reporters](https://mull-project.com/reference/reporters/), and the
[0.38.0 SQLite reporter implementation](https://github.com/mull-project/mull/blob/0.38.0/rust/mull-reporters/src/sqlite_reporter.rs).
