# Qt 6.12 and quality improvement plan

P0 and the broad behavior suites are implemented. C++23 is qualified, main CI
is green and v0.15.13 is the latest release. The 80% mutation goal is met
(1390/1734, 80.16%, Mull 0.39.1) and enforced. The owner's live test scrobble
and the final completion review remain open. Updated 2026-11-28; see "Remaining work" below.

This is the living execution record. Every implementation commit must update this file with
what changed, evidence, remaining work, and blockers. Use small atomic commits. The first
commit contained only this plan and `AGENTS.md`; implementation has started.

## Remaining work — 2026-11-28 (resume here)

State: `main` at 0.15.15 equals `origin/main`; CI is green through 0.15.14
(46089541735). v0.15.13 is the latest release, verified. No open issues or
PRs. Complete gate: 99.02% lines, 96.64% branches, 1390/1734 (80.16%)
assertion-killed with Mull 0.39.1, ratchet 80%. Every dependency is at its
latest stable release (2026-11-27 review).

Needs the owner:
1. Live Last.fm test scrobble (vision acceptance): with real credentials in
   the GUI settings, open a small log with a few recent plays, submit, and
   confirm the tracks on the Last.fm profile. Record the date, client
   version and outcome here. Never automate it or store the credentials.

Recommended next work (no owner input needed):
2. Windows `gui` timeout: Release run 46041421879 hit the 20 s CTest limit
   once without output (CI and the rerun passed). If it recurs, run
   `test_gui -v2` on the runner to name the hanging function, then fix
   or bound the wait in that test.
3. Mutation above 80%: 344 unresolved mutants in
   `~/.cache/qs-gate-0.15.11/reports/mutation/summary.json` (qtscrob.cpp
   91, libscrobble.cpp 54, parse-ipod.cpp 47, submit.cpp 24,
   parse-mtp-libmtp.cpp 21, cli/app.cpp 18, gettrackinfo.cpp 13, conf.cpp
   13, console.cpp 12, qt/main.cpp 12). The cheap kills in small files are
   used up; most of what is left is equivalent `close()`/`sync()` and
   initial-value mutants. Use the batch method from 0.15.9/0.15.10 (prove
   each kill, list equivalents with reasons). Raise `MUTATION_MINIMUM` only
   to a measured integer floor.
4. AppImage: it dumps core under `QT_QPA_PLATFORM=offscreen` (v0.15.4 too)
   because only xcb/wayland are bundled. Bundle `libqoffscreen.so` with
   `EXTRA_PLATFORM_PLUGINS` if headless smoke tests of the AppImage are
   wanted; normal desktop sessions are unaffected.
5. Recurring: review Dependabot PRs and upstream releases (Qt 6.12.x/6.13,
   Mull, LLVM 20 line, Actions SHAs) with `updateDependencies`; run the
   complete gate before each release, and release only CI-green commits.

Local host housekeeping (not repository work):
- `~/.cache/qs-gate-*` holds 23 gate directories (35 GB). Keep the newest
  complete gate (`qs-gate-0.15.11`) as evidence; the rest can go.
- `~/.cache/qs-tools/native-tools` and `mull-callfix` are the Mull 0.38.0
  prefixes (`env-0.38.0.sh`, 675 MB); `env.sh` selects 0.39.1.
- `/tmp` is a 16 GB tmpfs that other projects keep nearly full; gates must
  keep using on-disk run directories.

## Session handoff — 2026-11-24

Resumed after a host reboot cleared the `/tmp` tools; all tools are now
re-provisioned on disk under `~/.cache/qs-tools` (see AGENTS.md). GitHub had
no open issues and one open PR (Dependabot #9, certifi), superseded by the
dependency commit. Each commit below was gated as an exact tree with the
complete `./localPipeline.sh --noRun --noOpen` before committing.

- `0.14.4` fix(tools): neutral installer User-Agent. Fresh provisioning
  failed with HTTP 403 from archive.ubuntu.com for Python's default agent
  (curl and a custom agent receive 200). New regression assertions in
  `mutation_installer_contract` and `zlib_installer_contract`.
  Gate: complete `./localPipeline.sh --noRun --noOpen` on this exact tree in
  `~/.cache/qs-gate-0.14.4` (reports/summary.txt; `EXIT=` in log).
- `0.14.5` build(deps): Qt 6.12.0 → 6.12.1 (official open-source release,
  build `6.12.1-0-202611180427`; 6.13 is pre-release only), CMake 4.5.0 →
  4.5.1, Graphviz 16.1.1 → 16.1.2, certifi/charset-normalizer/soupsieve/
  packaging updated; supersedes Dependabot PR #9. All five Linux SDK hashes
  equal the upstream sidecars. Reviewed and deferred: Mull 0.39.1 (changes
  the mutant set; the call-removal correction is bound to 0.38.0). Every other
  pin (Actions, linuxdeploy, Cppcheck 2.22.0, Clazy 1.17.1, zlib 1.4,
  pkgconf 3.0.11, libmtp 1.1.23, Python 3.15.2, LLVM 20.1.8) is current.
  Gate: complete `./localPipeline.sh --noRun --noOpen` on this exact tree in
  `~/.cache/qs-gate-0.14.5` (reports/summary.txt; `EXIT=` in log).
- `0.14.6` fix: Windows-portable log decoding and UTC conversion, found by
  the first Windows build (`timegm`/`tm_gmtoff` missing on MinGW;
  `parse_log` failed with `Bj\uFFFDrk ??`). The existing `parse_log` and
  `gui` recalculation tests cover both; `parse_log` failed before and passes
  after the fix on Windows.
  Gate: complete `./localPipeline.sh --noRun --noOpen` on this exact tree in
  `~/.cache/qs-gate-0.14.6` (reports/summary.txt; `EXIT=` in log).
- `0.15.0` feat(release): Windows x86-64 ZIP. Validated on the real runner
  from the temporary branch `ci/windows-validation` (runs 45410753831,
  45411322912 failed while porting the installer; 45412165069 passed the
  Windows job but failed Linux Format on the UTC expression; after the
  formatting fix 45436875712 passed both jobs). The
  gate covers 5 Qt Test suites; 6 suites remain test-side ports, measured in
  `documents/GITHUB_ACTIONS.md`. Release publishes AppImage, tar.gz and ZIP.
  Gate: complete `./localPipeline.sh --noRun --noOpen` on this exact tree in
  `~/.cache/qs-gate-0.15.0` (reports/summary.txt; `EXIT=` in log).

Pushed 0.14.4–0.15.0; main CI 45460038756 passed both jobs on `63fb7b9`;
PR #9 closed as superseded; `ci/windows-validation` deleted. Tag `v0.15.0`
points at `63fb7b9`. Its Release run 45476577898 built, tested and
smoke-tested the ZIP but failed before publishing anything: the Windows step
only matched CTest's old "N% tests passed, M tests failed out of K" summary,
and CTest 4.5 prints "100% tests passed out of 5".

- `0.15.1` ci(release): accept both CTest summary formats in the Windows
  step, as the Linux notes step already did;
  `release_workflow_contract` runs both steps' patterns on both formats
  (fails without the fix). Gate: `--quick` on this exact tree in
  `~/.cache/qs-gate-0.15.1` (workflow/test-only change on top of the
  complete-gated 0.15.0 tree).

Dispatched Release 45477856260 (workflow from `fe0ca6d`): the Windows job
passed; the Linux job stopped at the CI check before publishing. The API
query `ci.yml/runs?branch=main&head_sha=<sha>&status=success` returned
`total_count` 0 although run 45460038756 passed on `63fb7b9`; without
`status=` it returns that run, and `branch=main&status=success` listed a
week-old run first, so GitHub's status index lagged.

- `0.15.2` ci(release): query by branch and commit and select
  `conclusion == "success"` with jq; `release_workflow_contract` asserts no
  `status=` filter and runs the jq selection (fails without the fix). Gate:
  `--quick` on this exact tree in `~/.cache/qs-gate-0.15.2`.

Main CI 45477855417 (0.15.1) failed in "Install verified pkgconf and
Graphviz": the Graphviz download timed out (`The read operation timed out`);
the same pinned installer passed in 45460038756. The 0.15.2 run is the
confirmation run.

Release 45478989009 (workflow `f7ba755`) published v0.15.0:
https://github.com/marcelpetrick/QtScrobbler/releases/tag/v0.15.0 with the
AppImage, tar.gz, Windows ZIP and three `.sha256`; downloaded copies verify
with `sha256sum -c` (`~/.cache/qs-release-0.15.0`) and the AppImage prints
`QTScrob (v0.15.0)`. It also printed `Could not find the Qt platform plugin
"wayland"` in a Wayland session and fell back to xcb.

- `0.15.3` fix(release): bundle the Wayland platform plugin. linuxdeploy-plugin-qt
  deploys only xcb by default; `EXTRA_PLATFORM_PLUGINS=libqwayland.so` adds the
  plugin and `EXTRA_QT_MODULES=waylandcompositor` selects the deployer that
  copies `wayland-shell-integration`, `-decoration-client` and
  `-graphics-integration-client` (no compositor library is bundled). With only
  the first variable `libxdg-shell.so` was missing, which the new content check
  in `tools/build_appimage.sh` reported. A local AppImage built with the pinned
  tools in `~/.cache/qs-appimage-test` loaded the bundled Wayland plugin in a
  real Wayland session (`qt.qpa.plugin: Successfully loaded Qt platform plugin
  "wayland"`) and xcb with `QT_QPA_PLATFORM=xcb`; size 33 → 34 MB. Gate:
  complete `./localPipeline.sh --noRun --noOpen` on this exact tree in
  `~/.cache/qs-gate-0.15.3`.

0.15.3 gate: EXIT=0, every mandatory stage PASS; lines 2738/2765, branches
1037/1073, assertion-killed 1363/1749 (77.93%). Main CI 45478989326 passed
on 0.15.2.

- `0.15.4` docs: README badges/text and QUALITY.md measured results now
  cite the complete 0.15.3 gate. Gate: `--quick` on this exact tree in
  `~/.cache/qs-gate-0.15.4` (documentation-only on top of 0.15.3).

Main CI 45507621327 passed both jobs on `9204f77` (0.15.4); tag `v0.15.4`
on it ran Release 45519076056 (success), which published
https://github.com/marcelpetrick/QtScrobbler/releases/tag/v0.15.4 as the
latest release: AppImage, tar.gz, Windows ZIP and three `.sha256`. The
downloaded copies (`~/.cache/qs-release-0.15.4`) verify with `sha256sum -c`;
the AppImage bundles `libqxcb.so`, `libqwayland.so` and `libxdg-shell.so` and
in a real Wayland session loads the Wayland plugin and prints
`QTScrob (v0.15.4)`; the ZIP holds both executables and `qwindows.dll`.
No open issues or PRs.

## Session handoff — 2026-11-26 (resume here after the restart)

State at handoff: `main` pushed through `975174f` (0.15.7, goal 80%). Main CI
45837085475 (0.15.6) passed both jobs; CI 45857049890 (0.15.7) was still
running at handoff, so check it first with
`gh run list --workflow CI --branch main --limit 3`. 0.15.8 (this plan) is a
local commit; push it only when no main CI run is active. Latest release
remains v0.15.4 (no product change since). No open issues or PRs.

### Plan: mutation score to the 80% goal

Baseline (complete 0.15.3 gate, `~/.cache/qs-gate-0.15.3/reports/mutation/summary.json`):
1363/1749 assertion-killed (77.93%), 386 unresolved (381 covered, 5 uncovered;
249 survived only, 91 failed+survived, 32 with timeouts). 80% needs 1400,
i.e. **37 more proven kills** (or reviewed equivalent exclusions with reasons,
which also shrink the denominator).

Unresolved by file: `qtscrob.cpp` 96, `libscrobble.cpp` 74, `parse-ipod.cpp` 49,
`submit.cpp` 33, `parse-mtp-libmtp.cpp` 21, `cli/app.cpp` 18,
`gettrackinfo.cpp` 13, `conf.cpp` 13, `console.cpp` 12, `qt/main.cpp` 12,
`parse-log.cpp` 12, `settings.cpp` 10, `dbcache.cpp` 6, others 17. By operator:
`remove_void_call` 133, `replace_scalar_call_nz/zr` 97, `init_const` 34,
`assign_const` 33, comparison/increment operators about 90.

Order, cheapest proven kills first (library code with deterministic Qt Tests):
1. `parse-ipod.cpp` (49; 21 `init_const`): assert exact parsed fields and
   offsets of the iTunesDB fixtures, including the initial values the
   mutants change.
2. `libscrobble.cpp` (74): exact track-list state after remove/update,
   index arithmetic (`pre_inc_to_pre_dec`) and the assigned constants.
3. `submit.cpp` (33), `conf.cpp` (13), `gettrackinfo.cpp` (13),
   `parse-log.cpp` (12), `dbcache.cpp` (6): request bodies, persisted
   values and boundary comparisons.
4. Only then `cli/app.cpp` and the GUI files (mostly void-call removals of
   widget updates; review each for equivalence rather than forcing tests).

Per batch: list the targeted mutant ids, add exact tests, prove each mutant
fails and the original passes (AGENTS.md), run a complete detached gate
(`~/.cache/qs-tools/gate.sh <new dir> <tree>`, about 1.5 h) and record the
new count. Raise `MUTATION_MINIMUM` to the new measured integer floor in the
same commit; set it to 80 once a complete gate measures at least 1400/1749.
Expected effort: two or three batches, about one working day.

### Other open items

- Mull 0.39.1: approved by the owner on 2026-11-27, done in 0.15.11.
- Owner's live Last.fm test scrobble: still open, see "Remaining work".
- Final completion review: `reviewBranch` done (0.15.13), `githubAbout`
  applied with the owner's approval (2026-11-27).

### Mutation batch 1 (0.15.9)

Each targeted mutant was applied by hand to the source, rebuilt and run
against its suite (`~/.cache/qs-mut/prove.sh`); the original passes.
Killed by the new assertions:
- `parse-ipod.cpp:329` `mul_to_div` and `sub_to_add` (Play Counts header
  claiming more songs than the file holds).
- `libscrobble.cpp:302` `init_const` (30-day limit), `:393` and `:394` (earlier
  errors logged by `submit()`), `:59` `assign_const` (default stdout level).
- `submit.cpp:134` and `:145` (remove, nz, zr each), `:217`, `:262`
  (exact submission, time and handshake-error logs).
- `qtscrob.cpp:358` and the seven `setItem` calls of `loadtable()` (each
  cell is asserted to exist before its text is compared, so a missing cell
  fails an assertion instead of crashing), `:485` and `:490` `assign_const`
  (edits drop the MusicBrainz id).
- `missingtimeprogress.cpp:26`, `:27`, `:43` (fixed widths and size);
  `:42 setWindowFlags(Qt::Dialog)` is equivalent (QDialog's default).
Not killable by assertions: `parse-ipod.cpp:108` `eq_to_ne` makes the
inflate loop spin (timeout only); `--metadata_generation` in the new
`cancel_metadata()` is equivalent (any change invalidates the generation).
Refactors: removed the write-only `Scrobble::scrobble_method`, `have_mb` and
`Submit::submit_context::have_mb` (6 survivors were mutations of dead
state); six copies of the lookup-cancellation sequence became
`Scrobble::cancel_metadata()`. Both change the denominator; the complete
gate in `~/.cache/qs-gate-0.15.9b` measures the new score (the first run in
`~/.cache/qs-gate-0.15.9` was stopped to include the GUI tests).
Result: EXIT=0, every mandatory stage PASS, assertion-killed 1382/1734
(79.70%); 80% needs 1388.

### Mutation batch 2 (0.15.10)

Proven the same way (each mutant fails its test, the original passes):
- `help.cpp:26` `resize(800, 600)` and `:35` zero margins, `about.cpp:27`
  `setFixedSize(sizeHint())` (`helpAndAboutDialogGeometry`).
- `qtscrob.cpp:133` zero margins, `:134` and `:142` stretch of the outer
  columns only (`mainLayoutCentersTheButtonRow`).
- `qtscrob.cpp:517` `dt.setTimeZone(QTimeZone::utc())`: an edited time is
  UTC even when the process zone is Asia/Kolkata
  (`timestampEditsIgnoreTheProcessTimeZone`).
- `libscrobble.cpp:60` `log_print_level = LOG_INFO` to `LOG_ERROR`: the
  timezone probe child now opens an empty log and its stdout must contain
  the INFO line (`activeTimezone`).
Reviewed as equivalent (no test can observe them): `console.cpp:71`
`read_all_logs()` (the constructor's `setCurrentIndex` already reloads),
`:44` `setEditable(false)` (the default), `:81` `>` to `>=` (reading an empty
range), `settings.cpp:62` (an empty hash stays empty, a set hash is
overwritten with the mask), `:99` (the form already selects tab 0),
`progress.cpp:57` and `missingtimeprogress.cpp:38` (the layout already has the
dialog as parent), `parse-log.cpp:54`, `:112` and `:114` (overwritten flag,
comment header line, empty lines fail the field count), the `close()` calls
the destructors repeat, and the `cli/app.cpp` option resets
(`parse_cmd()` runs once per process).
Expected 1390/1734 (80.16%), so `MUTATION_MINIMUM` is raised to 80 in this
commit; the complete gate in `~/.cache/qs-gate-0.15.10` enforces it.
Result: EXIT=0, every mandatory stage PASS, assertion-killed 1388/1734
(80.05%, ratchet 80% PASS). The 80% goal is reached with Mull 0.38.0.
Main CI 45977856780 passed on 0.15.9.

GitHub About (owner-approved 2026-11-27, applied with `gh repo edit`):
"Qt 6.12 desktop app and CLI that imports listening histories from iPods,
MTP players and Audioscrobbler logs, lets you review and edit the tracks,
and scrobbles them to Last.fm or Libre.fm. Linux AppImage and Windows ZIP;
99% line coverage and mutation-tested. Fork of the original SourceForge
QTScrobbler." Topics: lastfm, librefm, scrobbler, audioscrobbler, ipod,
mtp, qt6, cplusplus, linux, mutation-testing.

### Dependency review and Mull 0.39.1 (0.15.11)

Owner approved the Mull upgrade on 2026-11-27. All 37 Python pins equal
PyPI's latest; Qt 6.12.1, CMake 4.5.1, the six Actions SHAs (latest
releases), linuxdeploy, Cppcheck 2.22.0, Clazy 1.17.1, zlib 1.4, pkgconf
3.0.11, libmtp 1.1.23, Graphviz 16.1.2, Python 3.15.2 and LLVM 20.1.8 are
current. Only Mull changes: 0.38.0 -> 0.39.1 (latest stable, 2026-11-19;
release asset digests pinned). 0.39 keeps libirm `dfaf966`, so the
call-removal correction builds unchanged; the gate's Qt semantic preflight
requalifies it against the 0.39.1 plugin. The SQLite schema only adds
`run_identity`. 0.39 caches results by default and keeps cached kills after
code changes, so both runner commands pass `--no-cache`. New mutants in
templates and default member initializers change the denominator.
Local prefixes `~/.cache/qs-tools/native-tools-0.39.1` and
`mull-callfix-0.39.1`, environment `~/.cache/qs-tools/env-0.39.1.sh`;
complete gate in `~/.cache/qs-gate-0.15.11`.
Result: EXIT=0, every mandatory stage PASS (including the Qt semantic
preflight of the correction with the 0.39.1 plugin); lines 2725/2752,
branches 1037/1073, assertion-killed 1390/1734 (80.16%). 0.39.1 generated
the same 1,734 mutants as 0.38.0 for this code. `~/.cache/qs-tools/env.sh`
now selects 0.39.1; the 0.38.0 environment is `env-0.38.0.sh`.

Release v0.15.10 (tag on `28f687a`, main CI 46000830248 green): the first
attempt of Release run 46041421879 failed before publishing, because the
Windows `gui` suite exceeded its 20 s CTest timeout without output. The same
commit passed it in 6.35 s on CI, and the failed job's rerun passed. The
release https://github.com/marcelpetrick/QtScrobbler/releases/tag/v0.15.10
is the latest, with the AppImage, tar.gz, Windows ZIP and three `.sha256`.
The downloaded copies (`~/.cache/qs-release-0.15.10`) pass `sha256sum -c`,
the AppImage prints `QTScrob (v0.15.10)` in the Wayland session and the ZIP
holds both executables and `qwindows.dll`. Under `QT_QPA_PLATFORM=offscreen`
the AppImage dumps core, and so does v0.15.4, because the AppImage does not
bundle the offscreen plugin.
Open: find the cause of the single Windows `gui` timeout if it recurs (run
the suite with `-v2` to name the hanging function).

- `0.15.12` docs: README badges and text, QUALITY.md and MUTATION_TESTING.md
  cite the 0.15.9–0.15.11 complete gates. Gate: `--quick` on this exact tree
  in `~/.cache/qs-gate-0.15.12` (documentation-only on top of 0.15.11).
  Main CI 46041425045 passed on 0.15.11 (Mull 0.39.1).

Completion review (`reviewBranch`, 06ef181..d08dfde): one LOW finding, no
blockers. The removed `scrobble_method`/`have_mb` fields had no readers;
MusicBrainz ids are still sent per entry (`submit.cpp:116`).
`cancel_metadata()` repeats the former three steps, and `--no-cache` sits in
the shared runner prefix (discovery and execution). Finding: the time-zone
guard restored an unset TZ as `TZ=""` (UTC) for later tests outside CTest.
- `0.15.13` test(gui): restore an unset TZ with `qunsetenv`; the test still
  kills `qtscrob.cpp:517` (re-proven). Gate: `--quick` on this exact tree in
  `~/.cache/qs-gate-0.15.13`.

Main CI 46070938586 (0.15.12) and 46079592027 (0.15.13) passed. Tag
`v0.15.13` on `51836fc` ran Release 46088538330 (success, first attempt),
which published https://github.com/marcelpetrick/QtScrobbler/releases/tag/v0.15.13
as the latest release: AppImage, tar.gz, Windows ZIP and three `.sha256`.
The downloaded copies (`~/.cache/qs-release-0.15.13`) pass `sha256sum -c`;
the AppImage prints `QTScrob (v0.15.13)` in the Wayland session, the tarball
holds `qtscrob` and `scrobbler`, the ZIP both executables and `qwindows.dll`.
Remaining: only the owner's live Last.fm test scrobble (real credentials).
- `0.15.14` docs: record this release. Gate: `--quick` on this exact tree in
  `~/.cache/qs-gate-0.15.14`.

### Completed this session

- `0.15.6` test(windows): port the six unported Qt Test suites and gate the
  Windows package on all eleven. Causes and ports per suite are in
  `documents/GITHUB_ACTIONS.md`; the shared isolation is
  `tests/test_environment.h` (INI settings paths into the test home, legacy
  registry cleanup opt-in via `QTSCROBBLER_TEST_WINDOWS_REGISTRY=1`, `\r\n`
  normalization, `QT_QPA_FONTDIR`). Validation branch run 45835640630 on
  Windows Server 2025: conf 21/21, dbcache 24/24, scrobble 65/65, cli 61/61,
  gui 46/46, translations 6/6 (before: 15/6, 23/1, 48/17, 9/52, 30/16, 5/1
  passed/failed). Linux: the six suites pass unchanged. Gate: `--quick` on
  this exact tree in `~/.cache/qs-gate-0.15.6` (test/CI-only change on top of
  the complete-gated 0.15.3 product tree); main CI runs the complete pipeline.

Still open: 80% mutation goal (77.93%, 37 more kills of 1749; owner lowered it from 95% on 2026-11-26), Mull 0.39.1 (deferred: changes the
mutant set and the call-removal correction is bound to 0.38.0), the owner's
live test scrobble (needs the owner's credentials), final completion review.
Windows test-suite porting is done.

- `0.15.7` docs/tools: owner lowered the mutation goal from 95% to 80%
  (README, AGENTS.md, QUALITY.md, MUTATION_TESTING.md, visions.md,
  `localPipeline.sh`, default `--minimum` of `tools/mutation_score.py` and
  `tools/run_mutation_tests.py`, release notes). `MUTATION_MINIMUM` stays 77%
  because the measured score is 77.93%; raise it to 80% once a complete gate
  measures at least 1400/1749. Historical entries keep their 95% wording.
  Gate: `--quick` on this exact tree in `~/.cache/qs-gate-0.15.7`.

## Session handoff — 2026-11-18T15:30+01:00

A new agent session resumed the previous session's work, which stopped at
12:36 with 5% of its usage window left. Entries are chronological; the
newest state is in the last bullets.

- At resume, remote main was `9d9a88c` (0.14.0). CI 44295096927
  failed before the gate because `libEGL.so.1` was absent. The pinned Qt
  archive checks all passed. No qualifying main run is active.
- Existing tag `v0.13.85` remains at `9bab0d9` with green exact CI
  44278965577 and green exact local reports in `~/.cache/qs-85full.b`.
  Release 44294766294 failed two contracts because `bsdtar` was absent.
  There are no public release assets from that run.
- Staged runner correction tree `491ced7970a23a362d899e97b7e0a3fa87e02609`
  retains the owner-authorized unreleased 0.14.0 version. It adds `libegl1`
  to CI/Release, `libarchive-tools` to Release, and checks out immutable
  workflow helpers from `github.workflow_sha` separately from the tagged
  application source. Actionlint passed; the staged diff was re-reviewed.
- **Interrupted gates, not evidence.** The first complete gate of that tree
  (`~/.cache/qs-140fix.a`) and the 0.14.1 gate (`~/.cache/qs-141full.b`,
  tree `e2b66a4`) both stopped at about 12:54 during the mutation stage
  without a verdict, when the previous session's process tree ended.
  Configure, build, static and docs had passed in both.
- **Complete gate passed (13:45).** The detached exact gate of tree `491ced7`
  at `~/.cache/qs-140fix.QwTPnh` ended `EXIT=0` with every mandatory stage
  PASS (format, static, docs, sanitizers, coverage, mutation toolchain,
  mutation, package). Coverage 2748/2775 lines (99.027%) and 1037/1073
  branches (96.645%); 1381/1775 assertion-killed mutants (77.802%, gate 67%).
  `run.sh` there records the exact environment.
- **Committed and pushed (13:10).** The fix is `0b5cf5f` (tree `491ced7`,
  0.14.0). Before pushing, the active gate had already passed on that exact
  tree: 27/27 CTests, coverage targets (`target_passed: true`), 27/27
  sanitizer tests, static and docs. That exceeds the `--quick` push minimum.
  The application source equals `9d9a88c`, whose complete gate passed in
  `~/.cache/qs-140full.a`. Main CI 44301468187 runs on `0b5cf5f`; the local
  mutation and package stages continue at `~/.cache/qs-140fix.QwTPnh`.
- **Main CI green (14:50).** CI 44301468187 on `0b5cf5f` succeeded
  (1h39m), so the public CI badge is passing again. `0b5cf5f` is now
  qualified for v0.14.0 by both exact CI and the complete local gate.
- **v0.13.85 release retry.** Dispatched from main as Release run
  44301500886 (`gh workflow run release.yml --ref main -f tag=v0.13.85`).
  Confirm its result, the exact tag `9bab0d9`, and all four public
  assets/checksums. Tag v0.14.0 on `0b5cf5f` only after CI 44301468187 and
  the local gate both pass, then verify that Release.
- **Badges.** CI, Release and Latest-release badges turn green only from
  those real runs. The corrected coverage/mutation badges and README text
  are part of the 0.14.1 follow-up below.
- **0.14.1 follow-up.** The 77% ratchet and release-note/report update is
  saved as `~/.cache/qs-141-ratchet-staged.patch`; its working-tree changes
  are preserved unstaged. After the runner fix is committed, restore a
  version-bumped 0.14.1 index on top and run a new complete exact gate
  (its earlier gate was interrupted). No v0.14.1 tag was requested.
- **v0.13.85 published (13:14).** Release run 44301500886 (dispatched from
  main, tag on `9bab0d9`) succeeded and published the first public release
  with four assets. Independent download: the tarball checksum verifies and
  the AppImage hash matches, but its `.sha256` names the absolute runner path
  (`/home/runner/.../dist/...AppImage`), so `sha256sum -c` fails for users.
  Cause: `release.yml` stripped `  dist/` from an absolute path. With the
  owner's approval the v0.13.85 AppImage `.sha256` was replaced by one naming
  the bare file (same hash `9d3ef81a…d314`); a fresh download of all four
  assets now passes `sha256sum -c`. The fix (unstaged, next commit after
  0.14.1) writes the bare file name, runs `sha256sum -c` in `dist/` before
  publishing, and downloads the published release to verify both checksum
  files again. New CTest `release_workflow_contract`
  (`tests/test_release_workflow.py`) executes the workflow's own checksum
  commands with absolute paths; it fails 2/3 on the old workflow and passes
  3/3 on the fix. actionlint 1.7.12 passed. Because a tag push runs the workflow of the
  tagged commit, publish v0.14.0 through a dispatch from main after this
  fix is on main, and cancel the automatic tag-push run before it publishes.
- **0.14.1 committed (15:30).** `de3544a` is the exact gated tree `90b96de`.
  Its detached complete gate (`~/.cache/qs-141.OmEYvd`) ended `EXIT=0` with
  every mandatory stage PASS: 2748/2775 lines, 1037/1073 branches and
  1383/1775 assertion-killed mutants (77.915%) against the new 77% ratchet.
- **0.14.2 checksum fix.** Release-workflow checksum fix plus
  `release_workflow_contract`; gated with an exact `--quick` snapshot before
  pushing (workflow, test and docs only). Then tag v0.14.0 on `0b5cf5f`,
  cancel the automatic tag-push run (old workflow), dispatch Release from
  main, and verify the downloaded assets with `sha256sum -c`.
- **v0.14.0 published (15:21).** Tag `v0.14.0` (annotated) on `0b5cf5f`.
  The automatic tag-push Release run 44321509274 (old workflow) was
  cancelled after 12 s, before publishing. Release run 44321624580,
  dispatched from main at `c10f621`, succeeded: tag/version/CI verification
  (CI 44301468187), extracted entry points with `sha256sum -c` in dist/, and
  the new post-publish download verification all passed. Independent
  download: both `.sha256` files verify; the tarball CLI (with Qt 6.12
  libraries) and the AppImage under Xvfb/xcb print `v0.14.0`. v0.14.0 is
  marked Latest; live badges show CI passing, Release passing and release
  v0.14.0, and main shows the measured 0.14.0 coverage/mutation badges.
- **Pushed.** `de3544a` (0.14.1) and `c10f621` (0.14.2, quick gate 28/28 at
  `~/.cache/qs-142q.UuVCic`) are on main; CI 44321463082 runs on `c10f621`.
- **Owner request: prepare a live test scrobble.** After the releases, set up
  an opt-in live check with the owner's Last.fm credentials and one
  deliberately chosen track, run from a temporary copy of the example log in
  an isolated HOME/XDG. It is labelled optional/live, never part of the
  default offline gate, and needs the owner present to supply credentials.
  Remind the owner if it is not done in the same session.
- **Host resources.** `/tmp` (16 GB tmpfs) was 94% full at 12:55, mostly
  other projects' agent scratch data; `/home` had about 20 GB free. Gate
  build, report and `TMPDIR` directories stay on disk under `~/.cache`.

## Latest continuation — 2026-11-18

- Hosted 0.13.85 tag release 44294766294 failed two installer contracts:
  the runner omitted libarchive-tools (bsdtar). No release or assets
  were published. Preserve the existing v0.13.85 tag on CI-qualified 9bab0d9.
- Hosted 0.14.0 CI 44295096927 on 9d9a88c failed Qt's strict ldd audit for
  libEGL.so.1. All five SDK archives passed their checksums. Ubuntu Noble
  provides the missing library in libegl1. CI and Release now provision it,
  and Release also provisions libarchive-tools. The release retry checks out
  its provisioning helpers from github.workflow_sha while retaining the
  verified tagged source and exact qualifying main CI for build and assets.
- Preserve the owner-authorized unreleased 0.14.0 version for this narrowly
  scoped runner correction. The hook has already prepared 0.14.1 for the
  separate measurement/ratchet follow-up; its patch is saved at
  ~/.cache/qs-141-ratchet-staged.patch. Reapply the bump after the 0.14.0 fix.
  Corrected 0.14.1 snapshot e2b66a4 continues at ~/.cache/qs-141full.b.
- After the fixed workflow reaches main, dispatch Release from main with
  tag v0.13.85. Confirm that its exact tag remains 9bab0d9 and that the
  four assets are public and their checksums verify. Qualify the corrected
  0.14.0 exact main commit before publishing v0.14.0. No force-push or tag
  deletion is needed. Full local runner-fix gate uses ~/.cache/qs-140fix.a.

- Exact local 0.13.85 complete gate **PASS** at ~/.cache/qs-85full.b.
  Normal and sanitizer suites passed 27/27, both coverage thresholds
  and package passed, and mutation killed 1384/1775 (77.97183098591549%).
  After verifying exact main CI 44278965577, pushed tag v0.13.85 on 9bab0d9.
  Release workflow 44294766294 is running. Confirm published assets and
  SHA-256 sidecars before advancing main to the separate 0.14.0 commit.
- Prepared the follow-up 0.14.1 gate and reporting commit. Exact reports
  from 0.13.85 main CI and the 0.13.85/0.14.0 local gates all pass a 77%
  mutation ratchet. Raise the ratchet to 77% and update the badge from the
  latest complete local run. The 65% absolute floor and 95% goal remain.
  Release notes now read both coverage thresholds from the CI report and
  still support historical single-threshold reports. Five release-note
  tests passed. The first full run at ~/.cache/qs-141full.a caught the stale
  pipeline contract that asserted 67 instead of 77. Preserve its failed
  CTest report. The corrected exact full gate runs at ~/.cache/qs-141full.b.

- At 10:45 CET main CI 44264412011 on ab41f20 concluded success with
  27/27 normal and sanitizer CTests, package and coverage PASS, and
  1193/1766 assertion kills (67.5537938844847%). Its quality and binaries
  artifacts were present. Pushed 9bab0d9 (0.13.85) after that run completed.
  [Exact 0.13.85 main CI](https://github.com/marcelpetrick/QtScrobbler/actions/runs/44278965577)
  passed at 12:18 CET. GitHub recorded 27/27 normal and sanitizer CTests,
  2748/2775 lines and 1037/1073 branches at the revised gates,
  1381/1775 assertion kills (77.80281690140845%), and package PASS.
  The quality-reports and binaries artifacts were both present. Tag only
  after the exact local complete gate finishes successfully.
- Both local exact gates passed normal tests, static analysis, docs,
  sanitizers, coverage (2748/2775 lines and 1037/1073 branches, no missing
  files), and the mutation-toolchain preflight. Native campaigns remain active.
  Local 0.13.85 tar and AppImage smoke and SHA-256 checks passed at
  ~/.cache/qs-pkg85.reCHSI, including the bundled xcb GUI window lifecycle.
  The separately prepared release-note improvement reads both coverage
  thresholds from the report. Five release-note contracts pass; the patch is
  unstaged and does not alter either running immutable snapshot.
- C++23 version 0.14.0 complete local gate **PASS** from exact tree
  cf55526c, committed as 9d9a88c. Reports: ~/.cache/qs-140full.a.
  The local native campaign killed 1385/1775 (78.02816901408451%) with
  exact candidate accounting. All mandatory stages including extracted
  package passed. GCC 16.2.1 separately passed 27/27 CTests in quick mode at
  ~/.cache/qs-140-gcc.c1WnNb. All 51 compiler commands selected C++23, and
  the language/library probe passed. Both binaries reported v0.14.0 with the
  exact Qt SDK. The GCC tarball and AppImage passed both checksums, both
  entry points, and a bundled-xcb window lifecycle at ~/.cache/qs-pkg140.CaVrED.
  Wait for the 0.13.85 release sequence before the 0.14.0 main push.

- Main CI 44264412011 on ab41f20 is still running. Preserve it and inspect the
  conclusion before pushing. GitHub has no published release.
- Exact 0.13.76 (534e642) and 0.13.80 (80af9d0) both failed sanitizer CTest
  gui_process (26/27). Xvfb reported BadWindow on X_SendEvent after the test
  resent WM_DELETE_WINDOW while the process was still in teardown. Both
  local runs stopped during mutation and lack final summaries. Preserve
  reports in ~/.cache/qs-76exact and ~/.cache/qs-80full. The supplemental
  0.13.76 run passed sanitizers but also stopped during mutation. Keep the
  failed exact candidate untagged. Verify the single-close fix with repeated
  sanitized GUI runs and qualify its replacement on the full local gate and
  exact-commit main CI before tagging. Keep that C++20 fix separate from 0.14.0.
- The exact staged 0.13.83 checkpoint tree 21d4b91 passed 27/27 with
  `./localPipeline.sh --quick --noRun --noOpen --report-dir <new-dir>`.
  Reports and logs: ~/.cache/qs-83q.90wr. Full qualification was skipped
  by the diagnostic mode. C++23 build and dependency changes are separate.
- Prepared C++20 replacement 0.13.84. The GUI smoke test waits for the mapped
  window, sends one close event and checks the resulting process exit. Sanitized
  offline repeats passed 30/30. A controlled 300 ms process-exit delay reproduced
  BadWindow in 3/3 original runs and passed 3/3 with the fix. Evidence is in
  ~/.cache/qs-gui-close-audit. Exact staged 0.13.84 tree c2a1e58 passed
  27/27 in quick mode at ~/.cache/qs-84.r1Ip/reports. Full qualification
  and matching successful main CI remain required before the release tag.
- Owner priority update: fix public CI and README badges first. The coverage
  target is now 99% lines, with the existing separate 95% branch gate. The
  absolute mutation floor is 65%; the current 67% ratchet remains above it.
  Carry the gate changes in a separate implementation commit and subject the
  eventual release commit to the revised gates.
- Prepared 0.13.85 with independent raw 99% line and 95% branch gates and
  strict unrounded comparisons. Eight coverage-tool cases pass. Preserve the
  67% mutation ratchet over the 65% owner floor. Release workflow normalizes
  manual and push events to exact tag refs and checks the peeled commit before
  looking up successful main CI. Actionlint passed for both workflow variants.
  Local checks rejected same-named branches, malformed or missing tags, and
  a version mismatch. README links use live CI, Release, release and license
  data. Coverage badges use the raw 0.13.80 counts, with the failed sanitizer
  stage disclosed. Mutation uses the latest complete local campaign, 1195/1766.
- Latest measured coverage from the 0.13.80 coverage stage was 2748/2775 lines
  and 1037/1073 branches. The LLVM 20 warning comprised 149 hash-zero duplicate
  records across 16 Qt-generated translation functions and five first-party
  getters. All had live positive profiles and all five getters remained in the
  coverage exports. The raw denominator is unchanged. The separate failed
  sanitizer stage prevents full qualification of that snapshot.
- C++20 release replacement is commit 9bab0d9 (0.13.85). Its exact staged
  tree ef67f45 passed 27/27 in quick mode at ~/.cache/qs-ci85.kZnA/reports.
  The complete local gate uses ~/.cache/qs-85full.b. Wait for run 44264412011
  before pushing main. Require a successful main CI run on 9bab0d9 and a
  complete local pass before pushing tag v0.13.85.
- Prepared 0.14.0 with its matching changelog and separate compiler/CI/docs
  tranche. The hook ran before preparing the authorized minor version. Run
  the complete gate with the private C++23 analyzers at ~/.cache/qs-140full.a.
  Keep the two release sequences separate and the prepared dependency change
  uncommitted until its complete gate passes.
- Monitor the five-hour usage window using the session's rate-limit metadata.
  If less than 5% remains, record the measured balance and reset time,
  active reports and processes, staged snapshot, remote CI, and next actions
  here and in AGENTS.md before the session is interrupted.

## Live handoff — 2026-11-18

- **Public CI has first priority.** `origin/main` is `ab41f20` (0.13.74).
  [CI run 44264412011](https://github.com/marcelpetrick/QtScrobbler/actions/runs/44264412011)
  has finished provisioning and is running the complete pipeline. The last
  completed main CI, 44193926851, failed the sanitizer Xvfb startup check.
  The public CI badge still reports failing, the Release badge has no status
  and GitHub has no published release. Preserve the active run. Inspect
  `gh run view 44264412011 --log-failed` first if it fails.
- **First release candidate.** `534e642` (0.13.76) has a matching changelog
  entry, requires a successful exact-commit CI run on main, and passes
  **27/27 CTests** on the exact quick snapshot. The local stripped tarball and
  AppImage pass entry-point and SHA-256 checks. The exact complete gate uses
  `~/.cache/qs-76exact/reports` and is running; the earlier application-source
  gate in `~/.cache/qs-76full` is supplemental. After the active CI concludes,
  push `534e642:refs/heads/main` if main is still at the reviewed ancestor and
  the prior run requires no additional fix. Verify the candidate’s own green CI
  and complete local gate before tagging `v0.13.76`. Verify the Release workflow,
  notes, published files, checksums, and live README badges.
- **Local work.** HEAD is `d2642a3` (0.13.82), ahead of origin by eight
  commits at this checkpoint. The GUI, database, and configuration tranche
  passed its exact quick gate with **27/27 CTests** in `~/.cache/qs-82q`.
  The 0.13.83 documentation checkpoint records this handoff and the updated
  operating instructions. The 0.13.80 complete campaign runs in `~/.cache/qs-80full`.
  It has passed raw coverage at **2748/2775 lines and 1037/1073 branches**.
  Keep the 67% mutation ratchet until a complete campaign supports a higher
  one. Focused tests and whole-campaign scores are tracked separately below.
- **C++23 release preparation.** The working tree holds the separate build and
  dependency changes for 0.14.0. CMake and the native sentinel select C++23.
  Private Clang 20.1.8 and GCC 16.2.1 quick gates each passed **27/27 CTests**.
  Both compiled and executed `if consteval` and `std::to_underlying`. Private
  Clazy 1.17.1 with LLVM 20.1.8 and Cppcheck 2.22.0 handled C++23. Qt SDK
  provisioning verifies five archives using the locked Python environment.
  ShellCheck and actionlint passed on the composed workflows. After finishing
  the documentation checkpoint, prepare `CLIENT_VERSION` **0.14.0** and run
  its fresh exact complete gate before committing or pushing it. Use the
  analyzers in `~/.cache/qtscrobbler-cpp23-patch.h2zoA5/run/installed-clazy/bin`
  and `~/.cache/qtscrobbler-cpp23-patch.h2zoA5/run/installed-cppcheck/bin`.
  Source the qualified LLVM environment, then run Clazy with the same resource
  directory selected by `"$CC" --print-resource-dir`. The 0.14.0 release
  requires green CI on its commit and successful release packaging.
- **GitHub settings.** Dependabot security updates, secret scanning, and
  push protection are enabled. [Initial CodeQL](https://github.com/marcelpetrick/QtScrobbler/actions/runs/44267841867)
  passed for C/C++, Python, and Actions. Dependabot version updates and
  PR dependency review are committed in `1debb51` and await a safe main push.
  The public About description and ten topics were updated and read back.
  The description identifies the independent SourceForge fork, the Qt 5 and
  Qt 6.12 ports, and Linux maintenance.

## Owner priorities — 2026-11-18

- [ ] Fix public CI first. The README CI badge reports failing and the Release
      badge reports no status. The last failed main run, 44193926851, failed
      sanitizer `gui_process` after Xvfb could not write to its closed display
      pipe. `ab41f20` changes Xvfb reset and pipe lifetime. CI 44264412011 is
      qualifying that fix. Inspect its actual result and logs. Keep main stable
      until it concludes, then advance the qualified release candidate.
- [ ] Publish a public release from a green main commit and check the real
      Release and Latest release badges, asset checksums and release notes.
      Candidate `534e642` (0.13.76) passed exact-snapshot quick qualification
      and the local Xvfb AppImage launch. Full local qualification is running.
      Refresh the measured line, branch and mutation badges from the newest
      complete reports and confirm the build and license badge metadata.
- [ ] Enable Dependabot alerts and security updates, add scheduled dependency
      version updates, and activate GitHub security checks for the public repo.
      Record effective settings, supported dependency scopes, and observed scan
      or workflow results. Keep this separate from the release-candidate fix.
      Dependabot alerts were already enabled with **0 open alerts**. Enabled and
      read back security updates, secret scanning and push protection. The first
      GitHub-managed CodeQL scan, run 44267841867, passed for Actions, Python
      and C/C++. GitHub reports weekly scheduling and **0 open CodeQL,
      Dependabot or secret-scanning alerts**. The scheduled version-update configuration checks pip and
      GitHub Actions weekly and limits each to one open version-update PR.
      The PR-only Dependency Review action is pinned to v5.0.1. Both YAML
      files parse successfully. Verify default-branch updater jobs and the
      first PR check after push.
- [ ] Enable C++23 in the maintained native build. Prove it with the pinned
      Clang/LLVM 20.1.8 and the release GCC toolchain, including C++23 language
      or library usage and analyzer compatibility. Update README and build
      documentation to match the verified configuration.
- [ ] Publish the first C++23 release as **0.14.0**. Set `CLIENT_VERSION` and
      add the exact changelog entry before qualifying the release snapshot.
      Tag `v0.14.0` only after the complete local gate and CI on main pass.
- [x] Apply the authorized GitHub About update after independent provenance
      review. Describe the fork from SourceForge, the Qt 5 and then Qt 6.12
      ports, and continued Linux maintenance. Remove stale qmake and Qt 6.11
      metadata and verify the public description and topics after updating.
      The original SourceForge import and Qt 5 port are recorded in
      `919faaf` and `9286e99`. The GitHub API read-back confirmed the new
      description and ten discovery topics on 2026-11-18.
- [x] Inventory current end-to-end and acceptance tests. The CLI suite launches
      the real process and uploads the fixture to loopback. GUI acceptance tests
      run real widgets for import, editing, submission and cancellation. The
      standalone GUI process test covers display-backed startup and orderly
      close. Three randomized runs passed **27/27 CTests each**. See
      [acceptance and end-to-end checks](ACCEPTANCE.md) for the coverage boundary
      and the unperformed real-device and live-service checks.


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
- [x] Check that all documents are current: CHANGELOG 0.13.69 entry; AGENTS,
      GitHub Actions, README (status, branch badge 96.61%, mutation badge), QUALITY
      and MUTATION_TESTING updated with the 0.13.69 measurements and the ratchet.
- [x] Release without mutation testing (owner: "too much overhead"): a separate,
      small release pipeline that publishes only commits pretested by green CI.
      `release.yml` now requires a successful CI run on the tagged commit, then only
      builds, runs unit tests (`--quick`), packages, smoke-tests and publishes.
      Mutation stays mandatory in CI and the local full gate. Verified locally with
      GCC and no LLVM/Mull on PATH: quick gate **26/26 CTests PASS**.
- [ ] Releases carry compiled Linux x86-64 binaries (stripped `.tar.gz`), an
      **AppImage** bundling Qt 6.12 (pinned linuxdeploy 1-alpha-20251107-1 and
      plugin-qt 1-alpha-20250213-1, SHA-256 verified), `.sha256` files and release
      notes with the changelog entry plus a **short list of the checks that passed**
      in the qualifying CI run (`tools/release_notes.py`, refuses any non-PASS
      mandatory stage; 4/4 contracts PASS). Local proof: `tools/build_appimage.sh`
      built a 34 MB AppImage from the GCC/Qt 6.12 build; with no system Qt on the
      library path it started under Xvfb via the bundled xcb plugin and printed
      `QTScrob (v0.13.69)`; it bundles 8 Qt 6 libraries, only the SQLite SQL driver
      (a qmake wrapper hides PostgreSQL/MySQL/ODBC drivers the app never uses) and the
      CLI. Fixes found on the way: CRLF desktop file, linuxdeploy's old `strip`
      (`NO_STRIP=1`). linuxdeploy-plugin-qt has no upstream digest; its SHA-256 was
      measured on download. The `[x]` waits for the first published release.
- [x] Owner decision on the mutation gap: CI measured **1193/1766 (67.554%)** in
      Actions 44167982834 and the local full gate **1195/1766 (67.667%)** against the
      95% gate. Owner chose a **ratchet at 67%** (`MUTATION_MINIMUM` in
      `localPipeline.sh`): CI fails if the score drops; 95% stays the goal and the
      ratchet rises as tests kill more of the 571 remaining mutants. Gate function
      check: both results PASS at 67 and FAIL at 68.
- [x] Fix the CI-only sanitizer failure: `gui_process` connected to Xvfb right after
      it announced the display and failed on the slower sanitizer run. The test now
      retries for up to 10 s and reports Xvfb's log/exit status on a real failure.
      Actions 44193926851 (0.13.73) then showed the real cause in that log: Xvfb died
      with `Cannot write display number to fd 5`. A server reset rewrites the number
      to `-displayfd`, and the test had already closed the pipe. 0.13.74 starts Xvfb
      with `-noreset` and keeps the pipe open. The same run passed every other stage,
      including the mutation ratchet (1193/1766, 67.55% ≥ 67%).
- [ ] Publish a release. `v0.13.69` (`d07b90e`) cannot be released: its CI failed
      (mutation 67.55% < 95%, Xvfb race), and its old-style release run 44169110976
      failed the same gate. The next green CI commit gets the first release. Its tag was pushed on `d07b90e` before this change, so the
      old full release workflow (run **44169110976**) is qualifying it in parallel with
      CI run **44167982834**; later releases use the lean workflow.

## Session handoff (2026-11-18, for the next agent)

### Execution plan for the resumed session

The owner reaffirmed completion of the full initiative and authorized the public release.
The existing tool prefixes are present, local `main` is clean at `7e1786f` and one
commit ahead, and CI run 44264412011 is still provisioning on `ab41f20`.

1. Keep `origin/main` stable while that CI runs. Qualify the exact local handoff
   commit with `--quick` and run the complete native gate on the release candidate
   in separate disk-backed directories. Inspect and record both results.
2. Keep `v0.13.74` untagged. The exact-version release-note preflight reproduced a
   failure: `changelog_entry(Path("documents/CHANGELOG"), "0.13.74")` raises
   `ValueError` because the newest changelog entry is 0.13.69. Prepare 0.13.76 with
   the required entry and verify it before pushing. If CI fails, inspect the failed
   log first and qualify the fix. Tag only after the replacement passes the
   complete local gate and CI on main. Verify the public release workflow,
   shipped assets, checksums and notes.
3. In parallel, audit the vision evidence and investigate mutation survivors.
   Add focused tests only where the original and targeted mutant results can be
   demonstrated. Measure the whole campaign before raising the ratchet.
4. Complete the upstream dependency review, branch review and GitHub About
   review. Fix verified findings. Run the complete gate on the final snapshot,
   push after main's active CI finishes and inspect the resulting Actions run.
   Reconcile the README badges, plan checklist and clean remote/local state
   against actual evidence.

Resumption evidence for the prepared 0.13.76 release candidate:

- The first `--quick` run on `7e1786f` and complete run on `ab41f20` reproduced
  `dbus-daemon: Socket name too long` before CTest with a long disk-backed
  `TMPDIR`. The exact `7e1786f` retry used a shorter disk-backed prefix
  (`~/.cache/qs-75q-b`) with Qt 6.12.0, CMake 4.5.0 and Python 3.15.2 and passed
  **27/27 CTests**, including the display-backed GUI test. The separate runner
  fix and regression proof are pending integration.
- 0.13.76 has an exact changelog section that `tools/release_notes.py` accepts.
  The workflow now searches for successful CI on main for the exact commit and
  runs package smoke tests in the offline namespace. An AppImage process error
  propagates through the output capture pipe. The native application gate is
  running under `~/.cache/qs-76full`; release workflow and public assets remain open.
- The exact prepared `0.13.76` index passed `--quick` with **27/27 CTests**
  (`~/.cache/qs-76q/reports`), then became release candidate `534e642`.
  A locally stripped AppImage from the same application source printed
  `QTScrob (v0.13.76)` under Xvfb with the bundled xcb plugin and no external Qt
  library/plugin path (`~/.cache/qs-76pkg/appimage-xcb.txt`). The exact
  `0.13.75` suite also passed three randomized runs, **27/27 each**.
  Once the previous main run concludes, push this candidate independently of
  later improvements so it receives its own qualifying CI result.
- In `0.13.77`, the offline runner binds its session bus to an abstract Unix
  socket inside the network namespace. It preserves the caller's disk-backed
  `TMPDIR` and removes the generated bus configuration. The long-path regression
  fails on the original with exit 126 and passes with the fix. All **13/13**
  runner contracts pass directly and inside another offline namespace.
  The exact staged `--quick` run passed **27/27 CTests** using the long
  `TMPDIR` under `~/.cache/qtscrobbler-runs/77q-long-offline`. Commit
  `ad83e91` contains the runner fix. Complete post-fix qualification remains open.
- Prepared 0.13.79 caps decoded handshake and submission replies at 64 KiB.
  Oversized input fails using existing translated diagnostics, and an ongoing
  oversized chunked stream aborts before end of stream. New size tests fail on
  the original (**3 regressions**) and the complete offline Submit suite passes
  **44/44** with the fix. The focused rows passed ten repeated runs. Evidence
  lives in `~/.cache/qtscrobbler-runs/submit-response.l7PNco`.
  The prepared index passed **27/27 CTests** in `--quick`
  (`~/.cache/qs-79q/reports`) and became `ae923ae`.
- Prepared 0.13.80 strengthens diagnostic, lifecycle, and displayed-progress
  assertions on the same targeted native mutant inventories. Metadata is
  **18/18 newly killed**, controller is **20/22 newly killed** (18 were whole
  campaign survivors), and GUI console and progress improves **10/40 to
  26/40** with zero losses. Unmutated Qt suites pass **45/45**, **65/65**, and
  **45/45**, respectively. Raw SQLite and comparison evidence lives beneath
  `~/.cache/qtscrobbler-runs/gettrackinfo-kU3Gbu` and
  `~/.cache/qtscrobbler-runs/gui-survivors.woRbQZ`. These are targeted results.
  The whole campaign must be rerun before changing the mutation ratchet.
- Prepared 0.13.81 targets three more test surfaces. The native iPod inventory
  improves **24/43 to 42/43** with 18 additional historical survivor kills.
  Main-window widget assertions improve **1/65 to 47/65**, and CLI usage and
  verbosity improve **0/4 to 4/4**. Unmutated suites pass **88/88**, **45/45**,
  and **65/65**, respectively. Raw before/after databases and original logs are
  in the `gettrackinfo-kU3Gbu/ipod`, `main-window-survivors.hkPAgf`, and
  `~/.cache/qs-cli-native` reports. These targeted runs do not replace the
  full campaign, which is running on the previous 0.13.80 snapshot.
- Prepared 0.13.82 strengthens settings, database cache, and configuration
  contracts. On identical native inventories the GUI improves **4/38 to
  24/38**, SQLite cache **1/11 to 10/11**, and settings persistence **2/18 to
  16/18**. Unmutated suites pass **46/46**, **24/24**, and **21/21**,
  respectively. The exact GUI suite also passes three seeded shuffled runs.
  Evidence lives under `gui-settings-survivors.4hFtrB`, `qs-db-native` and
  `gettrackinfo-kU3Gbu/conf`. The full 0.13.80 campaign passed raw coverage
  at **2748/2775 lines and 1037/1073 branches**, with no missing production
  files, and is still measuring mutation.

State when the session paused (the owner is switching agents):

- `main` = `origin/main` = `ab41f20` (client **0.13.74**, the Xvfb `-noreset` fix).
  Exact-HEAD snapshot `/tmp/qtscrobbler-74-source` passed `--quick --noRun --noOpen`:
  **27/27 CTests PASS**, `gui_process` included (reports
  `~/.cache/qtscrobbler-runs/74q/reports`, log `.../74q/log`). Diagnostic subset only.
- Actions CI **44264412011** for `ab41f20` was **in progress** at handoff (about 1.5 h
  per run). Do not push to `main` until it has finished: the concurrency group cancels it.
  Check it with `gh run view 44264412011`; on failure use `--log-failed` first.
- The previous run, 44193926851 (`f30f717`, 0.13.73), failed only `gui_process` in the
  sanitizer stage; every other stage passed, including mutation 1193/1766 (67.55%) ≥ 67%.
- This handoff note and the AGENTS.md resume section are a docs-only local commit
  (0.13.75, not pushed, not gated). Push it only after 44264412011 has finished, after a
  `--quick` run on its exact snapshot.

Next steps, in order:

1. If 44264412011 passes: record it here and proceed with the replacement release
   candidate described above. `release.yml` publishes the first release
   (tarball, AppImage, `.sha256`, notes). Check that release run, then tick the
   release items above and update the README badges.
2. If it fails: fix the cause, run `--quick` on the exact snapshot, push, and watch again.
3. Open goals: raise the mutation score toward 95% and lift `MUTATION_MINIMUM` with it
   (571 mutants left); finish the vision evidence audit, `reviewBranch`, `githubAbout`,
   and the complete gate on the final tree.

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
- [ ] Reach the 80% killed score (owner lowered the goal from 95% on 2026-11-26; plan in the 2026-11-26 handoff); retain full campaign reports and tool/config/source fingerprints.
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
