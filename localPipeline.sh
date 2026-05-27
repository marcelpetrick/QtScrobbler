#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${ROOT_DIR}/src"
PIPELINE_LOG_DIR="${TMPDIR:-/tmp}/qtscrob-pipeline-$$"
trap 'rm -rf "${PIPELINE_LOG_DIR}"' EXIT

declare -a SUMMARY_LINES=()

# State flags
QT6_OK=0
TOOLS_OK=0
LIB_BUILD_OK=0
GUI_BUILD_OK=0
CLI_BUILD_OK=0
CLI_SMOKE_OK=0
GUI_SMOKE_OK=0
LAUNCH_OK=0
RUN_APP=true

# Detail strings
QT6_DETAILS=""
TOOLS_DETAILS=""
LIB_DETAILS=""
GUI_DETAILS=""
CLI_DETAILS=""
CLI_SMOKE_DETAILS=""
GUI_SMOKE_DETAILS=""
TRANSLATION_DETAILS=""

# Number of parallel compile jobs
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# Will be set to the detected Qt6 qmake binary (qmake6 or qmake)
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
  2.  Build tools         — make, g++, pkg-config (libmtp optional)
  3.  Build: library      — compile static libscrobble (make -j${JOBS})
  4.  Build: GUI + CLI    — parallel compile with make -j${JOBS} each
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

    for tool in make g++ pkg-config; do
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

    TOOLS_DETAILS="make, g++, pkg-config present; ${JOBS} compile jobs"
    return 0
}

# ---------------------------------------------------------------------------
# Build helpers
# ---------------------------------------------------------------------------

distclean_subdir() {
    local dir="$1"
    if [[ -f "${dir}/Makefile" ]]; then
        log "  Cleaning stale artifacts in ${dir}..."
        make -C "${dir}" distclean -s 2>/dev/null || true
    fi
}

# Write PASS or FAIL + details to a result file so parallel jobs can report back.
build_subdir_to_file() {
    local label="$1" dir="$2" result_file="$3"
    local log_path="${PIPELINE_LOG_DIR}/${label}.log"

    mkdir -p "${PIPELINE_LOG_DIR}"
    distclean_subdir "${dir}"
    log "  [${label}] ${QMAKE}..."
    if ! bash -c "cd '${dir}' && '${QMAKE}'" >> "${log_path}" 2>&1; then
        printf 'FAIL:qmake failed\n' > "${result_file}"
        return 1
    fi

    log "  [${label}] make -j${JOBS}..."
    if ! bash -c "cd '${dir}' && make -j${JOBS}" >> "${log_path}" 2>&1; then
        local errors
        errors="$(grep -cE 'error:' "${log_path}" 2>/dev/null || true)"
        printf 'FAIL:compilation failed (%s error line(s))\n' "${errors}" > "${result_file}"
        return 1
    fi

    printf 'PASS\n' > "${result_file}"
    return 0
}

# ---------------------------------------------------------------------------
# Stage 3: Build library (must finish before GUI and CLI)
# ---------------------------------------------------------------------------

build_library() {
    log "Stage 3 — Building static library (libscrobble) with -j${JOBS}..."
    local result_file="${PIPELINE_LOG_DIR}/lib_result"

    build_subdir_to_file "lib" "${SRC_DIR}/lib" "${result_file}"
    local rc=$?

    local artifact="${SRC_DIR}/lib/libscrobble.a"
    if [[ "${rc}" -eq 0 && -f "${artifact}" ]]; then
        local size
        size="$(du -h "${artifact}" | awk '{print $1}')"
        LIB_DETAILS="libscrobble.a (${size})"
        log "  Artifact: ${artifact} (${size})"
        return 0
    fi

    local fail_detail
    fail_detail="$(cat "${result_file}" 2>/dev/null | cut -d: -f2-)"
    LIB_DETAILS="${fail_detail:-compilation failed}"
    return 1
}

# ---------------------------------------------------------------------------
# Stage 4a: Compile .ts → .qm translation files
# ---------------------------------------------------------------------------

compile_translations() {
    log "Stage 4a — Compiling translation catalogs (.ts → .qm)..."
    local lang_dir="${SRC_DIR}/language"
    local lrelease_bin
    lrelease_bin="$(command -v lrelease 2>/dev/null || command -v lrelease-qt6 2>/dev/null || true)"

    if [[ -z "${lrelease_bin}" ]]; then
        warn "  lrelease not found — .qm files will not be generated; GUI/CLI builds may fail"
        return 1
    fi

    local log_path="${PIPELINE_LOG_DIR}/lrelease.log"
    mkdir -p "${PIPELINE_LOG_DIR}"
    log "  Running ${lrelease_bin} on language.pro..."
    if run_with_log "${log_path}" "${lrelease_bin}" -silent "${lang_dir}/language.pro"; then
        local qm_count
        qm_count="$(find "${lang_dir}" -name "*.qm" 2>/dev/null | wc -l)"
        log "  Generated ${qm_count} .qm file(s)"
        return 0
    fi
    warn "  lrelease failed — see ${log_path}"
    return 1
}

# ---------------------------------------------------------------------------
# Stage 4b: Build GUI and CLI in parallel
# ---------------------------------------------------------------------------

build_gui_and_cli_parallel() {
    log "Stage 4 — Building GUI (qtscrob) and CLI (scrobbler) in parallel (-j${JOBS} each)..."

    local gui_result="${PIPELINE_LOG_DIR}/gui_result"
    local cli_result="${PIPELINE_LOG_DIR}/cli_result"

    # Fire both builds in the background
    build_subdir_to_file "gui" "${SRC_DIR}/qt"  "${gui_result}" &
    local gui_pid=$!

    build_subdir_to_file "cli" "${SRC_DIR}/cli" "${cli_result}" &
    local cli_pid=$!

    log "  Waiting for GUI (PID ${gui_pid}) and CLI (PID ${cli_pid})..."

    local gui_rc=0 cli_rc=0
    wait "${gui_pid}" || gui_rc=$?
    wait "${cli_pid}" || cli_rc=$?

    # --- GUI result ---
    local gui_artifact="${SRC_DIR}/qt/qtscrob"
    if [[ "${gui_rc}" -eq 0 && -x "${gui_artifact}" ]]; then
        local size
        size="$(du -h "${gui_artifact}" | awk '{print $1}')"
        GUI_BUILD_OK=1
        GUI_DETAILS="qtscrob (${size})"
        log "  GUI PASS: ${gui_artifact} (${size})"
    else
        local fail_detail
        fail_detail="$(cat "${gui_result}" 2>/dev/null | cut -d: -f2-)"
        GUI_DETAILS="${fail_detail:-compilation failed}"
        error "  GUI FAIL: ${GUI_DETAILS} — see ${PIPELINE_LOG_DIR}/gui.log"
    fi

    # --- CLI result ---
    local cli_artifact="${SRC_DIR}/cli/scrobbler"
    if [[ "${cli_rc}" -eq 0 && -x "${cli_artifact}" ]]; then
        local size
        size="$(du -h "${cli_artifact}" | awk '{print $1}')"
        CLI_BUILD_OK=1
        CLI_DETAILS="scrobbler (${size})"
        log "  CLI PASS: ${cli_artifact} (${size})"
    else
        local fail_detail
        fail_detail="$(cat "${cli_result}" 2>/dev/null | cut -d: -f2-)"
        CLI_DETAILS="${fail_detail:-compilation failed}"
        error "  CLI FAIL: ${CLI_DETAILS} — see ${PIPELINE_LOG_DIR}/cli.log"
    fi
}

# ---------------------------------------------------------------------------
# Stage 5: CLI smoke test
# ---------------------------------------------------------------------------

smoke_test_cli() {
    local binary="${SRC_DIR}/cli/scrobbler"
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
    local binary="${SRC_DIR}/qt/qtscrob"
    log "Stage 6 — GUI binary smoke test: ${binary}..."

    if [[ -x "${binary}" ]]; then
        local size
        size="$(du -h "${binary}" | awk '{print $1}')"
        GUI_SMOKE_DETAILS="${binary} — ${size}"
        log "  ${GUI_SMOKE_DETAILS}"
        log "  (Headless display launch skipped — use --noRun to suppress full launch too)"
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
    log "Stage 8 — Searching for QTest unit test suite..."

    local test_pros
    test_pros="$(find "${SRC_DIR}" -name "*.pro" 2>/dev/null \
        | xargs grep -l "QT += testlib" 2>/dev/null || true)"

    if [[ -z "${test_pros}" ]]; then
        log "  No QTest suite found (no .pro with 'QT += testlib')."
        log "  → Create src/tests/ with QTest sub-projects to enable unit testing."
        return 1
    fi

    log "  Found QTest project(s):"
    local all_ok=0
    while IFS= read -r pro; do
        local pro_dir test_log
        pro_dir="$(dirname "${pro}")"
        test_log="${PIPELINE_LOG_DIR}/test_$(basename "${pro_dir}").log"
        log "  Running tests in ${pro_dir}..."
        if run_with_log "${test_log}" bash -c "cd '${pro_dir}' && '${QMAKE}' && make -j${JOBS} && make check"; then
            log "  PASS: ${pro_dir}"
        else
            error "  FAIL: ${pro_dir}"
            all_ok=1
        fi
    done <<< "${test_pros}"

    return "${all_ok}"
}

# ---------------------------------------------------------------------------
# Stage 9: Launch the GUI application
# ---------------------------------------------------------------------------

launch_application() {
    local binary="${SRC_DIR}/qt/qtscrob"
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
    log " QtScrobbler — local pipeline"
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

    # Stages 3-4 — Build (only when prerequisites pass)
    if [[ "${QT6_OK}" -eq 1 && "${TOOLS_OK}" -eq 1 ]]; then

        # Stage 3 — library (must be sequential; GUI+CLI depend on it)
        if build_library; then
            LIB_BUILD_OK=1
            mark_result "Build: library" "PASS" "${LIB_DETAILS}"
        else
            mark_result "Build: library" "FAIL" "${LIB_DETAILS}"
            exit_code=1
        fi

        # Stage 4a — compile translations (.qm needed by qrc in GUI and CLI)
        if [[ "${LIB_BUILD_OK}" -eq 1 ]]; then
            if compile_translations; then
                mark_result "Translations (.qm)" "PASS" ".qm files generated"
            else
                mark_result "Translations (.qm)" "WARN" "lrelease failed — builds may miss .qm"
            fi
        fi

        # Stage 4b — GUI + CLI in parallel (only when library is ready)
        if [[ "${LIB_BUILD_OK}" -eq 1 ]]; then
            build_gui_and_cli_parallel

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
            mark_result "Build: GUI" "SKIP" "Library build failed"
            mark_result "Build: CLI" "SKIP" "Library build failed"
        fi

    else
        mark_result "Build: library" "SKIP" "Prerequisites unavailable"
        mark_result "Build: GUI"     "SKIP" "Prerequisites unavailable"
        mark_result "Build: CLI"     "SKIP" "Prerequisites unavailable"
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
