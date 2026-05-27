set(CLI_SOURCES
    scrobbler.cpp
    app.cpp
    scrobbler.qrc
)

add_executable(scrobbler ${CLI_SOURCES})

# Ensure .qm files are generated before the .qrc is processed
add_dependencies(scrobbler translations_qm)

target_include_directories(scrobbler PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src/lib"
    "${CMAKE_SOURCE_DIR}/src/common"
)

target_link_libraries(scrobbler
    PRIVATE
        scrobble
        Qt6::Core
        Qt6::Network
        Qt6::Sql
        Qt6::Xml
)
