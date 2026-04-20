include(Catch)

add_executable(guerrillamail-cpp-tests
    test_main.cpp
    unit/bootstrap_test.cpp
    unit/error_test.cpp
    unit/parsing_test.cpp
    unit/requests_test.cpp
    integration/client_bootstrap_test.cpp
    integration/live_protocol_validation_test.cpp
    unit/smoke_test.cpp
    integration/curl_session_test.cpp
    support/mock_http_server.cpp
    support/test_paths.cpp
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

if(WIN32)
    target_link_libraries(guerrillamail-cpp-tests PRIVATE ws2_32)
endif()

target_compile_definitions(guerrillamail-cpp-tests
    PRIVATE
        GUERRILLAMAIL_CPP_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

catch_discover_tests(guerrillamail-cpp-tests)
