set(GUI_SOURCES
    src/main.cpp
    src/qtscrob.cpp
    src/settings.cpp
    src/about.cpp
    src/help.cpp
    src/progress.cpp
    src/console.cpp
    src/missingtimeprogress.cpp
    qtscrob.qrc
)

add_executable(qtscrob ${GUI_SOURCES})

# Ensure .ui files are found by AUTOUIC when scanning headers
set_target_properties(qtscrob PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

# Ensure .qm files are generated before the .qrc is processed
add_dependencies(qtscrob translations_qm)

target_include_directories(qtscrob PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_SOURCE_DIR}/src/common"
)

target_link_libraries(qtscrob
    PRIVATE
        scrobble
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
        Qt6::Xml
)
