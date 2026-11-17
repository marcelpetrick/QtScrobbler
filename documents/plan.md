# Qt 6.12 and quality improvement plan

Status: P0 and broad behavior suites implemented; complete pipeline integration and final measured qualification remain open. Updated: 2026-11-17.

This is the living execution record. Every implementation commit must update this file with
what changed, evidence, remaining work, and blockers. Use small atomic commits. The first
commit contained only this plan and `AGENTS.md`; implementation has started.


## Highest priority owner requests — 2026-11-17 continuation

Execute these requests first, then finish the remaining initiative checklist. Quality
requirements remain acceptance conditions; public release must follow successful
qualification rather than bypassing an unfinished gate.

- [x] Record all new owner requests here, ahead of the remaining work.
- [ ] Run `updateDependencies` again for **all tools**: verify upstream releases,
      exact pins, SDK, native compiler/Mull, Python lock and GitHub Actions; record
      any deliberately retained compatibility pin with evidence.
- [x] Bump the version and commit the tested work. Prepared 0.13.62 already includes
      the executed version hook; validate that version before its atomic commit.
- [ ] Push the qualified work and run qualification **on GitHub via Actions**;
      inspect actual job conclusions and artifacts, fix failures and rerun.
- [ ] Cut a **public release**, explicitly authorized in this continuation, using
      the GitHub Actions release workflow after complete green qualification.
- [ ] Finish all remaining plan requirements and final evidence audit; ensure
      quality software through exact behavioral contracts, independent review,
      at least 95% line/branch coverage and native mutation evidence, sanitizer,
      extracted-package and real CLI/GUI workflow checks.
- [ ] Report the public release URL, actual Actions results, measured quality and
      final clean/pushed repository state. Never substitute targets for results.

Owner requests added 2026-11-17 evening (highest priority with the list above):

- [x] Fix all GitHub issues. Checked with `gh`: **0 open issues** (#1–#4, #6 closed),
      0 open pull requests, no code-scanning analysis; nothing to fix there yet.
- [ ] Get **every README badge green**: CI (latest `main` run), Release pipeline
      (has never run), Latest release (no release exists yet). Static badges
      (licence, Qt, CMake, C++20, platform, tests, style) only state facts; the
      coverage badges must match the newest measured report.
- [ ] **No new public failures**: run the matching local gate before every push,
      do not push mid-qualification (a push cancels the running CI run), and fix
      any red run before anything else.
- [ ] Publish the public GitHub release from a commit whose CI is observed green,
      by pushing tag `v<CLIENT_VERSION>`; report the actual release URL.
- [x] Ask the owner before deleting old failed/cancelled workflow runs. Approved;
      23 runs deleted (10 failed, 13 cancelled, all CI); 21 successful runs remain.
- [x] Clean our own temporary leftovers so full runs have space: `/tmp` (16 GB tmpfs)
      had filled and aborted the 0.13.69 full gate. Removed superseded builds,
      reports and duplicate toolchains in `/tmp` and `~/.cache`; kept only the
      in-use source snapshot and toolchains. Full runs now use
      `~/.cache/qtscrobbler-runs/<version>` on disk.
- [ ] Check that all documents are current: CHANGELOG gained the 0.13.69 entry;
      AGENTS and GitHub Actions updated. Pending: README status paragraph, branch
      badge (95.67% → measured 96.61%) and QUALITY/ACCEPTANCE_AUDIT results, once
      the local 0.13.69 mutation score is in.
- [x] Release without mutation testing (owner: "too much overhead"): a separate,
      small release pipeline that publishes only commits pretested by green CI.
      `release.yml` now requires a successful CI run on the tagged commit, then only
      builds, runs unit tests (`--quick`), packages, smoke-tests and publishes.
      Mutation stays mandatory in CI and the local full gate. Verified locally with
      GCC and no LLVM/Mull on PATH: quick gate **26/26 CTests PASS**.
- [ ] Publish `v0.13.69`. Its tag was pushed on `d07b90e` before this change, so the
      old full release workflow (run **44169110976**) is qualifying it in parallel with
      CI run **44167982834**; later releases use the lean workflow.

Current work (0.13.69, `d07b90e`): local full gate in
`~/.cache/qtscrobbler-runs/69` PASS through format, static, docs, sanitizers
(26/26) and coverage (lines 2722/2749 = 99.017824663514%, branches 1027/1063 =
96.61335841956726%); native mutation is running. CI **44167982834** and the old
full release run **44169110976** for tag `v0.13.69` are in progress. The
lean-release and documentation commit is held locally until CI finishes, because
a push to `main` cancels the running CI.

## Owner updates and final product requirements

The owner subsequently authorized parallel subagents, continuous pushes after green checks,
and working directly on `main`. This supersedes earlier no-push instructions in this plan.
The original integration point remains `origin/master` at `c3fbd8f` for reviewing the initiative.
There was no `visions.md` in the initial checkout; the explicit conversation requirements
are the source for the vision record to be added. Do not invent additional product features.

- [x] Preserve existing GPLv2 license: LICENSE, COPYING and original headers already say version 2.
- [x] Work on local `main` (renamed from master without rewriting commit history).
- [ ] Record the stated product vision in `visions.md` and trace every requirement to evidence.
      (`visions.md` recorded; per-requirement evidence audit remains open until final qualification.)
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
- [x] Add CMake presets or equivalent separate release/debug/coverage/sanitizer/mutation directories.
      Separate native builds exercised through frozen0.13.59; reports retained below.
- [x] Add explicit optional-MTP selection; require dependency discovery when ON and test ON/OFF.
      ON: frozen0.13.59; OFF: /tmp/qtscrobbler-resumed-mtp-off (18/18 CTests).
- [ ] Resolve Qt 6.12 compile/runtime regressions using characterization tests before fixes.
- [ ] Update CI and release dependency provisioning and immutable Action references.
- [x] Correct install instructions and Qt minimum claims; legacy qmake files explicitly
      unqualified in the historical port assessment, with CMake as the maintained build path.

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
- [x] Instrument all maintained production targets including CLI main and GUI; collect subprocess profiles.
- [x] Include never-executed compiled production files in denominator; publish file inventory and raw totals.
      Frozen0.13.59 coverage inventory has no missing production files.
- [ ] Exclude Qt/system headers, generated moc/uic/rcc, tests, and vendored xgetopt only with explicit scope.
      Windows-only source excluded on Linux is listed separately as unmeasured, not tested.
- [ ] Report raw compiler branch results and any justified normalized view separately; choose the gate's
      branch semantics before baseline and never relabel decision coverage as branch coverage.
- [ ] Register every exception/unreachable exclusion with file, reason, evidence and review; no blanket UI/MTP exclusions.
- [ ] Establish baseline; raise ratchet at verified milestones (e.g. 50/70/85/95), never lowering it silently.
- [x] Enforce separate 95% line/branch thresholds from raw totals, so 94.995 does not pass by display rounding.
      Integer gate contracts pass; both thresholds pass on frozen0.13.59.
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

- [x] Pin clang-format; Google base, four-space indentation and 120-column limit as already requested in TODO.
      Separate mechanical formatting commit and enforce source/test checks afterwards.
- [ ] Add clang-tidy, clazy and cppcheck incrementally; verify findings before changing behavior,
      document narrow false-positive suppressions, zero findings on enabled rules.
- [ ] Add ASan/UBSan and leak/lifetime checks; TSan only where Qt/toolchain produces actionable evidence.
- [x] Add Doxygen API docs with warnings fatal; document public contracts, ownership and signals.
- [x] Create current/history documentation map, requirements, architecture/state diagrams,
      quality/dependency instructions and an append-only mutation results record.
- [x] Add docs checks (required files, links, headings, examples, version/source consistency).
      Local Markdown links include moved and historical guides; compiler/source versions
      are checked through packaged entry points. Narrow validation scope is documented.
- [x] `localPipeline.sh --noRun --noOpen --report-dir <path>` becomes CI's quality entry point.
      It checks tools, format, analyzers, translations, docs, build, tests, coverage, packaging,
      mandatory native mutation and optional launch, with actual exit statuses and retained reports.
- [x] Exercise deliberate failure in every gate; missing mandatory mutation tools/reports fail.
      Pipeline fixtures exercise every stage failure; native semantic correctness has
      a separate preflight under implementation and remains a P7 qualification item.

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

| Import orchestration and CLI workflows | Deterministic parser completion, initialized sent flags, bounded argument validation, correct process exit status, metadata completion before submission and lifecycle cleanup | Qt 6.12 isolated CLI 64/64, Scrobble 43/43 and parser 25/25 rows pass; original comment-only log hung and missing-duration submission failed; root exact-staged-source Qt 6.12 gate passed 11/11 CTests, client 0.13.37; /tmp/qtscrobbler-orchestration-staged-reports-v3; sorted/reindexed metadata identities covered | Expanded GUI, complete scope coverage/mutation and full pipeline qualification remain |

| GUI workflow errors and modal lifetime | Queue completion/cancel transitions outside network callbacks; validate edits, persist custom site settings, release chooser/dialog ownership, validate GUI process arguments and window/clipboard/timezone flows | GUI 33/33, CLI 64/64 and Scrobble 43/43 isolated Qt 6.12 rows; original rejection dialog caused a Qt Network SIGSEGV and original custom-site save lost edited values; root exact-staged-source Qt 6.12 gate passed 11/11 CTests, client 0.13.38; /tmp/qtscrobbler-gui-expanded-staged-reports | Complete measured whole-scope quality gate, packaging/Actions and final audit remain |

| Settings migration preservation and safe defaults | Preserve old settings until destination sync succeeds; propagate legacy write errors; initialize every site disabled before any load/save | Both original migration regressions fail; Conf 19/19 rows pass, five ASan/UBSan/leak runs clean; missing-initializer original deterministically reports invalid bool19; root exact-staged-source Qt 6.12 gate passed 11/11 CTests, client 0.13.39; /tmp/qtscrobbler-conf-preservation-staged-reports | Native full gate and measured final coverage/mutation remain open |

| Actual GUI CI portability finding | Actions run 44069939057 failed only because the weak German help-process row selected a host locale absent on Ubuntu; ordinary help output was not a translation assertion | Read actual failed Actions log: 32 GUI rows passed, one failed on Qt locale warning | Replace with host-independent actual catalog tests; catalogs currently contain only vanished/obsolete translations and need restoration; do not claim translation completeness |

| Portable GUI process locale | Keep process option contracts in universally available C.UTF-8; remove a weak German help row that asserted English help and depended on an unprovisioned host locale | Original Actions 44069939057 failed on locale warning; real catalog translations are being restored and covered by independent QLocale/QTranslator assertions; root exact-staged-source Qt 6.12 gate passed 11/11 CTests, client 0.13.40; /tmp/qtscrobbler-locale-staged-reports | Actual replacement Actions and dedicated restored-catalog tests remain required |

| Reentrant submission teardown and cache availability | Disconnect/abort requests and defer manager destruction until Qt callbacks unwind; avoid writing metadata to an unavailable cache | Original direct controller deletion crashes in QtNetwork and cache failure warns; fixed Scrobble 57/57 and Submit 39/39 pass; managers released after DeferredDelete; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 11/11 CTests, client 0.13.41; /tmp/qtscrobbler-lifetime-staged-reports | Current full mutation baseline underway; complete quality gates and final Actions still required |

| iPod bounded block advancement | After `block_fits` validates status and the complete remaining block, advance with an unchecked in-buffer skip instead of re-checking (unkillable redundant guards); added valid-empty zlib stream and truncated dataset rows | Parse_Ipod 75/75 rows; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 11/11 CTests, client 0.13.42; reports in session scratchpad `ipod-reports`. Actual Actions run 44083427374 for `39f6e28` succeeded (2m9s) | Remaining uncommitted quality tooling (coverage, mutation, static checks, translations, MTP seam) still to be split into atomic commits |

| MTP backend seam and explicit selection | `QTSCROBBLER_MTP` option (default ON, libmtp REQUIRED via imported target when ON); injectable libmtp backend; release device lists on every error path, reopen without leaking, report allocation/unknown errors, buffer entries until a complete iteration succeeds | Fake-backend Parse_MTP 17/17 rows; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 12/12 CTests, client 0.13.43; CI already installs libmtp-dev | MTP OFF configuration still needs its own CI/gate run; real device behaviour unverified |

| Core test hardening | Fail on any Qt warning in common/dbcache/gettrackinfo; ordering boundary rows at INT_MIN/INT_MAX/timeless; clock-moved-back cache age; missing SQLite driver subprocess, retry after failed open, SQLite lock never overwrites an existing entry | Common 12/12, DBCache 24/24, GetTrackInfo 44/44; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 12/12 CTests, client 0.13.44 | Test-only; quality tooling, translations and GUI additions still uncommitted |

| Unreachable path-check code | Removed `break` statements after `return` and the unreachable trailing return in `check_path`; every switch label still returns | Root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 12/12 CTests without new compiler warnings, client 0.13.45 | None for this change |

| Per-build translation catalogs | lrelease is required from the selected Qt SDK and writes `.qm` into `<build>/translations`; GUI/CLI/test resources are generated per build directory, so parallel/instrumented builds never share or dirty source-tree catalogs | Root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 12/12 CTests, client 0.13.46; no `.qm` created under `src/language`; no remaining tracked reference to source-tree `.qm` | Catalog content restoration and dedicated translation tests follow separately |

| Restored translation catalogs | Re-extracted de/pl catalogs from current sources (223 active, 46 historical each, 0 unfinished); site/log-level labels resolve through the translator; runtime QTranslator test checks every active entry and placeholder multiset, host-locale independent | Translations 6/6 rows; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 13/13 CTests, client 0.13.47; pipeline counts 269 per catalog, 0 untranslated | No native-speaker linguistic review recorded; completeness is mechanical, not a quality claim |

| Product vision record | Added `visions.md` from the owner's explicit requirements; maps them to P1–P9 without inventing features | Root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 13/13 CTests, client 0.13.48 | Final per-requirement evidence audit remains open |

| Display-backed GUI process | Real `qtscrob` launched on a private Xvfb display; test finds its main window and requests a graceful WM_DELETE_WINDOW close; Xvfb added to CI/release provisioning; missing Xvfb/libX11 fails | gui 1.87 s and gui_process 0.22 s passed; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 14/14 CTests, client 0.13.49 | Actual Actions run for the Xvfb provisioning still to be observed |

| Pinned zlib 1.4 | Checksum-pinned official zlib 1.4 built static/PIC in a private prefix with upstream tests and manifest fingerprinting; CMake requires exactly 1.4; CI and release install it and export `ZLIB_ROOT` | Installer contract test passed; existing local prefix re-verified offline; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) linked `/tmp/qtscrobbler-zlib-1.4-qualified/lib/libz.a` (exact 1.4) and passed 15/15 CTests, client 0.13.50 | Actual Actions run with the zlib step still to be observed |

| Instrumentation build options | `QTSCROBBLER_COVERAGE`, `QTSCROBBLER_MULL_PLUGIN`, `QTSCROBBLER_SANITIZERS` (all OFF by default) instrument only production targets; mutually exclusive per build directory; LLVM coverage/Mull require Clang | Default root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 15/15 CTests, client 0.13.51 | Instrumented builds are exercised by the coverage/mutation/sanitizer runners committed next |

| LLVM coverage runner | `tools/run_coverage.py` builds a separate Clang coverage directory, merges profiles from all tests/subprocesses, counts integer line and branch totals over the first-party denominator (listing unexecuted and Windows/vendored unmeasured files) and fails below 95% each unless `--baseline` | Accounting contract test passed; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 16/16 CTests, client 0.13.52 | No coverage percentage is claimed by this commit; measured runs and pipeline integration follow |

| Native mutation tooling | Hash-checked LLVM 20.1.8 + Mull 0.38.0 installer, `mull.yml`, campaign runner and SQLite score reader counting only proven Qt Test assertion kills (timeouts/crashes/uncovered/invalid/infrastructure retained separately, source-ID dedup, fail-closed on unknown schema) | Score-accounting contract passed; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 17/17 CTests, client 0.13.53. Historical first broad snapshot (client 0.13.30): 83/1,396 assertion-killed = 5.95%, **95% FAIL** | Fresh full campaign on current sources not yet run; survivors remain; pipeline integration follows |

| Static and documentation check tooling | `tools/run_quality_checks.py` (clang-format/clang-tidy/clazy/cppcheck, Doxygen with fatal warnings, required-doc/link/whitespace checks, extracted-package checks) with retained evidence; pinned clang-format 23.1.5 / clang-tidy 23.1.4 wheels; Google-based 4-space/120-column `.clang-format`; Doxyfile; API contract document | Checker contract test passed; root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 18/18 CTests, client 0.13.54 | Not yet wired into localPipeline/CI; tree-wide formatting/analyzer results unmeasured in this commit |

| Session checkpoint 13:50 | Split the previously uncommitted work into gated atomic commits (`c46cd8f`…`2464fec`); dropped an order-only tests/CMakeLists diff | Actual Actions run 44085608557 (`4f5d2d3`, cumulative through zlib/Xvfb/i18n) succeeded; later commits pushed, Actions pending at checkpoint | **Still uncommitted, one unit:** `localPipeline.sh` complete/`--quick`/`--baseline` modes, CI/release LLVM/Mull/analyzer provisioning, `tests/test_pipeline.py`, README/AGENTS/DEPENDENCIES/QUALITY/ARCHITECTURE docs. Blocker for committing green: the new default gate enforces 95% coverage and mutation, which are unmet (last mutation snapshot 5.95%); decide CI mode (`--baseline` measurement until targets pass) and run a fresh full baseline before committing |

| CI Xvfb startup fix | Actual Actions run 44086044676 (`63671cf`) failed: the Xvfb workflow ran twice concurrently (standalone `gui_process` plus nested in `test_gui`) and the nested copy hit the 5 s startup bound. Removed the duplicate nested run; startup bound 20 s with the Xvfb log in the error; CTest timeout 40 s | Root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline) passed 18/18 CTests, client 0.13.56; gui+gui_process repeated 3× in parallel (`-j4`) passed | Replacement Actions run must be observed green |

| Tree-wide clang-format | Mechanical formatting only: pinned clang-format 23.1.5 with the committed Google/4-space/120-column style over all 52 C++ files in src/lib, src/cli, src/qt/src and tests; `--dry-run --Werror` now clean | Root exact-staged-source Qt 6.12 gate (`--noRun --noOpen`, pre-quality-stage pipeline, GCC `-Werror`) passed 18/18 CTests, client 0.13.57 | Format stage enforcement is wired in with the pipeline integration commit |


| Current-state review and continuation | Read the complete plan and pending integration; reviewBranch resolves main/origin/main at dd8c51c (empty committed diff), supplemented by owner-requested working-state and initiative reviews. Verified unrelated-prefix overwrite and conflicting pipeline modes; fixes and regression checks implemented | Installer offline native fixture: 6/6 tests, 12 scenarios; pipeline mode test in progress. Latest pushed Actions 44094748009 (dd8c51c) observed successful. Fresh baseline /tmp/qtscrobbler-resumed-baseline-1: normal Qt 6.12 build and 18/18 CTests passed, static/docs PASS; sanitizer/coverage/mutation/package pending | Final all-stage gates and public release remain required; no threshold claim or final verdict yet |
| Documentation and presentation continuation | Owner requested more badges modelled on local myLastFmPlayer/Cullendula READMEs, a pleasing project, root cleanup and updated links. Moved plan/vision/CI guide/port history/TODO/changelog/authors into documents; retained README, AGENTS, license and required root tool configuration. Added documentation index, review record and README technology/test/release badges | Inspected both reference READMEs locally; local Markdown links remapped, quality checker paths updated; complete docs gate pending | Add final measured quality badges only when corresponding current reports exist; preserve GPLv2 |

The owner explicitly authorizes pushing and triggering a public release **when done**.
All pipelines must be observed green; do not weaken coverage or mutation requirements to
publish. The living plan is now `documents/plan.md`; root `AGENTS.md` points here.

| Full diagnostic baseline interrupted truthfully | /tmp/qtscrobbler-resumed-baseline-1 ran normal build/tests, format, static/docs, sanitizer and coverage stages; mutation stopped after systemic filter defect confirmed | Normal 18/18 PASS; static/docs PASS. Sanitizer CTests: 17/18 (old corrupt-cache fixture expected checksum failure but new earlier prefix guard rejects safely; fixture corrected). Coverage tests 18/18 but final integrity check **FAIL** because CMake registration changed during collection. Raw diagnostic 2704/2731 lines and 1008/1053 branches; **not qualified evidence**. Interrupted mutation has no completed score; package was not reached | Rerun frozen full gate after integration; use isolated snapshots to prevent concurrent changes invalidating evidence |
| Native mutation false-uncovered investigation | Found completed historical .39 campaign at ~/.cache/qtscrobbler-work/complete-reports-39/mutation; rescored current reader. Independently proved Mull incorrectly skips detectable mutations due coverage filtering | Historical .39: 379/1554 assertion kills (24.388674%, not current). Exact .39 libscrobble :243 <→>= reports uncovered yet direct activation fails20 Qt assertions. Private binary with only mapping sections removed: official Mull executes same mutant and records status1/exit20, confirmed assertion_killed. Evidence /tmp/qtscrobbler-agent-mutation-investigation/no-map-target.sqlite | Correct mutation configuration to execute all generated mutants; retain full denominator and separate LLVM coverage. No threshold reduction or blanket exclusion |
| Explicit MTP OFF build | Configured separate /tmp/qtscrobbler-resumed-mtp-off with Qt6.12, pinned zlib and QTSCROBBLER_MTP=OFF, then built and ran CTest | 18/18 CTests PASS (12.59s); native MTP ON normal build previously18/18 PASS | Hardware remains unqualified; no claim that simulated tests exercise a device |

| GitHub About review | Applied githubAbout workflow to current code/docs/tests and actual public metadata | Current description still says Qt6.11 and topics include qmake; documented one recommended description, two alternatives, ten evidence-backed topics and swap candidates in documents/GITHUB_ABOUT.md | Proposal prepared; public metadata unchanged because no explicit About edit was requested |

| Corrected native mutation execution and independent classifications | Mutation configuration retains profiling but removes coverage mappings from production and all consumers, avoiding reproduced false-uncovered filtering. Every generated mutant executes. New trusted LLVM classifier keeps covered/uncovered/unknown unresolved positions separate without altering raw outcomes or score | Official Mull same exact historical :243 mutant changes from status8 to assertion-killed status1/exit20 when mapping sections absent.16 runner/scoring and9 classification contracts PASS; separate coverage and mutation fingerprint inventories now match and exports/profile are hashed | Fresh whole-scope campaign still required. No score improvement is assumed from targeted proof |

| Complete gate integration, presentation and review fixes (prepared client0.13.58) | Wired mandatory format/static/docs/sanitizer/coverage/native mutation/package stages into localPipeline and both Actions; diagnostic --quick/--baseline remain labelled. Fixed both verified review findings; restored package documentation/screenshots; moved root docs and updated all local links; captured readable1120px real running GUI screenshot | Exact-version incremental gate (`--quick --noRun --noOpen`) /tmp/qtscrobbler-integration-58-final-reports20/20 CTests PASS; isolated screenshot recapture3/3 PASS. Installer6/6, mutation reader16/16, trusted classifier10/10, docs checker8/8; shellcheck/diff whitespace clean. This is diagnostic qualification, **not** a passing complete gate | Fresh frozen full gate plus actual green Actions still required before push/release; full mutation score unmeasured after filtering correction |

Additional prepared0.13.58 checks: standalone mandatory documentation/Doxygen stage
PASS (/tmp/qtscrobbler-integration-58-docs); extracted package CLI/GUI/dependency
stage PASS (/tmp/qtscrobbler-integration-58-package), including shipped documentation
and the actual screenshot. These independent passes do not replace the full gate.

| Submission retry source identity (prepared0.13.59) | Retain the original parser's cleanup identity after failure/cancel while reporting import idle; restore it on retry, skip already-successful sites, discard it on replacement import/delete-all/destruction | Original fails POST/auth/cancel retry source-clear assertions and mixed-site retry. Prepared fix65/65 Scrobble rows PASS, including independent ASan/UBSan/leak run (/tmp/qtscrobbler-retry-fix/asan/full.log). New exact30s submission contract detects the specific <30→<=30 mutant while original passes | Root exact-version incremental gate pending; fresh frozen full gate then actual Actions/public release remain open. This source-clear defect predates the initiative and is a plan-driven behavior fix, not an introduced review finding |

Root prepared0.13.59 incremental gate (`--quick --noRun --noOpen`) PASS20/20 CTests
(/tmp/qtscrobbler-retry-59-reports), including65/65 Scrobble rows. Pinned clang-format
check PASS for all three changed C++ files. Existing CRLF is preserved in library
sources; `git -c core.whitespace=cr-at-eol diff --check` passes without treating the
retained line endings as trailing whitespace. Whole-source native gates remain next.

Independent review of the integrated retry diff found no verified introduced finding:
removed parser deletion is replaced by explicit retained-parser ownership; every
release/replacement path is covered, fully successful sites skip resubmission while
partially successful sites retry unsent tracks. Final native qualification is separate.

| Frozen0.13.59 qualification | Immutable git archive of20bd7f8, full default command (not quick/baseline) using explicit Qt6.12, native LLVM20 and zlib1.4 prefixes | Normal20/20 and ASan/UBSan/leak20/20 PASS; format/static/docs PASS. Valid source-fingerprinted coverage gate PASS:2722/2749 lines (99.017824663514%),1017/1063 branches (95.67262464722484%), no missing production files; HTML/JUnit/exports retained /tmp/qtscrobbler-frozen-59-full-reports. Separate extracted-package stage on same frozen binaries PASS /tmp/qtscrobbler-frozen-59-package | Whole pipeline interrupted at mutation because verified tool defect makes source replacements invalid; no full-green/release claim |
| Native void-call replacement defect | Fresh no-filter campaign produced Parse_Log signal-removal survivors despite exact QSignalSpy assertions. Direct activation reproduces25/25 passes; LLVM IR proves actual Invoke signal is untouched and generated cleanup destructor removed instead, with duplicate internal clones sharing public ID | Latest stable/main rechecked via official GitHub API:0.38.0 b383a52. Raw SQLite /tmp/qtscrobbler-frozen-59-full-reports/mutation; IR /tmp/qtscrobbler-conf-fallback-proof/parse-log.instrumented.ll | Native correction being built privately under updateDependencies workflow; no equivalent exclusion or false kill permitted. Need semantic sentinel and successful exact-mutant proof before full campaign |
| Boundary and diagnostic contracts (prepared0.13.60) | Added exact GUI timestamp/edit/MRU/settings/rejected-submit boundaries, repeated iPod/MTP count summaries, and isolated system-settings fallback contracts. Added evidence-linked line/branch README badges for last qualified coverage measurement0.13.59 | Isolated originals: GUI43/43, iPod77/77, MTP18/18, Conf20/20 PASS. Manual source mutants for timestamp boundaries and parser counters produce assertion failures; supplementary probes are not added to native scores. Whole-source incremental gate20/20 PASS (/tmp/qtscrobbler-boundary-60-reports) | Native tool correction, complete mutation threshold, actual green Actions, final audit and public release remain open |

Prepared0.13.60 root incremental gate PASS20/20 CTests (8.59s), reports
`/tmp/qtscrobbler-boundary-60-reports`. Supported individual checklist items above
are now marked against retained evidence; whole-package/final-CI/release criteria
remain open. Independent subagents resumed successfully to finish semantic-tool
validation, qualify a private native call-removal correction, and review boundary tests.

Independent .60 review found no verified defect in the new test contracts or measured
badges. Corrected stale plan wording: native mutation is mandatory in the default gate.
New semantic-toolchain script/tests remain a separate infrastructure change. The only
format correction joins the new Conf log assertion into the pinned formatter's form.

Prepared0.13.60 follow-up checks: pinned format PASS for all four changed C++ tests;
Conf rebuilt after format-only adjustment and its CTest PASS; documentation/Doxygen
stage PASS (/tmp/qtscrobbler-boundary-60-docs). Independent read-only review found no
verified finding. Existing sourceversion hook was already run before the20-test gate;
commit invocation disables only that already-executed hook.

| README-first and acceptance audit (prepared0.13.61) | Owner requested finishing README first, then pushing, and checking acceptance/E2E. Retained reference-style badges and actual screenshot; corrected exactzlib/qualifiedcompiler/private-prefix setup. Added acceptance/E2E labels and scenario/evidence map distinguishing real CLI processes, real GUI display smoke, in-process widget workflows and unperformed live/device checks | Prior20-test .60 gate PASS. Added missing QSignalSpy size assertions so removed signals yield ordinary test failures instead of later QList crashes; fixed native tool proves exact Parse_Log :134/:143/:145 mutants assertion-killed, original25/25PASS. Fresh .61 quick/acceptance/E2E/docs/package runs pending | Push README/test work after green diagnostic checks as requested; full mutation gate remains open and actual Actions must be inspected, with no complete-green claim until the corrected native dependency and threshold pass |

Prepared0.13.61 validation: incremental pipeline20/20 PASS
(/tmp/qtscrobbler-readme-61-reports); acceptance label5/5 PASS (7.32s),
process E2E label2/2 PASS (0.79s), with separate JUnit files
/tmp/qtscrobbler-readme-61-acceptance.xml and -e2e.xml. Documentation/Doxygen
PASS (/tmp/qtscrobbler-readme-61-docs); extracted-package version/dependency/
CLI/GUI startup PASS (/tmp/qtscrobbler-readme-61-package). Parse_Log formatting and
CRLF-aware whitespace checks PASS. Owner explicitly requested pushing the README-first
work now; these are green diagnostic checks, not full mutation qualification. The next
actual Actions run must be inspected and any full-gate failure remains open until fixed.

README-first work pushed successfully: main advanced dd8c51c→b79aca1. Actual Actions
run44130632157 was observed in progress, not claimed green. Acceptance/E2E documentation
and labels are public. Work continues on the native correction and full qualification.

Native correction progress: original official0.38 signal preflight genuinely fails;
private corrected compiler passes the exact missing-signal assertion. Native settings
fallback operators previously missed are now generated and assertion-killed by the new
Conf contract. The source-corrected shim and installer remain uncommitted dependency
work until full gates pass. A native LLVM CFG test and post-preflight artifact provenance
are being integrated, with four mutation workers and bounded longer campaign/CI limits.

| Native dependency and integrity integration (prepared0.13.62) | Pinned libirm/LLVM-header correction built in a separate prefix; ordinary/intrinsic/invoke/cleanup and shared-unwind-PHI CFG checks pass. Native Qt signal-removal preflight is mandatory; wrapper/shim/manifest/CFG/original LLVM inputs and every compiler command are bound to fresh raw proof and loader environment. Workflow provisioning and four-worker/7200s command/360min job bounds configured | Private real install/reuse /tmp/qtscrobbler-mull-callfix-qualified2 PASS (2257 entries); Qt sentinel1/1 genuine kill and3 real Parse_Log signal kills; Conf fallback2/2 kills. Installer10, semantic10, evidence14, reader16, pipeline12 contracts PASS. Independent review found loader-env mismatch acceptance; fixed and regression-tested. No dependency commit or full mutation-score claim yet | Register new contracts, run prepared exact-source full native gate and inspect actual Actions. Mutation95%, full dependency/environment audit, final review and release remain open |

The dependency correction now has a successful real installed-prefix CFG verifier,
Qt sentinel and recognized reuse, with all artifacts retained. One independent
MEDIUM review finding (loader environment not bound to proof) was fixed: changed
LD_LIBRARY_PATH or LD_PRELOAD now fails evidence validation;14 evidence contracts
pass. The coverage summary explicitly lists the build-only native correction and
CFG helper as separately qualified tooling; application Linux scope is unchanged.

## Restart checkpoint — 2026-11-17

Owner requested documenting state before restarting. No local full pipeline or
mutation campaign was running. No checks were rerun, commits made or releases
published during this checkpoint.

- Branch `main`, HEAD and last pushed commit `b79aca1` (client 0.13.61).
  `git push origin main` returned `Everything up-to-date`. README's 12 badges
  and real application screenshot are already pushed.
- Prepared client **0.13.62** is staged in `src/lib/common.h`. The version hook
  already ran for this prepared version; do not bump again merely to resume its
  validation. Commit tested content with only that already-executed hook disabled,
  as AGENTS.md specifies.
- Actual Actions run **44130632157** for `b79aca1` was checked again and remained
  `in_progress`, without a conclusion. Recheck
  <https://github.com/marcelpetrick/QtScrobbler/actions/runs/44130632157> after restart.
- Checklist accounting: 24 complete / 73 open (97 total). This is not an estimate
  of implementation effort. Some open requirements have historical or partial
  evidence and need an explicit evidence audit before being checked off.

Pending changes, all uncommitted:

- New `tools/install_mull_callfix.py`, `tools/check_mutation_toolchain.py`,
  `tools/mutation_toolchain_evidence.py`, `tools/native/` and three Python test
  files implement private native correction, semantic preflight and provenance
  validation. `tests/CMakeLists.txt` now registers those three contracts.
- `localPipeline.sh`, `tools/run_mutation_tests.py`, `tests/test_pipeline.py` and
  `mull.yml` integrate mandatory preflight, evidence checks, four workers and longer
  campaign bounds. Both workflows provision the correction and allow 360-minute jobs.
- `src/lib/CMakeLists.txt` separately switches libmtp linkage to `PkgConfig::LIBMTP`;
  include this build adjustment in exact-source qualification and atomic review.
- `tools/run_coverage.py` lists build-only native tooling separately from the
  unchanged application denominator. README setup, AGENTS and dependency/Actions/
  mutation/quality documents describe the pending correction. README's remaining
  local diff concerns setup instructions, not unpublished badges.
- Preserve every tracked and untracked file. Pushing HEAD has not published this
  pending dependency/infrastructure work.

Retained evidence directories were confirmed present:

- `/tmp/qtscrobbler-frozen-59-full-reports`: historical 0.13.59 coverage PASS,
  2722/2749 lines and 1017/1063 branches; normal and sanitizer suites 20/20 PASS.
  Whole pipeline did not pass: mutation exposed the native tool defect.
- `/tmp/qtscrobbler-readme-61-reports`: historical quick diagnostic 20/20 PASS;
  separate acceptance/E2E/docs/package evidence is recorded above. This is not
  complete qualification of prepared 0.13.62.
- `/tmp/qtscrobbler-mull-callfix-qualified2`: private installed correction. Its
  `env.sh` sources `/tmp/qtscrobbler-native-llvm20-patched/env.sh` and selects
  corrected CXX plus manifest/shim/source evidence. Targeted results are recorded
  above and were not rerun in this checkpoint. Temporary prefixes/reports may need
  reprovisioning after reboot.

Resume in this order:

1. Read AGENTS.md and this plan; inspect tracked/untracked changes and recheck
   actual Actions. Verify private SDK/tool/zlib prefixes. Apply updateDependencies
   to native dependency work; do not upgrade the OS globally.
2. Rerun targeted infrastructure contracts and confirm their CTest registrations.
   Freeze the entire prepared tree, including untracked tooling, into an isolated
   snapshot so concurrent edits cannot invalidate collected evidence.
3. Source the corrected native `env.sh`; set `QTSCROBBLER_QT_PREFIX`, `ZLIB_ROOT`,
   `QTSCROBBLER_JOBS=4` and an isolated `QTSCROBBLER_BUILD_DIR`. Run
   `./localPipeline.sh --noRun --noOpen --report-dir <new-dir>` on frozen sources.
   Use fresh reports. Quick/baseline modes do not qualify final completion.
4. Investigate the actual campaign survivors and add exact contracts until at least
   95% proven assertion kills. Preserve unresolved outcome categories and scope.
   No trustworthy current full mutation score or 0.13.62 full-green result exists.
5. Finish dependency/environment and vision/checklist audits, lifecycle/workflow/
   package qualification, reviewBranch and verified fixes, then githubAbout.
   The existing About proposal is not an applied public metadata change.
6. Commit/push validated atomic work with plan evidence, observe actual green
   Actions, and finish clean-tree/package/release qualification under the owner's
   recorded authorization. Do not weaken thresholds to publish.

No user input is currently needed. The principal technical blocker remains a fresh,
semantically trustworthy full native mutation campaign reaching 95%; final review,
the complete gate and actual green remote CI also remain mandatory.

## Continuation execution — current owner priorities

- Recorded every continuation request at highest priority above. Public release and
  GitHub Actions execution are explicitly authorized, subject to the existing gates.
- Fresh `updateDependencies` audit checked all 31 Python resolutions and all four
  Action SHAs against upstream: no changes required there. Detailed SDK, native,
  host/runtime and Qt patch evidence is in
  [the fresh dependency audit](DEPENDENCY_REVIEW_2026-11-17.md). Private pkgconf
  3.0.11 and Graphviz 16.1.1 provisioning is underway; compatibility retentions
  and environment qualification remain explicit, not a completed all-tools claim.
- Independent native review found no verified defect: 24 semantic/provenance and
  10 installer contracts PASS; native CFG verifier PASS; fresh real Qt signal
  removal sentinel PASS at `/tmp/qtscrobbler-native-independent-review-62`.
- Current frozen snapshot `/tmp/qtscrobbler-resume-62-source`, qualified toolchain
  build `/tmp/qtscrobbler-resume-62-qualified-build`, new reports
  `/tmp/qtscrobbler-resume-62-qualified-reports`: normal **23/23 CTests PASS**
  (12.41s). Static/docs, sanitizers, coverage, mutation and package remain running
  or pending; this is not a full passing gate.
- Two startup attempts were stopped rather than used as qualification: the first
  encountered existing `.62` paths and overwrote normal configure/build logs before
  stopping (historical mutation files were untouched); the second selected the
  known broken user CTest because the CMake prefix lacked its nested directory.
  The current run verifies the actual sibling CMake/CTest 4.5.0 executables first.
  Preserve these failed attempts as failures, not fresh successful evidence.
- Review follow-up now makes localPipeline reject nonempty report directories
  before any stage, preserving historical evidence. Both Actions tee their outer
  log outside the fresh report directory, then retain it with an EXIT trap.
  A regression test verifies nonempty/hidden historical files remain untouched in
  every pipeline mode. Fresh checks and final exact-tree qualification are pending.
- New parser diagnostic contracts and auxiliary tool provisioning remain separate
  pending work, outside the running immutable snapshot. Every implementation
  commit still requires the prepared version, updated plan and appropriate gate.

Continuation measurements on the immutable .62 snapshot: static/docs PASS;
ASan/UBSan/leak **23/23 CTests PASS** (13.27s); coverage gate PASS with
**2722/2749 lines (99.017824663514%)** and **1027/1063 branches
(96.613358419567%)**, no missing production files. Full mutation remains running
and package has not yet run; these results do not qualify a release.
Random-order, three-repeat CTest run PASS (47.06s), retained at
`/tmp/qtscrobbler-resume-62-random-repeat.xml`. A separate network-namespace
probe passed22/23; display smoke failed because inherited abstract session DBus
is inaccessible in the fresh namespace. A private session bus is being qualified;
do not suppress the unexpected warning or call that probe green.

Private auxiliary tools pkgconf3.0.11/Graphviz16.1.1 installed and reuse verified:
429 upstream pkgconf tests, actual Graphviz SVG/plugin contract and9 installer
contracts PASS, `/tmp/qtscrobbler-aux-tools-current-reports3`. Both Actions now
provision this exact selection and CTest registers its installer contracts.
Separate application qualification is still pending. Parser diagnostic assertions
prove six exact previously surviving signal-removal mutants now assertion-killed;
original25/25 rows PASS, evidence `/tmp/qtscrobbler-parser-survivor-review/proof`.

Atomic parser/audit commit qualification (client0.13.62): staged-only source
snapshot `/tmp/qtscrobbler-parser62-index-source` ran the authorized incremental
`--quick --noRun --noOpen` gate; **20/20 CTests PASS**, reports
`/tmp/qtscrobbler-parser62-index-reports`. This commit contains the exact parser
diagnostic assertions and audit/priority records, not the pending native-tool,
auxiliary/Python dependency or network-isolation implementation. The prepared
version hook had already run before testing; only that hook is disabled for the
commit. These diagnostic checks authorize an incremental push under AGENTS.md;
they do not establish the full gate, mutation95% or release readiness.

Parser/audit commit **eaea9ac** (client0.13.62) pushed to main. Actual Actions
**44150860625** concluded **failure** after 2m34s: `tools/install_zlib.py` reported
`zlib source SHA-256 mismatch` while provisioning, before the pipeline ran. A fresh
local download from zlib.net and the GitHub release asset both returned 1507140 bytes
matching the pinned SHA-256, so the CI response was a transient bad transfer.
Client **0.13.63** makes the installer try the two official locations of the same
pinned artifact, record every attempt (URL, HTTP status, size, actual/expected
digest) in `downloads.json`, report digests in errors and keep an explicit
`--archive` mismatch fatal. `zlib_installer_contract` **13/13 PASS**; incremental
`--quick --noRun --noOpen` gate on the exact staged snapshot
`/tmp/qtscrobbler-63zlib-source` PASS, **20/20 CTests**, reports
`/tmp/qtscrobbler-63zlib-reports`. Diagnostic subset only; no remote-green claim.

The interrupted .62 whole-scope mutation campaign
(`/tmp/qtscrobbler-resume-62-qualified-reports`) stopped at 20:29 while running
its tenth `scrobble` campaign, when the hosting session ended; nine module
reports exist but there is no summary, so it yields **no** mutation score and
remains incomplete diagnostic evidence only. Final mutation qualification must
run on the final tree.

GUI resource fix (client **0.13.64**): new exact main-window assertions (layout,
table, buttons, icons, menus/actions; MTP entry follows `HAVE_MTP`) exposed a real
defect. The generated build `qtscrob.qrc` replaced `resources/...` with absolute
paths without an alias, so every `:/resources/icons/...` fallback and the window
icon were missing from the shipped GUI (masked only where a desktop icon theme
supplies `fromTheme`). CMake now keeps the original resource name as an alias.
Proof: the same test on the unfixed CMake fails at `!control->icon().isNull()`
(`/tmp/qtscrobbler-64orig-build`); fixed, `test_gui` **44/44 PASS** and every
bundled icon path resolves. Incremental `--quick --noRun --noOpen` on the exact
staged snapshot `/tmp/qtscrobbler-64gui-source` PASS, **20/20 CTests**, reports
`/tmp/qtscrobbler-64gui-reports`. Actions **44159701792** for `7fee3db` was
observed in progress; no remote-green claim.

Quality infrastructure/dependency commit (client **0.13.65**). Refreshed
interpreter and bootstrap qualification: private CPython3.15.2 official source
hash and recipe in [Python toolchain](PYTHON_TOOLCHAIN.md). Six CPython suites
PASS (2027 run,92 skips); all31 locked quality packages install, pip check and
real7-Zip roundtrip PASS; source wheels rebuilt without build isolation against
six exact latest bootstrap/build backend pins. Project Python contracts
**123/123 PASS** under selected3.15.2 (`/tmp/qtscrobbler-63-python-contracts.log`).
Both Actions select exact setup-python7.1.0 SHA and Python3.15.2, install the
bootstrap lock, then the quality lock without floating build backends.

Independent auxiliary review reproduced host-plugin acceptance via GVBINDIR and
fixed it before publication: explicit conflicting overrides fail before writes;
actual private Graphviz loaded paths are checked. **13/13 installer contracts**
and real reuse/override probes PASS. Network isolation exposed a real
escaped-grandchild cleanup defect; the final `tools/run_offline.py` wrapper uses
user/network/PID namespaces, signed exit-status forwarding and kernel subtree
teardown. **11/11 standalone and 11/11 nested contracts PASS**; CTests PASS
through private-bus/PID isolation with no host portal activation. These are
targeted infrastructure checks, not a full application qualification.
Incremental `--quick --noRun --noOpen` on the exact staged snapshot
`/tmp/qtscrobbler-65infra-source` under Python3.15.2 PASS, **26/26 CTests**,
reports `/tmp/qtscrobbler-65infra-reports`. Diagnostic subset only: the full
final gate (static, sanitizers, coverage, native mutation95%, package) and
observed green Actions remain open.

Full local gate on exact `c8c4693` (client0.13.65, `/tmp/qtscrobbler-65-final-reports`)
**FAILED two stages**; build, 26/26 CTests, format, static, docs, coverage,
mutation-toolchain preflight and package PASS. (1) Sanitizers: the new PID
namespace kept the host `/proc`, so LeakSanitizer could not read
`/proc/<pid>/task` and aborted every CLI child at exit. Client **0.13.67**
adds `unshare --mount-proc`; a new contract checks `/proc/self` equals the
namespace PID. The escape test used to read PIDs *inside* the namespace and kill
them on the host. With a private `/proc` those would be unrelated host PIDs, so
it now finds descendants on the host by a unique token in their `-c` script.
Proof: without `--mount-proc` the `/proc` contract fails; without `--kill-child`
the escape test fails (escapees hold the pipe) and cleanup leaves 0 processes.
(2) Mutation: `run_mutation_tests.py` fingerprinted `requirements-bootstrap.txt`
but `run_coverage.py` kept its own list without it, so every full run rejected
the coverage evidence ("fingerprints differ"). Client **0.13.68** moves the scope
into one shared `tools/source_fingerprint.py` used by both runners; a contract
asserts both import the same function and that the bootstrap lock is included.

Actions **44161814413** on `c8c4693` concluded **failure**. All provisioning
passed (including the zlib fallback). Two CI-only causes, besides (1)/(2):
(3) Ubuntu 24.04 AppArmor blocks unprivileged user namespaces
(`unshare: write failed /proc/self/uid_map`), so CTest, sanitizers, coverage,
mutation and package failed before any test ran. (4) Ubuntu's Clazy 1.11
(Clang 15) cannot parse GCC 14 libstdc++ `<format>`; this already failed static in
44130632157, and the earlier green CI runs were only `--quick`. Client
**0.13.69** sets the sysctl and probes the exact namespace set in both workflows,
and adds `QTSCROBBLER_CLAZY_EXTRA_ARGS` (Clazy-only arguments, contract-tested)
which the workflows use to select GCC 13 headers via `libstdc++-13-dev`. The
Clazy fix cannot be reproduced on this Arch host without Docker (excluded), so
it is verified only by the next observed Actions run.
