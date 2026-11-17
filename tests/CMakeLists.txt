foreach(name IN ITEMS parse_log common cli)
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
