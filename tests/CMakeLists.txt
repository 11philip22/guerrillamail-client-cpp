include(Catch)

add_executable(guerrillamail-cpp-tests
    unit/bootstrap_test.cpp
    unit/parsing_test.cpp
    unit/requests_test.cpp
    integration/live_protocol_validation_test.cpp
    integration/live_end_to_end_test.cpp
)

target_include_directories(guerrillamail-cpp-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(guerrillamail-cpp-tests
    PRIVATE
        guerrillamail::guerrillamail-cpp
        Catch2::Catch2WithMain
)

catch_discover_tests(guerrillamail-cpp-tests)
