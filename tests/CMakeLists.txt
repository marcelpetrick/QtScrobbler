foreach(name IN ITEMS parse_log common cli gui conf dbcache parse_ipod)
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
target_sources(test_gui PRIVATE "${PROJECT_SOURCE_DIR}/src/qt/qtscrob.qrc")
add_dependencies(test_gui translations_qm)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME pipeline_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_pipeline.py")
set_tests_properties(pipeline_contract PROPERTIES TIMEOUT 40)
