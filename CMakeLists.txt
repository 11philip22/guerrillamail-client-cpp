cmake_minimum_required(VERSION 3.22)

project(guerrillamail-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(_GUERRILLAMAIL_CPP_STANDALONE ON)
else()
    set(_GUERRILLAMAIL_CPP_STANDALONE OFF)
endif()

option(GUERRILLAMAIL_CPP_BUILD_TESTS    "Build guerrillamail-cpp tests"    ${_GUERRILLAMAIL_CPP_STANDALONE})
option(GUERRILLAMAIL_CPP_BUILD_EXAMPLES "Build guerrillamail-cpp examples" ${_GUERRILLAMAIL_CPP_STANDALONE})

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "CMAKE_TOOLCHAIN_FILE is not set. Expected vcpkg toolchain: third_party/vcpkg/scripts/buildsystems/vcpkg.cmake")
elseif(NOT CMAKE_TOOLCHAIN_FILE MATCHES "third_party[/\\]vcpkg[/\\]scripts[/\\]buildsystems[/\\]vcpkg\.cmake$")
    message(STATUS "Using non-default toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(guerrillamail-cpp STATIC
    src/client.cpp
    src/error.cpp
    src/transport/curl_session.cpp
    src/protocol/bootstrap.cpp
    src/protocol/requests.cpp
    src/protocol/parsing.cpp
)

add_library(guerrillamail::guerrillamail-cpp ALIAS guerrillamail-cpp)

target_compile_features(guerrillamail-cpp PUBLIC cxx_std_20)

target_include_directories(guerrillamail-cpp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(guerrillamail-cpp
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

if(GUERRILLAMAIL_CPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(GUERRILLAMAIL_CPP_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 3 REQUIRED)
    add_subdirectory(tests)
endif()
