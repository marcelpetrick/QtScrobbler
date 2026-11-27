#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${ROOT_DIR}/src"
BUILD_DIR="${QTSCROBBLER_BUILD_DIR:-${ROOT_DIR}/build}"
APP_VERSION="$(grep -oP '#define CLIENT_VERSION "\K[^"]+' "${SRC_DIR}/lib/common.h" 2>/dev/null || echo "unknown")"
PIPELINE_LOG_DIR="${TMPDIR:-/tmp}/qtscrob-pipeline-$$"
KEEP_REPORTS=false
# Invoked through a trap or quality_command.
# shellcheck disable=SC2329
cleanup() {
    if [[ "${KEEP_REPORTS}" == false ]]; then rm -rf "${PIPELINE_LOG_DIR}"; fi
}
trap cleanup EXIT

declare -a SUMMARY_LINES=()

# State flags
QT6_OK=0
TOOLS_OK=0
CMAKE_CONFIG_OK=0
CMAKE_BUILD_OK=0
GUI_BUILD_OK=0
CLI_BUILD_OK=0
RUN_APP=true
QUICK=false
BASELINE=false
FORCE_FULL=false
# The 80% goal (owner decision 2026-11-26, was 95%) is reached: tests proven
# against their mutants raised the score from 77.93% (0.15.3) past 80%, so
# the ratchet now enforces the goal. Coverage requires 99% lines and 95% branches.
MUTATION_MINIMUM=80

# Detail strings
QT6_DETAILS=""
TOOLS_DETAILS=""
CMAKE_CONFIG_DETAILS=""
CMAKE_BUILD_DETAILS=""
GUI_DETAILS=""
CLI_DETAILS=""
CLI_SMOKE_DETAILS=""
GUI_SMOKE_DETAILS=""
TRANSLATION_DETAILS=""

# Number of parallel compile jobs
JOBS="${QTSCROBBLER_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

# Will be set to the detected Qt6 qmake binary (qmake6 or qmake) — used for Qt6 check only

# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

log()   { printf '[INFO]  %s\n' "$*"; }
warn()  { printf '[WARN]  %s\n' "$*" >&2; }
error() { printf '[ERROR] %s\n' "$*" >&2; }

mark_result() {
    local label="$1" status="$2" details="$3"
    SUMMARY_LINES+=("$(printf '%-22s : %-4s %s' "${label}" "${status}" "${details}")")
}

run_with_log() {
    local log_path="$1"; shift
    mkdir -p "${PIPELINE_LOG_DIR}"
    "$@" 2>&1 | tee "${log_path}"
    return "${PIPESTATUS[0]}"
}

print_summary() {
    printf '\n============ Local Pipeline Summary ============\n'
    printf ' QtScrobbler v%s\n' "${APP_VERSION}"
    local line
    for line in "${SUMMARY_LINES[@]}"; do printf '%s\n' "${line}"; done
    printf '================================================\n'
}

print_usage() {
    cat <<EOF
Usage: ./localPipeline.sh [--noRun] [--noOpen] [--report-dir PATH] [--quick|--baseline] [--help]

Local build and quality pipeline for QtScrobbler (Qt 6, Linux).

Stages:
  1.  Qt 6 check          — verify Qt >= 6.x via qmake
  2.  Build tools         — cmake, g++, pkg-config (libmtp optional)
  3.  CMake configure     — cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
  4.  CMake build         — cmake --build build --parallel ${JOBS}
                            (compiles translations, lib, GUI and CLI in one pass)
  5.  Smoke: CLI          — scrobbler --help
  6.  Smoke: GUI binary   — qtscrob present and executable
  7.  Translations        — count unfinished strings in .ts catalogs
  8.  Unit tests          — run QTest suite (missing or failed tests are fatal)
  9.  Launch app          — start qtscrob once (suppress with --noRun)
  10. Format/static/docs   — native analyzers, fatal Doxygen and documentation checks
  11. Sanitizers           — isolated ASan/UBSan build and complete CTest suite
  12. Coverage             — isolated LLVM build, separate 99% line and 95% branch gates
  13. Mutation toolchain   — native Qt signal-removal semantic proof (fail-closed)
  14. Mutation             — isolated native Mull campaign, assertion-killed ratchet gate
                            (MUTATION_MINIMUM; goal 80%)
  15. Package              — extracted CLI and actual GUI startup, dependency checks
  16. Summary              — actual stage results (missing tools or reports fail)

Options:
  --quick   Explicit build/test diagnostic subset; does not qualify final quality
  --baseline  Run all stages but MEASURE coverage/mutation below target; not a release gate
  --mutation  Run the complete gate (mutation is mandatory in the default full gate)
  --noRun   Skip launching the GUI app at the end
  --noOpen  Keep reports closed (the pipeline never opens a browser)
  --report-dir PATH  Retain logs and JUnit XML in PATH
  --help    Show this message

Exit code 0 when all mandatory stages pass.
EOF
}

# ---------------------------------------------------------------------------
# Stage 1: Qt6 availability
# ---------------------------------------------------------------------------

check_qt6() {
    log "Stage 1 — Checking Qt6 availability..."

    # Prefer the explicit Qt6 binary; fall back to plain qmake only when it is Qt6.
    local candidates=("qmake6" "qmake6-qt6" "qmake")
    if [[ -n "${QTSCROBBLER_QT_PREFIX:-}" ]]; then
        candidates=("${QTSCROBBLER_QT_PREFIX}/bin/qmake")
    fi
    local qmake_bin="" qt_version="" major=""

    for candidate in "${candidates[@]}"; do
        local bin
        bin="$(command -v "${candidate}" 2>/dev/null)" || continue
        qt_version="$("${bin}" --version 2>&1 | grep -oP 'Qt version \K[0-9]+\.[0-9]+\.[0-9]+')"
        major="${qt_version%%.*}"
        if [[ "${major}" == "6" && "${qt_version#6.}" != "${qt_version}" ]]; then
            local minor="${qt_version#6.}"
            minor="${minor%%.*}"
            if [[ "${minor}" -lt 12 ]]; then
                log "  ${bin}: Qt ${qt_version} — Qt >= 6.12 required"
                continue
            fi
            qmake_bin="${bin}"
            break
        fi
        log "  ${bin}: Qt ${qt_version} — not Qt 6, skipping"
    done

    if [[ -z "${qmake_bin}" ]]; then
        QT6_DETAILS="No Qt >= 6.12 qmake found (tried: ${candidates[*]})"
        error "  ${QT6_DETAILS}"
        return 1
    fi
    QT6_DETAILS="Qt ${qt_version} at ${qmake_bin}"
    log "  Qt version : ${qt_version}"
    log "  qmake path : ${qmake_bin}"
    return 0
}

# ---------------------------------------------------------------------------
# Stage 2: Build tools
# ---------------------------------------------------------------------------

check_build_tools() {
    log "Stage 2 — Checking required build tools..."
    log "  Parallel compile jobs: ${JOBS} (from nproc)"

    local missing=()

    for tool in cmake g++ pkg-config; do
        if command -v "${tool}" >/dev/null 2>&1; then
            local ver
            ver="$("${tool}" --version 2>&1 | head -1)"
            log "  ${tool}: ${ver}"
        else
            missing+=("${tool}")
            error "  ${tool}: NOT FOUND"
        fi
    done

    if pkg-config --exists libmtp 2>/dev/null; then
        local mtp_ver
        mtp_ver="$(pkg-config --modversion libmtp 2>/dev/null)"
        log "  libmtp: ${mtp_ver} — MTP device support will be compiled"
    else
        warn "  libmtp: not found — MTP device support will be excluded"
    fi

    if [[ "${#missing[@]}" -gt 0 ]]; then
        TOOLS_DETAILS="missing: ${missing[*]}"
        return 1
    fi

    TOOLS_DETAILS="cmake, g++, pkg-config present; ${JOBS} compile jobs"
    return 0
}

# ---------------------------------------------------------------------------
# Stage 3: CMake configure
# ---------------------------------------------------------------------------

cmake_configure() {
    log "Stage 3 — CMake configure (out-of-tree build in ${BUILD_DIR})..."
    local log_path="${PIPELINE_LOG_DIR}/cmake_configure.log"
    mkdir -p "${PIPELINE_LOG_DIR}"

    if run_with_log "${log_path}" cmake \
            -B "${BUILD_DIR}" \
            -S "${ROOT_DIR}" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_PREFIX_PATH="${QTSCROBBLER_QT_PREFIX:-${CMAKE_PREFIX_PATH:-}}" \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=ON \
            -DQTSCROBBLER_MTP=ON -DQTSCROBBLER_COVERAGE=OFF \
            -DQTSCROBBLER_MULL_PLUGIN:FILEPATH= -DQTSCROBBLER_SANITIZERS=OFF; then
        CMAKE_CONFIG_DETAILS="configured in ${BUILD_DIR}"
        log "  Configure PASS"
        return 0
    fi

    local errors
    errors="$(grep -cE 'CMake Error' "${log_path}" 2>/dev/null || true)"
    CMAKE_CONFIG_DETAILS="configure failed (${errors} CMake error(s)) — see ${log_path}"
    error "  ${CMAKE_CONFIG_DETAILS}"
    return 1
}

# ---------------------------------------------------------------------------
# Stage 4: CMake build (library + translations + GUI + CLI in one pass)
# ---------------------------------------------------------------------------

cmake_build() {
    log "Stage 4 — CMake build (--parallel ${JOBS}, -Wall -Werror enforced)..."
    local log_path="${PIPELINE_LOG_DIR}/cmake_build.log"

    if run_with_log "${log_path}" cmake --build "${BUILD_DIR}" --parallel "${JOBS}"; then
        local gui_artifact="${BUILD_DIR}/qtscrob"
        local cli_artifact="${BUILD_DIR}/scrobbler"
        local lib_artifact="${BUILD_DIR}/libscrobble.a"

        local all_ok=1

        if [[ -x "${gui_artifact}" ]]; then
            local size
            size="$(du -h "${gui_artifact}" | awk '{print $1}')"
            GUI_BUILD_OK=1
            GUI_DETAILS="qtscrob (${size})"
            log "  GUI PASS: ${gui_artifact} (${size})"
        else
            GUI_DETAILS="qtscrob binary not found after build"
            all_ok=0
            error "  GUI FAIL: ${GUI_DETAILS}"
        fi

        if [[ -x "${cli_artifact}" ]]; then
            local size
            size="$(du -h "${cli_artifact}" | awk '{print $1}')"
            CLI_BUILD_OK=1
            CLI_DETAILS="scrobbler (${size})"
            log "  CLI PASS: ${cli_artifact} (${size})"
        else
            CLI_DETAILS="scrobbler binary not found after build"
            all_ok=0
            error "  CLI FAIL: ${CLI_DETAILS}"
        fi

        if [[ -f "${lib_artifact}" ]]; then
            local size
            size="$(du -h "${lib_artifact}" | awk '{print $1}')"
            CMAKE_BUILD_DETAILS="libscrobble.a (${size}); qtscrob; scrobbler"
            log "  Library: ${lib_artifact} (${size})"
        fi

        [[ "${all_ok}" -eq 1 ]]
        return $?
    fi

    local errors warnings
    errors="$(grep -cE ' error:' "${log_path}" 2>/dev/null || true)"
    warnings="$(grep -cE ' warning:' "${log_path}" 2>/dev/null || true)"
    CMAKE_BUILD_DETAILS="build failed (${errors} error(s), ${warnings} warning(s)) — see ${log_path}"
    error "  ${CMAKE_BUILD_DETAILS}"
    if [[ "${warnings}" -gt 0 ]]; then
        error "  Warnings treated as errors (-Werror). Fix all warnings before committing."
        grep -E ' warning:' "${log_path}" | head -10 >&2 || true
    fi
    return 1
}

# ---------------------------------------------------------------------------
# Stage 5: CLI smoke test
# ---------------------------------------------------------------------------

smoke_test_cli() {
    local binary="${BUILD_DIR}/scrobbler"
    log "Stage 5 — CLI smoke test: ${binary} --help..."

    if [[ ! -x "${binary}" ]]; then
        CLI_SMOKE_DETAILS="binary not found at ${binary}"
        error "  ${CLI_SMOKE_DETAILS}"
        return 1
    fi

    local log_path="${PIPELINE_LOG_DIR}/cli_smoke.log"
    # The CLI spins a Qt event loop; protect with a 5-second timeout.
    if timeout 5 "${binary}" --help > "${log_path}" 2>&1; then
        CLI_SMOKE_DETAILS="--help exited 0 — usage text printed"
        log "  ${CLI_SMOKE_DETAILS}"
        return 0
    else
        local rc=$?
    fi

    if [[ "${rc}" -eq 124 ]]; then
        CLI_SMOKE_DETAILS="timed out after 5 s (event-loop binary needs a log file argument)"
        warn "  ${CLI_SMOKE_DETAILS}"
    else
        CLI_SMOKE_DETAILS="--help exited ${rc}"
        warn "  ${CLI_SMOKE_DETAILS}"
    fi
    return 1
}

# ---------------------------------------------------------------------------
# Stage 6: GUI binary smoke test
# ---------------------------------------------------------------------------

smoke_test_gui_binary() {
    local binary="${BUILD_DIR}/qtscrob"
    log "Stage 6 — GUI binary smoke test: ${binary}..."

    if [[ -x "${binary}" ]]; then
        local size
        size="$(du -h "${binary}" | awk '{print $1}')"
        GUI_SMOKE_DETAILS="${binary} — ${size}"
        log "  ${GUI_SMOKE_DETAILS}"
        return 0
    fi

    GUI_SMOKE_DETAILS="binary not found at ${binary}"
    error "  ${GUI_SMOKE_DETAILS}"
    return 1
}

# ---------------------------------------------------------------------------
# Stage 7: Translation audit
# ---------------------------------------------------------------------------

check_translations() {
    log "Stage 7 — Auditing Qt translation catalogs in src/language/..."

    local total_sources=0 total_unfinished=0
    local detail_parts=()

    shopt -s nullglob
    for catalog in "${SRC_DIR}/language/"*.ts; do
        local language source_count unfinished_count
        language="$(basename "${catalog}" .ts)"
        source_count="$(grep -c "<source>" "${catalog}" 2>/dev/null || true)"
        unfinished_count="$(grep -c 'type="unfinished"' "${catalog}" 2>/dev/null || true)"
        total_sources=$((total_sources + source_count))
        total_unfinished=$((total_unfinished + unfinished_count))
        detail_parts+=("${language}: ${source_count}/${unfinished_count} untranslated")
        log "  ${language}: ${source_count} strings, ${unfinished_count} untranslated"
    done
    shopt -u nullglob

    if [[ "${#detail_parts[@]}" -eq 0 ]]; then
        TRANSLATION_DETAILS="no .ts catalogs found"
        warn "  ${TRANSLATION_DETAILS}"
        return 1
    fi

    local joined
    joined="$(printf '%s;  ' "${detail_parts[@]}")"
    TRANSLATION_DETAILS="${joined%%;  } — total: ${total_sources} strings, ${total_unfinished} untranslated"

    [[ "${total_unfinished}" -eq 0 ]]
}

# ---------------------------------------------------------------------------
# Stage 8: Unit tests
# ---------------------------------------------------------------------------

run_unit_tests() {
    log "Stage 8 — Searching for CMake/QTest unit test suite..."

    # Check for CTest tests registered in the build
    if [[ -f "${BUILD_DIR}/CTestTestfile.cmake" ]]; then
        local test_log="${PIPELINE_LOG_DIR}/ctest.log"
        log "  Running ctest in ${BUILD_DIR}..."
        if run_with_log "${test_log}" python3 "${ROOT_DIR}/tools/run_offline.py" -- ctest --test-dir "${BUILD_DIR}" --output-on-failure --no-tests=error \
                --output-junit "${PIPELINE_LOG_DIR}/junit.xml" -j "${JOBS}"; then
            log "  All tests passed."
            return 0
        fi
        error "  Some tests failed — see ${test_log}"
        return 1
    fi

    log "  No CTest suite found in build directory."
    log "  → Add a src/tests/CMakeLists.txt with add_test() entries to enable unit testing."
    return 1
}

# ---------------------------------------------------------------------------
# Stage 9: Launch the GUI application
# ---------------------------------------------------------------------------

launch_application() {
    local binary="${BUILD_DIR}/qtscrob"
    log "Stage 9 — Launching ${binary}..."

    if [[ ! -x "${binary}" ]]; then
        warn "  Binary not found — cannot launch."
        return 1
    fi

    if [[ -z "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
        warn "  No DISPLAY or WAYLAND_DISPLAY set — skipping launch in headless environment."
        return 1
    fi

    log "  Starting qtscrob (display: ${DISPLAY:-${WAYLAND_DISPLAY:-}}). The pipeline will not wait for it."
    "${binary}" &
    local app_pid=$!
    sleep 1
    if kill -0 "${app_pid}" 2>/dev/null; then
        disown "${app_pid}" 2>/dev/null || true
        log "  qtscrob is running (PID ${app_pid})."
        return 0
    fi

    wait "${app_pid}"
    local rc=$?
    if [[ "${rc}" -eq 0 ]]; then
        return 0
    fi
    warn "  qtscrob exited immediately (exit ${rc})."
    return 1
}

# Complete qualification. Each stage retains its own output and propagates failure.
quality_command() {
    local label="$1"; shift
    local stage_status="PASS"
    if run_with_log "${PIPELINE_LOG_DIR}/${label}.log" "$@"; then
        if [[ "${BASELINE}" == true && ( "${label}" == Coverage || "${label}" == Mutation ) ]]; then
            stage_status="MEASURE"
        fi
        mark_result "${label}" "${stage_status}" "see ${PIPELINE_LOG_DIR}/${label}.log"
        return 0
    fi
    mark_result "${label}" "FAIL" "see ${PIPELINE_LOG_DIR}/${label}.log"
    return 1
}

# Invoked through a trap or quality_command.
# shellcheck disable=SC2329
format_check() {
    command -v clang-format >/dev/null || return 1
    local sources=()
    local source_dirs=("${SRC_DIR}/lib" "${SRC_DIR}/cli" "${SRC_DIR}/qt/src" "${ROOT_DIR}/tests")
    [[ ! -d "${ROOT_DIR}/tools/native" ]] || source_dirs+=("${ROOT_DIR}/tools/native")
    mapfile -t sources < <(find "${source_dirs[@]}" -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
    [[ "${#sources[@]}" -gt 0 ]] || return 1
    clang-format --dry-run --Werror "${sources[@]}"
}

# Invoked through a trap or quality_command.
# shellcheck disable=SC2329
sanitizer_check() {
    local sanitizer_build="${BUILD_DIR}-sanitizers"
    cmake -S "${ROOT_DIR}" -B "${sanitizer_build}" -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_PREFIX_PATH="${QTSCROBBLER_QT_PREFIX:-}" -DBUILD_TESTING=ON \
        -DQTSCROBBLER_MTP=ON -DQTSCROBBLER_SANITIZERS=ON \
        -DQTSCROBBLER_COVERAGE=OFF -DQTSCROBBLER_MULL_PLUGIN:FILEPATH= || return 1
    cmake --build "${sanitizer_build}" --parallel "${JOBS}" || return 1
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        python3 "${ROOT_DIR}/tools/run_offline.py" -- ctest --test-dir "${sanitizer_build}" --output-on-failure --no-tests=error -j "${JOBS}" \
        --output-junit "${PIPELINE_LOG_DIR}/sanitizers-junit.xml"
}

# Called from the quality_command callbacks below.
# shellcheck disable=SC2329
check_mutation_environment() {
    [[ -n "${QTSCROBBLER_MULL_PLUGIN:-}" && -f "${QTSCROBBLER_MULL_PLUGIN}" ]] || {
        error "Source the qualified native LLVM/Mull env.sh before full qualification"; return 1;
    }
    if [[ -z "${CC:-}" || -z "${CXX:-}" ]] ||
            ! command -v "${CC}" >/dev/null 2>&1 || ! command -v "${CXX}" >/dev/null 2>&1; then
        error "Source the qualified native LLVM/Mull env.sh with executable CC and CXX before mutation checks"
        return 1
    fi
}

# Invoked through quality_command.
# shellcheck disable=SC2329
mutation_toolchain_check() {
    check_mutation_environment || return 1
    local artifacts=() artifact
    for artifact in "${QTSCROBBLER_MULL_CALLFIX_MANIFEST:-}" "${QTSCROBBLER_MULL_CALLFIX_SHIM:-}" \
                    "${QTSCROBBLER_MULL_CALLFIX_SOURCE:-}"; do
        [[ -z "${artifact}" ]] || artifacts+=(--artifact "${artifact}")
    done
    python3 "${ROOT_DIR}/tools/check_mutation_toolchain.py" \
        --report-dir "${PIPELINE_LOG_DIR}/mutation-toolchain" \
        --compiler "${CXX}" --plugin "${QTSCROBBLER_MULL_PLUGIN}" \
        "${artifacts[@]}"
}

# Invoked through quality_command.
# shellcheck disable=SC2329
mutation_check() {
    check_mutation_environment || return 1
    local mutation_build="${BUILD_DIR}-mutation"
    local measure=()
    [[ "${BASELINE}" == true ]] && measure+=(--baseline)
    MULL_CONFIG="${ROOT_DIR}/mull.yml" cmake -S "${ROOT_DIR}" -B "${mutation_build}" \
        -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="${QTSCROBBLER_QT_PREFIX:-}" \
        -DCMAKE_C_COMPILER="${CC:?}" -DCMAKE_CXX_COMPILER="${CXX:?}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=ON -DQTSCROBBLER_MTP=ON \
        -DQTSCROBBLER_MULL_PLUGIN:FILEPATH="${QTSCROBBLER_MULL_PLUGIN}" \
        -DQTSCROBBLER_COVERAGE=OFF -DQTSCROBBLER_SANITIZERS=OFF || return 1
    python3 "${ROOT_DIR}/tools/run_offline.py" -- python3 "${ROOT_DIR}/tools/run_mutation_tests.py" --build-dir "${mutation_build}" \
        --report-dir "${PIPELINE_LOG_DIR}/mutation" --jobs "${JOBS}" --workers "${JOBS}" \
        --command-timeout 7200 --toolchain-summary "${PIPELINE_LOG_DIR}/mutation-toolchain/summary.json" \
        --coverage-summary "${PIPELINE_LOG_DIR}/coverage/summary.json" --minimum "${MUTATION_MINIMUM}" "${measure[@]}"
}

full_quality_checks() {
    local failed=0
    local measure=()
    [[ "${BASELINE}" == true ]] && measure+=(--baseline)
    quality_command Format format_check || failed=1
    for stage in static docs; do
        quality_command "${stage}" python3 "${ROOT_DIR}/tools/run_quality_checks.py" \
            --stage "${stage}" --build-dir "${BUILD_DIR}" --report-dir "${PIPELINE_LOG_DIR}/${stage}" || failed=1
    done
    quality_command Sanitizers sanitizer_check || failed=1
    quality_command Coverage python3 "${ROOT_DIR}/tools/run_coverage.py" \
        --build-dir "${BUILD_DIR}-coverage" --report-dir "${PIPELINE_LOG_DIR}/coverage" \
        --jobs "${JOBS}" "${measure[@]}" || failed=1
    if quality_command MutationToolchain mutation_toolchain_check; then
        quality_command Mutation mutation_check || failed=1
    else
        mark_result Mutation FAIL "native mutation toolchain semantic preflight failed"
        failed=1
    fi
    quality_command Package python3 "${ROOT_DIR}/tools/run_offline.py" -- python3 "${ROOT_DIR}/tools/run_quality_checks.py" \
        --stage package --build-dir "${BUILD_DIR}" --report-dir "${PIPELINE_LOG_DIR}/package" || failed=1
    return "${failed}"
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

parse_arguments() {
    while [[ "$#" -gt 0 ]]; do
        case "$1" in
            --quick) QUICK=true ;;
            --baseline) BASELINE=true ;;
            --mutation) FORCE_FULL=true ;;
            --noRun)  RUN_APP=false ;;
            --noOpen) ;;
            --report-dir)
                if [[ "$#" -lt 2 || "$2" == --* ]]; then
                    error "--report-dir requires a path"; exit 2
                fi
                PIPELINE_LOG_DIR="$2"
                KEEP_REPORTS=true
                shift
                ;;
            --help|-h) print_usage; exit 0 ;;
            *) error "Unknown argument: $1"; print_usage; exit 2 ;;
        esac
        shift
    done
    local modes=0
    [[ "${QUICK}" == true ]] && modes=$((modes + 1))
    [[ "${BASELINE}" == true ]] && modes=$((modes + 1))
    [[ "${FORCE_FULL}" == true ]] && modes=$((modes + 1))
    if [[ "${modes}" -gt 1 ]]; then
        error "Choose only one mode: --quick, --baseline or --mutation"
        exit 2
    fi
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

main() {
    local exit_code=0

    parse_arguments "$@"
    if [[ "${KEEP_REPORTS}" == false && "${QUICK}" == false ]]; then
        PIPELINE_LOG_DIR="${ROOT_DIR}/quality-reports/$(date -u '+%Y%m%dT%H%M%SZ')-$$"
        KEEP_REPORTS=true
    fi
    if [[ -e "${PIPELINE_LOG_DIR}" ]] &&
            { [[ ! -d "${PIPELINE_LOG_DIR}" ]] || [[ -n "$(find "${PIPELINE_LOG_DIR}" -mindepth 1 -print -quit)" ]]; }; then
        error "Report directory must be new or empty: ${PIPELINE_LOG_DIR}"
        return 2
    fi
    mkdir -p "${PIPELINE_LOG_DIR}" || return 1

    log "================================================"
    log " QtScrobbler v${APP_VERSION} — local pipeline"
    log " Root    : ${ROOT_DIR}"
    log " Jobs    : ${JOBS} parallel compile threads"
    log " Date    : $(date '+%Y-%m-%d %H:%M:%S')"
    log " Run app : ${RUN_APP}"
    log "================================================"

    # Stage 1 — Qt6
    if check_qt6; then
        QT6_OK=1
        mark_result "Qt6 Check" "PASS" "${QT6_DETAILS}"
    else
        mark_result "Qt6 Check" "FAIL" "${QT6_DETAILS}"
        exit_code=1
    fi

    # Stage 2 — Build tools
    if check_build_tools; then
        TOOLS_OK=1
        mark_result "Build Tools" "PASS" "${TOOLS_DETAILS}"
    else
        mark_result "Build Tools" "FAIL" "${TOOLS_DETAILS}"
        exit_code=1
    fi

    # Stages 3-4 — CMake configure + build (only when prerequisites pass)
    if [[ "${QT6_OK}" -eq 1 && "${TOOLS_OK}" -eq 1 ]]; then

        # Stage 3 — CMake configure
        if cmake_configure; then
            CMAKE_CONFIG_OK=1
            mark_result "CMake configure" "PASS" "${CMAKE_CONFIG_DETAILS}"
        else
            mark_result "CMake configure" "FAIL" "${CMAKE_CONFIG_DETAILS}"
            exit_code=1
        fi

        # Stage 4 — CMake build (translations + lib + GUI + CLI in one pass)
        if [[ "${CMAKE_CONFIG_OK}" -eq 1 ]]; then
            if cmake_build; then
                CMAKE_BUILD_OK=1
                mark_result "CMake build" "PASS" "${CMAKE_BUILD_DETAILS}"
            else
                mark_result "CMake build" "FAIL" "${CMAKE_BUILD_DETAILS}"
                exit_code=1
            fi

            if [[ "${GUI_BUILD_OK}" -eq 1 ]]; then
                mark_result "Build: GUI" "PASS" "${GUI_DETAILS}"
            else
                mark_result "Build: GUI" "FAIL" "${GUI_DETAILS}"
                exit_code=1
            fi

            if [[ "${CLI_BUILD_OK}" -eq 1 ]]; then
                mark_result "Build: CLI" "PASS" "${CLI_DETAILS}"
            else
                mark_result "Build: CLI" "FAIL" "${CLI_DETAILS}"
                exit_code=1
            fi
        else
            mark_result "CMake build"    "SKIP" "Configure failed"
            mark_result "Build: GUI"     "SKIP" "Configure failed"
            mark_result "Build: CLI"     "SKIP" "Configure failed"
        fi

    else
        mark_result "CMake configure" "SKIP" "Prerequisites unavailable"
        mark_result "CMake build"     "SKIP" "Prerequisites unavailable"
        mark_result "Build: GUI"      "SKIP" "Prerequisites unavailable"
        mark_result "Build: CLI"      "SKIP" "Prerequisites unavailable"
    fi

    # Stage 5 — CLI smoke test
    if [[ "${CLI_BUILD_OK}" -eq 1 ]]; then
        if smoke_test_cli; then
            mark_result "Smoke: CLI" "PASS" "${CLI_SMOKE_DETAILS}"
        else
            mark_result "Smoke: CLI" "FAIL" "${CLI_SMOKE_DETAILS}"
            exit_code=1
        fi
    else
        mark_result "Smoke: CLI" "SKIP" "CLI build not available"
    fi

    # Stage 6 — GUI binary smoke test
    if [[ "${GUI_BUILD_OK}" -eq 1 ]]; then
        if smoke_test_gui_binary; then
            mark_result "Smoke: GUI binary" "PASS" "${GUI_SMOKE_DETAILS}"
        else
            mark_result "Smoke: GUI binary" "FAIL" "${GUI_SMOKE_DETAILS}"
            exit_code=1
        fi
    else
        mark_result "Smoke: GUI binary" "SKIP" "GUI build not available"
    fi

    # Stage 7 — Translations
    if check_translations; then
        mark_result "Translations" "PASS" "${TRANSLATION_DETAILS}"
    else
        mark_result "Translations" "FAIL" "${TRANSLATION_DETAILS}"
        exit_code=1
    fi

    # Stage 8 — Unit tests
    if run_unit_tests; then
        mark_result "Unit Tests" "PASS" "QTest suite passed"
    else
        mark_result "Unit Tests" "FAIL" "CTest suite missing or failed"
        exit_code=1
    fi

    if [[ "${QUICK}" == true ]]; then
        mark_result "Full qualification" "SKIP" "explicit --quick diagnostic subset"
    elif [[ "${CMAKE_BUILD_OK}" -eq 1 ]]; then
        full_quality_checks || exit_code=1
    else
        mark_result "Full qualification" "FAIL" "build unavailable"
        exit_code=1
    fi

    # Stage 9 — Launch app
    if [[ "${GUI_BUILD_OK}" -eq 1 ]]; then
        if [[ "${RUN_APP}" == true ]]; then
            if launch_application; then
                mark_result "Launch App" "PASS" "qtscrob started (detached)"
            else
                mark_result "Launch App" "WARN" "Launch skipped or failed (non-fatal)"
            fi
        else
            mark_result "Launch App" "SKIP" "Suppressed by --noRun"
        fi
    else
        mark_result "Launch App" "SKIP" "GUI build not available"
    fi

    # Final verdict
    if [[ "${exit_code}" -eq 0 ]]; then
        log "Pipeline completed successfully."
    else
        error "Pipeline completed with one or more failing mandatory stages."
    fi

    print_summary
    printf '%s\n' "${SUMMARY_LINES[@]}" > "${PIPELINE_LOG_DIR}/summary.txt"
    exit "${exit_code}"
}

main "$@"
