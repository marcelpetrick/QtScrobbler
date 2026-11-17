# Acceptance evidence audit — 2026-11-17

This audit reads the current source, tests, CMake registrations, documentation and
retained historical reports. It does not run a new build or certify the pending
working tree. The execution record remains [plan.md](plan.md); this audit does not
mark a whole work package complete. The owner's newest requests to finish the
initiative, refresh every tool dependency, bump and commit, run GitHub Actions and
publish a qualified public release are acceptance requirements.

“Implemented” below means an identifiable artifact or contract exists. “Historical
PASS” remains bound to the version and report named. Neither means the current
complete gate, CI or release has passed.

## P1 — Dependencies and actual Qt 6.12

| Requirement | Inspected implementation or retained evidence | Remaining qualification / accounting |
| --- | --- | --- |
| Upstream inventory and downloadable Qt patch | [Dependency review](DEPENDENCY_REVIEW_2026-11-17.md) freshly checks SDK, native tools, 31 Python pins and four Actions | Private auxiliary updates and runtime/CI inventory remain; latest SDK does not prove later source security patches applied |
| Exact toolchain and transitive pins | requirements-quality.txt; install_cmake.py, install_mutation_toolchain.py, install_mull_callfix.py and install_zlib.py retain exact inputs/hashes | Host/Ubuntu analyzers, runtime packages and platform utilities are not all locked |
| Qt modules selected and runtime exercised | CMake requires Qt6.12; tests assert runtime/version; historical .59 normal/sanitizer20/20 and package PASS | Rerun on final prepared version and actual Actions |
| Separate build directories | cmake/Instrumentation.cmake and localPipeline select normal, sanitizer, coverage and mutation builds | Current integrated correction still needs whole gate |
| Explicit MTP ON/OFF | CMake default ON requires libmtp; fake backend remains linked to real symbols; plan records OFF18/18 and ON normal passes | Final CI package/compiler identity must be captured |
| Qt regressions characterized before fixes | Plan records original failures for inherited parser signals, configuration, storage, lifecycle and UI behavior | This box is substantially stale as an implementation description; final suite still required |
| Unified pinned CI/release | Both workflows use exact Actions SHAs, explicit SDK/private native tools and localPipeline | Observe current actual complete workflow results |
| Setup and supported minima | README, DEPENDENCIES and port history distinguish maintained CMake from unqualified legacy qmake | Keep final environment changes synchronized |

## P2 — Isolation and seams

| Requirement | Inspected artifacts | Remaining qualification / accounting |
| --- | --- | --- |
| Temporary settings/DB/log and synthetic tracks | QTemporaryDir fixtures in parser/config/DB/CLI/GUI/controller tests; tracks helpers in Submit/Scrobble; examples copied | Implemented across suites; reusable helpers are selective rather than one generic framework |
| HOME/XDG before QApplication | Custom GUI main sets HOME/XDG before QApplication; config/controller/DB mains isolate storage; CLI subprocess environment isolates settings/cache | Verify every newly added process continues isolation; no real credentials used by fixtures |
| Clock/storage/network/device seams | DBCache path/clock constructor; fake metadata replies; loopback HttpFixture; Parse_MTP::Backend | Implemented; exact contracts in associated suites |
| Production GUI library | scrobble_gui linked by real app and GUI/translation tests | Already checked in plan |
| Qt signal/event contracts | QSignalSpy, QTRY bounded waits and real widget button actions throughout suites | CLI qWait(1) pumps parent loopback events under elapsed deadline; display polling is bounded; these are purposeful polling, not fixed success sleeps |
| Unexpected Qt diagnostics fail | failOnWarning or message handlers across C++ behavior suites; narrow GUI offscreen allowances; process stderr checks | Current suite/sanitizer rerun required |
| Fake/loopback network and external deny | HttpFixture listens only on LocalHost; CLI uses local custom service; metadata uses injected replies/proxy | No process-level external-network deny mechanism was found. Local routing of intended requests is not proof that accidental external requests are impossible |
| Lifetime cleanup and repetition | GetTrackInfo cancellation/manager ownership; Submit managersAreOwned; DBCache connection cleanup; repeated parser/controller operations; deferred deletion assertions | Historical repeats/sanitizer passes exist; final repeated suite and order-dependence evidence remains |

## P3 — Parsers, configuration, storage and controller

| Production area | Identifiable exact contracts | Remaining qualification / accounting |
| --- | --- | --- |
| common | test_common ordering boundaries including extreme integers/timeless, log/iPod/MTP discovery | Implementation box is stale; final source-qualified suite required |
| parse-log | test_parse_log exact example fields, timezone/version/header rows, malformed/played fields, unreadable/missing files, clear/reopen safety and retry | Signal counts now asserted before indexing; retain current original/mutant proof |
| parse-ipod | test_parse_ipod plain/compressed databases, optional headers, truncation sweep, malformed lengths, decompression limits, failed reopen/clear retry and summary counts | Synthetic device files only; no physical iPod qualification |
| conf | test_conf defaults/full roundtrip/custom paths, invalid formats, migration preservation, write failures, unknown keys, credential logging and isolated system fallbacks | Current native correction must preserve credible mutation results for fallback contracts |
| dbcache | test_dbcache Unicode/quotes/replacement, concurrent/reopen connections, expiry exact boundary/fake clock, missing driver, locked/incompatible/read failure paths | Tests verify connectionNames empty after rows; final sanitizer run required |
| libscrobble | test_scrobble parsing/sorting/edit/delete, metadata identities, timestamp/DST cases, cancel/reentrancy, disabled/mixed sites and failure retry source identity | Historical65/65 rows recorded at .59; current suite exists, final campaign/full gate open |

These six unchecked implementation criteria have substantial concrete suites and
historical execution evidence. They must not be treated as six unimplemented
subsystems; completion still requires the final source-qualified validation.

## P4 — Protocol, metadata and device behavior

| Requirement | Inspected contracts | Remaining qualification / accounting |
| --- | --- | --- |
| Exact request and batching | test_submit batching1/49/50/51/100/101; empty context cases; exact GET/POST method, target, headers, auth/version, UTF-8 fields and MusicBrainz ID | Zero sends is covered separately from nonempty batching |
| Response/error/session/retry/cancel | handshakeFailure, responseFailure, retry, retriesAreBoundedAndKeepSuccessfulTracks, cancellation and networkFailure data rows | No oversized response test or explicit accumulated-body cap was found in Submit; plan's oversized-response clause remains a real gap |
| Retention and no duplicate success | Submit sent-state assertions and Scrobble retry/mixed-site source cleanup contracts | Final exact-mutant/current campaign evidence remains |
| Metadata XML/error/lifetime | test_gettrackinfo exact request/success, malformed XML, duration boundaries, network errors, overlapping generations, cancellation, manager/reply/thread ownership | Fake replies test real production request logic; live API compatibility unperformed |
| MTP backend and real adapter link | test_parse_mtp discovery/read/update/free/reopen/count errors; defaultsRetainRealAdapter compares actual LIBMTP symbols | Physical device operations unperformed; no claim that fake backend demonstrates hardware compatibility |
| Optional live/device checks | ACCEPTANCE and QUALITY explicitly disclose unperformed real-service/device checks | No opt-in executable live/device test harness was established by this audit; limitation labeling exists |

## P5 — CLI and GUI workflows

| Requirement | Inspected contracts | Remaining qualification / accounting |
| --- | --- | --- |
| CLI processes | test_cli help/invalid/conflicting/config/parser/no-site cases, real process upload with exact fields, metadata ordering, failure retention and exit/version checks | Actual process tests use temporary files and loopback service |
| GUI import/edit/settings/submit/cancel | test_gui importEditAndClose, all-field/rejected edits, timestamp bounds, selection/delete/reimport, settings save/cancel/reopen, auxiliary windows and submissionProgress | Real production widgets/controller execute in-process under Qt Test |
| Correct handlers/no duplicate connects | QSignalSpy counts, repeated parser/UI/controller opens, cancellation and repeated lifecycle contracts | No exhaustive signal-to-handler trace table; current mutation campaign should expose remaining gaps |
| Actual catalog loading/placeholders | test_translations compares every active TS entry against QTranslator and actual widgets; TRANSLATIONS documents unchanged vocabulary and text limitations | Native-speaker linguistic review unrecorded; English HTML/CLI help disclosed |
| Stable layout snapshots if valuable | Behavior/properties asserted; no host-style golden image introduced | Conditional plan item can be explicitly recorded as not adopted: no demonstrated need |
| Golden images only for app-owned output | Real screenshot in documents/images; capture script and actual GUI evidence | Screenshot is product evidence, not regression golden image |
| Offscreen plus real display startup/close | test_gui_process launches actual qtscrob on private Xvfb and graceful WM_DELETE_WINDOW; historical .59 sanitizer20/20 | Full standalone GUI import/upload workflow is not automated; accurate distinction exists in ACCEPTANCE |

The acceptance labels cover cli, gui, scrobble, translations and gui_process;
process e2e labels cover cli and gui_process. Labels do not broaden what each test
actually asserts. See [workflow mapping](ACCEPTANCE.md).

## P6 — Coverage denominator and thresholds

Read directly from retained
`/tmp/qtscrobbler-frozen-59-full-reports/coverage/summary.json`:

| Historical .59 fact | Observed value |
| --- | --- |
| target_passed | true |
| Lines covered / denominator | 2722 /2749 (99.017824663514%) |
| Branches covered / denominator | 1017 /1063 (95.67262464722484%) |
| Missing production files | [] |
| Branch semantics | LLVM explicit source branch regions; implicit exception edges absent |

| Requirement | Implementation and remaining accounting |
| --- | --- |
| Select compatible frontend/semantics | run_coverage.py uses native LLVM20 export; comments and summary state raw LLVM explicit source branches. Earlier GCC-vs-LLVM comparative spike evidence was not independently located in this audit |
| Whole maintained source scope | Expected production inventory includes library/CLI/GUI/MTP; source fingerprints and missing-file failure enforced |
| Explicit exclusions | Qt/generated/tests/system code omitted from first-party denominator; Windows adapter and vendored xgetopt separately unmeasured; build-only correction tooling is separately qualified |
| Unreachable/equivalent exclusions | No broad production-file/logging exclusions applied by coverage runner; any future exception must retain reviewed reason |
| Baseline/ratchet | Historical full-scope measurements and integer95% gate exist; never infer current numbers from .59 |
| Machine/HTML/JUnit/integrity reports | Runner emits exports, profiles, source inventory, HTML and JUnit; fresh report directory/source identity enforced |
| Current local and remote success | Open until fresh final-source local gate and actual Actions demonstrate both metrics |

Several P6 boxes describe implemented accounting, but the package exit requires
fresh local and remote results. The LLVM source-branch definition must not be
misrepresented as GCC implicit-exception-edge coverage.

## P7 — Native mutation

| Requirement | Inspected implementation / recorded evidence | Remaining qualification |
| --- | --- | --- |
| ABI and semantic spike | Native installer pins LLVM20/Mull0.38; check_mutation_toolchain performs actual Qt signal-removal preflight; native CFG helper validates corrected invoke/cleanup handling | Targeted correction evidence is not a complete campaign |
| Scope/operator isolation | Instrumentation CMake, mull.yml, run_mutation_tests; no mapping-based false-uncovered filtering; production scope fingerprints | Verify fresh campaign inventories match coverage |
| Stable IDs and aggregation | mutation_score reads unique operator/file/line/column identities across executables | Final full report required |
| Honest outcomes | Assertion output proof, survivors, uncovered classification, invalid/timeouts/crashes/infrastructure categories retained | Unexplained timeouts/crashes never count as assertion kills |
| Score definition | Integer gate requires95% assertion-detected kills over valid generated mutants; trusted coverage classification separate | No trustworthy passing current full score available to this audit |
| Baseline/time calibration | Bounded runner and separate original test execution exist; plan contains targeted timings | Complete idle/bounded original and campaign results pending |
| Survivor regressions | Plan records specific comparison, signals, boundaries and retry original/mutant proofs | Examine new full campaign and eliminate actual survivors |
| Narrow exclusions | No blanket logging exclusion; build-only tool qualification separate from application denominator | Publish any per-mutant equivalents explicitly; do not invent exclusions to reach target |
| Unified mandatory pipeline | Default pipeline and CI/release invoke native preflight/campaign; --quick and --baseline labelled diagnostic | Final full default command must pass before release |

This is the principal unproven acceptance target. Historical campaigns with known
native-tool defects cannot establish a qualified score.

## P8 — Static checks, docs and unified gate

| Requirement | Inspected implementation / historical evidence | Remaining qualification |
| --- | --- | --- |
| Formatter/style | Exact Python wheel pin; .clang-format and pipeline source checks | Final changed files must pass selected formatter |
| Static analyzers | run_quality_checks selected clang-tidy/clazy/cppcheck checks; documented scope/suppressions; .59 static summary PASS | Enabled rules are incremental, not every available check; host/CI differences unresolved |
| Sanitizers | Separate instrumentation build and leak detection; .59 sanitizer JUnit retained, plan records20/20 | Fresh final-source run required; no TSan success claimed |
| API/Doxygen/current/history docs | API, ARCHITECTURE, README index, vision, dependency, mutation and quality docs exist; .59 docs summary PASS | Update changed environment/results after qualification |
| Negative gate contracts | test_pipeline exercises stages, missing tools/reports, exit statuses and conflicting modes; accounting/installer/evidence Python contracts registered in CTest | Current registered suite must pass |
| One complete command | Both workflows run localPipeline default command with real reports; default requires all mandatory quality/package stages | No full-green current gate observed by this audit |

## P9 — Release and final review

| Requirement | Available evidence | Remaining action |
| --- | --- | --- |
| Extracted package/dependencies/resources/version | `/tmp/qtscrobbler-frozen-59-package/summary.json` directly inspected: package PASS, ldd and CLI/GUI help PASS, real offscreen startup observed | Rebuild/package final clean committed version; runtime dependencies remain external |
| Display and optional device/live limits | Xvfb process test plus real screenshot; explicit unperformed device/live limitations | Retain accurate limits in public release notes |
| Repetition/order/lifetime | Historical repeated DB/parser/config/controller runs and .59 sanitizer pass | No randomized suite-order report located; run bounded repeat/order check on final source |
| Review and removed guards | REVIEW records two verified resolved tooling findings; plan records retry ownership/guard review | Run final reviewBranch against latest relevant merge-base and pending final changes; resolve verified findings |
| About review | GITHUB_ABOUT proposal exists | Run githubAbout after final review as owner requires; do not claim proposed metadata already applied |
| Attached evidence | CI/release artifacts upload full quality reports and release archives | Require actual complete reports for published tag |
| Actual Actions/public release | Workflows implemented; owner explicitly authorized publication | Observe actual green qualification, push matching version tag, inspect Release run and public assets; never publish around a failing95% gate |
| Commit/version/clean tree | Hook-based tested-version procedure documented | Each implementation commit updates plan; test prepared version before disabling already-executed hook only for commit; push and finish clean |

## Actionable gaps and stale checklist treatment

The largest gaps are the fresh trustworthy95% mutation result, complete final
native pipeline and actual green remote qualification. Dependency/environment
refresh also needs the private auxiliary-tool updates or justified compatibility
retentions, with exact CI/runtime inventory.

Two concrete testing gaps deserve explicit treatment: accumulated oversized
submission responses have no dedicated contract/cap found here, and intended
loopback routing does not enforce a global external-network deny. Repeated suite
order evidence is also missing. These findings are acceptance gaps, not a claim
of a reproduced production regression.

P2–P5 and much of P6–P8 contain stale unchecked implementation descriptions despite
substantial artifacts and historical passing evidence. A primary-agent checklist
update can acknowledge the specifically supported subcriteria, retaining each
package's current qualification gaps. Conditional snapshot/golden-image choices
should be recorded explicitly rather than leaving them indistinguishable from
missing mandatory work. No package or final completion should be inferred merely
from this audit's existence.
