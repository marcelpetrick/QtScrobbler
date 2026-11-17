# Qt 6.12 and quality improvement plan

Status: implementation started; P0 in progress. Updated: 2026-11-17.

This is the living execution record. Every implementation commit must update this file with
what changed, evidence, remaining work, and blockers. Use small atomic commits. The first
commit contained only this plan and `AGENTS.md`; implementation has started.


## Owner updates and final product requirements

The owner subsequently authorized parallel subagents, continuous pushes after green checks,
and working directly on `main`. This supersedes earlier no-push instructions in this plan.
The original integration point remains `origin/master` at `c3fbd8f` for reviewing the initiative.
There was no `visions.md` in the initial checkout; the explicit conversation requirements
are the source for the vision record to be added. Do not invent additional product features.

- [x] Preserve existing GPLv2 license: LICENSE, COPYING and original headers already say version 2.
- [x] Work on local `main` (renamed from master without rewriting commit history).
- [ ] Record the stated product vision in `visions.md` and trace every requirement to evidence.
- [ ] Push green atomic conventional commits to main throughout the initiative; verify remote CI.
- [ ] Bump patch on every commit; bump minor for significant new product features. Run the gate on
      the prepared version before committing, rather than relying on a post-validation hook mutation.
- [ ] Do not add Docker, GHCR publication, container checks or Docker docs; the owner explicitly
      excluded Docker from the supplied generic workflow.
- [ ] Keep scripts small, reusable and documented; complete lint/format/static/coverage/build/E2E gates.
- [x] Add README badges grounded in actual CI/metrics and a real screenshot of the running Qt UI.
- [ ] Run the reviewBranch skill, fix verified issues, then run githubAbout; public About changes
      follow that skill's authorization rule and must not make unmeasured quality claims.
- [ ] Final vision audit, complete pipeline, actual green Actions, all work pushed, clean working tree.

Delegated work: CI/release pipeline and CMake provisioning; native mutation qualification;
configuration and database contracts. Primary agent owns shared integration, GUI coverage,
plan updates, exact-version gates, commits and pushes. No Docker is used for this work going forward.

## Objective and acceptance criteria

Upgrade QtScrobbler to a verified stable Qt 6.12 patch release, review and update its other
dependencies, and keep the CLI and GUI working. Reach **at least 95% line coverage and 95%
branch coverage separately** over maintained first-party production C/C++ on Linux, including
the GUI, CLI, and optional Linux MTP adapter. Coverage is a target, not a current claim.

Adopt mutation testing with a proposed **95% killed-mutant release gate** over the same
applicable production scope. This is a separate metric from coverage; establish a measured
baseline before enabling its final threshold. Do not copy Python-specific tools or score
definitions from the supplied myLastFmPlayer playbook into this C++ application.

Completion requires:

- [ ] Qt 6.12 is actually installed, selected, compiled against, and exercised locally and in CI.
- [ ] Exact tool/dependency versions and GitHub Action commit SHAs are recorded and reproducible.
- [ ] One local pipeline runs unchanged in CI; every mandatory failure causes nonzero exit.
- [ ] Deterministic offline Qt Test/CTest tests exercise parsing, storage, submission, CLI, and UI.
- [ ] Both raw line and branch percentages meet 95 without rounding up or shrinking the scope.
- [ ] A full mutation campaign meets the agreed threshold, with survivors and exclusions explained.
- [ ] Normal, coverage, sanitizer, and mutation builds are isolated from each other.
- [ ] Clean release builds, real GUI startup, fixture import, and packaged CLI checks pass.
- [ ] Hardware and live-service limitations are recorded honestly; simulated tests are labelled.
- [ ] Architecture, contribution rules, quality commands, dependency inventory, and reports are current.
- [ ] Evidence-based final review is resolved and the working tree contains no accidental artifacts.

## Investigation and baseline

Repository inspected at `c3fbd8f` on `master`; original working tree was clean.

| Item | Observed state | Consequence |
| --- | --- | --- |
| Build | C++20; CMake minimum 3.28; Qt minimum 6.8; `-Wall -Werror` | Keep warning enforcement; distinguish supported minima from exact CI pins |
| Targets | Static `scrobble`, GUI `qtscrob`, CLI `scrobbler` | All three need build and behavior checks |
| Tests | No test directory, Qt Test dependency, or CTest registration; `/usr/bin/ctest --test-dir build -N` reports 0 | Establish test infrastructure before claiming coverage |
| Existing smoke | Existing CLI `--help` exits successfully; GUI file executable | Neither establishes functional correctness |
| Existing binaries | CLI reports 0.13.11 while `common.h` declares 0.13.21 | Rebuild before using runtime evidence; prevent stale artifacts |
| Local Qt | qmake6 reports Qt 6.11.2 | Requested upgrade requires provisioning, not just editing a version string |
| Local tools | CMake 4.4.4, Clang 23.1.1, libmtp 1.1.23; gcovr is a development snapshot | Verify stable releases and install isolated, pinned quality tooling |
| Tool lookup | PATH `ctest` Python wrapper fails to import `cmake`; `/usr/bin/ctest` works | Check executable provenance; select consistent CMake/CTest installation |
| CI | Ubuntu 24.04, Qt `6.8.*`, major-only Action tags, apt-selected system libraries | Update and pin; qualify runner compatibility |
| Pipeline | CLI failure is WARN; failed unit suite is reported SKIP; unit tests do not fail pipeline | Fix fail-open behavior before relying on any new gate |
| Reports | CI's always-run report hardcodes successful stage results | Generate reports from actual stage outcomes |
| Sources | Approximately 5,862 C++ source/header lines including Windows adapter | Count executable instrumented lines, not physical lines, for coverage |
| MTP | Linux source only added if libmtp is auto-discovered; Windows adapter is not in CMake | Require explicit ON/OFF builds so missing libmtp cannot silently shrink scope |
| Translations | German/Polish TS catalogs; source-tree QM generation | Verify catalogs and runtime loading; move generated outputs into build tree |

Risk areas found by reading code (investigation leads, not a completed code review):

- `common.cpp`: path precedence and chronological comparison have boundary decisions.
- `parse-log.cpp`: real tab-separated format, timeless records, malformed lines, and clear/write behavior.
- `parse-ipod.cpp`: binary headers, offsets, truncation, decompression, and destructive play-count clearing.
- `dbcache.cpp`: fixed SQL connection name, global user path, wall clock, schema and SQL errors.
- `conf.cpp`: user settings, legacy migration, custom locations, and credentials need isolated storage.
- `submit.cpp`: batches of 50, percent encoding, retries, session expiry, cancellation, and reply ownership.
- `gettrackinfo.cpp`: QThread subclass with network manager ownership and event-loop/thread affinity.
- `libscrobble.cpp`: parser switching, time recalculation, status transitions, signal wiring, cancellation.
- `src/cli/app.cpp`: getopt state, error exits, conflicting options, and actual file-processing behavior.
- `src/qt/src/qtscrob.cpp`: table edits, selection, reentrancy, settings/progress lifetime, signal connections.

## Dependency review and upgrade strategy

Follow the `updateDependencies` skill: inspect all manifests, verify upstream stable releases,
pin exact resolutions, update deliberately, run the complete existing gate, and commit a
dependency change only when its gate passes. The owner now authorizes continuous green pushes.

Do not upgrade the user's OS globally. Provision Qt and quality tools under an isolated prefix
or a reproducible build environment. Keep runtime ABI dependencies supplied by Qt/OS visible
in the inventory, including SQLite plugin, TLS/OpenSSL, libusb, zlib, and graphics plugins.

| Dependency | Current selection | Investigated candidate / next verification |
| --- | --- | --- |
| Qt Core/Network/Sql/Xml/Widgets, tools and new Test module | CMake >=6.8; CI 6.8.*; local 6.11.2 | Qt 6.12.0 is announced released; 6.12.1 is scheduled for today, so confirm actual downloadable availability before selecting patch |
| CMake / CTest | >=3.28; local 4.4.4 | Upstream announcements list 4.5.0; verify official artifact/checksum and minimum Qt/tool compatibility |
| C++ compiler / gcov or LLVM tools | OS-selected GCC; local Clang 23.1.1 | Select a stable compiler/tool set supported by Qt and Mull; matching LLVM plugin ABI is mandatory |
| libmtp | Unversioned pkg-config; local 1.1.23 | Upstream release list reports 1.1.23; verify source checksum and packaged version |
| pkg-config/pkgconf and build backend | OS-selected | Record exact versions in reproducible environment, preserve reasonable consumer build minima |
| gcovr or LLVM coverage frontend | No project configuration | Prefer stable pinned gcovr with matched gcov, or llvm-cov with matched Clang; select after denominator spike |
| Mull | Absent | Verify stable release and matching Clang packages; evaluate on a tiny real Qt Test target before rollout |
| clang-format / clang-tidy / clazy / cppcheck | No project gates | Verify compatible stable versions; adopt incrementally with narrow documented suppressions |
| Doxygen / Graphviz / docs checker | Absent | Verify stable versions; use C++ API documentation rather than Python autodoc |
| checkout / upload-artifact / install-qt-action / action-gh-release | v4 / v4 / v3 / v2 tags | Verify each current stable release, runner/Node requirements, immutable SHA and installer support for Qt 6.12 |
| Qt installer and its Python dependencies, if used | Indirectly selected by Action | Pin installer plus transitive Python dependencies and artifact hashes |
| Legacy qmake and Windows SDK paths | Still present | Document compatibility status; no false Windows/macOS support claim |

Exact pins belong in one version inventory consumed by provisioning and checked by the pipeline;
do not confuse a CMake compatibility floor with the lock of the tested build environment.
Record observed version, selected version, upstream URL, checksum/SHA, rationale, and verification.

## Work packages and atomic commit sequence

Each numbered package may need several small commits. Never mix a formatting sweep, dependency
upgrade, and behavioral change. Each implementation commit updates the execution log below.

### P0 — Trust the gate and establish fixtures

- [x] Add a fictional, credential-free `examples/scrobble.log` accepted by the real parser.
      Explain timestamp units, tabs, format headers, and safe use on a copy; test fixture import.
- [x] Preserve initial baseline outputs and build from current sources, not pre-existing binaries.
- [x] Make CLI timeout, nonzero exit, and absent/failed unit suite fatal; preserve exact exit codes.
- [x] Use CTest `--no-tests=error`, bounded per-test timeouts, and truthful PASS/FAIL/SKIP summaries.
- [x] Add meaningful gate regression checks proving a failed test and timed-out command fail the pipeline.
- [x] Add Qt Test and CTest registration plus first contract tests; never claim that zero tests passed.
- [x] Add retained logs/report directory and stable headless settings; optional app launch remains optional.

Exit: at least one real parser test and one process test pass; deliberate failures make the gate fail.

### P1 — Reproducible dependencies and Qt 6.12

- [ ] Complete upstream inventory, patch availability, installer and compiler/Mull compatibility spike.
- [ ] Provision and lock toolchain; record hashes and exact resolved transitive dependencies.
- [x] Upgrade all used Qt modules/tools together; select prefix explicitly and verify runtime Qt version.
- [ ] Add CMake presets or equivalent separate release/debug/coverage/sanitizer/mutation directories.
- [ ] Add explicit optional-MTP selection; require dependency discovery when ON and test ON/OFF.
- [ ] Resolve Qt 6.12 compile/runtime regressions using characterization tests before fixes.
- [ ] Update CI and release dependency provisioning and immutable Action references.
- [ ] Correct install instructions and Qt minimum claims (CMake/SDK instructions updated); characterize or retire duplicate qmake path explicitly.

Exit: clean Qt 6.12 release build plus passing P0 tests, isolated GUI startup/close, translations,
SQL plugin loading, and library dependencies verified on the actual requested Qt version.

### P2 — Deterministic test support and minimal seams

- [ ] Add reusable temporary-directory/INI/SQLite fixtures, synthetic track factory, and log fixtures.
- [ ] Set test-only HOME/XDG settings before QApplication; prevent real credentials/cache access.
- [ ] Introduce narrow clock, storage path, network endpoint/reply, and MTP backend injection where needed.
- [x] Link GUI behavior into a testable target without duplicating `main()` or changing production semantics.
- [ ] Use QSignalSpy/QTest events and bounded QTRY assertions; avoid arbitrary sleeps and private-access hacks.
- [ ] Capture unexpected Qt warnings/fatal conditions; scope allowances for known platform messages.
- [ ] Provide loopback/fake HTTP replies and deny unintended external network traffic in default tests.
- [ ] Join threads, drain deferred deletion, close database handles and prove repeated construction works.

Exit: fixtures are isolated, repeatable, and work in randomized/repeated runs without touching user data.

### P3 — Core parser, configuration, and storage contracts

- [ ] `common`: ordering ties, valid/invalid paths, timed/timeless precedence, iPod DB variants, MTP selection.
- [ ] `parse-log`: exact parsed fields, Unicode/tabs, header/version, malformed/truncated records,
      played flags, empty input, timestamps/timezones, timeless logs, unreadable files, clear success/failure.
- [ ] `parse-ipod`: synthetic valid plain/compressed databases, play counts, missing files,
      lengths/offsets/endian boundaries, corruption, decompression limits, write/clear failure safety.
- [ ] `conf`: defaults, round trips, custom paths, invalid values, legacy migration, proxy/site settings,
      inaccessible paths, credential redaction in reports, preservation of existing settings.
- [ ] `dbcache`: schema creation/reopen, bound SQL with quotes/Unicode, replace, miss/hit,
      expiry boundary with fake clock, malformed schema, locked/read-only database, connection cleanup.
- [ ] `libscrobble`: parsing transitions, sorting/filtering, duplicate behavior, missing duration,
      timestamp recalculation/DST, cancellation, parser switching, per-site success/failure and retention.

Exit: exact outputs and error signals are asserted, boundaries included, regressions fixed in separate commits.

### P4 — Submission, networking, and device contracts

- [ ] Assert exact method, URL, headers, content type, UTF-8 percent encoding, authentication parameters,
      MusicBrainz ID behavior and 0/1/49/50/51/100/101-track batching.
- [ ] Simulate all implemented handshake/submission responses, session expiry, rejected credentials,
      malformed/empty/oversized response, network errors, retry bounds and cancel during flight.
- [ ] Assert tracks are neither lost nor marked successful on failure; retries do not duplicate success.
- [ ] `gettrackinfo`: XML success/missing fields/bad duration/error, elapsed-time contract,
      cancellation, manager/reply lifetime and thread affinity.
- [ ] `parse-mtp-libmtp`: fake backend discovery/open/read/write/free/cancel/error paths;
      retain at least one real-adapter link/integration check so mocks cannot replace the adapter.
- [ ] Real MTP/iPod device and live-service checks are opt-in, credential-safe, and separately reported.

Exit: default suite uses no live service/device and exercises real application logic behind test seams.

### P5 — CLI and GUI regression evidence

- [ ] CLI process tests: help, invalid/missing/conflicting arguments, file import and fake submission,
      configuration, exit codes, timeouts, diagnostics and source/runtime version consistency.
- [ ] GUI: construct window, open example copy, assert table fields/counts and action states,
      edit/delete/recalculate, settings save/cancel/reopen, console, help/about and progress/cancel.
- [ ] Assert each production signal reaches correct handlers; repeated opens/cancels do not double-connect.
- [ ] Verify translations through actual resource/catalog loading and placeholders, with a reviewed
      allowlist for intentionally untranslated dynamic/user/server text.
- [ ] Add reviewed stable widget/layout snapshots if valuable; exclude fonts/screen-derived geometry.
- [ ] Use golden images only for app-owned rendered output worth protecting; no brittle OS-style screenshots.
- [ ] Run headless and Xvfb/display-backed startup/close tests; repeat lifecycle tests with sanitizers.

Exit: the user-visible import/edit/submit/cancel workflow passes on Qt 6.12, beyond executable presence.

### P6 — Coverage accounting and 95% gates

- [ ] Spike GCC/gcovr versus Clang/llvm-cov against Qt-generated and exception branches; choose and pin one.
- [ ] Instrument all maintained production targets including CLI main and GUI; collect subprocess profiles.
- [ ] Include never-executed compiled production files in denominator; publish file inventory and raw totals.
- [ ] Exclude Qt/system headers, generated moc/uic/rcc, tests, and vendored xgetopt only with explicit scope.
      Windows-only source excluded on Linux is listed separately as unmeasured, not tested.
- [ ] Report raw compiler branch results and any justified normalized view separately; choose the gate's
      branch semantics before baseline and never relabel decision coverage as branch coverage.
- [ ] Register every exception/unreachable exclusion with file, reason, evidence and review; no blanket UI/MTP exclusions.
- [ ] Establish baseline; raise ratchet at verified milestones (e.g. 50/70/85/95), never lowering it silently.
- [ ] Enforce separate 95% line/branch thresholds from raw totals, so 94.995 does not pass by display rounding.
- [ ] Emit HTML, machine-readable coverage, JUnit and per-module gaps; enforce missing/stale reports as errors.

Exit: fresh complete report meets both thresholds locally and in CI; no excluded high-risk subsystem.

### P7 — Mutation testing and survivor elimination

- [ ] Prove Mull/Clang/plugin compatibility on a real QTest executable; deliberately alter a comparison
      and demonstrate its regression test fails while original passes. Select alternate only with evidence.
- [ ] Build isolated mutation targets with debug/source mapping; enumerate mutation scope and operators.
- [ ] Aggregate all relevant test executables with stable unique mutant identifiers; prevent double counting.
- [ ] Produce results for killed, survived, uncovered, timeout, crash, invalid and infrastructure-error states.
- [ ] Score = assertion-detected killed / valid generated mutants after reviewed equivalent exclusions.
      Survived/uncovered/timeouts/unexplained crashes count against score; infrastructure faults fail campaign.
      Report compiler-rejected mutants separately, not as successful behavioral kills.
- [ ] Establish baseline on a reasonably idle machine; bounded timeout calibrated from repeated baseline durations.
- [ ] Add exact contract/boundary/error tests for survivors; prove targeted mutant fails and original passes.
- [ ] Keep exclusions narrow: true equivalent/unreachable with rationale, no blanket logging-line exclusion
      that could hide state changes or credentials; publish excluded counts and diffs.
- [ ] Reach proposed 95% killed score; retain full campaign reports and tool/config/source fingerprints.
- [ ] Add `--mutation` to local pipeline; run full campaign before releases and scheduled/manual CI,
      with targeted changed-code feedback on ordinary work when feasible.

Exit: a reproducible full campaign passes score and integrity gates; scores from flaky timeouts cannot pass.

### P8 — Static checks, docs, and unified pipeline

- [ ] Pin clang-format; Google base, four-space indentation and 120-column limit as already requested in TODO.
      Separate mechanical formatting commit and enforce source/test checks afterwards.
- [ ] Add clang-tidy, clazy and cppcheck incrementally; verify findings before changing behavior,
      document narrow false-positive suppressions, zero findings on enabled rules.
- [ ] Add ASan/UBSan and leak/lifetime checks; TSan only where Qt/toolchain produces actionable evidence.
- [ ] Add Doxygen API docs with warnings fatal; document public contracts, ownership and signals.
- [ ] Create current/history documentation map, requirements, architecture/state diagrams,
      quality/dependency instructions and an append-only mutation results record.
- [ ] Add docs checks (required files, links, headings, whitespace, examples, version/source consistency).
- [x] `localPipeline.sh --noRun --noOpen --report-dir <path>` becomes CI's quality entry point.
      It checks tools, format, analyzers, translations, docs, build, tests, coverage, packaging,
      optional mutation and optional launch, with actual exit statuses and retained reports.
- [ ] Exercise deliberate failure in every gate; optional mutation absence must never look like a passed campaign.

Exit: one documented quality command provides truthful, reproducible results in local and CI environments.

### P9 — Release qualification and final review

- [ ] Build/package clean tree; install/extract artifacts into temporary prefix; check plugin discovery,
      shared-library dependencies, translations/resources, Qt/application versions and example workflow.
- [ ] Record real display session smoke and optional live/device tests; keep unperformed checks visible.
- [ ] Repeat tests for order dependence, Qt lifetime failures and flaky network/time assumptions.
- [ ] Review branch against the most recent merge-base among main/master/develop; read touched files
      and call sites; separately audit deleted/weakened guards and remaining invariants.
- [ ] Report only verified Code/Architecture findings in supplied severity/evidence/fix format; resolve findings.
- [ ] Attach test/coverage/mutation/static/docs/package evidence to release preparation.
- [ ] Wait for actual green CI if available; publishing/merging requires separate authorization; green pushes to main are authorized.

Exit: Qt 6.12 app works through documented workflows; both coverage gates and mutation gate pass with evidence.

## Red-team review of this plan

Trade-off: stricter branch and mutation accounting will take longer than reaching a headline line percentage.
These checks prevent the plan from declaring success by weakening its measurements.

| Attack / failure | Required defense and acceptance evidence |
| --- | --- |
| Change minimum Qt string but still build 6.11 or use stale binary | Clean prefix-selected build; compile/runtime Qt and client version assertions; provenance in report |
| Qt 6.12.1 scheduled but unavailable to installer | Probe real official artifacts and installer metadata; select released 6.12.0 until patch is downloadable |
| Upgrade LLVM beyond Mull's plugin ABI | Compatibility spike before locking compiler; no mutation score until actual mutants execute |
| A 95% number omits GUI, CLI, MTP, or untouched files | Explicit source inventory and zero-hit file accounting; MTP ON mandatory coverage job |
| Template/exception branches make 95% infeasible | Baseline compiler semantics early; publish raw and adjusted figures with justified exclusions; do not silently change metric |
| Tests snapshot buggy behavior | Review expected behavior against input/protocol; separate characterization from corrected regression contracts |
| Mocked submission passes while real wiring breaks | Real Qt event-loop/HTTP loopback and process tests plus packaged workflow |
| Timeout/crash kills artificially inflate mutation score | Do not count unexplained crashes/timeouts as detected; rerun unstable mutants and fail infrastructure errors |
| Equivalent-mutant exclusions erase meaningful checks | Per-mutant rationale and reviewed diffs; report excluded and invalid totals separately |
| CTest exit 0 with zero tests, stale report, or swallowed status | `--no-tests=error`, expected targets, fresh reports, negative gate tests and exit-code checks |
| Pipeline says PASS after failed CI stage | Generate summary from recorded outcomes, never hardcoded success text |
| Test corrupts user's log/settings/cache or submits real tracks | Test-only isolated paths and endpoints; example copy; external network blocked by default |
| Optional MTP missing silently reduces denominator | Explicit ON/OFF CMake option; fail ON if dependency absent; test both configurations |
| GUI snapshot depends on host fonts | Assert stable properties, keep display-backed workflow and offscreen tests separate |
| Qt callback failure aborts instead of reporting test | C++-appropriate message/crash handling and per-executable CTest isolation; no Python excepthook assumptions |
| Extensive refactoring breaks app before tests protect it | Characterization first, minimal seams, atomic fixes and clean release smoke after each package |
| Exact pins freeze vulnerable transitive libraries forever | Repeat dependency review; track runtime library inventory and compatible security updates |
| Reuse Python playbook mechanically | Qt Test, CTest, native sanitizers/Mull/Doxygen; adopt practices rather than Python-specific implementations |

Review verdict: executable as a staged initiative; the final target remains unproven until measured.
Tool availability, branch semantics, real device access, and mutation runtime remain explicit early spikes.

## Evidence and upstream sources

Checked 2026-11-17; recheck patch/tool versions when implementing rather than treating this list as a lock.

- [Qt 6.12 release schedule](https://wiki.qt.io/Qt_6.12_Release)
- [Qt 6.12 release announcement](https://www.qt.io/blog/qt-6.12-released)
- [Qt official 6.12 downloads](https://download.qt.io/official_releases/qt/6.12/)
- [CMake official announcements](https://discourse.cmake.org/c/announcements/8)
- [libmtp upstream releases](https://github.com/libmtp/libmtp/releases)
- [Mull setup and matching Clang plugin requirements](https://mull-project.com/getting-started/setup/)
- [Mull upstream releases](https://github.com/mull-project/mull/releases)
- [gcovr documentation](https://gcovr.com/en/stable/)
- [GitHub checkout releases](https://github.com/actions/checkout/releases)
- [Qt Test documentation](https://doc.qt.io/qt-6/qtest-overview.html)

## Execution log

| Change | Completed | Validation | Remaining / blockers |
| --- | --- | --- | --- |
| `a0c5a75` planning | Repository baseline, dependency candidate assessment, staged plan, red-team review, contribution rules | Inspected CMake/CI/pipeline, source risks and local tools; CTest listed 0 tests; existing CLI help passed | P0–P9 implementation; exact version lock and actual Qt 6.12 installation still pending |
| `aebf52a` example log | Added three fictional Audioscrobbler 1.1 tracks and safe-copy instructions | Rebuilt source targets; a temporary C++ harness using the real Parse_Log imported exactly three entries without network | Automated Qt Test contracts and strict pipeline remain P0 work |
| `68273f3` initial Qt Test suite | Registered parser, common/path-discovery and CLI-process tests; exact fixture fields, header errors, timezone rules, clear safety, version/help assertions | Full existing local pipeline passed; CTest 3/3 executables passed (0.13 s), Qt 6.11.2 | Strict failure propagation and coverage instrumentation remain; no 95% claim |
| Strict local gate | Fatal missing/failed CTest, fatal CLI error/timeout, preserved exit status, retained logs/JUnit, configurable build directory/jobs, noOpen flag | Full pipeline passed 4/4 CTest executables (6.38 s); 5 black-box pipeline tests deliberately exercise failures | CI still needs unified invocation; dependency qualification and further tests remain |
| `0df3efc` Qt 6.12 dependency upgrade | Raised CMake Qt floor to 6.12, pinned CI/release Qt 6.12.0 and four Action SHAs, resolved Python tooling lock, documented inventory; qualified CMake 4.5.0 | Official Qt SDK + ICU provisioned outside repo; full pipeline 4/4 passed on Qt 6.12.0/CMake 4.5.0; ldd resolves Qt to selected prefix; isolated GUI survived offscreen startup | CI not run; full environment/runtime-library pins, Mull-compatible Linux compiler and packaging still pending; GUI startup is not yet a full workflow test |
| `8c7ab63` parser signal regression fix | Removed duplicate derived signal declarations from log, iPod and MTP parsers so typed base-class connections receive emissions | New base-signal fixture assertion failed on original (0 completions instead of 1), then complete Qt 6.12 pipeline passed 4/4 after fix | GUI workflow, malformed record behavior, coverage/mutation targets still outstanding |
| Delegation authorization | Updated AGENTS.md to use subagents for independent work with explicit file ownership and primary-agent integration | Owner explicitly requested delegation and this rule commit; documentation diff checked | Parallel CI, mutation-tool and storage-test tasks to begin; GUI tests currently in progress |
| Updated owner requirements | Main branch, continuous green pushes, semver, no Docker, preserved GPLv2, final review/About/screenshot and vision audit added to plan and rules | Existing license files and source headers inspected; main renamed from master; missing visions.md confirmed | Complete native product/quality initiative and audit; delegated work active |
| `b1b8331` GUI workflow and screenshot | Shared production GUI library, real Qt import/display/edit/settings-reopen/clear/close contract, SQL/runtime Qt checks, real screenshot and capture script | Full Qt 6.12 baseline 5/5 CTest executables passed; targeted screenshot test passed; image visually inspected | More UI/error/cancel coverage, coverage measurements, full mutation campaign and other P2–P9 items remain |

| `cedda6a` unified CI/release pipeline | Both workflows invoke localPipeline; pinned isolated CMake installer, explicit Qt SDK, truthful logs/JUnit, source/tag and extracted artifact checks | Native actionlint/shellcheck and installer positive/negative/idempotency checks passed; initial main push had actual successful Actions | New workflow run and full final gates still required; storage/parser/mutation work continues |

| Clean CI translation dependency fix | Include qtdeclarative SDK runtime required by Qt Linguist; document clean-generation requirement | Actual CI cedda6a failed when clean lrelease could not load libQt6Qml.so.6; local cached .qm had masked this | Verify clean lrelease plus full local gate and replacement Actions run; no claim that failed CI was green |

| Configuration contracts | Custom-path and truthful load/save fixes; isolated defaults, full settings roundtrip, migration and error tests | Agent reproduced four failures on original and passed 12/12 Qt Test rows on fixed code; root Qt 6.12 pipeline passed 6/6 CTest executables | Database, iPod, Scrobble, networking and final quality targets continue independently |

| Database contracts and lifecycle | Isolated path/clock seams, unique SQL connections and cleanup, schema validation and 64-bit cache timestamps | Agent reproduced connection interference and invalid schema acceptance; 20/20 rows passed five repeated warning-free runs; root exact-staged-source Qt 6.12 gate passed 7/7 CTests (5.87 s), client 0.13.33; reports /tmp/qtscrobbler-database-staged-reports | Core orchestration, device/network/CLI/UI gaps and final coverage/mutation targets remain |

Discovered repository-local pre-commit hook auto-bumps `CLIENT_VERSION` on every commit.
The planning commit was amended without that hook to preserve its documentation-only scope.
For subsequent exact-version gates, execute the version hook before validation, then commit
with the already-executed hook disabled for that invocation to avoid a second bump.

Next actions: unify CI on the strict pipeline; add actual GUI import/close contracts and parser
error-path regressions; establish complete coverage accounting and qualify a Linux Mull toolchain.

| iPod corruption and clear safety | Validate every binary offset/length before publishing tracks, bounded streaming decompression, reject truncated counts and failed-reopen clearing, repeated plain/compressed reuse | 73/73 synthetic Qt Test rows, three repeat runs; targeted failed-reopen and truncation tests fail on original; root exact-staged-source Qt 6.12 gate passed 8/8 CTests; client 0.13.34; /tmp/qtscrobbler-ipod-staged-reports | Real hardware remains unverified; coverage and full mutation target still open |

| Submission protocol and lifecycle | Exact handshake/response recognition, bounded retries with accurate batch progress, owned managers/replies, cancellation and UTF-8 percent escaping | 30/30 loopback Qt Test rows; six original failures reproduced; root exact-staged-source Qt 6.12 gate passed 9/9 CTests (6.13 s), client 0.13.35; /tmp/qtscrobbler-submit-staged-reports | Live service compatibility remains opt-in; full coverage/mutation targets remain open |

| Metadata request identity and lifetime | Per-reply track identity/elapsed time, bounded XML duration parsing, owned default network manager, delayed-request generation cancellation and thread joining | 44/44 Qt Test rows with fatal warnings; root exact-staged-source Qt 6.12 gate passed 10/10 CTests, client 0.13.36; /tmp/qtscrobbler-trackinfo-staged-reports | Full orchestration and final coverage/mutation targets remain open |
