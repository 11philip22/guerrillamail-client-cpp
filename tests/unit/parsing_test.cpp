#include <catch2/catch_test_macros.hpp>

#include "guerrillamail/error.hpp"
#include "protocol/parsing.hpp"

TEST_CASE("invalid json syntax becomes json_parse", "[parsing]") {
    try {
        (void)guerrillamail::protocol::parsing::parse_json("{");
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::json_parse);
    }
}

TEST_CASE("missing required member becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(R"({"ok":true})");

    try {
        (void)guerrillamail::protocol::parsing::require_string_member(json, "email_addr");
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}

TEST_CASE("wrong member type becomes response_parse", "[parsing]") {
    const auto json = guerrillamail::protocol::parsing::parse_json(R"({"email_addr":123})");

    try {
        (void)guerrillamail::protocol::parsing::require_string_member(json, "email_addr");
        FAIL("expected exception");
    } catch (const guerrillamail::Error& error) {
        REQUIRE(error.code() == guerrillamail::ErrorCode::response_parse);
    }
}
