cmake_minimum_required(VERSION 3.28)
project(QtScrobbler LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Treat every warning as an error — zero-warning policy
add_compile_options(-Wall -Werror)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# All binaries land in the build root so the pipeline can find them easily
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Qt6
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Network Sql Xml Widgets)

# Explicit selection keeps device code in the required quality denominator.
option(QTSCROBBLER_MTP "Build the Linux libmtp adapter" ON)
set(HAVE_LIBMTP FALSE)
if(QTSCROBBLER_MTP)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBMTP REQUIRED IMPORTED_TARGET libmtp)
    set(HAVE_LIBMTP TRUE)
    message(STATUS "libmtp ${LIBMTP_VERSION} found — MTP device support enabled")
else()
    message(STATUS "MTP device support explicitly disabled")
endif()

# Compile catalogs into each build directory; parallel builds never share generated files.
find_program(LRELEASE_EXECUTABLE NAMES lrelease
    HINTS "${Qt6_DIR}/../../../bin" NO_DEFAULT_PATH REQUIRED)
set(TRANSLATION_DIR "${CMAKE_SOURCE_DIR}/src/language")
set(TRANSLATION_OUTPUT_DIR "${CMAKE_BINARY_DIR}/translations")
file(MAKE_DIRECTORY "${TRANSLATION_OUTPUT_DIR}")
foreach(language IN ITEMS de pl)
    add_custom_command(
        OUTPUT "${TRANSLATION_OUTPUT_DIR}/${language}.qm"
        COMMAND "${LRELEASE_EXECUTABLE}" -silent "${TRANSLATION_DIR}/${language}.ts"
                -qm "${TRANSLATION_OUTPUT_DIR}/${language}.qm"
        DEPENDS "${TRANSLATION_DIR}/${language}.ts"
        VERBATIM)
endforeach()
add_custom_target(translations_qm ALL DEPENDS
    "${TRANSLATION_OUTPUT_DIR}/de.qm" "${TRANSLATION_OUTPUT_DIR}/pl.qm")
file(READ "${CMAKE_SOURCE_DIR}/src/qt/qtscrob.qrc" QTSCROBBLER_RESOURCE_CONTENT)
# Absolute source paths change the resource name; keep the relative name as an alias
# so the code's ":/resources/..." lookups still resolve.
string(REGEX REPLACE "<file>resources/([^<]*)</file>"
    "<file alias=\"resources/\\1\">${CMAKE_SOURCE_DIR}/src/qt/resources/\\1</file>"
    QTSCROBBLER_RESOURCE_CONTENT "${QTSCROBBLER_RESOURCE_CONTENT}")
string(REPLACE ">resources/" ">${CMAKE_SOURCE_DIR}/src/qt/resources/"
    QTSCROBBLER_RESOURCE_CONTENT "${QTSCROBBLER_RESOURCE_CONTENT}")
string(REPLACE ">../language/" ">${TRANSLATION_OUTPUT_DIR}/"
    QTSCROBBLER_RESOURCE_CONTENT "${QTSCROBBLER_RESOURCE_CONTENT}")
set(QTSCROBBLER_RESOURCE_FILE "${CMAKE_BINARY_DIR}/qtscrob.qrc")
file(CONFIGURE OUTPUT "${QTSCROBBLER_RESOURCE_FILE}" CONTENT "${QTSCROBBLER_RESOURCE_CONTENT}" @ONLY)

file(READ "${CMAKE_SOURCE_DIR}/src/cli/scrobbler.qrc" QTSCROBBLER_CLI_RESOURCE_CONTENT)
string(REPLACE ">../language/" ">${TRANSLATION_OUTPUT_DIR}/"
    QTSCROBBLER_CLI_RESOURCE_CONTENT "${QTSCROBBLER_CLI_RESOURCE_CONTENT}")
set(QTSCROBBLER_CLI_RESOURCE_FILE "${CMAKE_BINARY_DIR}/scrobbler.qrc")
file(CONFIGURE OUTPUT "${QTSCROBBLER_CLI_RESOURCE_FILE}" CONTENT "${QTSCROBBLER_CLI_RESOURCE_CONTENT}" @ONLY)

add_subdirectory(src/lib)
add_subdirectory(src/qt)
add_subdirectory(src/cli)
include(cmake/Instrumentation.cmake)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
