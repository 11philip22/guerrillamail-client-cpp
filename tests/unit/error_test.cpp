#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/error.hpp"

TEST_CASE("error keeps code and optional http status", "[error]") {
    const guerrillamail::Error status_error(
        guerrillamail::ErrorCode::http_status,
        "bad status",
        503
    );

    REQUIRE(status_error.code() == guerrillamail::ErrorCode::http_status);
    REQUIRE(status_error.http_status().has_value());
    REQUIRE(status_error.http_status().value() == 503);
}

TEST_CASE("error without status reports nullopt", "[error]") {
    const guerrillamail::Error error(guerrillamail::ErrorCode::transport, "boom");

    REQUIRE(error.code() == guerrillamail::ErrorCode::transport);
    REQUIRE_FALSE(error.http_status().has_value());
}
