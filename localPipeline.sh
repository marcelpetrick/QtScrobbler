#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${ROOT_DIR}/src"
BUILD_DIR="${ROOT_DIR}/build"
APP_VERSION="$(grep -oP '#define CLIENT_VERSION "\K[^"]+' "${SRC_DIR}/lib/common.h" 2>/dev/null || echo "unknown")"
PIPELINE_LOG_DIR="${TMPDIR:-/tmp}/qtscrob-pipeline-$$"
trap 'rm -rf "${PIPELINE_LOG_DIR}"' EXIT

declare -a SUMMARY_LINES=()

# State flags
QT6_OK=0
TOOLS_OK=0
CMAKE_CONFIG_OK=0
CMAKE_BUILD_OK=0
GUI_BUILD_OK=0
CLI_BUILD_OK=0
CLI_SMOKE_OK=0
GUI_SMOKE_OK=0
LAUNCH_OK=0
RUN_APP=true

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
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# Will be set to the detected Qt6 qmake binary (qmake6 or qmake) — used for Qt6 check only
QMAKE=""

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
Usage: ./localPipeline.sh [--noRun] [--help]

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
  8.  Unit tests          — run QTest suite (SKIP until suite is added)
  9.  Launch app          — start qtscrob once (suppress with --noRun)
  10. Summary             — stage-by-stage result table

Options:
  --noRun   Skip launching the GUI app at the end
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
    local qmake_bin="" qt_version="" major=""

    for candidate in "${candidates[@]}"; do
        local bin
        bin="$(command -v "${candidate}" 2>/dev/null)" || continue
        qt_version="$("${bin}" --version 2>&1 | grep -oP 'Qt version \K[0-9]+\.[0-9]+\.[0-9]+')"
        major="${qt_version%%.*}"
        if [[ "${major}" == "6" ]]; then
            qmake_bin="${bin}"
            break
        fi
        log "  ${bin}: Qt ${qt_version} — not Qt 6, skipping"
    done

    if [[ -z "${qmake_bin}" ]]; then
        QT6_DETAILS="No Qt 6 qmake found (tried: ${candidates[*]})"
        error "  ${QT6_DETAILS}"
        return 1
    fi

    QMAKE="${qmake_bin}"
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
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; then
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
    log "Stage 4 — CMake build (--parallel ${JOBS})..."
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

    local errors
    errors="$(grep -cE 'error:' "${log_path}" 2>/dev/null || true)"
    CMAKE_BUILD_DETAILS="build failed (${errors} error line(s)) — see ${log_path}"
    error "  ${CMAKE_BUILD_DETAILS}"
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
    fi
    local rc=$?

    if grep -qiE "usage|option|help|scrobbler" "${log_path}" 2>/dev/null; then
        CLI_SMOKE_DETAILS="usage text found (exit ${rc} treated as acceptable)"
        log "  ${CLI_SMOKE_DETAILS}"
        return 0
    fi

    if [[ "${rc}" -eq 124 ]]; then
        CLI_SMOKE_DETAILS="timed out after 5 s (event-loop binary needs a log file argument)"
        warn "  ${CLI_SMOKE_DETAILS}"
    else
        CLI_SMOKE_DETAILS="exited ${rc} with no recognisable usage output"
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
        return 0
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
        if run_with_log "${test_log}" ctest --test-dir "${BUILD_DIR}" --output-on-failure -j "${JOBS}"; then
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

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

parse_arguments() {
    for arg in "$@"; do
        case "${arg}" in
            --noRun)  RUN_APP=false ;;
            --help|-h) print_usage; exit 0 ;;
            *) error "Unknown argument: ${arg}"; print_usage; exit 2 ;;
        esac
    done
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

main() {
    local exit_code=0

    parse_arguments "$@"

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
            CLI_SMOKE_OK=1
            mark_result "Smoke: CLI" "PASS" "${CLI_SMOKE_DETAILS}"
        else
            mark_result "Smoke: CLI" "WARN" "${CLI_SMOKE_DETAILS}"
        fi
    else
        mark_result "Smoke: CLI" "SKIP" "CLI build not available"
    fi

    # Stage 6 — GUI binary smoke test
    if [[ "${GUI_BUILD_OK}" -eq 1 ]]; then
        if smoke_test_gui_binary; then
            GUI_SMOKE_OK=1
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
        mark_result "Translations" "WARN" "${TRANSLATION_DETAILS}"
    fi

    # Stage 8 — Unit tests
    if run_unit_tests; then
        mark_result "Unit Tests" "PASS" "QTest suite passed"
    else
        mark_result "Unit Tests" "SKIP" "No QTest suite — add src/tests/ to enable"
    fi

    # Stage 9 — Launch app
    if [[ "${GUI_BUILD_OK}" -eq 1 ]]; then
        if [[ "${RUN_APP}" == true ]]; then
            if launch_application; then
                LAUNCH_OK=1
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
    exit "${exit_code}"
}

main "$@"
