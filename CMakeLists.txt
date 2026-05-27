cmake_minimum_required(VERSION 3.16)
project(QtScrobbler LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# All binaries land in the build root so the pipeline can find them easily
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Qt6
find_package(Qt6 6.0 REQUIRED COMPONENTS Core Network Sql Xml Widgets)

# Optional MTP support via libmtp (pkg-config)
find_package(PkgConfig QUIET)
set(HAVE_LIBMTP FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBMTP libmtp)
    if(LIBMTP_FOUND)
        set(HAVE_LIBMTP TRUE)
        message(STATUS "libmtp ${LIBMTP_VERSION} found — MTP device support enabled")
    else()
        message(STATUS "libmtp not found — MTP device support disabled")
    endif()
else()
    message(STATUS "pkg-config not found — MTP device support disabled")
endif()

# Translation catalogs (.ts -> .qm) — must exist before the .qrc files are processed
find_program(LRELEASE_EXECUTABLE NAMES lrelease lrelease-qt6
    HINTS "${Qt6_DIR}/../../../bin"
)
set(TRANSLATION_DIR "${CMAKE_SOURCE_DIR}/src/language")
if(LRELEASE_EXECUTABLE)
    add_custom_command(
        OUTPUT
            "${TRANSLATION_DIR}/de.qm"
            "${TRANSLATION_DIR}/pl.qm"
        COMMAND "${LRELEASE_EXECUTABLE}" -silent "${TRANSLATION_DIR}/language.pro"
        DEPENDS
            "${TRANSLATION_DIR}/de.ts"
            "${TRANSLATION_DIR}/pl.ts"
        WORKING_DIRECTORY "${TRANSLATION_DIR}"
        COMMENT "Compiling Qt translations (.ts -> .qm)"
    )
    add_custom_target(translations_qm ALL
        DEPENDS "${TRANSLATION_DIR}/de.qm" "${TRANSLATION_DIR}/pl.qm"
    )
else()
    message(WARNING "lrelease not found — translations will not be compiled automatically")
    add_custom_target(translations_qm)
endif()

add_subdirectory(src/lib)
add_subdirectory(src/qt)
add_subdirectory(src/cli)
