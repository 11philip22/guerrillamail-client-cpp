include(Catch)

add_executable(guerrillamail-cpp-tests
    test_main.cpp
    unit/smoke_test.cpp
    support/test_paths.cpp
)

target_include_directories(guerrillamail-cpp-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(guerrillamail-cpp-tests
    PRIVATE
        guerrillamail::guerrillamail-cpp
        Catch2::Catch2WithMain
)

target_compile_definitions(guerrillamail-cpp-tests
    PRIVATE
        GUERRILLAMAIL_CPP_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

catch_discover_tests(guerrillamail-cpp-tests)
