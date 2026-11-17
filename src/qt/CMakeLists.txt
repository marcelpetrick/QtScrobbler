set(GUI_SOURCES
    src/qtscrob.cpp
    src/settings.cpp
    src/about.cpp
    src/help.cpp
    src/progress.cpp
    src/console.cpp
    src/missingtimeprogress.cpp
)

add_library(scrobble_gui STATIC ${GUI_SOURCES})

# Ensure .ui files are found by AUTOUIC when scanning headers
set_target_properties(scrobble_gui PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

# Ensure .qm files are generated before the .qrc is processed
add_dependencies(scrobble_gui translations_qm)

target_include_directories(scrobble_gui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_SOURCE_DIR}/src/common"
)

target_link_libraries(scrobble_gui
    PUBLIC
        scrobble
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
        Qt6::Xml
)

add_executable(qtscrob src/main.cpp qtscrob.qrc)
add_dependencies(qtscrob translations_qm)
target_link_libraries(qtscrob PRIVATE scrobble_gui)
