foreach(name IN ITEMS parse_log common cli gui conf dbcache parse_ipod submit gettrackinfo scrobble)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE scrobble Qt6::Test)
    target_compile_definitions(test_${name} PRIVATE
        EXAMPLE_LOG="${PROJECT_SOURCE_DIR}/examples/scrobble.log"
        CLI_BINARY="$<TARGET_FILE:scrobbler>"
    )
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;TZ=UTC"
    )
endforeach()
add_dependencies(test_cli scrobbler)
target_link_libraries(test_gui PRIVATE scrobble_gui)
target_sources(test_gui PRIVATE "${QTSCROBBLER_RESOURCE_FILE}")
add_dependencies(test_gui translations_qm)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME pipeline_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_pipeline.py")
set_tests_properties(pipeline_contract PROPERTIES TIMEOUT 40)

if(HAVE_LIBMTP)
    add_executable(test_parse_mtp test_parse_mtp.cpp)
    target_link_libraries(test_parse_mtp PRIVATE scrobble Qt6::Test PkgConfig::LIBMTP)
    add_test(NAME parse_mtp COMMAND test_parse_mtp)
    set_tests_properties(parse_mtp PROPERTIES TIMEOUT 20 ENVIRONMENT "TZ=UTC;QT_QPA_PLATFORM=offscreen")
endif()

target_include_directories(test_gui PRIVATE "${PROJECT_BINARY_DIR}/src/qt/scrobble_gui_autogen/include")
target_compile_definitions(test_gui PRIVATE GUI_BINARY="$<TARGET_FILE:qtscrob>")
add_dependencies(test_gui qtscrob)
